#pragma once

#include <QMap>
#include <QPointer>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTabWidget;
class QTableWidget;
class QTreeWidget;
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
  // 跨节点引用校验（Sections.material、Assembly.part 等）需要稳定的模型树
  // 句柄；仅依赖 current_item_ 时，项目重开后未选中任何节点会误判引用悬空。
  void set_model_tree(QTreeWidget* tree);
  void set_boundary_groups(const QStringList& names);
  void set_volume_groups(const QStringList& names);
  // W-03a：显示→求解单位换算因子（decision 7），键为量纲（pressure 等），
  // 满足 solver = display * factor；来自活动档案 unit_contract 的
  // display_to_solver_factors，缺省时 CDP 应力字段按 1e6（MPa→Pa）换算。
  void set_display_unit_factors(const QMap<QString, double>& factors);
  // W-03b：Physics action 下拉候选（QuasiStatic 恒定；CDPQuasiStatic 仅当
  // 活动档案 extra.physics_action 声明时由 MainWindow 注入）。
  void set_physics_action_options(const QStringList& options);
  // G1：候选由活动 Application Profile + mapping registry 注入，避免
  // 表单展示当前求解应用不能生成的载荷/接触类型。
  void set_load_type_options(const QStringList& options);
  void set_interaction_type_options(const QStringList& options);
  void refresh_form_options();
  bool validate_current(QStringList* issues = nullptr);
  // 参数级校验（纯查询）：返回缺失/异常项列表，供表单与巡览合同复用。
  QStringList validate_params(const QString& kind,
                              const QVariantMap& params) const;
  const QStringList& boundary_groups() const { return boundary_groups_; }
  const QStringList& volume_groups() const { return volume_groups_; }

  static constexpr int kKindRole = Qt::UserRole + 1;
  static constexpr int kParamsRole = Qt::UserRole + 2;
  static constexpr int kStatusRole = Qt::UserRole + 3;
  static constexpr int kObjectIdRole = Qt::UserRole + 4;

 signals:
  // TASK-V02-014：表单对 current_item_ 的每次写入（名称/参数）经此信号
  // 通知 ModelTreeAdapter 标脏投影；读路径仍走 Tree Data Role。
  void item_written(QTreeWidgetItem* item);

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
  // 缺陷 2026-09-19-026：已知单位键（unit_key_info 登记的键）的高级表
  // 值单元格提示存储单位与换算后显示值；无单位机制时返回空。
  QString unit_tooltip_for_param(const QString& key,
                                 const QString& stored_text) const;
  void clear_form();
  void update_group_summary();
  void update_advanced_visibility();
  void refresh_validation_summary();
  void select_validation_row(int row);
  QVariantMap build_type_template(const QString& kind,
                                  const QString& type) const;
  void apply_template_values(const QVariantMap& values, bool overwrite);
  QStringList collect_model_names(const QString& root_name) const;
  QStringList pressure_variable_candidates() const;
  // W-03a：CDP 应力类字段显示↔存储换算用的比例因子。
  double display_unit_factor(const QString& quantity, double fallback) const;

  QTreeWidgetItem* current_item_ = nullptr;
  QPointer<QTreeWidget> model_tree_;
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
  QMap<QString, double> display_unit_factors_;
  QStringList physics_action_options_ = {"QuasiStatic"};
  QStringList load_type_options_ = {"BodyForce", "TimeDerivative",
                                    "MatDiffusion", "HeatConduction",
                                    "TensorMechanics"};
  QStringList interaction_type_options_ = {"Contact"};
};

}  // namespace gmp
