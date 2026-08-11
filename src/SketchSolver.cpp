#include "gmp/SketchSolver.h"

#include "gmp/SketchDocument.h"

// WS2 实装: 把 SketchDocument 映射到 planegcs 的 GCS::System, 求解后回写。
//
// 策略: 每次 solve 全量重建 System (参数 double 内存/GCS 图元/约束对象全部
// 活在函数内的容器中, 保证 applySolution 前指针有效), 不做增量复用, 避免
// 状态腐烂。
//
// 注意点 (与 src/OccBridge.cpp 的 planegcs_smoke 一致):
//   - System::solve() 在内部参数副本上迭代, 必须 applySolution() 后参数内存
//     才是解;
//   - 参数内存用 std::deque<double>, push_back 不失效已取出的指针;
//   - 弧用 GCS::Arc (center/rad/startAngle/endAngle + start/end 点),
//     角度参数即极角 (Circle::Value(u) = center + r*(cos u, sin u)),
//     内部规则 addConstraintArcRules 打 tag=-1, 不计入诊断/冲突统计。

#include <GCS.h>
#include <Geo.h>

#include <cmath>
#include <deque>
#include <unordered_map>
#include <vector>

namespace gmp {

namespace {

// 一个图元在 GCS 中的视图: 参数指针 + GCS 图元对象 (均指向共享参数内存)。
struct GcsEntity {
  int id = -1;
  SketchEntityType type = SketchEntityType::Line;
  double* cx = nullptr;   // Circle/Arc 圆心; Line 不用
  double* cy = nullptr;
  double* rad = nullptr;  // Circle/Arc 半径
  double* sa = nullptr;   // Arc 起/止极角
  double* ea = nullptr;
  GCS::Line line;         // type==Line 时有效 (p1/p2 指向参数内存)
  GCS::Circle circle;     // type==Circle 时有效
  GCS::Arc arc;           // type==Arc 时有效
};

// 单次求解的完整上下文: 所有 GCS 相关内存的生命周期宿主。
struct SolveContext {
  std::deque<double> params;      // 参数内存 (deque: 指针永不失效)
  std::deque<GcsEntity> entities; // GCS 图元 (deque: 引用永不失效)
  std::unordered_map<int, GcsEntity*> by_id;
  std::vector<double*> unknowns;  // 传给 declareUnknowns 的参数指针表
  int skipped = 0;                // 不支持的约束组合被跳过的数量

  // 分配一个参数 double 并登记为未知量
  double* alloc(double init) {
    params.push_back(init);
    double* p = &params.back();
    unknowns.push_back(p);
    return p;
  }

  // 分配一个固定值参数 (驱动尺寸的目标值/Fixed 的锁定坐标):
  // 不登记为未知量, 求解器把它当常量, 约束方程才会真正钉住几何
  double* alloc_fixed(double init) {
    params.push_back(init);
    return &params.back();
  }

  // 建图元: 分配参数内存, 构造 GCS 视图
  GcsEntity& add_entity(const SketchEntity& e) {
    entities.push_back(GcsEntity());
    GcsEntity& g = entities.back();
    g.id = e.id;
    g.type = e.type;
    switch (e.type) {
      case SketchEntityType::Line:
        g.line.p1 = GCS::Point(alloc(e.p1.x), alloc(e.p1.y));
        g.line.p2 = GCS::Point(alloc(e.p2.x), alloc(e.p2.y));
        break;
      case SketchEntityType::Circle:
        g.cx = alloc(e.center.x);
        g.cy = alloc(e.center.y);
        g.rad = alloc(e.radius);
        g.circle.center = GCS::Point(g.cx, g.cy);
        g.circle.rad = g.rad;
        break;
      case SketchEntityType::Arc: {
        g.cx = alloc(e.center.x);
        g.cy = alloc(e.center.y);
        g.rad = alloc(e.radius);
        g.sa = alloc(e.start_angle);
        g.ea = alloc(e.end_angle);
        const double sx = e.center.x + e.radius * std::cos(e.start_angle);
        const double sy = e.center.y + e.radius * std::sin(e.start_angle);
        const double ex = e.center.x + e.radius * std::cos(e.end_angle);
        const double ey = e.center.y + e.radius * std::sin(e.end_angle);
        g.arc.center = GCS::Point(g.cx, g.cy);
        g.arc.rad = g.rad;
        g.arc.startAngle = g.sa;
        g.arc.endAngle = g.ea;
        g.arc.start = GCS::Point(alloc(sx), alloc(sy));
        g.arc.end = GCS::Point(alloc(ex), alloc(ey));
        break;
      }
    }
    by_id[g.id] = &g;
    return g;
  }

  // 取图元上某角色对应的 GCS 点; 不支持的组合返回 false
  // (Line 的 Center=中点没有对应 GCS 参数点, v1 不支持)
  bool point_at(int entity_id, SketchPointRole role, GCS::Point* out) {
    auto it = by_id.find(entity_id);
    if (it == by_id.end() || !out) return false;
    GcsEntity& g = *it->second;
    switch (g.type) {
      case SketchEntityType::Line:
        if (role == SketchPointRole::Start) { *out = g.line.p1; return true; }
        if (role == SketchPointRole::End) { *out = g.line.p2; return true; }
        return false;
      case SketchEntityType::Circle:
        if (role == SketchPointRole::Center) { *out = g.circle.center; return true; }
        return false;
      case SketchEntityType::Arc:
        if (role == SketchPointRole::Start) { *out = g.arc.start; return true; }
        if (role == SketchPointRole::End) { *out = g.arc.end; return true; }
        if (role == SketchPointRole::Center) { *out = g.arc.center; return true; }
        return false;
    }
    return false;
  }

  GcsEntity* find(int entity_id) {
    auto it = by_id.find(entity_id);
    return it == by_id.end() ? nullptr : it->second;
  }

  // 取 GCS::Line 视图 (仅 Line 图元)
  GCS::Line* line_of(int entity_id) {
    GcsEntity* g = find(entity_id);
    return (g && g->type == SketchEntityType::Line) ? &g->line : nullptr;
  }

  // 添加一条约束; 不支持的组合跳过并计数 (返回 false)
  bool add_constraint(GCS::System& sys, const SketchConstraint& c) {
    const int tag = c.id;  // 用户约束用自身 id 作 tag, 供冲突诊断回溯
    switch (c.type) {
      case SketchConstraintType::Coincident: {
        GCS::Point p1, p2;
        if (!point_at(c.entity1, c.role1, &p1) ||
            !point_at(c.entity2, c.role2, &p2))
          return false;
        sys.addConstraintP2PCoincident(p1, p2, tag);
        return true;
      }
      case SketchConstraintType::Horizontal: {
        GCS::Line* l = line_of(c.entity1);
        if (!l) return false;
        sys.addConstraintHorizontal(*l, tag);
        return true;
      }
      case SketchConstraintType::Vertical: {
        GCS::Line* l = line_of(c.entity1);
        if (!l) return false;
        sys.addConstraintVertical(*l, tag);
        return true;
      }
      case SketchConstraintType::Parallel: {
        GCS::Line* l1 = line_of(c.entity1);
        GCS::Line* l2 = line_of(c.entity2);
        if (!l1 || !l2) return false;
        sys.addConstraintParallel(*l1, *l2, tag);
        return true;
      }
      case SketchConstraintType::Perpendicular: {
        GCS::Line* l1 = line_of(c.entity1);
        GCS::Line* l2 = line_of(c.entity2);
        if (!l1 || !l2) return false;
        sys.addConstraintPerpendicular(*l1, *l2, tag);
        return true;
      }
      case SketchConstraintType::EqualLength: {
        GCS::Line* l1 = line_of(c.entity1);
        GCS::Line* l2 = line_of(c.entity2);
        if (!l1 || !l2) return false;
        sys.addConstraintEqualLength(*l1, *l2, tag);
        return true;
      }
      case SketchConstraintType::EqualRadius: {
        GcsEntity* g1 = find(c.entity1);
        GcsEntity* g2 = find(c.entity2);
        if (!g1 || !g2 || g1->type == SketchEntityType::Line ||
            g2->type == SketchEntityType::Line)
          return false;
        // Arc 继承自 Circle, 圆/弧任意组合都可走 (Circle, Circle) 重载
        GCS::Circle& c1 = (g1->type == SketchEntityType::Arc)
                              ? static_cast<GCS::Circle&>(g1->arc)
                              : g1->circle;
        GCS::Circle& c2 = (g2->type == SketchEntityType::Arc)
                              ? static_cast<GCS::Circle&>(g2->arc)
                              : g2->circle;
        sys.addConstraintEqualRadius(c1, c2, tag);
        return true;
      }
      case SketchConstraintType::Fixed: {
        // 锁定点的当前坐标: X/Y 各加一条等值约束, 目标值即当前值
        GCS::Point p;
        if (!point_at(c.entity1, c.role1, &p)) return false;
        sys.addConstraintCoordinateX(p, alloc_fixed(*p.x), tag);
        sys.addConstraintCoordinateY(p, alloc_fixed(*p.y), tag);
        return true;
      }
      case SketchConstraintType::Distance: {
        if (!c.driving) return true;  // 参考尺寸: 不加约束, 仅保留数据
        GCS::Point p1, p2;
        if (c.entity2 >= 0) {
          // 两点间距离
          if (!point_at(c.entity1, c.role1, &p1) ||
              !point_at(c.entity2, c.role2, &p2))
            return false;
        } else {
          // 单图元: 线长 (两端点距离); 圆/弧请用 Radius 约束
          GCS::Line* l = line_of(c.entity1);
          if (!l) return false;
          p1 = l->p1;
          p2 = l->p2;
        }
        sys.addConstraintP2PDistance(p1, p2, alloc_fixed(c.value), tag);
        return true;
      }
      case SketchConstraintType::Radius: {
        if (!c.driving) return true;  // 参考尺寸: 跳过
        GcsEntity* g = find(c.entity1);
        if (!g || g->type == SketchEntityType::Line) return false;
        if (g->type == SketchEntityType::Arc)
          sys.addConstraintArcRadius(g->arc, alloc_fixed(c.value), tag);
        else
          sys.addConstraintCircleRadius(g->circle, alloc_fixed(c.value), tag);
        return true;
      }
      case SketchConstraintType::Angle: {
        if (!c.driving) return true;  // 参考尺寸: 跳过
        GCS::Line* l1 = line_of(c.entity1);
        GCS::Line* l2 = line_of(c.entity2);
        if (!l1 || !l2) return false;
        sys.addConstraintL2LAngle(*l1, *l2, alloc_fixed(c.value), tag);  // 弧度
        return true;
      }
    }
    return false;
  }
};

}  // namespace

SketchSolver::SketchSolver() = default;
SketchSolver::~SketchSolver() = default;

bool SketchSolver::solve(SketchDocument& doc, QString* error) {
  last_dof_ = -1;
  last_conflicts_ = 0;

  SolveContext ctx;
  GCS::System sys;

  // 1) 图元 -> GCS (参数内存全部在 ctx 内)
  const std::vector<SketchEntity> ents = doc.entities();
  for (const SketchEntity& e : ents) {
    GcsEntity& g = ctx.add_entity(e);
    if (e.type == SketchEntityType::Arc) {
      // 弧的内部规则: 起/止点钉在 Value(startAngle/endAngle) 上;
      // tag=-1 使其不参与自由度/冲突诊断
      sys.addConstraintArcRules(g.arc, -1);
    }
  }

  // 2) 约束 -> GCS
  for (const SketchConstraint& c : doc.constraints()) {
    if (!ctx.add_constraint(sys, c)) ++ctx.skipped;
  }

  // 空文档/无可解对象: 直接成功
  if (ctx.unknowns.empty()) {
    last_dof_ = 0;
    return true;
  }

  // 3) 求解 (solve 不写参数内存, 成功后需 applySolution)
  sys.declareUnknowns(ctx.unknowns);
  sys.initSolution();
  sys.diagnose();  // 填充 dofs/conflicting 诊断
  last_dof_ = sys.dofsNumber();

  // 诊断出矛盾约束 (如同一线长驱动到两个值): 直接判失败, 不回写
  // (tag=0 的约束被 planegcs 视为高优先级不参与冲突检测, 我们的用户约束
  //  tag 都是 >=1 的约束 id, 不受影响)
  if (sys.hasConflicting()) {
    GCS::VEC_I conflicting;
    sys.getConflicting(conflicting);
    last_conflicts_ = static_cast<int>(conflicting.size());
    if (error) {
      *error = QStringLiteral("约束矛盾 (冲突约束数: %1)")
                   .arg(last_conflicts_);
    }
    return false;
  }

  const int ret = sys.solve();
  if (ret != GCS::Success && ret != GCS::Converged &&
      ret != GCS::SuccessfulSolutionInvalid) {
    // 失败: 重新诊断取冲突约束数, 文档保持原样
    sys.diagnose();
    GCS::VEC_I conflicting;
    sys.getConflicting(conflicting);
    last_conflicts_ = static_cast<int>(conflicting.size());
    if (error) {
      *error = QStringLiteral(
          "约束求解失败 (矛盾或过约束, 冲突约束数: %1)")
          .arg(last_conflicts_);
    }
    return false;
  }

  sys.applySolution();

  // 4) 回写几何
  for (const SketchEntity& e : ents) {
    GcsEntity* g = ctx.find(e.id);
    SketchEntity* me = doc.entity(e.id);
    if (!g || !me) continue;
    switch (g->type) {
      case SketchEntityType::Line:
        me->p1 = {*g->line.p1.x, *g->line.p1.y};
        me->p2 = {*g->line.p2.x, *g->line.p2.y};
        break;
      case SketchEntityType::Circle:
        me->center = {*g->cx, *g->cy};
        me->radius = *g->rad;
        break;
      case SketchEntityType::Arc:
        me->center = {*g->cx, *g->cy};
        me->radius = *g->rad;
        // 角度从解出的起/止点反算 (比直接读角度参数更稳健)
        me->start_angle =
            std::atan2(*g->arc.start.y - *g->cy, *g->arc.start.x - *g->cx);
        me->end_angle =
            std::atan2(*g->arc.end.y - *g->cy, *g->arc.end.x - *g->cx);
        break;
    }
  }

  // 冗余约束不算失败, 但把冗余数记入冲突计数供 UI 提示? 不: 语义分开,
  // last_conflicts_ 仅在失败时填冲突数, 成功时保持 0。
  return true;
}

bool SketchSolver::set_driving_value(SketchDocument& doc, int constraint_id,
                                     double value, QString* error) {
  SketchConstraint* c = doc.constraint(constraint_id);
  if (!c) {
    if (error) *error = QStringLiteral("约束不存在: id=%1").arg(constraint_id);
    return false;
  }
  const bool is_dimension =
      c->type == SketchConstraintType::Distance ||
      c->type == SketchConstraintType::Radius ||
      c->type == SketchConstraintType::Angle;
  if (!is_dimension) {
    if (error)
      *error = QStringLiteral("约束 id=%1 不是尺寸类约束").arg(constraint_id);
    return false;
  }
  if (!c->driving) {
    if (error)
      *error = QStringLiteral("约束 id=%1 是参考尺寸, 不能驱动").arg(constraint_id);
    return false;
  }
  c->value = value;
  return solve(doc, error);
}

}  // namespace gmp
