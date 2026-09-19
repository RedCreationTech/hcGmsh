#pragma once

// v0.2 Stage 4 hc_cad 过渡层（Q5 批复：逻辑边界先行）。
// AssemblyGeometryService：装配实例几何构建——实例导入/变换落实、实例组与
// _surface 边界组生成、自定义组恢复（经 PhysicalGroupService）。只依赖
// Qt Core + gmsh API（GMP_ENABLE_GMSH_GUI 守护），不依赖 Qt Widgets；
// 模型选择器重登记、面板状态与日志呈现留在 GmshPanel。
// 实现逐字搬运自 GmshPanel::build_assembly；019 时序红线：自定义组恢复
// 仍发生在组创建之后、面板 note_external_model_loaded 发射之前。

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace gmp {

class PhysicalGroupService;

struct AssemblyBuildResult {
  bool ok = false;
  QString error;
  int visible_count = 0;
  QStringList log_lines;  // 自定义组恢复日志（面板按既有顺序呈现）
};

class AssemblyGeometryService {
 public:
  // 在当前进程内重建 "assembly" 模型。instances 为装配实例参数表
  // （name/source_path/visible/order/translate_*/rotate_*/scale_*）。
  static AssemblyBuildResult build(const QVariantList& instances,
                                   PhysicalGroupService* groups);
};

}  // namespace gmp
