#include "gmp/MooseSnapshot.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace gmp {

namespace {

// 常见网格文件扩展名（未在 template.json 声明时据此归类 mesh_files）
bool looks_like_mesh(const QString& name) {
  const QString lower = name.toLower();
  return lower.endsWith(".e") || lower.contains(".e-s") ||
         lower.endsWith(".msh") || lower.endsWith(".inp") ||
         lower.endsWith(".exo") || lower.endsWith(".xda");
}

QString strip_quotes(const QString& token) {
  QString s = token.trimmed();
  if (s.size() >= 2 &&
      ((s.startsWith('"') && s.endsWith('"')) ||
       (s.startsWith('\'') && s.endsWith('\'')))) {
    s = s.mid(1, s.size() - 2);
  }
  return s;
}

}  // namespace

QString sha256_hex(const QByteArray& data) {
  return QString::fromLatin1(
      QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString sha256_file_hex(const QString& path, bool* ok) {
  if (ok) {
    *ok = false;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return QString();
  }
  QCryptographicHash hash(QCryptographicHash::Sha256);
  if (!hash.addData(&file)) {
    return QString();
  }
  if (ok) {
    *ok = true;
  }
  return QString::fromLatin1(hash.result().toHex());
}

QStringList scan_input_file_refs(const QString& input_text) {
  QStringList refs;
  // 匹配 MOOSE 输入中引用外部文件的常见参数赋值，以及本构表格使用的
  // *_file 参数（例如 compression_hardening_file）。
  static const QRegularExpression re(
      R"m((?:^|\s)(?:[A-Za-z0-9_]*_file|file)\s*=\s*([^\s#]+))m");
  auto it = re.globalMatch(input_text);
  while (it.hasNext()) {
    const QString token = strip_quotes(it.next().captured(1));
    if (token.isEmpty() || token == "none" || token.contains('=')) {
      continue;
    }
    refs << token;
  }
  return refs;
}

SnapshotExportResult export_job_snapshot(const QString& dest_dir,
                                         const QString& input_text,
                                         const QString& input_file,
                                         const MooseTemplateInfo& tpl) {
  SnapshotExportResult result;
  result.dir = dest_dir;
  result.input_file = input_file;

  if (dest_dir.isEmpty() || input_file.isEmpty()) {
    result.error = "empty destination directory or input file name";
    return result;
  }
  if (!QDir().mkpath(dest_dir)) {
    result.error = "failed to create destination directory: " + dest_dir;
    return result;
  }

  // 1. 写主输入文件
  const QString input_path = QDir(dest_dir).filePath(input_file);
  {
    QFile file(input_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      result.error = "failed to write input file: " + input_path;
      return result;
    }
    file.write(input_text.toUtf8());
  }
  bool hash_ok = false;
  result.input_sha256 = sha256_file_hex(input_path, &hash_ok);
  if (!hash_ok) {
    result.error = "failed to hash input file: " + input_path;
    return result;
  }

  // 2. 归类并复制引用文件（模板声明优先，其余按扩展名）
  QSet<QString> mesh_names;
  QSet<QString> extra_names;
  QStringList refs = scan_input_file_refs(input_text);
  // template.json 中显式声明的文件即为快照合同的一部分。即使主输入不直接
  // 引用审计清单，也必须一并复制并进入 manifest.json。
  if (tpl.valid) {
    refs.append(tpl.mesh_files);
    refs.append(tpl.extra_files);
  }
  refs.removeDuplicates();
  for (const auto& ref : refs) {
    // 解析来源路径：模板目录相对路径 > 绝对/工作目录相对路径
    QString source;
    if (tpl.valid && !tpl.dir.isEmpty()) {
      const QString in_tpl = QDir(tpl.dir).filePath(ref);
      if (QFileInfo::exists(in_tpl)) {
        source = in_tpl;
      }
    }
    if (source.isEmpty() && QFileInfo::exists(ref)) {
      source = QFileInfo(ref).absoluteFilePath();
    }
    if (source.isEmpty()) {
      result.missing_files << ref;
      continue;
    }

    const QString base = QFileInfo(ref).fileName();
    const QString dest = QDir(dest_dir).filePath(base);
    if (QFileInfo::exists(dest)) {
      QFile::remove(dest);
    }
    if (!QFile::copy(source, dest)) {
      result.error = "failed to copy referenced file: " + ref;
      return result;
    }

    const bool declared_mesh = tpl.valid && tpl.mesh_files.contains(base);
    const bool declared_extra = tpl.valid && tpl.extra_files.contains(base);
    if (declared_extra) {
      extra_names.insert(base);
    } else if (declared_mesh || looks_like_mesh(base)) {
      mesh_names.insert(base);
    } else {
      extra_names.insert(base);
    }
  }
  result.mesh_files = mesh_names.values();
  result.extra_files = extra_names.values();
  result.mesh_files.sort();
  result.extra_files.sort();

  // 3. manifest.json（键由 QJsonObject 字典序输出，无时间戳，保证确定性）
  QJsonObject manifest;
  manifest.insert("contract", "CONTRACT-JOB");
  manifest.insert("contract_version", "0.1.0");
  manifest.insert("case_name",
                  tpl.valid ? tpl.key : QFileInfo(input_file).completeBaseName());

  if (tpl.valid) {
    QJsonObject tpl_obj;
    tpl_obj.insert("key", tpl.key);
    tpl_obj.insert("status", tpl.status);
    if (!tpl.status_note.isEmpty()) {
      tpl_obj.insert("status_note", tpl.status_note);
    }
    if (!tpl.source_desc.isEmpty()) {
      tpl_obj.insert("source", tpl.source_desc);
    }
    manifest.insert("template", tpl_obj);
    if (tpl.status == "prototype") {
      manifest.insert("status", "prototype");
      manifest.insert(
          "disclaimer",
          "prototype 模板：不作为工程结论 / prototype template, not for "
          "engineering conclusions");
    }
  }

  QJsonObject snapshot;
  snapshot.insert("input_file", input_file);
  snapshot.insert("input_sha256", result.input_sha256);
  QJsonArray mesh_arr;
  for (const auto& name : result.mesh_files) {
    bool ok = false;
    const QString sha =
        sha256_file_hex(QDir(dest_dir).filePath(name), &ok);
    if (!ok) {
      result.error = "failed to hash mesh file: " + name;
      return result;
    }
    QJsonObject entry;
    entry.insert("name", name);
    entry.insert("sha256", sha);
    mesh_arr.append(entry);
  }
  snapshot.insert("mesh_files", mesh_arr);
  QJsonArray extra_arr;
  for (const auto& name : result.extra_files) {
    bool ok = false;
    const QString sha =
        sha256_file_hex(QDir(dest_dir).filePath(name), &ok);
    if (!ok) {
      result.error = "failed to hash extra file: " + name;
      return result;
    }
    QJsonObject entry;
    entry.insert("name", name);
    entry.insert("sha256", sha);
    extra_arr.append(entry);
  }
  snapshot.insert("extra_files", extra_arr);
  manifest.insert("input_snapshot", snapshot);

  result.manifest_path = QDir(dest_dir).filePath("manifest.json");
  {
    QFile file(result.manifest_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      result.error = "failed to write manifest: " + result.manifest_path;
      return result;
    }
    file.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    file.write("\n");
  }

  result.ok = true;
  return result;
}

}  // namespace gmp
