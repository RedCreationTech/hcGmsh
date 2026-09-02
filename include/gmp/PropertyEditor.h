#pragma once

#include <QMap>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTabWidget;
class QTableWidget;
class QTreeWidgetItem;
class QListWidget;
class QGroupBox;
class QFormLayout;
class QCheckBox;
class QWidget;
class QHBoxLayout;
class QComboBox;
class QPlainTextEdit;

namespace gmp {

class PropertyEditor : public QWidget {
  Q_OBJECT
 public:
  explicit PropertyEditor(QWidget* parent = nullptr);

  void set_item(QTreeWidgetItem* item);
  void set_boundary_groups(const QStringList& names);
  void set_volume_groups(const QStringList& names);
  void refresh_form_options();
  bool validate_current(QStringList* issues = nullptr);
  const QStringList& boundary_groups() const { return boundary_groups_; }
  const QStringList& volume_groups() const { return volume_groups_; }

  static constexpr int kKindRole = Qt::UserRole + 1;
  static constexpr int kParamsRole = Qt::UserRole + 2;
  static constexpr int kStatusRole = Qt::UserRole + 3;

 private slots:
  void on_name_changed(const QString& value);
  void on_add_param();
  void on_remove_param();
  void on_param_changed(int row, int column);
  void on_apply_groups();
  void on_validate_model();
  void on_apply_template();
  void refresh_preview();

 private:
  void load_from_item();
  void save_params_to_item();
  bool is_root_item() const;
  // 常规页摘要: 按节点类型给出关键信息 (如 Features 的来源草图/参数)
  QString build_node_summary(const QString& kind,
                             const QVariantMap& params) const;
  void update_group_widget_for_kind(const QString& kind);
  void update_validation();
  void build_form_for_kind(const QString& kind);
  void set_param_value(const QString& key, const QString& value);
  void clear_form();
  void update_group_summary();
  void update_advanced_visibility();
  QStringList validate_params(const QString& kind,
                              const QVariantMap& params) const;
  void refresh_validation_summary();
  void select_validation_row(int row);
  QVariantMap build_type_template(const QString& kind,
                                  const QString& type) const;
  void apply_template_values(const QVariantMap& values, bool overwrite);
  QStringList collect_model_names(const QString& root_name) const;

  QTreeWidgetItem* current_item_ = nullptr;
  QLabel* header_label_ = nullptr;
  QLabel* kind_label_ = nullptr;
  QLabel* status_label_ = nullptr;
  QLabel* summary_label_ = nullptr;
  QLabel* validation_label_ = nullptr;
  QPushButton* validate_model_btn_ = nullptr;
  QLineEdit* name_edit_ = nullptr;
  QTabWidget* tabs_ = nullptr;
  QWidget* general_tab_ = nullptr;
  QWidget* params_tab_ = nullptr;
  QWidget* preview_tab_ = nullptr;
  QTableWidget* params_table_ = nullptr;
  QPushButton* add_param_btn_ = nullptr;
  QPushButton* remove_param_btn_ = nullptr;
  QGroupBox* groups_box_ = nullptr;
  QLabel* groups_hint_ = nullptr;
  QListWidget* groups_list_ = nullptr;
  QLabel* groups_summary_ = nullptr;
  QWidget* groups_chips_container_ = nullptr;
  QHBoxLayout* groups_chips_layout_ = nullptr;
  QPushButton* apply_groups_btn_ = nullptr;
  QCheckBox* advanced_toggle_ = nullptr;
  QComboBox* sync_mode_ = nullptr;
  QComboBox* template_combo_ = nullptr;
  QPushButton* apply_template_btn_ = nullptr;
  QWidget* params_container_ = nullptr;
  QWidget* params_buttons_container_ = nullptr;
  QStringList current_variables_;
  QStringList current_functions_;
  QStringList current_materials_;
  QMap<QString, QVariantMap> template_presets_;
  QGroupBox* validation_box_ = nullptr;
  QLabel* validation_summary_label_ = nullptr;
  QTableWidget* validation_table_ = nullptr;
  QPushButton* validation_refresh_btn_ = nullptr;
  QPushButton* validation_goto_btn_ = nullptr;
  QCheckBox* validation_filter_current_ = nullptr;
  QCheckBox* validation_only_with_issues_ = nullptr;
  QLabel* preview_summary_label_ = nullptr;
  QPlainTextEdit* preview_text_ = nullptr;

  QTabWidget* template_tabs_ = nullptr;
  QPlainTextEdit* template_preview_ = nullptr;
  QMap<QString, QString> template_descriptions_;
  QGroupBox* form_box_ = nullptr;
  QFormLayout* form_layout_ = nullptr;
  QMap<QString, QWidget*> form_widgets_;
  bool form_updating_ = false;
  QStringList boundary_groups_;
  QStringList volume_groups_;
};

}  // namespace gmp
