#include "gmp/MooseTemplates.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace gmp {

namespace {

QStringList json_string_list(const QJsonValue& value) {
  QStringList out;
  const QJsonArray arr = value.toArray();
  for (const auto& v : arr) {
    const QString s = v.toString();
    if (!s.isEmpty()) {
      out << s;
    }
  }
  return out;
}

}  // namespace

QString moose_templates_root() {
  const QByteArray env = qgetenv("GMP_MOOSE_TEMPLATES");
  if (!env.isEmpty()) {
    const QString path = QString::fromLocal8Bit(env);
    if (QDir(path).exists()) {
      return QDir(path).absolutePath();
    }
  }

#ifdef GMP_TEMPLATE_DIR
  const QString compiled = QString::fromUtf8(GMP_TEMPLATE_DIR);
  if (QDir(compiled).exists()) {
    return QDir(compiled).absolutePath();
  }
#endif

  const QString app_dir = QCoreApplication::applicationDirPath();
  const QStringList candidates = {
      app_dir + "/templates/moose",
      app_dir + "/../templates/moose",
      app_dir + "/../../templates/moose",
      QDir::currentPath() + "/templates/moose",
  };
  for (const auto& c : candidates) {
    if (QDir(c).exists()) {
      return QDir(c).absolutePath();
    }
  }
  return QString();
}

MooseTemplateInfo load_moose_template(const QString& root, const QString& key) {
  MooseTemplateInfo info;
  info.key = key;
  if (root.isEmpty() || key.isEmpty()) {
    return info;
  }

  info.dir = QDir(root).filePath(key);
  info.input_path = QDir(info.dir).filePath("input.i");
  if (!QFileInfo::exists(info.input_path)) {
    return info;
  }

  const QString json_path = QDir(info.dir).filePath("template.json");
  QFile file(json_path);
  if (!file.open(QIODevice::ReadOnly)) {
    return info;
  }
  const QJsonDocument doc =
      QJsonDocument::fromJson(QString::fromUtf8(file.readAll()).toUtf8());
  if (!doc.isObject()) {
    return info;
  }
  const QJsonObject obj = doc.object();

  info.display_name = obj.value("display_name").toString();
  info.status = obj.value("status").toString("prototype");
  info.status_note = obj.value("status_note").toString();
  info.mesh_files = json_string_list(obj.value("mesh_files"));
  info.extra_files = json_string_list(obj.value("extra_files"));

  const QJsonObject src = obj.value("source").toObject();
  QStringList src_parts;
  const QString repo = src.value("repo").toString();
  const QString path = src.value("path").toString();
  const QString commit = src.value("commit").toString();
  if (!repo.isEmpty()) {
    src_parts << repo;
  }
  if (!path.isEmpty()) {
    src_parts << path;
  }
  if (!commit.isEmpty()) {
    src_parts << ("@ " + commit);
  }
  info.source_desc = src_parts.join(" ");

  info.valid = true;
  return info;
}

}  // namespace gmp
