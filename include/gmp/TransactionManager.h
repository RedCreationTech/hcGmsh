#pragma once

// v0.2.1 核心事务层：事务管理、可逆领域命令与审计。
// 本阶段不提供用户可见的 Undo/Redo 入口，不依赖 Qt Widgets。

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>
#include <memory>
#include <vector>

#include "gmp/ProjectDocument.h"

namespace gmp::core {

class PropertyBag;

// 可执行/可回放的命令。before/after 摘要用于审计与回放。
class Command {
 public:
  virtual ~Command() = default;
  virtual bool execute(QString* error = nullptr) = 0;
  virtual bool revert(QString* error = nullptr) = 0;
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
  bool execute(std::unique_ptr<Command> command, QString* error = nullptr);

  bool commit();  // 生效：命令不回滚，审计记录 committed
  bool rollback(QString* error = nullptr);  // 逆序 revert，失败可重试

  const QList<TransactionRecord>& auditLog() const { return audit_log_; }

  // 旁路审计（TASK-V02-061）：动作已由调用方完成（如属性表单既有缓冲
  // 提交），仅登记一条 committed 审计记录。Q4 红线：不提供回放入口。
  void record_committed(const QString& label, const QString& description,
                        const QVariantMap& before, const QVariantMap& after);

 private:
  struct ActiveTransaction {
    QString label;
    std::vector<std::unique_ptr<Command>> commands;
    std::size_t applied = 0;
    bool failed = false;
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

  bool execute(QString* error = nullptr) override;
  bool revert(QString* error = nullptr) override;
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

// 闭包命令（TASK-V02-061）：对象 CRUD 等既有逻辑以 apply/revert 闭包形式
// 接入事务层——execute 调 apply，revert 调 revert，审计摘要随命令携带。
class ClosureCommand : public Command {
 public:
  ClosureCommand(QString description, QVariantMap before, QVariantMap after,
                 std::function<void()> apply, std::function<void()> revert);

  bool execute(QString* error = nullptr) override;
  bool revert(QString* error = nullptr) override;
  QString description() const override { return description_; }
  QVariantMap beforeSummary() const override { return before_; }
  QVariantMap afterSummary() const override { return after_; }

 private:
  QString description_;
  QVariantMap before_;
  QVariantMap after_;
  std::function<void()> apply_;
  std::function<void()> revert_;
};

// HARD-040：只依赖 ProjectDocument/ObjectId/值的最小领域命令集。
class CreateObjectCommand : public Command {
 public:
  CreateObjectCommand(ProjectDocument& document,
                      std::unique_ptr<ProjectObject> object,
                      ObjectId parent = ObjectId(), int index = -1);

  bool execute(QString* error = nullptr) override;
  bool revert(QString* error = nullptr) override;
  QString description() const override;
  QVariantMap afterSummary() const override;
  ObjectId createdId() const { return id_; }

 private:
  ProjectDocument& document_;
  ObjectId id_;
  ObjectId parent_;
  ObjectKind kind_;
  QString name_;
  QString status_;
  QVariantMap params_;
  int index_ = -1;
  bool valid_ = false;
};

class DeleteObjectCommand : public Command {
 public:
  DeleteObjectCommand(ProjectDocument& document, ObjectId id);

  bool execute(QString* error = nullptr) override;
  bool revert(QString* error = nullptr) override;
  QString description() const override {
    return QStringLiteral("delete object");
  }
  QVariantMap beforeSummary() const override;

 private:
  ProjectDocument& document_;
  ObjectId id_;
  ObjectId parent_;
  QVariantList subtree_;
  int index_ = -1;
};

class RenameObjectCommand : public Command {
 public:
  RenameObjectCommand(ProjectDocument& document, ObjectId id, QString name);

  bool execute(QString* error = nullptr) override;
  bool revert(QString* error = nullptr) override;
  QString description() const override {
    return QStringLiteral("rename object");
  }
  QVariantMap beforeSummary() const override;
  QVariantMap afterSummary() const override;

 private:
  ProjectDocument& document_;
  ObjectId id_;
  QString before_;
  QString after_;
  bool captured_ = false;
};

class SetPropertiesCommand : public Command {
 public:
  SetPropertiesCommand(ProjectDocument& document, ObjectId id,
                       QVariantMap properties);

  bool execute(QString* error = nullptr) override;
  bool revert(QString* error = nullptr) override;
  QString description() const override {
    return QStringLiteral("set object properties");
  }
  QVariantMap beforeSummary() const override { return before_; }
  QVariantMap afterSummary() const override { return after_; }

 private:
  ProjectDocument& document_;
  ObjectId id_;
  QVariantMap before_;
  QVariantMap after_;
  bool captured_ = false;
};

class SetStatusCommand : public Command {
 public:
  SetStatusCommand(ProjectDocument& document, ObjectId id, QString status);
  SetStatusCommand(ProjectDocument& document, ObjectId id,
                   ObjectStatus status);

  bool execute(QString* error = nullptr) override;
  bool revert(QString* error = nullptr) override;
  QString description() const override {
    return QStringLiteral("set object status");
  }
  QVariantMap beforeSummary() const override;
  QVariantMap afterSummary() const override;

 private:
  ProjectDocument& document_;
  ObjectId id_;
  QString before_;
  QString after_;
  bool captured_ = false;
};

}  // namespace gmp::core
