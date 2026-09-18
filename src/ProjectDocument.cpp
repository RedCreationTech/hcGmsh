#include "gmp/ProjectDocument.h"

#include <QSet>
#include <QUuid>

#include <vector>

#include "gmp/PropertyBag.h"

namespace gmp::core {

ObjectId ObjectId::generate() {
  return ObjectId(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QString to_string(ObjectStatus status) {
  switch (status) {
    case ObjectStatus::Ready:
      return QStringLiteral("ready");
    case ObjectStatus::Incomplete:
      return QStringLiteral("incomplete");
    case ObjectStatus::Invalid:
      return QStringLiteral("invalid");
    case ObjectStatus::Stale:
      return QStringLiteral("stale");
    case ObjectStatus::Disabled:
      return QStringLiteral("disabled");
  }
  return QStringLiteral("ready");
}

ObjectStatus object_status_from_string(const QString& text, bool* ok) {
  const QString lower = text.trimmed().toLower();
  if (ok) {
    *ok = true;
  }
  if (lower.isEmpty() || lower == "ready") {
    return ObjectStatus::Ready;
  }
  if (lower == "incomplete") {
    return ObjectStatus::Incomplete;
  }
  if (lower == "invalid") {
    return ObjectStatus::Invalid;
  }
  if (lower == "stale") {
    return ObjectStatus::Stale;
  }
  if (lower == "disabled") {
    return ObjectStatus::Disabled;
  }
  if (ok) {
    *ok = false;
  }
  return ObjectStatus::Ready;
}

ProjectObject::ProjectObject(ObjectKind kind, const QString& name, ObjectId id)
    : id_(std::move(id)),
      name_(name),
      kind_(std::move(kind)),
      properties_(std::make_unique<PropertyBag>()) {}

PropertyBag& ProjectObject::properties() {
  return *properties_;
}

const PropertyBag& ProjectObject::properties() const {
  return *properties_;
}

ProjectDocument::ProjectDocument() = default;
ProjectDocument::~ProjectDocument() = default;

ObjectId ProjectDocument::addObject(std::unique_ptr<ProjectObject> object,
                                    ObjectId parent) {
  if (!object) {
    return ObjectId();
  }
  if (parent.isValid() && objects_.find(parent.toString()) == objects_.end()) {
    return ObjectId();
  }
  if (!object->id().isValid()) {
    object->id_ = ObjectId::generate();
  }
  const QString key = object->id().toString();
  if (objects_.find(key) != objects_.end()) {
    return ObjectId();
  }
  const ObjectId id = object->id();
  parent_.insert(key, parent);
  children_[parent.toString()].append(id);
  objects_.emplace(key, std::move(object));
  return id;
}

bool ProjectDocument::removeObject(ObjectId id) {
  const QString key = id.toString();
  auto it = objects_.find(key);
  if (it == objects_.end()) {
    return false;
  }
  // 先递归删除子树，再摘除自身。
  const QList<ObjectId> offspring = children_.value(key);
  for (const ObjectId& child : offspring) {
    removeObject(child);
  }
  const ObjectId parent = parent_.value(key);
  children_[parent.toString()].removeAll(id);
  children_.remove(key);
  parent_.remove(key);
  objects_.erase(it);
  return true;
}

void ProjectDocument::clear() {
  objects_.clear();
  parent_.clear();
  children_.clear();
}

ProjectObject* ProjectDocument::object(ObjectId id) {
  auto it = objects_.find(id.toString());
  return it == objects_.end() ? nullptr : it->second.get();
}

const ProjectObject* ProjectDocument::object(ObjectId id) const {
  auto it = objects_.find(id.toString());
  return it == objects_.end() ? nullptr : it->second.get();
}

bool ProjectDocument::contains(ObjectId id) const {
  return objects_.find(id.toString()) != objects_.end();
}

int ProjectDocument::count() const {
  return static_cast<int>(objects_.size());
}

ObjectId ProjectDocument::parentOf(ObjectId id) const {
  return parent_.value(id.toString());
}

QList<ObjectId> ProjectDocument::children(ObjectId parent) const {
  return children_.value(parent.toString());
}

bool ProjectDocument::setStatus(ObjectId id, ObjectStatus status) {
  ProjectObject* target = object(id);
  if (!target) {
    return false;
  }
  target->setStatus(status);
  return true;
}

QVariantList ProjectDocument::to_variant_list() const {
  QVariantList list;
  // QMap 按键排序遍历，输出与挂载顺序无关，保证确定性。
  for (const auto& entry_pair : objects_) {
    const ProjectObject* object = entry_pair.second.get();
    QVariantMap entry;
    entry.insert("id", object->id().toString());
    entry.insert("name", object->name());
    entry.insert("kind", object->kind());
    entry.insert("status", to_string(object->status()));
    entry.insert("parent", parent_.value(object->id().toString()).toString());
    entry.insert("params", object->properties().to_variant_map());
    list.append(entry);
  }
  return list;
}

bool ProjectDocument::from_variant_list(const QVariantList& list,
                                        QString* error) {
  auto fail = [error](const QString& message) {
    if (error) {
      *error = message;
    }
    return false;
  };
  // 两遍装载：先建对象再挂层级，允许条目乱序。
  std::vector<std::unique_ptr<ProjectObject>> loaded;
  QMap<QString, ObjectId> parents;
  QSet<QString> seen;
  for (const QVariant& value : list) {
    const QVariantMap entry = value.toMap();
    const ObjectId id(entry.value("id").toString());
    if (!id.isValid()) {
      return fail("object entry is missing a stable id");
    }
    if (seen.contains(id.toString())) {
      return fail("duplicate object id: " + id.toString());
    }
    seen.insert(id.toString());
    auto object = std::make_unique<ProjectObject>(
        entry.value("kind").toString(), entry.value("name").toString(), id);
    bool status_ok = false;
    object->setStatus(object_status_from_string(
        entry.value("status").toString(), &status_ok));
    if (!status_ok) {
      return fail("unknown object status: " +
                  entry.value("status").toString());
    }
    object->properties() =
        PropertyBag::from_variant_map(entry.value("params").toMap());
    parents.insert(id.toString(),
                   ObjectId(entry.value("parent").toString()));
    loaded.push_back(std::move(object));
  }
  for (auto it = parents.cbegin(); it != parents.cend(); ++it) {
    if (it.value().isValid() && !seen.contains(it.value().toString())) {
      return fail("object parent does not exist: " + it.value().toString());
    }
  }
  clear();
  for (auto& object : loaded) {
    const ObjectId parent = parents.value(object->id().toString());
    if (!addObject(std::move(object), parent).isValid()) {
      return fail("failed to mount object during load");
    }
  }
  return true;
}

}  // namespace gmp::core
