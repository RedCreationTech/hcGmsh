#include "gmp/TransactionManager.h"

#include "gmp/PropertyBag.h"

namespace gmp::core {

bool TransactionManager::begin(const QString& label) {
  if (active_) {
    return false;  // 嵌套事务拒绝
  }
  active_ = std::make_unique<ActiveTransaction>();
  active_->label = label;
  return true;
}

bool TransactionManager::execute(std::unique_ptr<Command> command) {
  if (!active_ || !command) {
    return false;
  }
  command->execute();
  active_->commands.push_back(std::move(command));
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
  if (!active_) {
    return false;
  }
  finish(true);
  return true;
}

bool TransactionManager::rollback() {
  if (!active_) {
    return false;
  }
  for (auto it = active_->commands.rbegin(); it != active_->commands.rend();
       ++it) {
    (*it)->revert();
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

void SetPropertyValueCommand::execute() {
  target_.set(key_, after_value_);
}

void SetPropertyValueCommand::revert() {
  if (had_before_) {
    target_.set(key_, before_value_);
  } else {
    target_.remove(key_);
  }
}

QVariantMap SetPropertyValueCommand::beforeSummary() const {
  return {{key_, before_value_}};
}

QVariantMap SetPropertyValueCommand::afterSummary() const {
  return {{key_, after_value_}};
}

}  // namespace gmp::core
