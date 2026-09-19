#include "gmp/GmshMesher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QStandardPaths>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <set>

#ifdef GMP_ENABLE_GMSH_GUI
#include <gmsh.h>
#endif

namespace gmp {

#ifdef GMP_ENABLE_GMSH_GUI
namespace {

std::atomic<unsigned long long> generated_mesh_model_sequence{0};

struct StructuredMeshEligibility {
  bool eligible = false;
  QString reason;
  std::map<int, int> curve_nodes;
  std::set<int> surface_tags;
  std::set<int> volume_tags;
};

// TransfiniteAutomatic only creates a genuine mapped quad/brick mesh for
// quadrangular surfaces and six-sided volumes.  Do this topology check before
// setting constraints: tetrahedron subdivision also reports "Hexahedron", but
// produces the highly skewed honeycomb-like cells that users do not mean by a
// structured brick mesh.
StructuredMeshEligibility CheckStructuredMeshEligibility(
    int dim, double target_size,
    const std::vector<std::pair<int, int>>& top_entities, bool chinese) {
  StructuredMeshEligibility result;
  if (dim != 2 && dim != 3) {
    result.reason = chinese
                        ? QString::fromUtf8("结构化四边形/砖形网格仅适用于 2D/3D")
                        : QString("structured quad/brick meshing is only "
                                  "available in 2D/3D");
    return result;
  }
  if (top_entities.empty()) {
    result.reason = chinese ? QString::fromUtf8("模型没有最高维几何实体")
                            : QString("the model has no top-dimensional entities");
    return result;
  }

  for (const auto& entity : top_entities) {
    if (dim == 3) {
      result.volume_tags.insert(entity.second);
    } else {
      result.surface_tags.insert(entity.second);
    }
    std::vector<std::pair<int, int>> boundary;
    gmsh::model::getBoundary({entity}, boundary, false, false, false);
    const std::size_t expected = dim == 3 ? 6u : 4u;
    if (boundary.size() != expected) {
      if (chinese) {
        result.reason =
            dim == 3
                ? QString::fromUtf8(
                      "体 %1 有 %2 个面（映射砖形需要 6 个；带孔几何"
                      "通常需要先分区）")
                      .arg(entity.second)
                      .arg(boundary.size())
                : QString::fromUtf8(
                      "面 %1 有 %2 条边界曲线（映射四边形需要 4 条）")
                      .arg(entity.second)
                      .arg(boundary.size());
      } else {
        result.reason =
            dim == 3
                ? QString("volume %1 has %2 faces (a mapped brick requires 6; "
                          "holes usually require geometry partitioning first)")
                      .arg(entity.second)
                      .arg(boundary.size())
                : QString("surface %1 has %2 boundary curves (a mapped quad "
                          "requires 4)")
                      .arg(entity.second)
                      .arg(boundary.size());
      }
      return result;
    }
    if (dim == 3) {
      for (const auto& face : boundary) {
        result.surface_tags.insert(face.second);
        std::vector<std::pair<int, int>> curves;
        gmsh::model::getBoundary({face}, curves, false, false, false);
        if (curves.size() != 4u) {
          result.reason = chinese
                              ? QString::fromUtf8(
                                    "面 %1 有 %2 条边界曲线（映射砖形的每个面"
                                    "需要 4 条）")
                                    .arg(face.second)
                                    .arg(curves.size())
                              : QString("face %1 has %2 boundary curves (a "
                                        "mapped brick face requires 4)")
                                    .arg(face.second)
                                    .arg(curves.size());
          return result;
        }
        std::vector<int> face_node_counts;
        for (const auto& curve : curves) {
          double length = 0.0;
          gmsh::model::occ::getMass(1, curve.second, length);
          const int nodes =
              std::max(2, static_cast<int>(std::ceil(length / target_size)) +
                              1);
          result.curve_nodes[curve.second] = nodes;
          face_node_counts.push_back(nodes);
        }
        std::sort(face_node_counts.begin(), face_node_counts.end());
        if (face_node_counts[0] != face_node_counts[1] ||
            face_node_counts[2] != face_node_counts[3]) {
          result.reason =
              chinese
                  ? QString::fromUtf8(
                        "按当前网格尺寸计算后，面 %1 的对边分段数不兼容；"
                        "请调整尺寸或对几何分区")
                        .arg(face.second)
                  : QString("face %1 has incompatible opposite-edge divisions "
                            "at the requested size; partition or adjust the "
                            "geometry")
                        .arg(face.second);
          return result;
        }
      }
    } else {
      std::vector<int> face_node_counts;
      for (const auto& curve : boundary) {
        double length = 0.0;
        gmsh::model::occ::getMass(1, curve.second, length);
        const int nodes =
            std::max(2, static_cast<int>(std::ceil(length / target_size)) + 1);
        result.curve_nodes[curve.second] = nodes;
        face_node_counts.push_back(nodes);
      }
      std::sort(face_node_counts.begin(), face_node_counts.end());
      if (face_node_counts[0] != face_node_counts[1] ||
          face_node_counts[2] != face_node_counts[3]) {
        result.reason = chinese
                            ? QString::fromUtf8(
                                  "按当前网格尺寸计算后，面 %1 的对边分段数"
                                  "不兼容")
                                  .arg(entity.second)
                            : QString("surface %1 has incompatible opposite-"
                                      "edge divisions at the requested size")
                                  .arg(entity.second);
        return result;
      }
    }
  }
  result.eligible = true;
  return result;
}

// 在独立 model 中读取子进程生成的网格，收集舞台/质量/manifest 数据后
// 恢复原 OCC model。建模状态与文件预览状态必须隔离。
class ScopedGeneratedMeshModel {
 public:
  ScopedGeneratedMeshModel(const QString& path, const QString& restore_path)
      : restore_path_(restore_path.toStdString()) {
    try {
      // Gmsh 的文件模型注册表及解析状态在长会话里会残留同名/空模型。
      // 回读前重建干净会话，以保证 open() 读取的就是本轮离散网格；
      // 析构时再重建会话并从未附加 worker 指令的快照恢复 OCC 几何。
      reset_gmsh_session();
      gmsh::open(path.toStdString());
      select_opened_model(path);
    } catch (...) {
      restore();
      throw;
    }
  }

  ScopedGeneratedMeshModel(const ScopedGeneratedMeshModel&) = delete;
  ScopedGeneratedMeshModel& operator=(const ScopedGeneratedMeshModel&) = delete;
  ~ScopedGeneratedMeshModel() { restore(); }

 private:
  void restore() noexcept {
    try {
      reset_gmsh_session();
      if (!restore_path_.empty()) {
        gmsh::open(restore_path_);
        select_opened_model(QString::fromStdString(restore_path_));
      }
    } catch (...) {
    }
  }

  static void reset_gmsh_session() {
    if (gmsh::isInitialized()) {
      gmsh::finalize();
    }
    gmsh::initialize(0, nullptr, false, false);
    gmsh::option::setNumber("General.Terminal", 0);
  }

  static void select_opened_model(const QString& path) {
    std::vector<std::string> model_names;
    gmsh::model::list(model_names);
    if (model_names.empty()) {
      throw std::runtime_error("Gmsh opened a file without creating a model.");
    }
    const std::string expected =
        QFileInfo(path).completeBaseName().toStdString();
    const auto match =
        std::find(model_names.begin(), model_names.end(), expected);
    gmsh::model::setCurrent(match != model_names.end() ? *match
                                                       : model_names.back());
  }

  std::string restore_path_;
};

}  // namespace
#endif  // GMP_ENABLE_GMSH_GUI

MeshJobPlan GmshMesher::prepare(
    const MeshJobSpec& spec, const std::function<void(const QString&)>& log) {
#ifdef GMP_ENABLE_GMSH_GUI
  bool logger_started = false;
  try {
    // 舞台文件读取和前一次生成结果回读可能暂时改变 current model；
    // 外部部件已明确登记时，以登记的模型为网格输入源。
    if (!spec.external_model_name.isEmpty()) {
      std::vector<std::string> model_names;
      gmsh::model::list(model_names);
      const std::string expected = spec.external_model_name.toStdString();
      if (std::find(model_names.begin(), model_names.end(), expected) !=
          model_names.end()) {
        gmsh::model::setCurrent(expected);
      } else {
        throw MeshJobError(
            "The selected part model is no longer available. Select it again "
            "or rebuild the part before generating the mesh.",
            MeshJobError::Kind::Generic);
      }
    }

    gmsh::option::setNumber("General.Terminal", 0);
    gmsh::logger::start();
    logger_started = true;

    const double dx = spec.size_x;
    const double dy = spec.size_y;
    const double dz = spec.size_z;
    const double lc = spec.mesh_size;
    const int order = spec.elem_order;
    const int msh_version = spec.msh_version;

    gmsh::option::setNumber("Mesh.CharacteristicLengthMin", lc);
    gmsh::option::setNumber("Mesh.CharacteristicLengthMax", lc);
    gmsh::option::setNumber("Mesh.ElementOrder", order);
    if (spec.high_order_opt > 0 && order > 1) {
      gmsh::option::setNumber("Mesh.HighOrderOptimize", spec.high_order_opt);
    } else {
      gmsh::option::setNumber("Mesh.HighOrderOptimize", 0);
    }
    gmsh::option::setNumber("Mesh.Algorithm", spec.algo2d);
    gmsh::option::setNumber("Mesh.Algorithm3D", spec.algo3d);
    // 禁用全局重组和四面体细分。后者虽会在文件中报告 Hexahedron，
    // 却会生成继承四面体方向的扭曲蜂窝单元，不是用户所指的砖形网格。
    gmsh::option::setNumber("Mesh.RecombineAll", 0);
    gmsh::option::setNumber("Mesh.SubdivisionAlgorithm", 0);
    gmsh::option::setNumber("Mesh.Smoothing", spec.smoothing);
    gmsh::option::setNumber("Mesh.Optimize", spec.optimize ? 1 : 0);
    gmsh::option::setNumber("Mesh.MshFileVersion", msh_version == 2 ? 2.2 : 4.1);

    MeshJobPlan plan;

    // 是否用示例盒兜底只取决于“当前模型是否真的没有几何实体”，
    // 不看 model_loaded_ 标志或示例盒勾选状态——部件特征等外部通道
    // 导入的几何绝不允许被清空换盒（历史缺陷：勾选框/面板状态把用户
    // 的部件几何误清，产出 12 节点的退化网格）。
    std::vector<std::pair<int, int>> existing_entities;
    gmsh::model::getEntities(existing_entities, -1);
    const bool model_empty = existing_entities.empty();
    if (model_empty) {
      if (!spec.use_sample_box) {
        throw MeshJobError(
            "No geometry in the current model. Import geometry, create a "
            "part feature, or enable the sample box before generating.",
            MeshJobError::Kind::Generic);
      }
      gmsh::clear();
      gmsh::model::add("box_model");
      const int box = gmsh::model::occ::addBox(0, 0, 0, dx, dy, dz);
      gmsh::model::occ::synchronize();

      const int phys = gmsh::model::addPhysicalGroup(3, {box});
      gmsh::model::setPhysicalName(3, phys, "solid");
      std::vector<std::pair<int, int>> faces;
      gmsh::model::getEntities(faces, 2);
      if (!faces.empty()) {
        std::vector<int> face_tags;
        face_tags.reserve(faces.size());
        for (const auto& f : faces) {
          face_tags.push_back(f.second);
        }
        const int bnd = gmsh::model::addPhysicalGroup(2, face_tags);
        gmsh::model::setPhysicalName(2, bnd, "boundary");
      }
      plan.box_created = true;
    } else {
      gmsh::model::mesh::clear();
    }

    int dim = 0;
    {
      std::vector<std::pair<int, int>> ents;
      gmsh::model::getEntities(ents);
      for (const auto& e : ents) {
        if (e.first > dim) {
          dim = e.first;
        }
      }
      if (dim < 1) {
        dim = 1;
      }
      if (dim > 3) {
        dim = 3;
      }
    }
    if (spec.configured_dim >= 1 && spec.configured_dim <= 3) {
      if (spec.configured_dim <= dim) {
        dim = spec.configured_dim;
      } else if (log) {
        log(QString("Requested mesh dim %1 exceeds geometry dim %2, "
                    "fallback to %2.")
                .arg(spec.configured_dim)
                .arg(dim));
      }
    }
    plan.dim = dim;
    plan.lc = lc;
    const int topology_mode = spec.topology_mode;
    const bool prefer_structured = topology_mode != 1;
    const bool require_structured = topology_mode == 2;
    // 规模预检：草图/OCC 使用模型坐标原值（通常为 mm）。沿用面向单位盒的
    // 默认 lc=0.2 到百毫米级实体时，单元数会按 lc^-dim 爆炸，表现为应用
    // “卡死”。优先用 OCC 的真实长度/面积/体积，其他内核回退到包围盒估算。
    std::vector<std::pair<int, int>> top_entities;
    gmsh::model::getEntities(top_entities, dim);
    const StructuredMeshEligibility structured =
        prefer_structured
            ? CheckStructuredMeshEligibility(dim, lc, top_entities,
                                             spec.chinese_ui)
            : StructuredMeshEligibility{};
    const bool use_structured_mesh = prefer_structured && structured.eligible;
    if (require_structured && !structured.eligible) {
      const QString message =
          spec.chinese_ui
              ? QString::fromUtf8(
                    "严格结构化网格未生成：%1。该模式不会回退为四面体；请先"
                    "将几何划分为可映射/可扫掠块体，或改选“自动”或“通用"
                    "四面体”。")
                    .arg(structured.reason)
              : QString("Strict structured mesh was not generated: %1. This "
                        "mode never falls back; partition the geometry into "
                        "mapped/sweepable blocks, or select Automatic or the "
                        "general simplex mode.")
                    .arg(structured.reason);
      throw MeshJobError(message, MeshJobError::Kind::StructuredMode);
    }
    if (use_structured_mesh) {
      if (log) {
        log(dim == 3
                ? "Structured brick mesh enabled: mapped constraints "
                  "applied to eligible six-faced volume(s)."
                : "Structured quadrilateral mesh enabled: mapped "
                  "constraints applied to four-sided surface(s).");
      }
    } else if (prefer_structured) {
      plan.structured_fallback_reason = structured.reason;
      if (log) {
        log(QString("Structured mesh unavailable: %1. Falling back to "
                    "%2; partition the geometry into sweepable blocks "
                    "to obtain a conforming structured mesh.")
                .arg(structured.reason)
                .arg(dim == 3 ? "tetrahedra" : "triangles"));
      }
    }
    plan.use_structured_mesh = use_structured_mesh;
    double occ_measure = 0.0;
    bool has_occ_measure = false;
    double bbox_measure = 0.0;
    for (const auto& entity : top_entities) {
      try {
        double mass = 0.0;
        gmsh::model::occ::getMass(dim, entity.second, mass);
        if (std::isfinite(mass) && mass > 0.0) {
          occ_measure += mass;
          has_occ_measure = true;
        }
      } catch (...) {
      }

      try {
        double xmin = 0.0;
        double ymin = 0.0;
        double zmin = 0.0;
        double xmax = 0.0;
        double ymax = 0.0;
        double zmax = 0.0;
        gmsh::model::getBoundingBox(dim, entity.second, xmin, ymin, zmin,
                                    xmax, ymax, zmax);
        std::vector<double> spans = {std::max(0.0, xmax - xmin),
                                     std::max(0.0, ymax - ymin),
                                     std::max(0.0, zmax - zmin)};
        std::sort(spans.begin(), spans.end(), std::greater<double>());
        if (dim == 3) {
          bbox_measure += spans[0] * spans[1] * spans[2];
        } else if (dim == 2) {
          bbox_measure += spans[0] * spans[1];
        } else if (dim == 1) {
          bbox_measure += spans[0];
        }
      } catch (...) {
      }
    }
    const double measure = has_occ_measure ? occ_measure : bbox_measure;
    const double simplex_factor = dim == 3 ? 6.0 : (dim == 2 ? 2.0 : 1.0);
    // 映射砖形网格约为 measure/lc^dim；保留 1.5 的保守余量。
    // 回退时按单纯形网格估算，不再为已删除的自动细分乘 3/4 倍。
    const double element_factor = use_structured_mesh ? 1.5 : simplex_factor;
    const double estimated_elements =
        (lc > 0.0 && measure > 0.0)
            ? element_factor * measure / std::pow(lc, dim)
            : 0.0;
    plan.estimated_elements = estimated_elements;
    const double max_estimated_elements = spec.max_estimated_elements;
    if (estimated_elements > max_estimated_elements) {
      const double recommended_lc =
          1.1 * std::pow(element_factor * measure / max_estimated_elements,
                         1.0 / static_cast<double>(dim));
      const double estimated_millions = estimated_elements / 1000000.0;
      const double limit_millions = max_estimated_elements / 1000000.0;
      const QString message =
          spec.chinese_ui
              ? QString::fromUtf8(
                    "网格尺寸过细：预计约 %1 百万个 %2D 单元，超过交互式生成"
                    "上限 %3 百万个。请将全局网格尺寸调至至少 %4（建议从 %5 "
                    "开始），再重新生成。")
                    .arg(estimated_millions, 0, 'f', 1)
                    .arg(dim)
                    .arg(limit_millions, 0, 'f', 1)
                    .arg(recommended_lc, 0, 'g', 4)
                    .arg(std::max(recommended_lc, lc * 2.0), 0, 'g', 4)
              : QString(
                    "Mesh size is too fine: about %1 million %2D elements are "
                    "estimated, above the interactive limit of %3 million. "
                    "Increase Global Mesh Size to at least %4 (start with %5) "
                    "and generate again.")
                    .arg(estimated_millions, 0, 'f', 1)
                    .arg(dim)
                    .arg(limit_millions, 0, 'f', 1)
                    .arg(recommended_lc, 0, 'g', 4)
                    .arg(std::max(recommended_lc, lc * 2.0), 0, 'g', 4);
      throw MeshJobError(message, MeshJobError::Kind::SizeGuard);
    }
    if (log) {
      log(QString("Mesh size preflight: estimated %1 top-dimensional "
                  "elements (limit %2, lc=%3).")
              .arg(estimated_elements, 0, 'g', 6)
              .arg(max_estimated_elements, 0, 'g', 6)
              .arg(lc, 0, 'g', 6));
    }

    // libgmsh 是进程级全局状态，不能安全地从 Qt 后台线程调用。把当前
    // model 序列化后交给独立 gmsh 进程：GUI 可响应、stdout 提供真实阶段/
    // 百分比，取消也只终止子进程，不会损坏应用内 OCC model。
    QString gmsh_executable = QStandardPaths::findExecutable("gmsh");
#ifdef Q_OS_MACOS
    if (gmsh_executable.isEmpty()) {
      const QString app_executable =
          "/Applications/Gmsh.app/Contents/MacOS/gmsh";
      if (QFileInfo::exists(app_executable)) {
        gmsh_executable = app_executable;
      }
    }
#endif
    if (gmsh_executable.isEmpty()) {
      throw MeshJobError(
          "The gmsh executable was not found; background mesh generation "
          "requires the Gmsh CLI on PATH.",
          MeshJobError::Kind::Generic);
    }
    plan.gmsh_executable = gmsh_executable;
    auto temp_dir = std::make_shared<QTemporaryDir>(
        QDir::tempPath() + "/gmp_mesh_generation_XXXXXX");
    if (!temp_dir->isValid()) {
      throw MeshJobError(
          "Could not create a temporary directory for mesh generation.",
          MeshJobError::Kind::Generic);
    }
    plan.temp_dir = temp_dir;
    plan.source_path = temp_dir->filePath("current_model.geo_unrolled");
    QString restore_model_name = spec.external_model_name.isEmpty()
                                     ? QString("current_model_restore")
                                     : spec.external_model_name;
    restore_model_name.replace(QRegularExpression("[^A-Za-z0-9_.-]"), "_");
    plan.restore_path =
        temp_dir->filePath(restore_model_name + ".geo_unrolled");
    const auto generated_sequence = generated_mesh_model_sequence.fetch_add(
        1, std::memory_order_relaxed);
    plan.generated_path = temp_dir->filePath(
        QString("generated_%1.msh").arg(generated_sequence));
    gmsh::write(plan.source_path.toStdString());
    if (!QFile::copy(plan.source_path, plan.restore_path)) {
      throw MeshJobError(
          "Could not snapshot the current OCC model for restoration.",
          MeshJobError::Kind::Generic);
    }
    QStringList worker_directives;
    auto tag_list = [](const auto& tags) {
      QStringList values;
      for (const auto tag : tags) {
        values << QString::number(tag);
      }
      return values.join(", ");
    };

    // Gmsh writes only entities covered by physical groups as soon as any
    // physical group exists. A newly created Part can therefore disappear if
    // older groups are present but do not yet cover the new top-dimensional
    // entity. Add a worker-only group for uncovered entities; the in-memory
    // OCC model and all existing user groups stay untouched.
    std::vector<std::pair<int, int>> top_physical_groups;
    gmsh::model::getPhysicalGroups(top_physical_groups, dim);
    std::set<int> grouped_top_entities;
    int next_physical_tag = 1;
    for (const auto& group : top_physical_groups) {
      next_physical_tag = std::max(next_physical_tag, group.second + 1);
      std::vector<int> entity_tags;
      gmsh::model::getEntitiesForPhysicalGroup(dim, group.second, entity_tags);
      grouped_top_entities.insert(entity_tags.begin(), entity_tags.end());
    }
    std::vector<int> ungrouped_top_entities;
    for (const auto& entity : top_entities) {
      if (!grouped_top_entities.count(entity.second)) {
        ungrouped_top_entities.push_back(entity.second);
      }
    }
    if (!ungrouped_top_entities.empty()) {
      const QString physical_kind =
          dim == 3 ? "Volume" : (dim == 2 ? "Surface" : "Curve");
      worker_directives
          << QString("Physical %1(\"unassigned_%2d\", %3) = {%4};")
                 .arg(physical_kind)
                 .arg(dim)
                 .arg(next_physical_tag)
                 .arg(tag_list(ungrouped_top_entities));
      if (log) {
        log(QString("Worker mesh group added for %1 unassigned %2D "
                    "entity/entities.")
                .arg(ungrouped_top_entities.size())
                .arg(dim));
      }
    }
    if (use_structured_mesh) {
      // OCC models are written as a .geo_unrolled file that only Merge-s an
      // accompanying XAO file; mesh attributes set through the API are not
      // serialized. Append explicit mapped-mesh directives for the worker.
      for (const auto& [curve_tag, node_count] : structured.curve_nodes) {
        worker_directives
            << QString("Transfinite Curve {%1} = %2 Using Progression 1;")
                   .arg(curve_tag)
                   .arg(node_count);
      }
      if (!structured.surface_tags.empty()) {
        const QString surfaces = tag_list(structured.surface_tags);
        worker_directives
            << QString("Transfinite Surface {%1};").arg(surfaces)
            << QString("Recombine Surface {%1};").arg(surfaces);
      }
      if (!structured.volume_tags.empty()) {
        worker_directives
            << QString("Transfinite Volume {%1};")
                   .arg(tag_list(structured.volume_tags));
      }
    }
    if (!worker_directives.empty()) {
      QFile source_file(plan.source_path);
      if (!source_file.open(QIODevice::Append | QIODevice::Text) ||
          source_file.write(
              ("\n" + worker_directives.join("\n") + "\n").toUtf8()) < 0) {
        throw MeshJobError(
            "Could not serialize worker mesh constraints.",
            MeshJobError::Kind::Generic);
      }
    }

    if (logger_started) {
      std::vector<std::string> setup_log;
      gmsh::logger::get(setup_log);
      gmsh::logger::stop();
      logger_started = false;
      if (log) {
        for (const auto& line : setup_log) {
          log(QString::fromStdString(line));
        }
      }
    }

    plan.process_args << plan.source_path << QString("-%1").arg(dim)
                      << "-nopopup"
                      << "-v" << "4" << "-o" << plan.generated_path << "-format"
                      << (msh_version == 2 ? "msh2" : "msh4") << "-clmin"
                      << QString::number(lc, 'g', 16) << "-clmax"
                      << QString::number(lc, 'g', 16) << "-order"
                      << QString::number(order) << "-setnumber"
                      << "Mesh.Algorithm" << QString::number(spec.algo2d)
                      << "-setnumber" << "Mesh.Algorithm3D"
                      << QString::number(spec.algo3d) << "-setnumber"
                      << "Mesh.RecombineAll"
                      << "0"
                      << "-setnumber" << "Mesh.SubdivisionAlgorithm"
                      << "0"
                      << "-setnumber" << "Mesh.Smoothing"
                      << QString::number(spec.smoothing) << "-setnumber"
                      << "Mesh.HighOrderOptimize"
                      << QString::number(spec.high_order_opt) << "-setnumber"
                      << "Mesh.Optimize"
                      << QString::number(spec.optimize ? 1 : 0);
    return plan;
  } catch (...) {
    if (logger_started) {
      try {
        std::vector<std::string> log_lines;
        gmsh::logger::get(log_lines);
        gmsh::logger::stop();
        if (log) {
          for (const auto& line : log_lines) {
            log(QString::fromStdString(line));
          }
        }
      } catch (...) {
      }
    }
    throw;
  }
#else
  Q_UNUSED(spec);
  Q_UNUSED(log);
  throw MeshJobError("Gmsh is not enabled in this build.",
                     MeshJobError::Kind::Generic);
#endif
}

}  // namespace gmp
namespace gmp {

MeshJobOutcome GmshMesher::finalize(
    MeshJobPlan&& plan, const MeshJobSpec& spec,
    const std::function<void(const QString&)>& log,
    const std::function<void(const MeshJobOutcome&)>& mid_hook) {
#ifdef GMP_ENABLE_GMSH_GUI
  MeshJobOutcome outcome;
  // 后处理暂时切到生成网格 model；作用域结束自动恢复原 OCC model。
  ScopedGeneratedMeshModel generated_model(plan.generated_path,
                                           plan.restore_path);
  const int dim = plan.dim;
  const int boundary_dim = std::max(0, dim - 1);

  // 空结果保护：生成不出任何节点时不得写文件或报告成功——空网格既无
  // 意义，又会覆盖上一个有效输出并造成“已生成但舞台为空”的假象。
  std::vector<std::size_t> node_tags;
  std::vector<double> node_coords;
  std::vector<double> node_params;
  gmsh::model::mesh::getNodes(node_tags, node_coords, node_params);
  if (node_tags.empty()) {
    throw MeshJobError(
        "Mesh generation produced no nodes; refusing to write an empty "
        "mesh file.",
        MeshJobError::Kind::Generic);
  }
  if (plan.use_structured_mesh) {
    std::vector<int> generated_types;
    std::vector<std::vector<std::size_t>> generated_tags;
    std::vector<std::vector<std::size_t>> generated_nodes;
    gmsh::model::mesh::getElements(generated_types, generated_tags,
                                   generated_nodes, dim);
    const QString expected = dim == 3 ? "Hexahedron" : "Quadrilateral";
    for (const int element_type : generated_types) {
      std::string type_name;
      int type_dim = 0;
      int type_order = 0;
      int type_nodes = 0;
      int type_primary_nodes = 0;
      std::vector<double> local_coords;
      gmsh::model::mesh::getElementProperties(
          element_type, type_name, type_dim, type_order, type_nodes,
          local_coords, type_primary_nodes);
      if (!QString::fromStdString(type_name).contains(expected,
                                                      Qt::CaseInsensitive)) {
        throw MeshJobError(
            QString("Mapped mesh validation failed: expected only %1 "
                    "cells, got %2. The previous output was preserved.")
                .arg(expected)
                .arg(QString::fromStdString(type_name)),
            MeshJobError::Kind::Generic);
      }
    }
  }

  const QString out_path = spec.output_path;
  QDir().mkpath(QFileInfo(out_path).absolutePath());
  QFile generated_file(plan.generated_path);
  if (!generated_file.open(QIODevice::ReadOnly)) {
    throw MeshJobError(
        QString("Could not read generated mesh: %1")
            .arg(generated_file.errorString()),
        MeshJobError::Kind::Generic);
  }
  QSaveFile output_file(out_path);
  if (!output_file.open(QIODevice::WriteOnly)) {
    throw MeshJobError(
        QString("Could not open mesh output: %1")
            .arg(output_file.errorString()),
        MeshJobError::Kind::Generic);
  }
  while (!generated_file.atEnd()) {
    const QByteArray chunk = generated_file.read(1024 * 1024);
    if (chunk.isEmpty() && generated_file.error() != QFile::NoError) {
      output_file.cancelWriting();
      throw MeshJobError(
          QString("Could not read generated mesh: %1")
              .arg(generated_file.errorString()),
          MeshJobError::Kind::Generic);
    }
    if (output_file.write(chunk) != chunk.size()) {
      output_file.cancelWriting();
      throw MeshJobError(
          QString("Could not write mesh output: %1")
              .arg(output_file.errorString()),
          MeshJobError::Kind::Generic);
    }
  }
  if (!output_file.commit()) {
    throw MeshJobError(
        QString("Could not commit mesh output: %1")
            .arg(output_file.errorString()),
        MeshJobError::Kind::Generic);
  }

  std::vector<std::pair<int, int>> phys_groups;
  gmsh::model::getPhysicalGroups(phys_groups);
  for (const auto& p : phys_groups) {
    if (p.first != boundary_dim) {
      continue;
    }
    std::string name;
    gmsh::model::getPhysicalName(p.first, p.second, name);
    if (name.empty()) {
      name = "boundary_" + std::to_string(p.second);
    }
    outcome.boundary_names << QString::fromStdString(name);
  }

  std::vector<std::pair<int, int>> vol_groups;
  gmsh::model::getPhysicalGroups(vol_groups, dim);
  for (const auto& g : vol_groups) {
    std::string name;
    gmsh::model::getPhysicalName(g.first, g.second, name);
    if (name.empty()) {
      name = "volume_" + std::to_string(g.second);
    }
    outcome.volume_names << QString::fromStdString(name);
  }

  std::vector<int> element_types;
  std::vector<std::vector<std::size_t>> element_tags;
  std::vector<std::vector<std::size_t>> element_nodes;
  gmsh::model::mesh::getElements(element_types, element_tags, element_nodes);
  for (const auto& tags : element_tags) {
    outcome.element_count += tags.size();
  }
  outcome.node_count = static_cast<int>(node_tags.size());

  // 面板在此发射 boundary_groups/volume_groups/mesh_written 并刷新面板
  // 列表，保持既有发射/日志交错顺序。
  if (mid_hook) {
    mid_hook(outcome);
  }

  double quality_min = 0.0;
  double quality_mean = 0.0;
  double quality_max = 0.0;
  bool has_quality = false;
  // minSICN 只对最高维单元有意义。把点/线等低维单元混入查询会让
  // Gmsh 抛出 "Unknown element type for ordered monomials: 1"，从而把
  // 一个有效的 3D 网格误报成质量统计失败。
  std::vector<int> quality_element_types;
  std::vector<std::vector<std::size_t>> quality_element_tag_groups;
  std::vector<std::vector<std::size_t>> quality_element_nodes;
  gmsh::model::mesh::getElements(quality_element_types,
                                 quality_element_tag_groups,
                                 quality_element_nodes, dim);
  std::vector<std::size_t> quality_element_tags;
  QStringList top_element_composition;
  for (const auto& tags : quality_element_tag_groups) {
    quality_element_tags.insert(quality_element_tags.end(), tags.begin(),
                                tags.end());
  }
  for (std::size_t i = 0; i < quality_element_types.size(); ++i) {
    std::string type_name;
    int type_dim = 0;
    int type_order = 0;
    int type_nodes = 0;
    int type_primary_nodes = 0;
    std::vector<double> local_coords;
    gmsh::model::mesh::getElementProperties(
        quality_element_types[i], type_name, type_dim, type_order,
        type_nodes, local_coords, type_primary_nodes);
    const std::size_t count = i < quality_element_tag_groups.size()
                                  ? quality_element_tag_groups[i].size()
                                  : 0;
    top_element_composition <<
        QString("%1=%2").arg(QString::fromStdString(type_name)).arg(count);
  }
  if (!top_element_composition.isEmpty() && log) {
    log("Top-dimensional element composition: " +
        top_element_composition.join(", "));
  }
  if (dim >= 2 && !quality_element_tags.empty()) {
    try {
      std::vector<double> qualities;
      qualities.resize(quality_element_tags.size());
      gmsh::model::mesh::getElementQualities(quality_element_tags, qualities,
                                             "minSICN");
      double qmin = qualities.front();
      double qmax = qualities.front();
      double qsum = 0.0;
      for (double q : qualities) {
        qmin = std::min(qmin, q);
        qmax = std::max(qmax, q);
        qsum += q;
      }
      const double qmean = qsum / static_cast<double>(qualities.size());
      quality_min = qmin;
      quality_mean = qmean;
      quality_max = qmax;
      has_quality = true;
      if (log) {
        log(QString("Quality (minSICN) min=%1 mean=%2 max=%3")
                .arg(qmin, 0, 'g', 6)
                .arg(qmean, 0, 'g', 6)
                .arg(qmax, 0, 'g', 6));
      }
    } catch (const std::exception& ex) {
      if (log) {
        log(QString("Quality report failed: %1").arg(ex.what()));
      }
    }
  }
  outcome.has_quality = has_quality;
  outcome.quality_min = quality_min;
  outcome.quality_mean = quality_mean;
  outcome.quality_max = quality_max;

  try {
    std::vector<std::pair<int, int>> groups;
    gmsh::model::getPhysicalGroups(groups);
    if (!groups.empty() && log) {
      log("Physical group element counts:");
    }
    for (const auto& g : groups) {
      std::string name;
      gmsh::model::getPhysicalName(g.first, g.second, name);
      std::vector<int> ent_tags;
      gmsh::model::getEntitiesForPhysicalGroup(g.first, g.second, ent_tags);
      std::size_t count = 0;
      for (int ent : ent_tags) {
        std::vector<int> etypes;
        std::vector<std::vector<std::size_t>> etags;
        std::vector<std::vector<std::size_t>> enodes;
        gmsh::model::mesh::getElements(etypes, etags, enodes, g.first, ent);
        for (const auto& tags : etags) {
          count += tags.size();
        }
      }
      const QString label = name.empty()
                                ? QString("%1:%2").arg(g.first).arg(g.second)
                                : QString("%1:%2 %3")
                                      .arg(g.first)
                                      .arg(g.second)
                                      .arg(QString::fromStdString(name));
      if (log) {
        log(QString("  %1 -> %2 elems").arg(label).arg(count));
      }
    }
  } catch (const std::exception& ex) {
    if (log) {
      log(QString("Physical group count failed: %1").arg(ex.what()));
    }
  }

  // W-00b：回读物理组清单 + 网格摘要 + 文件 SHA-256，作为
  // PhysicalGroupManifest 的生产者。读取失败仅降级为警告，不阻断生成。
  try {
    QVariantMap manifest;
    manifest.insert("mesh_path", out_path);
    QFile mesh_file(out_path);
    if (mesh_file.open(QIODevice::ReadOnly)) {
      manifest.insert(
          "mesh_sha256",
          QString::fromLatin1(
              QCryptographicHash::hash(mesh_file.readAll(),
                                       QCryptographicHash::Sha256)
                  .toHex()));
    }
    QVariantList group_list;
    std::vector<std::pair<int, int>> manifest_groups;
    gmsh::model::getPhysicalGroups(manifest_groups);
    for (const auto& g : manifest_groups) {
      std::string gname;
      gmsh::model::getPhysicalName(g.first, g.second, gname);
      QVariantMap entry;
      entry.insert("name",
                   gname.empty()
                       ? QString("group_%1_%2").arg(g.first).arg(g.second)
                       : QString::fromStdString(gname));
      entry.insert("dim", g.first);
      entry.insert("tags", QVariantList{g.second});
      std::vector<int> ent_tags;
      gmsh::model::getEntitiesForPhysicalGroup(g.first, g.second, ent_tags);
      entry.insert("entity_count", static_cast<int>(ent_tags.size()));
      std::size_t group_elements = 0;
      for (int ent : ent_tags) {
        std::vector<int> etypes;
        std::vector<std::vector<std::size_t>> etags;
        std::vector<std::vector<std::size_t>> enodes;
        gmsh::model::mesh::getElements(etypes, etags, enodes, g.first, ent);
        for (const auto& tags : etags) {
          group_elements += tags.size();
        }
      }
      entry.insert("element_count", static_cast<int>(group_elements));
      group_list.append(entry);
    }
    manifest.insert("physical_groups", group_list);
    manifest.insert("mesh_dim", dim);
    manifest.insert("node_count", outcome.node_count);
    manifest.insert("element_count", static_cast<int>(outcome.element_count));
    QString dominant_type;
    std::size_t dominant_count = 0;
    for (std::size_t t = 0; t < quality_element_types.size(); ++t) {
      if (t < quality_element_tag_groups.size() &&
          quality_element_tag_groups[t].size() > dominant_count) {
        dominant_count = quality_element_tag_groups[t].size();
        std::string type_name;
        int etype_dim = 0;
        int etype_order = 0;
        int num_nodes = 0;
        int num_primary_nodes = 0;
        std::vector<double> local_coords;
        gmsh::model::mesh::getElementProperties(
            quality_element_types[t], type_name, etype_dim, etype_order,
            num_nodes, local_coords, num_primary_nodes);
        dominant_type = QString::fromStdString(type_name);
      }
    }
    manifest.insert("element_type", dominant_type);
    if (has_quality) {
      manifest.insert("quality_summary",
                      QVariantMap{{"quality_min", quality_min},
                                  {"quality_avg", quality_mean},
                                  {"quality_max", quality_max}});
    }
    outcome.manifest = manifest;
  } catch (const std::exception& ex) {
    if (log) {
      log(QString("Physical group manifest collection failed: %1")
              .arg(ex.what()));
    }
  }
  return outcome;
#else
  Q_UNUSED(plan);
  Q_UNUSED(spec);
  Q_UNUSED(log);
  Q_UNUSED(mid_hook);
  throw MeshJobError("Gmsh is not enabled in this build.",
                     MeshJobError::Kind::Generic);
#endif
}

}  // namespace gmp
