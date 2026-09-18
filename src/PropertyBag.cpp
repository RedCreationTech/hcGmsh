#include "gmp/PropertyBag.h"

namespace gmp::core {

void PropertyBag::setDefinitions(const QList<PropertyDefinition>& definitions) {
  definitions_ = definitions;
}

const PropertyDefinition* PropertyBag::definition(const QString& key) const {
  for (const auto& def : definitions_) {
    if (def.key == key) {
      return &def;
    }
  }
  return nullptr;
}

QVariant PropertyBag::get(const QString& key) const {
  return values_.value(key);
}

bool PropertyBag::set(const QString& key, const QVariant& value) {
  const QVariant previous = values_.value(key);
  const bool existed = values_.contains(key);
  if (existed && previous == value) {
    return false;
  }
  values_.insert(key, value);
  if (on_changed_) {
    on_changed_(key);
  }
  return true;
}

bool PropertyBag::contains(const QString& key) const {
  return values_.contains(key);
}

QStringList PropertyBag::keys() const {
  return values_.keys();
}

void PropertyBag::setOnChanged(
    std::function<void(const QString& key)> callback) {
  on_changed_ = std::move(callback);
}

QStringList PropertyBag::validate() const {
  QStringList violations;
  for (const auto& def : definitions_) {
    const QVariant value = values_.value(def.key);
    const bool present =
        values_.contains(def.key) && !value.toString().trimmed().isEmpty();
    if (def.required && !present) {
      violations << def.key;
      continue;
    }
    if (!present) {
      continue;
    }
    if (!def.enumValues.isEmpty() &&
        !def.enumValues.contains(value.toString())) {
      violations << def.key;
      continue;
    }
    if (def.validator) {
      QString error;
      if (!def.validator(value, &error)) {
        violations << def.key;
      }
    }
  }
  return violations;
}

void PropertyBag::apply_defaults() {
  for (const auto& def : definitions_) {
    if (!values_.contains(def.key) && def.defaultValue.isValid()) {
      set(def.key, def.defaultValue);
    }
  }
}

QVariantMap PropertyBag::to_variant_map() const {
  return values_;
}

PropertyBag PropertyBag::from_variant_map(const QVariantMap& params) {
  PropertyBag bag;
  bag.values_ = params;
  return bag;
}

}  // namespace gmp::core
