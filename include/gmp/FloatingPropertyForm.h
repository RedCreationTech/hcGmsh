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
                       QWidget* parent = nullptr);

  QTreeWidgetItem* target_item() const { return target_item_; }
  void place_over_stage(QWidget* stage);
  // W-03a：显示→求解单位换算因子转发给内部 PropertyEditor（决策 7）。
  void set_display_unit_factors(const QMap<QString, double>& factors);

 signals:
  void committed(QTreeWidgetItem* item);

 protected:
  void done(int result) override;

 private:
  void commit_if_valid();
  QString settings_key() const;

  QTreeWidgetItem* target_item_ = nullptr;
  QTreeWidget* buffer_tree_ = nullptr;
  QTreeWidgetItem* buffer_item_ = nullptr;
  PropertyEditor* editor_ = nullptr;
  QDialogButtonBox* buttons_ = nullptr;
};

}  // namespace gmp
