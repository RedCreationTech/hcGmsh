#include "gmp/MooseMappingRegistry.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace gmp {

MooseMappingRegistry::MooseMappingRegistry() {}

MooseMappingRegistry::MooseMappingRegistry(const QString& file_path) {
  load(file_path);
}

bool MooseMappingRegistry::load(const QString& file_path) {
  blocks_.clear();
  order_.clear();
  last_error_.clear();
  version_.clear();
  file_path_ = file_path;

  QFile file(file_path);
  if (!file.open(QIODevice::ReadOnly)) {
    last_error_ = QStringLiteral("Failed to open mapping registry: %1").arg(file_path);
    return false;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isObject()) {
    last_error_ = QStringLiteral("Invalid JSON in mapping registry: %1").arg(file_path);
    return false;
  }

  const QJsonObject root = doc.object();
  version_ = root.value("version").toString("unknown");

  const QJsonObject blocks_obj = root.value("blocks").toObject();
  for (auto it = blocks_obj.begin(); it != blocks_obj.end(); ++it) {
    const QJsonObject block_obj = it.value().toObject();
    MooseMappingBlock block;
    block.name = it.key();
    block.description = block_obj.value("description").toString();
    block.raw = block_obj;

    const QJsonObject objects_obj = block_obj.value("objects").toObject();
    for (auto ot = objects_obj.begin(); ot != objects_obj.end(); ++ot) {
      block.objects.insert(ot.key(), ot.value().toObject());
    }
    blocks_.insert(block.name, block);
  }

  const QJsonArray order_arr = root.value("ordering").toArray();
  for (const QJsonValue& v : order_arr) {
    if (v.isString()) {
      order_.append(v.toString());
    }
  }

  // 将 ordering 中未包含的 block 按字母顺序追加，保证完整性。
  QStringList all = blocks_.keys();
  std::sort(all.begin(), all.end());
  for (const QString& name : all) {
    if (!order_.contains(name)) {
      order_.append(name);
    }
  }

  return true;
}

bool MooseMappingRegistry::is_loaded() const {
  return !blocks_.isEmpty();
}

QString MooseMappingRegistry::version() const {
  return version_;
}

QStringList MooseMappingRegistry::block_names() const {
  return blocks_.keys();
}

bool MooseMappingRegistry::has_block(const QString& name) const {
  return blocks_.contains(name);
}

MooseMappingBlock MooseMappingRegistry::block(const QString& name) const {
  return blocks_.value(name);
}

QStringList MooseMappingRegistry::block_order() const {
  return order_;
}

QStringList MooseMappingRegistry::object_types(const QString& block_name) const {
  const MooseMappingBlock b = blocks_.value(block_name);
  return b.objects.keys();
}

QJsonObject MooseMappingRegistry::object_schema(const QString& block_name,
                                                const QString& object_type) const {
  const MooseMappingBlock b = blocks_.value(block_name);
  return b.objects.value(object_type);
}

bool MooseMappingRegistry::has_object_type(const QString& block_name,
                                          const QString& object_type) const {
  const MooseMappingBlock b = blocks_.value(block_name);
  return b.objects.contains(object_type);
}

QString MooseMappingRegistry::last_error() const {
  return last_error_;
}

QString MooseMappingRegistry::default_path() {
  const QString app_dir = QCoreApplication::applicationDirPath();
  const QStringList candidates = {
      app_dir + "/templates/moose/mapping-v1.json",
      app_dir + "/../templates/moose/mapping-v1.json",
      app_dir + "/../../templates/moose/mapping-v1.json",
      QDir::currentPath() + "/templates/moose/mapping-v1.json",
  };
  for (const auto& c : candidates) {
    if (QFile::exists(c)) {
      return QFileInfo(c).absoluteFilePath();
    }
  }
  return QString();
}

}  // namespace gmp
