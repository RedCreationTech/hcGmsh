#include "gmp/TransactionManager.h"

#include <QSet>

#include <exception>
#include <utility>

#include "gmp/PropertyBag.h"

namespace gmp::core {

namespace {

bool fail(QString* error, const QString& message) {
  if (error) {
    *error = message;
  }
  return false;
}

void clear_error(QString* error) {
  if (error) {
    error->clear();
  }
}

QVariantMap object_entry(const ProjectDocument& document, ObjectId id) {
  const ProjectObject* object = document.object(id);
  if (!object) {
    return {};
  }
  return {{"id", id.toString()},
          {"name", object->name()},
          {"kind", object->kind()},
          {"status", object->statusText()},
          {"parent", document.parentOf(id).toString()},
          {"params", object->properties().to_variant_map()}};
}

void append_subtree(const ProjectDocument& document, ObjectId id,
                    QVariantList* entries) {
  const QVariantMap entry = object_entry(document, id);
  if (entry.isEmpty()) {
    return;
  }
  entries->append(entry);
  for (const ObjectId& child : document.children(id)) {
    append_subtree(document, child, entries);
  }
}

std::unique_ptr<ProjectObject> object_from_entry(const QVariantMap& entry) {
  auto object = std::make_unique<ProjectObject>(
      entry.value("kind").toString(), entry.value("name").toString(),
      ObjectId(entry.value("id").toString()));
  object->setStatusText(entry.value("status").toString());
  object->properties() =
      PropertyBag::from_variant_map(entry.value("params").toMap());
  return object;
}

bool restore_subtree(ProjectDocument* document, const QVariantList& entries,
                     ObjectId parent, int index, QString* error) {
  if (entries.isEmpty()) {
    return fail(error, "delete command has no captured subtree");
  }
  if (parent.isValid() && !document->contains(parent)) {
    return fail(error, "deleted object's parent no longer exists");
  }
  QSet<QString> restored_ids;
  for (const QVariant& value : entries) {
    const QString id = value.toMap().value("id").toString();
    if (id.isEmpty() || restored_ids.contains(id) ||
        document->contains(ObjectId(id))) {
      return fail(error, "cannot restore duplicate object id: " + id);
    }
    restored_ids.insert(id);
  }

  ObjectId restored_root;
  for (int row = 0; row < entries.size(); ++row) {
    const QVariantMap entry = entries.at(row).toMap();
    const ObjectId entry_parent(
        row == 0 ? parent.toString() : entry.value("parent").toString());
    const ObjectId restored = document->addObject(
        object_from_entry(entry), entry_parent, row == 0 ? index : -1);
    if (!restored.isValid()) {
      if (restored_root.isValid()) {
        document->removeObject(restored_root);
      }
      return fail(error, "failed to restore deleted object: " +
                             entry.value("id").toString());
    }
    if (row == 0) {
      restored_root = restored;
    }
  }
  clear_error(error);
  return true;
}

}  // namespace

bool TransactionManager::begin(const QString& label) {
  if (active_) {
    return false;
  }
  active_ = std::make_unique<ActiveTransaction>();
  active_->label = label;
  return true;
}

bool TransactionManager::execute(std::unique_ptr<Command> command,
                                 QString* error) {
  clear_error(error);
  if (!active_) {
    return fail(error, "no active transaction");
  }
  if (!command) {
    return fail(error, "command is null");
  }
  if (active_->failed) {
    return fail(error, "transaction has a failed command");
  }
  if (active_->applied != active_->commands.size()) {
    return fail(error, "transaction rollback is incomplete");
  }
  try {
    if (!command->execute(error)) {
      active_->failed = true;
      return false;
    }
  } catch (const std::exception& e) {
    active_->failed = true;
    return fail(error, QString("command execution failed: %1").arg(e.what()));
  }
  active_->commands.push_back(std::move(command));
  ++active_->applied;
  return true;
}

TransactionRecord TransactionManager::finish(bool committed) {
  TransactionRecord record;
  record.label = active_->label;
  record.committed = committed;
  for (const auto& command : active_->commands) {
    record.commands << command->description();
    record.before << command->beforeSummary();
    record.after << command->afterSummary();
  }
  audit_log_.append(record);
  active_.reset();
  return record;
}

bool TransactionManager::commit() {
  if (!active_ || active_->failed ||
      active_->applied != active_->commands.size()) {
    return false;
  }
  finish(true);
  return true;
}

bool TransactionManager::rollback(QString* error) {
  clear_error(error);
  if (!active_) {
    return fail(error, "no active transaction");
  }
  while (active_->applied > 0) {
    Command* command = active_->commands.at(active_->applied - 1).get();
    try {
      if (!command->revert(error)) {
        return false;
      }
    } catch (const std::exception& e) {
      return fail(error, QString("command revert failed: %1").arg(e.what()));
    }
    --active_->applied;
  }
  finish(false);
  return true;
}

SetPropertyValueCommand::SetPropertyValueCommand(PropertyBag& target,
                                                 QString key, QVariant value,
                                                 QString description)
    : target_(target),
      key_(std::move(key)),
      after_value_(std::move(value)),
      before_value_(target.get(key_)),
      had_before_(target.contains(key_)),
      description_(description.isEmpty()
                       ? QStringLiteral("set %1").arg(key_)
                       : std::move(description)) {}

bool SetPropertyValueCommand::execute(QString* error) {
  clear_error(error);
  target_.set(key_, after_value_);
  return true;
}

bool SetPropertyValueCommand::revert(QString* error) {
  clear_error(error);
  if (had_before_) {
    target_.set(key_, before_value_);
  } else {
    target_.remove(key_);
  }
  return true;
}

QVariantMap SetPropertyValueCommand::beforeSummary() const {
  return {{key_, before_value_}};
}

QVariantMap SetPropertyValueCommand::afterSummary() const {
  return {{key_, after_value_}};
}

void TransactionManager::record_committed(const QString& label,
                                          const QString& description,
                                          const QVariantMap& before,
                                          const QVariantMap& after) {
  TransactionRecord record;
  record.label = label;
  record.committed = true;
  record.commands << description;
  record.before << before;
  record.after << after;
  audit_log_.append(record);
}

ClosureCommand::ClosureCommand(QString description, QVariantMap before,
                               QVariantMap after, std::function<void()> apply,
                               std::function<void()> revert)
    : description_(std::move(description)),
      before_(std::move(before)),
      after_(std::move(after)),
      apply_(std::move(apply)),
      revert_(std::move(revert)) {}

bool ClosureCommand::execute(QString* error) {
  clear_error(error);
  if (apply_) {
    apply_();
  }
  return true;
}

bool ClosureCommand::revert(QString* error) {
  clear_error(error);
  if (revert_) {
    revert_();
  }
  return true;
}

CreateObjectCommand::CreateObjectCommand(
    ProjectDocument& document, std::unique_ptr<ProjectObject> object,
    ObjectId parent, int index)
    : document_(document), parent_(std::move(parent)), index_(index) {
  if (!object) {
    return;
  }
  id_ = object->id().isValid() ? object->id() : ObjectId::generate();
  kind_ = object->kind();
  name_ = object->name();
  status_ = object->statusText();
  params_ = object->properties().to_variant_map();
  valid_ = true;
}

bool CreateObjectCommand::execute(QString* error) {
  clear_error(error);
  if (!valid_) {
    return fail(error, "create command has no object");
  }
  auto object = std::make_unique<ProjectObject>(kind_, name_, id_);
  object->setStatusText(status_);
  object->properties() = PropertyBag::from_variant_map(params_);
  if (document_.addObject(std::move(object), parent_, index_) != id_) {
    return fail(error, "failed to create object: " + id_.toString());
  }
  return true;
}

bool CreateObjectCommand::revert(QString* error) {
  clear_error(error);
  return document_.removeObject(id_)
             ? true
             : fail(error, "created object no longer exists: " +
                               id_.toString());
}

QString CreateObjectCommand::description() const {
  return QString("create %1 object").arg(kind_);
}

QVariantMap CreateObjectCommand::afterSummary() const {
  return {{"id", id_.toString()},
          {"parent", parent_.toString()},
          {"kind", kind_},
          {"name", name_},
          {"status", status_},
          {"params", params_}};
}

DeleteObjectCommand::DeleteObjectCommand(ProjectDocument& document,
                                         ObjectId id)
    : document_(document), id_(std::move(id)) {}

bool DeleteObjectCommand::execute(QString* error) {
  clear_error(error);
  if (!document_.contains(id_)) {
    return fail(error, "object does not exist: " + id_.toString());
  }
  if (subtree_.isEmpty()) {
    parent_ = document_.parentOf(id_);
    index_ = document_.children(parent_).indexOf(id_);
    append_subtree(document_, id_, &subtree_);
  }
  return document_.removeObject(id_)
             ? true
             : fail(error, "failed to delete object: " + id_.toString());
}

bool DeleteObjectCommand::revert(QString* error) {
  return restore_subtree(&document_, subtree_, parent_, index_, error);
}

QVariantMap DeleteObjectCommand::beforeSummary() const {
  return {{"id", id_.toString()},
          {"parent", parent_.toString()},
          {"index", index_},
          {"subtree", subtree_}};
}

RenameObjectCommand::RenameObjectCommand(ProjectDocument& document,
                                         ObjectId id, QString name)
    : document_(document), id_(std::move(id)), after_(std::move(name)) {}

bool RenameObjectCommand::execute(QString* error) {
  clear_error(error);
  ProjectObject* object = document_.object(id_);
  if (!object) {
    return fail(error, "object does not exist: " + id_.toString());
  }
  if (!captured_) {
    before_ = object->name();
    captured_ = true;
  }
  object->setName(after_);
  return true;
}

bool RenameObjectCommand::revert(QString* error) {
  clear_error(error);
  ProjectObject* object = document_.object(id_);
  if (!object || !captured_) {
    return fail(error, "renamed object cannot be restored: " + id_.toString());
  }
  object->setName(before_);
  return true;
}

QVariantMap RenameObjectCommand::beforeSummary() const {
  return {{"id", id_.toString()}, {"name", before_}};
}

QVariantMap RenameObjectCommand::afterSummary() const {
  return {{"id", id_.toString()}, {"name", after_}};
}

SetPropertiesCommand::SetPropertiesCommand(ProjectDocument& document,
                                           ObjectId id,
                                           QVariantMap properties)
    : document_(document), id_(std::move(id)), after_(std::move(properties)) {}

bool SetPropertiesCommand::execute(QString* error) {
  clear_error(error);
  ProjectObject* object = document_.object(id_);
  if (!object) {
    return fail(error, "object does not exist: " + id_.toString());
  }
  if (!captured_) {
    before_ = object->properties().to_variant_map();
    captured_ = true;
  }
  object->properties() = PropertyBag::from_variant_map(after_);
  return true;
}

bool SetPropertiesCommand::revert(QString* error) {
  clear_error(error);
  ProjectObject* object = document_.object(id_);
  if (!object || !captured_) {
    return fail(error,
                "object properties cannot be restored: " + id_.toString());
  }
  object->properties() = PropertyBag::from_variant_map(before_);
  return true;
}

SetStatusCommand::SetStatusCommand(ProjectDocument& document, ObjectId id,
                                   QString status)
    : document_(document), id_(std::move(id)), after_(std::move(status)) {}

SetStatusCommand::SetStatusCommand(ProjectDocument& document, ObjectId id,
                                   ObjectStatus status)
    : SetStatusCommand(document, std::move(id), to_string(status)) {}

bool SetStatusCommand::execute(QString* error) {
  clear_error(error);
  ProjectObject* object = document_.object(id_);
  if (!object) {
    return fail(error, "object does not exist: " + id_.toString());
  }
  if (!captured_) {
    before_ = object->statusText();
    captured_ = true;
  }
  object->setStatusText(after_);
  return true;
}

bool SetStatusCommand::revert(QString* error) {
  clear_error(error);
  ProjectObject* object = document_.object(id_);
  if (!object || !captured_) {
    return fail(error, "object status cannot be restored: " + id_.toString());
  }
  object->setStatusText(before_);
  return true;
}

QVariantMap SetStatusCommand::beforeSummary() const {
  return {{"id", id_.toString()}, {"status", before_}};
}

QVariantMap SetStatusCommand::afterSummary() const {
  return {{"id", id_.toString()}, {"status", after_}};
}

}  // namespace gmp::core
