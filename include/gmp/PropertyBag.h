#pragma once

// v0.2 核心层（hc_core 过渡形态，Q5 批复：逻辑边界先行）。属性系统
// （doc/ref/03 §6）：PropertyDefinition 描述属性的类型/单位/必填/默认值/
// 枚举/引用/校验钩子，PropertyBag 承载运行时值并提供变更回调。
// 只依赖 Qt Core；Stage 1 不接管任何 UI 读写（表单 kind 分支保留在
// PropertyEditor，Q6 批复）。

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include <functional>

namespace gmp::core {

struct PropertyDefinition {
  QString key;
  QString displayName;
  QString type;           // string | number | boolean | enum | reference …
  QString unit;           // 量纲（pressure/length…），可空
  bool required = false;
  QVariant defaultValue;
  QStringList enumValues;
  QString referenceKind;  // 引用目标根节点（Materials/Parts…），可空
  // 校验钩子：返回 false 并写 error 即视为违规。可空。
  std::function<bool(const QVariant& value, QString* error)> validator;
};

// 属性值包：内部按 QVariant 原样存储，保证与 YAML `params` 的 QVariantMap
// 双向转换零漂移（不隐式注入默认值、不丢未定义键）。
class PropertyBag {
 public:
  void setDefinitions(const QList<PropertyDefinition>& definitions);
  const QList<PropertyDefinition>& definitions() const { return definitions_; }
  const PropertyDefinition* definition(const QString& key) const;

  QVariant get(const QString& key) const;
  template <typename T>
  T get(const QString& key) const {
    return get(key).value<T>();
  }
  // 值真正变化时返回 true 并触发 onChanged 回调；同值写入返回 false。
  bool set(const QString& key, const QVariant& value);
  bool contains(const QString& key) const;
  // 移除键；存在并移除返回 true（触发 onChanged）。供事务 revert 使用。
  bool remove(const QString& key);
  QStringList keys() const;

  void setOnChanged(std::function<void(const QString& key)> callback);

  // 定义驱动校验：required 缺失、enum 越界、validator 钩子失败。
  // 返回违规 key 清单；无定义约束的键不参与校验。
  QStringList validate() const;

  // 仅为“缺失且有 defaultValue 的定义键”补默认值；不覆盖既有值。
  // 默认不自动调用——round-trip 必须保持加载原样。
  void apply_defaults();

  QVariantMap to_variant_map() const;
  static PropertyBag from_variant_map(const QVariantMap& params);

 private:
  QList<PropertyDefinition> definitions_;
  QVariantMap values_;
  std::function<void(const QString&)> on_changed_;
};

}  // namespace gmp::core
