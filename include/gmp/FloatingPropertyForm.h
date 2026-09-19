#pragma once

#include <QDialog>
#include <QMap>
#include <QStringList>

class QDialogButtonBox;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;

namespace gmp {

class PropertyEditor;

// I-01: transaction-style property form. PropertyEditor operates on a cloned
// model tree, so field changes cannot leak into the project before acceptance.
class FloatingPropertyForm : public QDialog {
  Q_OBJECT

 public:
  FloatingPropertyForm(QTreeWidgetItem* target,
                       const QStringList& boundary_groups,
                       const QStringList& volume_groups,
                       const QStringList& physics_action_options,
                       const QStringList& load_type_options,
                       const QStringList& interaction_type_options,
                       QWidget* parent = nullptr);

  QTreeWidgetItem* target_item() const { return target_item_; }
  void place_over_stage(QWidget* stage);
  // W-03a：显示→求解单位换算因子转发给内部 PropertyEditor（决策 7）。
  void set_display_unit_factors(const QMap<QString, double>& factors);
  // TASK-V02-061：提交审计回调（旁路记录，不改变缓冲提交语义）。
  // 参数：label、before（名称+参数）、after（名称+参数）。
  void set_commit_audit_callback(
      std::function<void(const QString& label, const QVariantMap& before,
                         const QVariantMap& after)> callback);

 signals:
  void committed(QTreeWidgetItem* item);

 protected:
  void done(int result) override;

 private:
  void commit_if_valid();
  QString settings_key() const;
  void fit_to_current_tab();
  QSize preferred_size_for_current_tab() const;

  QTreeWidgetItem* target_item_ = nullptr;
  std::function<void(const QString&, const QVariantMap&, const QVariantMap&)>
      commit_audit_callback_;
  QTreeWidget* buffer_tree_ = nullptr;
  QTreeWidgetItem* buffer_item_ = nullptr;
  PropertyEditor* editor_ = nullptr;
  QDialogButtonBox* buttons_ = nullptr;
  QWidget* stage_ = nullptr;
};

}  // namespace gmp
