#pragma once

#include <QString>
#include <QStringList>

#include "gmp/MooseTemplates.h"

namespace gmp {

struct SnapshotExportResult {
  bool ok = false;
  QString error;
  QString dir;
  QString input_file;
  QString input_sha256;
  QStringList mesh_files;    // 已复制的网格文件名（排序后）
  QStringList extra_files;   // 已复制的附件文件名（排序后）
  QStringList missing_files; // .i 中引用但未能定位的文件
  QString manifest_path;
};

QString sha256_hex(const QByteArray& data);
QString sha256_file_hex(const QString& path, bool* ok);

// 扫描 .i 文本中引用的外部文件（file = / data_file = / nodal_mass_file = 等），
// 按出现顺序返回原始引用串（未去重、未解析路径）。
QStringList scan_input_file_refs(const QString& input_text);

// 导出任务快照到 dest_dir：
//   1. 写 <input_file>（编辑区当前内容，UTF-8）；
//   2. 复制引用且可定位的网格/附件（模板声明的清单优先，其余按扩展名归类）；
//   3. 写 manifest.json（contract=CONTRACT-JOB / contract_version=0.1.0，
//      input_snapshot 结构对齐 job-manifest.schema.json）。
// 确定性：manifest 不含时间戳等易变字段，文件清单排序，同一编辑内容两次导出
// 所有文件 SHA-256 一致。
SnapshotExportResult export_job_snapshot(const QString& dest_dir,
                                         const QString& input_text,
                                         const QString& input_file,
                                         const MooseTemplateInfo& tpl);

}  // namespace gmp
