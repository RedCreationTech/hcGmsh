#include "gmp/SnapshotService.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace gmp {

QString SnapshotService::preflight_error(
    const ApplicationProfile& profile, const PhysicalGroupManifest& manifest) {
  // W-00c：快照合同 v2——缺活动档案或 Physical Groups 清单时拒绝导出，
  // 不产出半成品快照目录。
  if (!profile.valid || profile.mapping_version.trimmed().isEmpty()) {
    return QStringLiteral(
        "No active application profile (with mapping version). Select an "
        "application profile in the work context bar before exporting "
        "(snapshot contract v2 requires application_profile).");
  }
  // 导出就绪检查：清单需有物理组、SHA-256 与网格维度。生成时 mesh_path
  // 记录为绝对路径是合法的；相对路径约束作用于打包后的快照清单，
  // 导出时以快照内 basename 重写（见 export_snapshot 的 cfg 组装）。
  static const QRegularExpression sha256_re(
      QStringLiteral("^[0-9a-fA-F]{64}$"));
  if (manifest.groups.isEmpty() ||
      !sha256_re.match(manifest.mesh_sha256).hasMatch() ||
      manifest.mesh_dim < 1 || manifest.mesh_dim > 3) {
    return QStringLiteral(
        "Physical group manifest is missing or incomplete. Generate the mesh "
        "first (snapshot contract v2 requires traceability.physical_groups).");
  }
  return QString();
}

QString SnapshotService::normalize_refs(
    const QString& mesh_path_text,
    const QMap<QString, QString>& extra_file_sources, QString* input_text,
    QMap<QString, QString>* file_sources, QMap<QString, QString>* file_roles) {
  QStringList refs = scan_input_file_refs(*input_text);
  refs.removeDuplicates();
  // 先处理较长的引用，避免一个绝对路径是另一个的前缀时替换错位。
  std::sort(refs.begin(), refs.end(),
            [](const QString& a, const QString& b) {
              return a.size() > b.size();
            });
  const QString mesh_text = mesh_path_text.trimmed();
  const QString mesh_abs =
      mesh_text.isEmpty() ? QString() : QFileInfo(mesh_text).absoluteFilePath();
  QString text = *input_text;
  for (const QString& ref : refs) {
    if (!QFileInfo(ref).isAbsolute()) {
      continue;  // 相对引用由 v2 合同直接校验
    }
    if (!QFileInfo::exists(ref)) {
      return "referenced file does not exist: " + ref;
    }
    const QString abs = QFileInfo(ref).absoluteFilePath();
    const QString base = QFileInfo(ref).fileName();
    if (file_sources->contains(base) && file_sources->value(base) != abs) {
      return "referenced files from different directories share the same "
             "file name: " +
             base;
    }
    file_sources->insert(base, abs);
    text.replace(ref, base);
    if (base.toLower().endsWith(".e")) {
      // .e 必须显式角色：仅当用户经 Mesh File 字段显式指定该文件为网格时
      // 才标记 input_mesh，其余情形拒绝并说明。
      if (!mesh_abs.isEmpty() && mesh_abs == abs) {
        file_roles->insert(base, QStringLiteral("input_mesh"));
      } else {
        return "Exodus file must be explicitly imported as the mesh "
               "(role=input_mesh) before export: " +
               ref;
      }
    }
  }
  // W-03a：并入材料 CSV 等显式来源表（不覆盖 .i 绝对引用已登记的条目）。
  for (auto it = extra_file_sources.constBegin();
       it != extra_file_sources.constEnd(); ++it) {
    if (!file_sources->contains(it.key())) {
      file_sources->insert(it.key(), it.value());
    }
  }
  *input_text = text;
  return QString();
}

SnapshotExportOutcome SnapshotService::export_snapshot(
    const SnapshotExportRequest& request) {
  SnapshotExportOutcome outcome;

  // 合同 §5：每次导出生成新的 case-<timestamp> 版本目录，旧快照保持不变。
  QString dir_name =
      "case-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
  int suffix = 2;
  const QString base_name = dir_name;
  while (QFileInfo::exists(QDir(request.dest_parent).filePath(dir_name))) {
    dir_name = base_name + QString("-%1").arg(suffix++);
  }
  const QString dest = QDir(request.dest_parent).filePath(dir_name);
  outcome.dir_name = dir_name;

  QString input_name;
  if (request.current_template.valid) {
    input_name = request.current_template.key + ".i";
  } else {
    input_name = QFileInfo(request.input_path_text).fileName();
    if (!input_name.endsWith(".i")) {
      input_name = "custom.i";
    }
  }

  SnapshotExportConfig cfg;
  cfg.case_name = request.current_template.valid
                      ? request.current_template.key
                      : QFileInfo(input_name).completeBaseName();
  cfg.case_id = dir_name;
  cfg.input_mode = request.input_mode;
  cfg.generator_version = request.generator_version;
  cfg.project_path = request.project_path;
  if (!request.project_path.isEmpty() &&
      QFileInfo::exists(request.project_path)) {
    bool hash_ok = false;
    cfg.project_sha256 = sha256_file_hex(request.project_path, &hash_ok);
  }
  cfg.profile = request.profile;
  const QVariantMap factors =
      request.unit_contract.value("display_to_solver_factors").toMap();
  for (auto it = factors.begin(); it != factors.end(); ++it) {
    cfg.unit_factors.insert(it.key(), it.value().toDouble());
  }
  cfg.file_roles = request.file_roles;
  cfg.file_sources = request.file_sources;
  // 快照内清单使用包内相对网格路径（网格文件以 basename 复制入包），
  // 满足 v2 合同对相对路径的约束；项目内记录的清单仍保留绝对路径。
  PhysicalGroupManifest packaged_manifest = request.physical_groups;
  packaged_manifest.mesh_path =
      QFileInfo(packaged_manifest.mesh_path).fileName();
  cfg.physical_groups = packaged_manifest;

  outcome.result = export_job_snapshot_v2(dest, request.input_text, input_name,
                                          request.current_template, cfg);
  outcome.ok = outcome.result.ok;
  if (!outcome.ok) {
    outcome.error = outcome.result.error;
  }
  return outcome;
}

}  // namespace gmp
