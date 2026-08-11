#pragma once

// 草图数据模型 (Sketcher/Part 方案的核心契约)。
// 一个草图 = XY 平面上的 2D 图元集合 + 几何/尺寸约束集合。
// 三方共用:
//   - WS1 草图绘制: 创建/编辑图元, 命中测试, 交互绘制
//   - WS2 约束求解: 把图元+约束映射到 planegcs, 求解后回写几何
//   - WS3 Part 特征: 读取闭环轮廓, 经 OCC 拉伸/旋转/扫掠/放样成 3D
//
// 持久化: 序列化为单个 YAML 字符串, 存于模型树草图节点 params["data"]
// (项目文件 params 仅支持标量字符串, 见 MainWindow::save_project)。
//
// 约定:
//   - 坐标系: XY 平面, Z=0, 单位毫米(与 OCC/Gmsh 一致)
//   - 角度: 弧度制
//   - id: 文档内单调递增, 删除不复用

#include <QString>
#include <vector>

namespace gmp {

struct SketchPoint2d {
  double x = 0.0;
  double y = 0.0;
};

bool operator==(const SketchPoint2d& a, const SketchPoint2d& b);
bool operator!=(const SketchPoint2d& a, const SketchPoint2d& b);

enum class SketchEntityType { Line, Circle, Arc };

// 图元上特征点的角色, 约束通过 (entity, role) 引用点
enum class SketchPointRole { None, Start, End, Center };

struct SketchEntity {
  int id = 0;
  SketchEntityType type = SketchEntityType::Line;
  SketchPoint2d p1;        // Line: 起点
  SketchPoint2d p2;        // Line: 终点
  SketchPoint2d center;    // Circle/Arc: 圆心
  double radius = 0.0;     // Circle/Arc: 半径
  double start_angle = 0.0;  // Arc: 起始角 (弧度, 相对 +X 轴)
  double end_angle = 0.0;    // Arc: 终止角 (弧度, 逆时针为正)
};

enum class SketchConstraintType {
  Coincident,      // 两点重合 (entity1.role1 与 entity2.role2)
  Horizontal,      // 线水平 (单图元)
  Vertical,        // 线竖直 (单图元)
  Parallel,        // 两线平行
  Perpendicular,   // 两线垂直
  EqualLength,     // 两线等长
  EqualRadius,     // 两圆/弧等半径
  Fixed,           // 固定点 (entity1.role1 锁定当前位置)
  Distance,        // 尺寸: 两点间距离 / 线长 (entity1 或 entity1+role)
  Radius,          // 尺寸: 圆/弧半径 (单图元)
  Angle            // 尺寸: 两线夹角 (弧度)
};

struct SketchConstraint {
  int id = 0;
  SketchConstraintType type = SketchConstraintType::Coincident;
  int entity1 = -1;                    // 图元 id, -1 表示不用
  int entity2 = -1;
  SketchPointRole role1 = SketchPointRole::None;
  SketchPointRole role2 = SketchPointRole::None;
  double value = 0.0;                  // 尺寸类约束 (Distance/Radius/Angle) 的目标值
  bool driving = true;                 // true=驱动尺寸(改值改图); false=参考尺寸(只读)
};

class SketchDocument {
 public:
  // ---- 图元 CRUD ----
  int add_entity(const SketchEntity& e);   // 忽略 e.id, 分配新 id 并返回
  bool remove_entity(int id);              // 同时删除引用该图元的约束
  const SketchEntity* entity(int id) const;
  SketchEntity* entity(int id);            // 直接改几何; 约束求解由调用方触发
  std::vector<SketchEntity> entities() const { return entities_; }
  int entity_count() const { return static_cast<int>(entities_.size()); }

  // ---- 约束 CRUD ----
  int add_constraint(const SketchConstraint& c);  // 忽略 c.id, 分配新 id 并返回
  bool remove_constraint(int id);
  const SketchConstraint* constraint(int id) const;
  SketchConstraint* constraint(int id);
  std::vector<SketchConstraint> constraints() const { return constraints_; }

  void clear();
  bool empty() const { return entities_.empty() && constraints_.empty(); }

  // ---- 几何工具 ----
  // 取图元上某角色的点坐标; role 不适用时返回 false
  // (Line: Start/End/Center=中点; Circle: Center; Arc: Start/End/Center)
  static bool point_at_role(const SketchEntity& e, SketchPointRole role,
                            SketchPoint2d* out);
  // 点到图元的最短距离
  static double distance_to_entity(const SketchEntity& e, const SketchPoint2d& pt);
  // 命中测试: 返回距离 <= tol 的最近图元 id, 无命中返回 -1
  int hit_test(const SketchPoint2d& pt, double tol) const;

  // 闭环检测: 返回所有简单闭环, 每个闭环为按邻接顺序排列的图元 id 列表。
  // 单独的 Circle 自身构成一个闭环; Line/Arc 通过端点吸附 (tol) 连成环。
  // 供 WS3 构建 OCC wire/face 使用。
  std::vector<std::vector<int>> closed_loops(double tol = 1e-6) const;

  // ---- 序列化 (YAML 字符串, 存模型树 params["data"]) ----
  QString to_yaml_string() const;
  bool from_yaml_string(const QString& yaml, QString* error = nullptr);

 private:
  std::vector<SketchEntity> entities_;
  std::vector<SketchConstraint> constraints_;
  int next_entity_id_ = 1;
  int next_constraint_id_ = 1;
};

}  // namespace gmp
