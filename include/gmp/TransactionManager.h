#pragma once

// v0.2 核心层（hc_core 过渡形态，Q5 批复：逻辑边界先行）。事务骨架
// （doc/ref/03 §5.1 事务职责；Q4 批复：v0.2 只建 Transaction 骨架，
// 不提供任何用户可见的 Undo/Redo 入口）。本阶段仅接管“属性表单
// 确定/取消”的编辑缓冲语义作为参考用法，表单现有缓冲代码不变。
// 只依赖 Qt Core。

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>
#include <memory>
#include <vector>

namespace gmp::core {

class PropertyBag;

// 可执行/可回放的命令。before/after 摘要用于审计与回放。
class Command {
 public:
  virtual ~Command() = default;
  virtual void execute() = 0;
  virtual void revert() = 0;
  virtual QString description() const = 0;
  virtual QVariantMap beforeSummary() const { return {}; }
  virtual QVariantMap afterSummary() const { return {}; }
};

struct TransactionRecord {
  QString label;
  bool committed = false;
  QStringList commands;       // 命令描述，执行序
  QList<QVariantMap> before;  // 与 commands 对齐的 before 摘要
  QList<QVariantMap> after;   // 与 commands 对齐的 after 摘要
};

// 非嵌套事务管理器：begin → execute* → commit | rollback。
// 嵌套 begin、无事务 commit/rollback、事务外 execute 均拒绝（返回 false）。
class TransactionManager {
 public:
  bool begin(const QString& label);
  bool inTransaction() const { return active_ != nullptr; }

  // 事务内执行命令并登记；命令所有权移交给当前事务。
  bool execute(std::unique_ptr<Command> command);

  bool commit();   // 生效：命令不回滚，审计记录 committed
  bool rollback(); // 逆序 revert 全部命令，审计记录 rolled back

  const QList<TransactionRecord>& auditLog() const { return audit_log_; }

 private:
  struct ActiveTransaction {
    QString label;
    std::vector<std::unique_ptr<Command>> commands;
  };
  std::unique_ptr<ActiveTransaction> active_;
  QList<TransactionRecord> audit_log_;

  TransactionRecord finish(bool committed);
};

// 参考用法（TASK-V02-013）：FloatingPropertyForm 缓冲语义的事务映射。
//   打开表单   = begin(label)（建立缓冲快照）
//   编辑字段   = 在缓冲上 execute(SetPropertyValueCommand)
//   确定       = 校验通过后 commit（写回目标；表单侧现有代码不变）
//   取消/校验失败 = rollback（目标保持原值）
class SetPropertyValueCommand : public Command {
 public:
  SetPropertyValueCommand(PropertyBag& target, QString key, QVariant value,
                          QString description = QString());

  void execute() override;
  void revert() override;
  QString description() const override { return description_; }
  QVariantMap beforeSummary() const override;
  QVariantMap afterSummary() const override;

 private:
  PropertyBag& target_;
  QString key_;
  QVariant after_value_;
  QVariant before_value_;
  bool had_before_ = false;
  QString description_;
};

}  // namespace gmp::core
