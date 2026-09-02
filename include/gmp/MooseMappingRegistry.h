#pragma once

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

namespace gmp {

// MOOSE 映射注册表中单个 block（如 Materials、BCs）的元数据。
struct MooseMappingBlock {
  QString name;
  QString description;
  QMap<QString, QJsonObject> objects;  // object_type -> object schema
  QJsonObject raw;
};

// MOOSE 类型/参数映射注册表。
// 对应需求 REQ-013/REQ-014：表单、校验器和 .i 生成器共用同一份定义。
class MooseMappingRegistry {
 public:
  explicit MooseMappingRegistry();
  explicit MooseMappingRegistry(const QString& file_path);

  bool load(const QString& file_path);
  bool is_loaded() const;

  QString version() const;
  QStringList block_names() const;
  bool has_block(const QString& name) const;
  MooseMappingBlock block(const QString& name) const;

  // 返回注册表推荐的 block 输出顺序。
  QStringList block_order() const;

  // 返回指定 block 下可用的对象类型。
  QStringList object_types(const QString& block_name) const;

  // 返回指定对象类型的 schema。
  QJsonObject object_schema(const QString& block_name,
                            const QString& object_type) const;

  // 检查对象类型是否真实存在。
  bool has_object_type(const QString& block_name,
                       const QString& object_type) const;

  QString last_error() const;

  // 默认映射注册表路径：templates/moose/mapping-v1.json
  static QString default_path();

 private:
  QString file_path_;
  QMap<QString, MooseMappingBlock> blocks_;
  QStringList order_;
  QString version_;
  QString last_error_;
  bool loaded_ = false;
};

}  // namespace gmp
