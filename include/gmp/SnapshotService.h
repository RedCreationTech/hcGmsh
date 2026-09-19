#pragma once

// v0.2 Stage 3 hc_simulation 过渡层（Q5 批复：逻辑边界先行）。
// SnapshotService：作业快照导出的编排层——导出前数据闸门、.i 引用归一化、
// case-<timestamp> 版本目录分配、SnapshotExportConfig 组装、调用
// MooseSnapshot 的 export_job_snapshot_v2。只依赖 Qt Core，不依赖
// Qt Widgets；UI 决策（目录选择对话框、弹窗、日志呈现）留在 MoosePanel。
// 实现逐字搬运自 MoosePanel::on_export_snapshot（行为冻结）。

#include <QMap>
#include <QString>
#include <QVariantMap>

#include "gmp/ApplicationProfile.h"
#include "gmp/MooseSnapshot.h"
#include "gmp/MooseTemplates.h"
#include "gmp/PhysicalGroupManifest.h"

namespace gmp {

struct SnapshotExportRequest {
  QString dest_parent;  // UI 已选定的导出父目录
  QString input_text;   // 已 normalize_refs 归一化的 .i 内容
  QString input_path_text;   // 输入路径字段当前值（input_name 回退用）
  QString mesh_path_text;    // Mesh File 字段当前值（.e 角色判定）
  MooseTemplateInfo current_template;
  QString input_mode;
  QString project_path;      // 用于 project_sha256；可空
  ApplicationProfile profile;
  QVariantMap unit_contract;          // display_to_solver_factors 取自其中
  QMap<QString, QString> file_sources;  // normalize 结果 + 显式来源表合并后
  QMap<QString, QString> file_roles;
  PhysicalGroupManifest physical_groups;  // 项目内清单（绝对路径）
  QString generator_version;
};

struct SnapshotExportOutcome {
  bool ok = false;
  QString error;  // 失败原因（UI 呈现）
  QString dir_name;
  SnapshotExportResult result;
};

class SnapshotService {
 public:
  // 导出前数据闸门（不含 UI 工作流态）：活动档案 + Physical Groups 清单
  // 完整性。返回空串 = 通过；否则为既有拒绝文案（逐字保持）。
  static QString preflight_error(const ApplicationProfile& profile,
                                 const PhysicalGroupManifest& manifest);

  // .i 引用归一化：绝对引用改写为快照内 basename、登记来源与角色；
  // extra_file_sources（材料 CSV 显式来源表）并入但不覆盖已登记条目。
  // 返回空串 = 成功；否则为拒绝原因（既有文案逐字保持）。
  static QString normalize_refs(const QString& mesh_path_text,
                                const QMap<QString, QString>& extra_file_sources,
                                QString* input_text,
                                QMap<QString, QString>* file_sources,
                                QMap<QString, QString>* file_roles);

  // 总编排：版本目录分配（case-<timestamp>，撞名加 -2 后缀）→ cfg 组装
  // （项目哈希、单位因子、快照内 basename 网格路径重写）→ 导出。
  static SnapshotExportOutcome export_snapshot(
      const SnapshotExportRequest& request);
};

}  // namespace gmp
