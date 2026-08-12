#include "gmp/OccBridge.h"

#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

#include <gmsh.h>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GC_MakeSegment.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <GCS.h>
#include <Geo.h>

#include "gmp/SketchDocument.h"

namespace gmp {

// ---- WS3 内部辅助 (不导出) ----
namespace {

constexpr double kTwoPi = 2.0 * 3.14159265358979323846;

// 确保 gmsh 已初始化 (与 occ_direct_call_smoke 相同模式)
void ensure_gmsh() {
  if (!gmsh::isInitialized())
    gmsh::initialize();
}

// 单个草图图元 -> OCC edge (草图为 XY 平面, 毫米, 弧度制)
TopoDS_Edge edge_from_entity(const SketchEntity& e) {
  switch (e.type) {
    case SketchEntityType::Line: {
      GC_MakeSegment seg(gp_Pnt(e.p1.x, e.p1.y, 0.0),
                         gp_Pnt(e.p2.x, e.p2.y, 0.0));
      return BRepBuilderAPI_MakeEdge(seg.Value());
    }
    case SketchEntityType::Circle: {
      const gp_Circ circ(
          gp_Ax2(gp_Pnt(e.center.x, e.center.y, 0.0), gp_Dir(0.0, 0.0, 1.0)),
          e.radius);
      return BRepBuilderAPI_MakeEdge(circ);
    }
    case SketchEntityType::Arc: {
      const gp_Circ circ(
          gp_Ax2(gp_Pnt(e.center.x, e.center.y, 0.0), gp_Dir(0.0, 0.0, 1.0)),
          e.radius);
      const double span = std::fmod(e.end_angle - e.start_angle, kTwoPi);
      // 首末角重合视为整圆 (closed_loops 中首尾同点的退化环)
      if (std::fabs(span) < 1e-12)
        return BRepBuilderAPI_MakeEdge(circ);
      double a2 = e.end_angle;
      if (a2 <= e.start_angle)
        a2 += kTwoPi;  // 逆时针为正, 归一化使 a2 > a1
      GC_MakeArcOfCircle arc(circ, e.start_angle, a2, Standard_True);
      return BRepBuilderAPI_MakeEdge(arc.Value());
    }
  }
  return TopoDS_Edge();
}

// 一个闭环 (closed_loops 返回的按邻接顺序的图元 id 列表) -> OCC wire
TopoDS_Wire wire_from_loop(const SketchDocument& doc,
                           const std::vector<int>& loop) {
  BRepBuilderAPI_MakeWire mw;
  for (const int id : loop) {
    const SketchEntity* e = doc.entity(id);
    if (!e)
      throw Standard_Failure("internal error: unknown entity id in loop");
    mw.Add(edge_from_entity(*e));
    if (!mw.IsDone())
      throw Standard_Failure("failed to connect profile edges into a wire");
  }
  return mw.Wire();
}

// 采样一个闭环为 2D 多边形 (用于环间包含关系判断)
std::vector<SketchPoint2d> sample_loop_polygon(const SketchDocument& doc,
                                               const std::vector<int>& loop) {
  std::vector<SketchPoint2d> poly;
  for (const int id : loop) {
    const SketchEntity* e = doc.entity(id);
    if (!e) {
      continue;
    }
    if (e->type == SketchEntityType::Line) {
      poly.push_back(e->p1);
    } else if (e->type == SketchEntityType::Circle) {
      for (int i = 0; i < 36; ++i) {
        const double a = kTwoPi * i / 36.0;
        poly.push_back({e->center.x + e->radius * std::cos(a),
                        e->center.y + e->radius * std::sin(a)});
      }
    } else {  // Arc: 起止角之间采样
      double span = std::fmod(e->end_angle - e->start_angle, kTwoPi);
      if (span <= 0.0) {
        span += kTwoPi;
      }
      for (int i = 0; i < 24; ++i) {
        const double a = e->start_angle + span * i / 24.0;
        poly.push_back({e->center.x + e->radius * std::cos(a),
                        e->center.y + e->radius * std::sin(a)});
      }
    }
  }
  return poly;
}

// 射线法: 点是否在多边形内
bool point_in_polygon(const SketchPoint2d& p,
                      const std::vector<SketchPoint2d>& poly) {
  bool inside = false;
  const size_t n = poly.size();
  for (size_t i = 0, j = n - 1; i < n; j = i++) {
    const auto& a = poly[i];
    const auto& b = poly[j];
    if ((a.y > p.y) != (b.y > p.y) &&
        p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) {
      inside = !inside;
    }
  }
  return inside;
}

// 草图 -> 轮廓 face 列表。
// 支持多闭环: 相离的环各自成面 (如两个圆 -> 拉伸得到两个圆柱);
// 嵌套环按奇偶层: 偶数层为实体区域, 奇数层作其最近偶数层容器的内孔。
std::vector<TopoDS_Face> faces_from_sketch(const SketchDocument& doc,
                                           QString* error) {
  const auto loops = doc.closed_loops();
  if (loops.empty()) {
    *error = "no closed profile in sketch";
    return {};
  }
  struct LoopInfo {
    std::vector<int> ids;
    std::vector<SketchPoint2d> poly;
    double area = 0.0;
    int depth = 0;
  };
  std::vector<LoopInfo> infos(loops.size());
  for (size_t i = 0; i < loops.size(); ++i) {
    infos[i].ids = loops[i];
    infos[i].poly = sample_loop_polygon(doc, loops[i]);
    if (infos[i].poly.empty()) {
      *error = "internal error: empty loop polygon";
      return {};
    }
    double area2 = 0.0;
    const auto& pl = infos[i].poly;
    for (size_t k = 0, j = pl.size() - 1; k < pl.size(); j = k++) {
      area2 += pl[j].x * pl[k].y - pl[k].x * pl[j].y;  // 鞋带公式
    }
    infos[i].area = std::fabs(area2) / 2.0;
  }
  // 包含深度: 用各环边界上的采样点 (不能用质心——质心可能恰落在别的环内,
  // 例如矩形质心恰为内孔圆心, 会造成双向包含误判)。取各环首个采样点
  // (位于该环边界上), 统计它落在多少其他环内部。
  for (size_t i = 0; i < infos.size(); ++i) {
    for (size_t j = 0; j < infos.size(); ++j) {
      if (i != j && point_in_polygon(infos[i].poly.front(), infos[j].poly)) {
        ++infos[i].depth;
      }
    }
  }
  // 偶数层 -> 外轮廓 face
  std::vector<TopoDS_Face> faces;
  std::vector<int> face_index(infos.size(), -1);
  for (size_t i = 0; i < infos.size(); ++i) {
    if (infos[i].depth % 2 != 0) {
      continue;
    }
    BRepBuilderAPI_MakeFace mf(wire_from_loop(doc, infos[i].ids),
                               /*OnlyPlane=*/Standard_True);
    if (!mf.IsDone()) {
      *error = "failed to build a planar face from the closed profile";
      return {};
    }
    face_index[i] = static_cast<int>(faces.size());
    faces.push_back(mf.Face());
  }
  // 奇数层 -> 内孔, 加入最近偶数层容器 (孔线框取反向)
  for (size_t i = 0; i < infos.size(); ++i) {
    if (infos[i].depth % 2 == 0) {
      continue;
    }
    int parent = -1;
    for (size_t j = 0; j < infos.size(); ++j) {
      if (i == j || infos[j].depth != infos[i].depth - 1) {
        continue;
      }
      if (point_in_polygon(infos[i].poly.front(), infos[j].poly) &&
          (parent < 0 || infos[j].area < infos[parent].area)) {
        parent = static_cast<int>(j);
      }
    }
    if (parent < 0 || face_index[parent] < 0) {
      continue;
    }
    TopoDS_Wire hole = wire_from_loop(doc, infos[i].ids);
    BRepBuilderAPI_MakeFace adder(faces[face_index[parent]],
                                  TopoDS::Wire(hole.Reversed()));
    if (adder.IsDone()) {
      faces[face_index[parent]] = adder.Face();
    }
  }
  if (faces.empty() && error->isEmpty()) {
    *error = "no valid outer profile found";
  }
  return faces;
}

// 多个特征结果合并: 单个直接返回, 多个打成 Compound
// (importShapes 会把 Compound 里的每个实体各自导入 gmsh)
TopoDS_Shape combine_shapes(const std::vector<TopoDS_Shape>& shapes) {
  if (shapes.empty()) {
    return TopoDS_Shape();
  }
  if (shapes.size() == 1) {
    return shapes.front();
  }
  TopoDS_Compound comp;
  BRep_Builder builder;
  builder.MakeCompound(comp);
  for (const auto& s : shapes) {
    builder.Add(comp, s);
  }
  return comp;
}

// 特征结果收尾: 可选落盘 brep + 临时 brep 回注 gmsh (brew gmsh 的
// importShapes 只认文件路径, 见 occ_direct_call_smoke 的注释)。
// 成功填 ok / gmsh_volume_tag / brep_path, 失败填 error 并返回 false。
bool finalize_feature(const TopoDS_Shape& shape, const QString& brep_out_path,
                      FeatureResult* res) {
  ensure_gmsh();
  gmsh::model::add("gmp_feature");

  // 用户指定路径落盘失败不致命: 仅不回显 brep_path
  if (!brep_out_path.isEmpty() &&
      BRepTools::Write(shape, brep_out_path.toUtf8().constData())) {
    res->brep_path = brep_out_path;
  }

  namespace fs = std::filesystem;
  const auto stamp =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path tmp = fs::temp_directory_path() /
                       ("gmp_ws3_feature_" + std::to_string(stamp) + ".brep");
  if (!BRepTools::Write(shape, tmp.string().c_str())) {
    res->error = "failed to serialize shape to brep";
    return false;
  }
  std::vector<std::pair<int, int> > dimTags;
  gmsh::model::occ::importShapes(tmp.string(), dimTags,
                                 /*highestDimOnly=*/true, /*format=*/"brep");
  fs::remove(tmp);
  gmsh::model::occ::synchronize();

  // 取首个 volume; 无 volume 则取最高维实体 (如壳/面)
  int best_dim = -1;
  int best_tag = 0;
  for (const auto& dt : dimTags) {
    if (dt.first == 3) {
      res->gmsh_volume_tag = dt.second;
      break;
    }
    if (dt.first > best_dim) {
      best_dim = dt.first;
      best_tag = dt.second;
    }
  }
  if (res->gmsh_volume_tag == 0)
    res->gmsh_volume_tag = best_tag;
  res->ok = true;
  return true;
}

QString occ_error(const Standard_Failure& f, const char* prefix) {
  const char* msg = f.GetMessageString();
  QString text = QString::fromUtf8(prefix);
  if (msg && *msg) {
    text += ": ";
    text += QString::fromUtf8(msg);
  }
  return text;
}

// 把 path 草图的全部 Line/Arc 按端点邻接排成单条开链 (按值返回图元)。
// v1 要求 path 恰好是一条连通链: 不支持分支、多条独立链, 也不支持整圆。
bool order_path_chain(const SketchDocument& path,
                      std::vector<SketchEntity>* chain, QString* error) {
  // entities() 按值返回, 先拷贝持有
  std::vector<SketchEntity> pool = path.entities();
  for (const auto& e : pool) {
    if (e.type == SketchEntityType::Circle) {
      *error = "path sketch: full circles are not supported in v1";
      return false;
    }
  }
  if (pool.empty()) {
    *error = "path sketch is empty";
    return false;
  }

  constexpr double kTol = 1e-6;
  // 注意: 不能命名 near/far, Windows 头文件把它们定义为空的宏 (MSVC 编译炸)
  auto points_near = [](const SketchPoint2d& a, const SketchPoint2d& b) {
    return std::hypot(a.x - b.x, a.y - b.y) <= kTol;
  };

  chain->push_back(pool.front());
  pool.erase(pool.begin());
  bool grew = true;
  while (grew && !pool.empty()) {
    grew = false;
    SketchPoint2d head, tail;
    SketchDocument::point_at_role(chain->front(), SketchPointRole::Start, &head);
    SketchDocument::point_at_role(chain->back(), SketchPointRole::End, &tail);
    for (size_t i = 0; i < pool.size(); ++i) {
      SketchPoint2d s, t;
      SketchDocument::point_at_role(pool[i], SketchPointRole::Start, &s);
      SketchDocument::point_at_role(pool[i], SketchPointRole::End, &t);
      if (points_near(s, tail)) {
        chain->push_back(pool[i]);
      } else if (points_near(t, head)) {
        chain->insert(chain->begin(), pool[i]);
      } else {
        continue;
      }
      pool.erase(pool.begin() + static_cast<long>(i));
      grew = true;
      break;
    }
  }
  if (!pool.empty()) {
    *error = "path sketch must be a single connected chain of lines/arcs";
    return false;
  }
  return true;
}

}  // namespace

bool occ_direct_call_smoke() {
  try {
    if (!gmsh::isInitialized())
      gmsh::initialize();
    gmsh::model::add("occ_smoke");

    // 两个不同尺寸的圆截面 (z=0 处 R=10, z=30 处 R=5)
    const gp_Ax2 ax1(gp_Pnt(0., 0., 0.), gp_Dir(0., 0., 1.));
    const gp_Ax2 ax2(gp_Pnt(0., 0., 30.), gp_Dir(0., 0., 1.));
    const TopoDS_Edge e1 = BRepBuilderAPI_MakeEdge(gp_Circ(ax1, 10.));
    const TopoDS_Edge e2 = BRepBuilderAPI_MakeEdge(gp_Circ(ax2, 5.));
    const TopoDS_Wire w1 = BRepBuilderAPI_MakeWire(e1);
    const TopoDS_Wire w2 = BRepBuilderAPI_MakeWire(e2);

    // OCC 直调: 实体 loft
    BRepOffsetAPI_ThruSections thru(/*isSolid=*/Standard_True,
                                    /*ruled=*/Standard_False);
    thru.AddWire(w1);
    thru.AddWire(w2);
    thru.Build();
    if (!thru.IsDone())
      return false;

    // 转 BREP 字符串
    std::stringstream brep;
    BRepTools::Write(thru.Shape(), brep);

    // 回注 gmsh OCC 内核。brew gmsh 4.15 的 importShapes 只接受文件路径
    // (直接传 BREP 内容字符串会报 "File ... does not exist"),
    // 因此将 BREP 字符串落盘为临时文件再导入, 导入后删除。
    namespace fs = std::filesystem;
    const fs::path tmp =
        fs::temp_directory_path() / "gmp_ws0_occ_smoke.brep";
    {
      std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
      out << brep.str();
    }
    std::vector<std::pair<int, int> > dimTags;
    gmsh::model::occ::importShapes(tmp.string(), dimTags,
                                   /*highestDimOnly=*/true, /*format=*/"brep");
    fs::remove(tmp);
    gmsh::model::occ::synchronize();

    int volumes = 0;
    for (const auto& dt : dimTags) {
      if (dt.first == 3)
        ++volumes;
    }
    return volumes >= 1;
  } catch (...) {
    return false;
  }
}

bool planegcs_smoke() {
  try {
    // 最小约束系统: 两点 + 一条线段 + 一个 P2P 距离约束
    double p1x = 0., p1y = 0., p2x = 7., p2y = 1.;
    double dist = 10.;
    GCS::Point p1(&p1x, &p1y);
    GCS::Point p2(&p2x, &p2y);
    GCS::Line line;
    line.p1 = p1;
    line.p2 = p2;

    GCS::System sys;
    sys.addConstraintP2PDistance(line.p1, line.p2, &dist);

    std::vector<double*> params = {&p1x, &p1y, &p2x, &p2y};
    sys.declareUnknowns(params);
    sys.initSolution();
    const int ret = sys.solve();
    if (ret != GCS::Success && ret != GCS::Converged)
      return false;
    // planegcs 的 solve 在内部参数副本上迭代, 必须 applySolution()
    // 才能把解写回调用方传入的参数内存
    sys.applySolution();

    // 校验求解结果确实满足距离约束
    const double dx = p2x - p1x, dy = p2y - p1y;
    const double actual = std::sqrt(dx * dx + dy * dy);
    return std::fabs(actual - dist) < 1e-6;
  } catch (...) {
    return false;
  }
}

// ---- WS3: 草图特征操作 ----

FeatureResult extrude_sketch(const SketchDocument& doc, double distance,
                             const QString& brep_out_path) {
  FeatureResult res;
  try {
    if (distance == 0.0) {
      res.error = "extrusion distance must be non-zero";
      return res;
    }
    QString err;
    const auto faces = faces_from_sketch(doc, &err);
    if (faces.empty()) {
      res.error = err;
      return res;
    }
    // 沿 +Z 拉伸; 负值自动反向。每个闭环轮廓各自成体, 多体合并 Compound
    std::vector<TopoDS_Shape> solids;
    for (const auto& f : faces) {
      BRepPrimAPI_MakePrism prism(f, gp_Vec(0.0, 0.0, distance));
      prism.Build();
      if (!prism.IsDone()) {
        res.error = "extrude failed (invalid prism)";
        return res;
      }
      solids.push_back(prism.Shape());
    }
    finalize_feature(combine_shapes(solids), brep_out_path, &res);
  } catch (const Standard_Failure& f) {
    res.error = occ_error(f, "extrude failed");
  } catch (const std::exception& e) {
    res.error = QString::fromUtf8(e.what());
  }
  return res;
}

FeatureResult revolve_sketch(const SketchDocument& doc, double angle_deg,
                             const QString& brep_out_path) {
  FeatureResult res;
  try {
    if (angle_deg == 0.0) {
      res.error = "revolution angle must be non-zero";
      return res;
    }
    QString err;
    const auto faces = faces_from_sketch(doc, &err);
    if (faces.empty()) {
      res.error = err;
      return res;
    }
    // 绕 Y 轴 (过原点, 草图平面内) 旋转; 角度转弧度。多闭环各自成体后合并
    const gp_Ax1 axis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 1.0, 0.0));
    const double angle_rad = angle_deg * 3.14159265358979323846 / 180.0;
    std::vector<TopoDS_Shape> solids;
    for (const auto& f : faces) {
      BRepPrimAPI_MakeRevol revol(f, axis, angle_rad);
      revol.Build();
      if (!revol.IsDone()) {
        res.error = "revolve failed (invalid revolved shape)";
        return res;
      }
      solids.push_back(revol.Shape());
    }
    finalize_feature(combine_shapes(solids), brep_out_path, &res);
  } catch (const Standard_Failure& f) {
    // 轮廓与旋转轴相交时 OCC 通常在此抛异常
    res.error = occ_error(
        f, "revolve failed (profile must not intersect the Y axis)");
  } catch (const std::exception& e) {
    res.error = QString::fromUtf8(e.what());
  }
  return res;
}

FeatureResult loft_sketches(
    const std::vector<std::pair<const SketchDocument*, double>>& sections,
    bool solid, const QString& brep_out_path) {
  FeatureResult res;
  try {
    if (sections.size() < 2) {
      res.error = "loft needs at least two sections";
      return res;
    }
    BRepOffsetAPI_ThruSections thru(/*isSolid=*/solid ? Standard_True
                                                      : Standard_False,
                                    /*ruled=*/Standard_False);
    int index = 0;
    for (const auto& [doc, z_offset] : sections) {
      ++index;
      if (!doc) {
        res.error = QString("loft section %1 is null").arg(index);
        return res;
      }
      const auto loops = doc->closed_loops();
      if (loops.empty()) {
        res.error = QString("loft section %1: no closed profile").arg(index);
        return res;
      }
      // v1 限制: 放样每个截面只取第一个闭环 (多闭环截面间的配对规则暂不支持)
      TopoDS_Wire wire = wire_from_loop(*doc, loops.front());
      if (z_offset != 0.0) {
        gp_Trsf lift;
        lift.SetTranslation(gp_Vec(0.0, 0.0, z_offset));
        BRepBuilderAPI_Transform xform(wire, lift, /*Copy=*/Standard_True);
        wire = TopoDS::Wire(xform.Shape());
      }
      thru.AddWire(wire);
    }
    thru.Build();
    if (!thru.IsDone()) {
      res.error = "loft failed (ThruSections did not converge)";
      return res;
    }
    finalize_feature(thru.Shape(), brep_out_path, &res);
  } catch (const Standard_Failure& f) {
    res.error = occ_error(f, "loft failed");
  } catch (const std::exception& e) {
    res.error = QString::fromUtf8(e.what());
  }
  return res;
}

FeatureResult sweep_sketch(const SketchDocument& profile,
                           const SketchDocument& path,
                           const QString& brep_out_path) {
  FeatureResult res;
  try {
    QString err;
    const auto faces = faces_from_sketch(profile, &err);
    if (faces.empty()) {
      res.error = err;
      return res;
    }
    std::vector<SketchEntity> chain;
    if (!order_path_chain(path, &chain, &err)) {
      res.error = err;
      return res;
    }
    BRepBuilderAPI_MakeWire mw;
    for (const SketchEntity& e : chain) {
      mw.Add(edge_from_entity(e));
      if (!mw.IsDone()) {
        res.error = "failed to connect path edges into a wire";
        return res;
      }
    }
    // 每个轮廓面各自沿路径扫掠, 多体合并
    std::vector<TopoDS_Shape> solids;
    for (const auto& f : faces) {
      BRepOffsetAPI_MakePipe pipe(mw.Wire(), f);
      pipe.Build();
      if (!pipe.IsDone()) {
        res.error = "sweep failed (invalid pipe)";
        return res;
      }
      solids.push_back(pipe.Shape());
    }
    finalize_feature(combine_shapes(solids), brep_out_path, &res);
  } catch (const Standard_Failure& f) {
    res.error = occ_error(f, "sweep failed");
  } catch (const std::exception& e) {
    res.error = QString::fromUtf8(e.what());
  }
  return res;
}

} // namespace gmp

namespace gmp {

bool mesh_current_model(const QString& msh_out_path, QString* error) {
  ensure_gmsh();
  // 优先 3D 体网格, 失败回退 2D 面网格 (壳/面模型或 3D 算法失败时)
  bool meshed = false;
  try {
    gmsh::model::mesh::generate(3);
    meshed = true;
  } catch (const std::exception&) {
    try {
      gmsh::model::mesh::generate(2);
      meshed = true;
    } catch (const std::exception& e) {
      if (error) {
        *error = QString::fromUtf8(e.what());
      }
    }
  }
  if (!meshed) {
    return false;
  }
  try {
    gmsh::write(msh_out_path.toStdString());
  } catch (const std::exception& e) {
    if (error) {
      *error = QString::fromUtf8(e.what());
    }
    return false;
  }
  return true;
}

} // namespace gmp
