#pragma once

// v0.2 Stage 4 hc_mesh 过渡层（Q5 批复：逻辑边界先行）。
// GmshMesher：网格生成执行——结构化砖形网格约束、规模预检、worker 序列化
// 与进程参数组装（prepare）、生成后校验/落盘/清单产出（finalize）。
// 只依赖 Qt Core + gmsh API（GMP_ENABLE_GMSH_GUI 守护），不依赖
// Qt Widgets；进度窗/取消/进程等待循环与信号发射留在 GmshPanel，经
// log 回调与 mid_hook 保持既有日志/发射顺序逐点不变。
// 实现逐字搬运自 GmshPanel::on_generate（019 时序红线适用）。

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>
#include <memory>
#include <stdexcept>

class QTemporaryDir;

namespace gmp {

// 网格作业参数（面板控件值的纯数据快照）。
struct MeshJobSpec {
  double size_x = 1.0;
  double size_y = 1.0;
  double size_z = 1.0;
  double mesh_size = 0.2;
  int elem_order = 1;
  int msh_version = 2;
  int high_order_opt = 0;
  int algo2d = 2;
  int algo3d = 1;
  int smoothing = 0;
  bool optimize = false;
  int topology_mode = 0;   // 0=自动 1=通用单纯形 2=严格结构化
  int configured_dim = -1; // -1 = 按几何推断
  bool use_sample_box = false;
  QString external_model_name;  // 空 = 当前模型
  QString output_path;
  double max_estimated_elements = 2000000.0;
  bool chinese_ui = false;
};

// 生成失败/拒绝。kind 供面板区分“规模预检/严格结构化”对话框分支。
class MeshJobError : public std::runtime_error {
 public:
  enum class Kind { Generic, SizeGuard, StructuredMode };
  MeshJobError(const QString& message, Kind kind)
      : std::runtime_error(message.toStdString()),
        message_(message),
        kind_(kind) {}
  const QString& message() const { return message_; }
  Kind kind() const { return kind_; }

 private:
  QString message_;
  Kind kind_;
};

struct MeshJobPlan {
  int dim = -1;
  double lc = 0.0;
  double estimated_elements = 0.0;
  bool use_structured_mesh = false;
  QString structured_fallback_reason;
  QString source_path;
  QString restore_path;
  QString generated_path;
  QString gmsh_executable;
  QStringList process_args;
  bool box_created = false;  // 示例盒兜底已建（面板据此更新控件）
  std::shared_ptr<QTemporaryDir> temp_dir;  // 生命周期托管到 finalize 后
};

struct MeshJobOutcome {
  QStringList boundary_names;
  QStringList volume_names;
  QVariantMap manifest;
  int node_count = 0;
  std::size_t element_count = 0;
  bool has_quality = false;
  double quality_min = 0.0;
  double quality_mean = 0.0;
  double quality_max = 0.0;
};

class GmshMesher {
 public:
  // 生成前段：选项设置、空模型/示例盒兜底、维度解析、结构化资格与规模
  // 预检、worker 序列化与进程参数组装。装配重建由调用方（面板）在
  // prepare 前完成——019 时序不变。拒绝/失败抛 MeshJobError。
  static MeshJobPlan prepare(
      const MeshJobSpec& spec,
      const std::function<void(const QString&)>& log);

  // 生成后段：结构化结果校验、原子落盘、物理组回读、质量统计与清单
  // 产出。mid_hook 在“组清单回读完成、已落盘”之后调用一次——面板在
  // 其中发射 boundary_groups/volume_groups/mesh_written 并刷新面板列表，
  // 保持既有发射/日志交错顺序。失败抛 MeshJobError（Generic）。
  static MeshJobOutcome finalize(
      MeshJobPlan&& plan, const MeshJobSpec& spec,
      const std::function<void(const QString&)>& log,
      const std::function<void(const MeshJobOutcome&)>& mid_hook);
};

}  // namespace gmp
