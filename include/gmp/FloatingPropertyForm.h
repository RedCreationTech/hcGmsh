#pragma once

#include <QDialog>
#include <QMap>
#include <QStringList>

class QDialogButtonBox;
class QEvent;
class QScrollArea;
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
  // HARD-050：校验后的缓冲值由宿主提交为领域命令。
  void set_commit_callback(
      std::function<bool(QTreeWidgetItem*, const QString&,
                         const QVariantMap&)> callback);

 signals:
  void committed(QTreeWidgetItem* item);

 protected:
  void done(int result) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void commit_if_valid();
  QString settings_key() const;
  void fit_to_current_tab();
  QSize preferred_size_for_current_tab();
  // 按内容展开的真实编辑器高度（补足 QTabWidget/QScrollArea sizeHint
  // 低估当前页内容高度的差额）。
  QSize content_aware_editor_hint();
  // 滚动宿主的最小尺寸 = 编辑器按内容计算的 sizeHint：窗口被上限压住或
  // 用户手动改小时编辑器不收缩，内部各滚动区永远拿到完整内容高度，
  // 只有外层这一条垂直滚动条会滚动。
  void sync_scroll_host_minimum();

  QTreeWidgetItem* target_item_ = nullptr;
  std::function<bool(QTreeWidgetItem*, const QString&, const QVariantMap&)>
      commit_callback_;
  QTreeWidget* buffer_tree_ = nullptr;
  QTreeWidgetItem* buffer_item_ = nullptr;
  PropertyEditor* editor_ = nullptr;
  // 弹窗唯一的外层滚动承载：内容超高时只有这一条垂直滚动条，
  // 内部各 TAB/区块全部按自身内容展开（wrap_content）。
  QScrollArea* outer_scroll_ = nullptr;
  QWidget* scroll_host_ = nullptr;
  QDialogButtonBox* buttons_ = nullptr;
  QWidget* stage_ = nullptr;
};

}  // namespace gmp
