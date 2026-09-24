#include "gmp/PropertyEditor.h"

#include "gmp/IconFactory.h"
#include "gmp/L10n.h"
#include "gmp/SketchDocument.h"
#include "gmp/UnitDisplay.h"

#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QListWidget>
#include <QRegularExpression>
#include <QSet>
#include <QScrollArea>
#include <QDialog>
#include <QFileDialog>
#include <QPlainTextEdit>
#include <QFont>
#include <QSignalBlocker>
#include <QTimer>

#include "gmp/ComboPopupFix.h"

namespace gmp {

namespace {

// W-03d：场输出变量多选候选（v01 验收基线的 8 个 CDP 诊断量）。
// AuxKernels 的 property 命名约定（DamageC/DamageT 同名、其余 cdp_<名>）
// 由 MainWindow 生成侧实现，两处保持一致。
const QStringList kCdpFieldOutputVariables = {
    "DamageC",       "DamageT",           "kappa_c",
    "kappa_t",       "local_iterations",  "accepted_substeps",
    "jacobian_fallbacks", "integration_microseconds"};

// W-03b：Physics generate_output 默认值（v01 验收基线 16 项；
// 另有 max/mid/min_principal_strain 3 项候选可手补）。
// 与 MainWindow.cpp default_params_for_kind("Physics") 的默认值保持一致。
const char* kPhysicsGenerateOutputDefault =
    "stress_xx stress_xy stress_xz stress_yy stress_yz stress_zz "
    "strain_xx strain_xy strain_xz strain_yy strain_yz strain_zz "
    "max_principal_stress mid_principal_stress min_principal_stress "
    "vonmises_stress";

}  // namespace

PropertyEditor::PropertyEditor(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  header_label_ = new QLabel("No Selection", this);
  header_label_->setStyleSheet("font-weight: 600; padding: 2px 0;");
  layout->addWidget(header_label_);

  tabs_ = new QTabWidget(this);
  tabs_->setObjectName("propertyEditorTabs");
  layout->addWidget(tabs_, 1);

  general_tab_ = new QWidget(this);
  auto* general_layout = new QFormLayout(general_tab_);
  // macOS 的 QMacStyle 默认把 QFormLayout 强制 WrapAllRows (标签在字段上方),
  // 看起来像"空行", 显式改回标签与值同行
  general_layout->setRowWrapPolicy(QFormLayout::DontWrapRows);
  general_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  kind_label_ = new QLabel("-", general_tab_);
  status_label_ = new QLabel("-", general_tab_);
  name_edit_ = new QLineEdit(general_tab_);
  name_edit_->setObjectName("propertyNameEdit");
  summary_label_ = new QLabel("-", general_tab_);
  summary_label_->setObjectName("propertySummaryLabel");
  summary_label_->setWordWrap(true);
  general_layout->addRow("Kind", kind_label_);
  general_layout->addRow("Status", status_label_);
  general_layout->addRow("Name", name_edit_);
  general_layout->addRow("Summary", summary_label_);
  tabs_->addTab(general_tab_, "General");

  params_tab_ = new QWidget(this);
  // 参数页内容包一层滚动区（widgetResizable + NoFrame）：高度不设上限时
  // 它按内容自然展开、不会出现自己的滚动条。浮动弹窗由外层
  // FloatingPropertyForm 的整体滚动区承载超高内容；模块窗里本页仍以
  // 这一层为唯一滚动层。整个链条只保留最外层一条垂直滚动条。
  auto* params_page_layout = new QVBoxLayout(params_tab_);
  params_page_layout->setContentsMargins(0, 0, 0, 0);
  auto* params_scroll = new QScrollArea(params_tab_);
  params_scroll->setObjectName("paramsTabScroll");
  params_scroll->setWidgetResizable(true);
  params_scroll->setFrameShape(QFrame::NoFrame);
  auto* params_content = new QWidget(params_scroll);
  auto* params_layout = new QVBoxLayout(params_content);

  form_box_ = new QGroupBox("Quick Parameters", params_tab_);
  form_box_->setObjectName("propertyQuickParametersBox");
  form_layout_ = new QFormLayout(form_box_);
  form_layout_->setRowWrapPolicy(QFormLayout::DontWrapRows);
  form_layout_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  // 高级参数表格展开时，快捷表单不得被挤压到 sizeHint 以下（否则行重叠/
  // 截断）；空间缺口由带滚动条的高级表格区吸收。
  form_box_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

  groups_box_ = new QGroupBox("Groups", params_tab_);
  groups_box_->setObjectName("propertyGroupsBox");
  auto* groups_layout = new QVBoxLayout(groups_box_);
  groups_hint_ = new QLabel("Select physical groups to apply.", groups_box_);
  groups_hint_->setStyleSheet("color: #444;");
  groups_list_ = new QListWidget(groups_box_);
  groups_list_->setObjectName("propertyGroupsList");
  groups_list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  groups_list_->setMaximumHeight(120);
  groups_summary_ = new QLabel("Selection to apply:", groups_box_);
  groups_summary_->setStyleSheet("color: #333;");
  groups_chips_container_ = new QWidget(groups_box_);
  groups_chips_layout_ = new QHBoxLayout(groups_chips_container_);
  groups_chips_layout_->setContentsMargins(0, 0, 0, 0);
  groups_chips_layout_->setSpacing(6);
  apply_groups_btn_ = new QPushButton("Apply Groups", groups_box_);
  apply_groups_btn_->setIcon(gmp::icons::get("apply_groups"));
  apply_groups_btn_->setObjectName("applyGroupsBtn");
  groups_layout->addWidget(groups_hint_);
  groups_layout->addWidget(groups_list_, 1);
  groups_layout->addWidget(groups_summary_);
  groups_layout->addWidget(groups_chips_container_);
  groups_layout->addWidget(apply_groups_btn_);
  groups_box_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  // 分组指派决定对象实际作用域，优先于普通参数展示，避免用户只填完
  // 快捷参数便直接确认而遗漏“应用所选分组”。不适用分组的节点会隐藏该区。
  params_layout->addWidget(groups_box_);
  params_layout->addWidget(form_box_);

  advanced_toggle_ = new QCheckBox("Advanced Parameters", params_tab_);
  advanced_toggle_->setChecked(false);
  params_layout->addWidget(advanced_toggle_);

  auto* sync_row = new QHBoxLayout();
  sync_row->addWidget(new QLabel("Sync", params_tab_));
  sync_mode_ = new QComboBox(params_tab_);
  install_combo_popup_fix(sync_mode_);
  sync_mode_->addItem("Bidirectional (Recommended)");
  sync_mode_->addItem("Quick Form Wins");
  sync_mode_->setToolTip("Controls how Advanced Parameters sync with Quick form");
  sync_row->addWidget(sync_mode_);
  sync_row->addStretch(1);
  params_layout->addLayout(sync_row);

  params_container_ = new QWidget(params_tab_);
  auto* params_container_layout = new QVBoxLayout(params_container_);
  params_container_layout->setContentsMargins(0, 0, 0, 0);
  params_container_layout->setSpacing(4);
  params_table_ = new QTableWidget(params_container_);
  params_table_->setObjectName("propertyParamsTable");
  params_table_->setColumnCount(2);
  params_table_->setHorizontalHeaderLabels({"Key", "Value"});
  params_table_->horizontalHeader()->setStretchLastSection(true);
  params_table_->verticalHeader()->setVisible(false);
  params_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  params_table_->setSelectionMode(QAbstractItemView::SingleSelection);
  params_container_layout->addWidget(params_table_, 1);

  auto* buttons = new QHBoxLayout();
  params_buttons_container_ = new QWidget(params_container_);
  params_buttons_container_->setLayout(buttons);
  add_param_btn_ = new QPushButton("Add Param", params_container_);
  add_param_btn_->setIcon(gmp::icons::get("add_param"));
  add_param_btn_->setObjectName("gmpIcon_add_param");
  remove_param_btn_ = new QPushButton("Remove Param", params_container_);
  remove_param_btn_->setIcon(gmp::icons::get("remove_param"));
  remove_param_btn_->setObjectName("gmpIcon_remove_param");
  buttons->addWidget(add_param_btn_);
  buttons->addWidget(remove_param_btn_);
  buttons->addStretch(1);
  params_container_layout->addWidget(params_buttons_container_);
  params_table_->setMinimumHeight(120);
  // 高级表格区吃拉伸（也吸收空间不足），快捷表单/组区保持完整高度。
  params_layout->addWidget(params_container_, 1);

  validation_label_ = new QLabel(params_tab_);
  validation_label_->setObjectName("propertyValidationInline");
  validation_label_->setStyleSheet("color: #b00020;");
  validation_label_->setWordWrap(true);
  params_layout->addWidget(validation_label_);
  params_scroll->setWidget(params_content);
  params_page_layout->addWidget(params_scroll);

  auto* validation_tab = new QWidget(this);
  auto* validation_page_layout = new QVBoxLayout(validation_tab);
  validation_page_layout->setContentsMargins(8, 8, 8, 8);
  validation_box_ = new QGroupBox("Validation Summary", validation_tab);
  auto* validation_layout = new QVBoxLayout(validation_box_);
  validation_summary_label_ = new QLabel("No issues.", validation_box_);
  validation_summary_label_->setObjectName("propertyValidationSummary");
  validation_summary_label_->setStyleSheet("font-weight: 600;");
  validation_layout->addWidget(validation_summary_label_);
  validation_table_ = new QTableWidget(validation_box_);
  validation_table_->setObjectName("propertyValidationTable");
  validation_table_->setColumnCount(2);
  validation_table_->setHorizontalHeaderLabels({"Node", "Issues"});
  validation_table_->horizontalHeader()->setStretchLastSection(true);
  validation_table_->verticalHeader()->setVisible(false);
  validation_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  validation_table_->setSelectionMode(QAbstractItemView::SingleSelection);
  validation_layout->addWidget(validation_table_);
  auto* validation_filters = new QHBoxLayout();
  validation_filter_current_ = new QCheckBox("Current Type Only", validation_box_);
  validation_only_with_issues_ = new QCheckBox("Only With Issues", validation_box_);
  validation_only_with_issues_->setChecked(true);
  validation_filters->addWidget(validation_filter_current_);
  validation_filters->addWidget(validation_only_with_issues_);
  validation_filters->addStretch(1);
  validation_layout->addLayout(validation_filters);
  auto* validation_actions = new QHBoxLayout();
  validation_refresh_btn_ = new QPushButton("Refresh", validation_box_);
  validation_refresh_btn_->setIcon(gmp::icons::get("refresh"));
  validation_refresh_btn_->setObjectName("gmpIcon_refresh");
  validation_goto_btn_ = new QPushButton("Go To Node", validation_box_);
  validation_goto_btn_->setIcon(gmp::icons::get("go_to_node"));
  validation_goto_btn_->setObjectName("gmpIcon_go_to_node");
  validation_actions->addWidget(validation_refresh_btn_);
  validation_actions->addWidget(validation_goto_btn_);
  validation_actions->addStretch(1);
  validation_layout->addLayout(validation_actions);
  tabs_->addTab(params_tab_, "Parameters");
  // 校验汇总框按内容高度自适应：框不再拉伸占满整页，底部的筛选复选框
  // 与按钮紧随内容；只有表格自身超高（上限见 update_validation_table_height）
  // 时才在框内滚动。
  validation_page_layout->addWidget(validation_box_);
  validation_page_layout->addStretch(1);
  tabs_->addTab(validation_tab, "Validation");

  preview_tab_ = new QWidget(this);
  auto* preview_layout = new QVBoxLayout(preview_tab_);
  preview_summary_label_ = new QLabel("Selected item preview:", preview_tab_);
  preview_summary_label_->setStyleSheet("font-weight: 600; color: #333;");
  preview_layout->addWidget(preview_summary_label_);
  preview_text_ = new QPlainTextEdit(preview_tab_);
  preview_text_->setReadOnly(true);
  preview_text_->setLineWrapMode(QPlainTextEdit::NoWrap);
  preview_text_->setPlaceholderText("Select a node in the model tree.");
  QFont mono_font = QFont();
  mono_font.setFamilies({"SFMono-Regular", "Monaco", "Consolas", "Menlo"});
  mono_font.setStyleHint(QFont::Monospace);
  preview_text_->setFont(mono_font);
  preview_layout->addWidget(preview_text_);
  preview_layout->addStretch(1);
  tabs_->addTab(preview_tab_, "Preview");

  connect(name_edit_, &QLineEdit::textChanged, this,
          &PropertyEditor::on_name_changed);
  connect(add_param_btn_, &QPushButton::clicked, this,
          &PropertyEditor::on_add_param);
  connect(remove_param_btn_, &QPushButton::clicked, this,
          &PropertyEditor::on_remove_param);
  connect(params_table_, &QTableWidget::cellChanged, this,
          &PropertyEditor::on_param_changed);
  connect(apply_groups_btn_, &QPushButton::clicked, this,
          &PropertyEditor::on_apply_groups);
  connect(groups_list_, &QListWidget::itemSelectionChanged, this,
          &PropertyEditor::update_group_summary);
  if (advanced_toggle_) {
    connect(advanced_toggle_, &QCheckBox::toggled, this,
            &PropertyEditor::update_advanced_visibility);
  }
  if (sync_mode_) {
    connect(sync_mode_, &QComboBox::currentIndexChanged, this,
            &PropertyEditor::update_validation);
  }
  if (validation_refresh_btn_) {
    connect(validation_refresh_btn_, &QPushButton::clicked, this,
            &PropertyEditor::on_validate_model);
  }
  if (validation_goto_btn_) {
    connect(validation_goto_btn_, &QPushButton::clicked, this,
            [this]() { select_validation_row(
                validation_table_ ? validation_table_->currentRow() : -1); });
  }
  if (validation_table_) {
    connect(validation_table_, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int) { select_validation_row(row); });
  }
  if (validation_filter_current_) {
    connect(validation_filter_current_, &QCheckBox::toggled, this,
            &PropertyEditor::refresh_validation_summary);
  }
  if (validation_only_with_issues_) {
    connect(validation_only_with_issues_, &QCheckBox::toggled, this,
            &PropertyEditor::refresh_validation_summary);
  }

  // I-01 起属性编辑器承载于宽体弹窗，表单统一使用“标签 + 字段”同行布局；
  // 不再沿用旧窄右栏的 WrapAllRows，避免字段被拉成长纵向页面。
  for (auto* f : findChildren<QFormLayout*>()) {
    f->setRowWrapPolicy(QFormLayout::DontWrapRows);
    f->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  }

  set_item(nullptr);
}

void PropertyEditor::set_item(QTreeWidgetItem* item) {
  current_item_ = item;
  load_from_item();
}

void PropertyEditor::set_model_tree(QTreeWidget* tree) {
  model_tree_ = tree;
}

void PropertyEditor::set_boundary_groups(const QStringList& names) {
  boundary_groups_ = names;
  const QString kind =
      current_item_ ? current_item_->data(0, kKindRole).toString() : QString();
  update_group_widget_for_kind(kind);
}

void PropertyEditor::set_volume_groups(const QStringList& names) {
  volume_groups_ = names;
  const QString kind =
      current_item_ ? current_item_->data(0, kKindRole).toString() : QString();
  update_group_widget_for_kind(kind);
  update_validation();
}

void PropertyEditor::set_display_unit_factors(
    const QMap<QString, double>& factors) {
  display_unit_factors_ = factors;
}

void PropertyEditor::set_physics_action_options(const QStringList& options) {
  physics_action_options_ = options.isEmpty() ? QStringList{"QuasiStatic"}
                                              : options;
  if (current_item_) {
    const QString kind =
        current_item_->data(0, kKindRole).toString().isEmpty()
            ? current_item_->text(0)
            : current_item_->data(0, kKindRole).toString();
    if (kind == "Physics") {
      refresh_form_options();
    }
  }
}

void PropertyEditor::set_load_type_options(const QStringList& options) {
  load_type_options_ = options.isEmpty() ? QStringList{"BodyForce"} : options;
  if (current_item_ &&
      current_item_->data(0, kKindRole).toString() == "Loads") {
    refresh_form_options();
  }
}

void PropertyEditor::set_interaction_type_options(
    const QStringList& options) {
  interaction_type_options_ = options.isEmpty() ? QStringList{"Unsupported"}
                                                : options;
  if (current_item_ &&
      current_item_->data(0, kKindRole).toString() == "Interactions") {
    refresh_form_options();
  }
}

double PropertyEditor::display_unit_factor(const QString& quantity,
                                           double fallback) const {
  const double factor = display_unit_factors_.value(quantity, 0.0);
  return factor > 0.0 ? factor : fallback;
}

QString PropertyEditor::unit_tooltip_for_param(const QString& key,
                                               const QString& stored_text) const {
  gmp::UnitKeyInfo info;
  if (!gmp::unit_key_info(key, &info)) {
    return QString();
  }
  const bool chinese =
      gmp::l10n::current_language() == gmp::l10n::Language::Chinese;
  // 换算比例与快捷字段显示路径一致：优先 params 记录的 unit_factor_stress，
  // 缺省回落到当前档案单位合同/1e6。
  double factor = 0.0;
  if (current_item_) {
    bool factor_ok = false;
    const double stored_factor =
        current_item_->data(0, kParamsRole)
            .toMap()
            .value("unit_factor_stress")
            .toString()
            .toDouble(&factor_ok);
    if (factor_ok && stored_factor > 0.0) {
      factor = stored_factor;
    }
  }
  if (factor <= 0.0) {
    factor = display_unit_factor(info.quantity, 1e6);
  }
  const QString stored_unit_note =
      chinese
          ? QString::fromUtf8("存储单位：%1（SI 求解值）").arg(info.stored_unit)
          : QString("Stored in %1 (SI solver value)").arg(info.stored_unit);
  bool stored_ok = false;
  const double stored = stored_text.trimmed().toDouble(&stored_ok);
  if (!stored_ok) {
    return stored_unit_note;
  }
  const QString display_value = gmp::format_unit_display_value(stored, factor);
  return chinese
             ? QString::fromUtf8("%1；快捷字段按 %2 显示：%3")
                   .arg(stored_unit_note, info.display_unit, display_value)
             : QString("%1; quick field displays %2: %3")
                   .arg(stored_unit_note, info.display_unit, display_value);
}

void PropertyEditor::refresh_form_options() {
  if (!current_item_) {
    return;
  }
  const QString kind =
      current_item_->data(0, kKindRole).toString().isEmpty()
          ? current_item_->text(0)
          : current_item_->data(0, kKindRole).toString();
  build_form_for_kind(kind);
  update_group_widget_for_kind(kind);
  update_validation();
  refresh_preview();
}

bool PropertyEditor::validate_current(QStringList* issues) {
  QStringList current_issues;
  if (!current_item_ || is_root_item()) {
    current_issues << "name";
  } else {
    if (current_item_->text(0).trimmed().isEmpty()) {
      current_issues << "name";
    }
    if (auto* parent = current_item_->parent()) {
      for (int i = 0; i < parent->childCount(); ++i) {
        auto* sibling = parent->child(i);
        if (sibling != current_item_ &&
            sibling->text(0).trimmed() == current_item_->text(0).trimmed()) {
          current_issues << "name must be unique";
          break;
        }
      }
    }
    const QString kind =
        current_item_->data(0, kKindRole).toString().isEmpty()
            ? current_item_->text(0)
            : current_item_->data(0, kKindRole).toString();
    current_issues.append(
        validate_params(kind, current_item_->data(0, kParamsRole).toMap()));
  }

  if (issues) {
    *issues = current_issues;
  }
  if (current_issues.isEmpty()) {
    update_validation();
    return true;
  }

  if (validation_label_) {
    const QString prompt =
        l10n::current_language() == l10n::Language::Chinese
            ? QString::fromUtf8("请修正：")
            : QString("Please correct: ");
    validation_label_->setText(prompt + current_issues.join(", "));
  }
  const QString first = current_issues.first();
  if (first.startsWith("name")) {
    if (tabs_) {
      tabs_->setCurrentWidget(general_tab_);
    }
    if (name_edit_) {
      name_edit_->setFocus(Qt::OtherFocusReason);
      name_edit_->selectAll();
      QTimer::singleShot(0, name_edit_, [edit = name_edit_]() {
        edit->setFocus(Qt::OtherFocusReason);
        edit->selectAll();
      });
      QTimer::singleShot(50, name_edit_, [edit = name_edit_]() {
        edit->setFocus(Qt::OtherFocusReason);
        edit->selectAll();
      });
    }
  } else {
    if (tabs_) {
      tabs_->setCurrentWidget(params_tab_);
    }
    QString key = first.section(' ', 0, 0).section('/', 0, 0);
    if (auto* field = form_widgets_.value(key, nullptr)) {
      field->setFocus(Qt::OtherFocusReason);
      QTimer::singleShot(0, field,
                         [field]() { field->setFocus(Qt::OtherFocusReason); });
      QTimer::singleShot(50, field,
                         [field]() { field->setFocus(Qt::OtherFocusReason); });
    } else if (params_table_) {
      for (int row = 0; row < params_table_->rowCount(); ++row) {
        auto* key_item = params_table_->item(row, 0);
        if (key_item && key_item->text() == key) {
          params_table_->setCurrentCell(row, 1);
          params_table_->setFocus(Qt::OtherFocusReason);
          break;
        }
      }
    }
  }
  return false;
}

bool PropertyEditor::is_root_item() const {
  return current_item_ && current_item_->parent() == nullptr;
}

void PropertyEditor::load_from_item() {
  params_table_->blockSignals(true);
  name_edit_->blockSignals(true);

  if (!current_item_) {
    header_label_->setText("No Selection");
    kind_label_->setText("-");
    status_label_->setText("-");
    summary_label_->setText("-");
    name_edit_->setText("");
    params_table_->setRowCount(0);
    clear_form();
    if (groups_box_) {
      groups_box_->setVisible(false);
    }
    if (validation_label_) {
      validation_label_->clear();
    }
    if (validation_box_) {
      validation_box_->setEnabled(false);
    }
    if (advanced_toggle_) {
      advanced_toggle_->setEnabled(false);
    }
    update_advanced_visibility();
    name_edit_->setEnabled(false);
    params_table_->setEnabled(false);
    add_param_btn_->setEnabled(false);
    remove_param_btn_->setEnabled(false);
    if (tabs_) {
      tabs_->setEnabled(false);
    }
    refresh_preview();
    name_edit_->blockSignals(false);
    params_table_->blockSignals(false);
    return;
  }

  const QString kind =
      current_item_->data(0, kKindRole).toString().isEmpty()
          ? current_item_->text(0)
          : current_item_->data(0, kKindRole).toString();
  header_label_->setText(QString("%1 — %2").arg(kind, current_item_->text(0)));
  kind_label_->setText(kind);
  name_edit_->setText(current_item_->text(0));

  params_table_->setRowCount(0);
  const QVariantMap params =
      current_item_->data(0, kParamsRole).toMap();
  const QString status = params.value("status").toString().isEmpty()
                             ? params.value("state").toString()
                             : params.value("status").toString();
  status_label_->setText(status.isEmpty() ? "-" : status);
  summary_label_->setText(build_node_summary(kind, params));
  int row = 0;
  for (auto it = params.begin(); it != params.end(); ++it) {
    params_table_->insertRow(row);
    params_table_->setItem(row, 0, new QTableWidgetItem(it.key()));
    auto* value_item = new QTableWidgetItem(it.value().toString());
    const QString unit_tooltip =
        unit_tooltip_for_param(it.key(), it.value().toString());
    if (!unit_tooltip.isEmpty()) {
      value_item->setToolTip(unit_tooltip);
    }
    params_table_->setItem(row, 1, value_item);
    ++row;
  }

  const bool editable = !is_root_item();
  if (tabs_) {
    tabs_->setEnabled(true);
  }
  name_edit_->setEnabled(editable);
  params_table_->setEnabled(editable);
  add_param_btn_->setEnabled(editable);
  remove_param_btn_->setEnabled(editable);
  if (validation_box_) {
    validation_box_->setEnabled(true);
  }
  if (advanced_toggle_) {
    advanced_toggle_->setEnabled(editable);
  }
  update_advanced_visibility();

  name_edit_->blockSignals(false);
  params_table_->blockSignals(false);
  build_form_for_kind(kind);
  update_group_widget_for_kind(kind);
  update_validation();
  refresh_preview();
}

void PropertyEditor::on_name_changed(const QString& value) {
  if (!current_item_ || is_root_item()) {
    return;
  }
  write_item(value, current_item_->data(0, kParamsRole).toMap());
  refresh_preview();
}

void PropertyEditor::on_add_param() {
  if (!current_item_ || is_root_item()) {
    return;
  }
  const int row = params_table_->rowCount();
  params_table_->insertRow(row);
  params_table_->setItem(row, 0, new QTableWidgetItem("key"));
  params_table_->setItem(row, 1, new QTableWidgetItem("value"));
  save_params_to_item();
  update_validation();
  refresh_preview();
}

void PropertyEditor::on_remove_param() {
  if (!current_item_ || is_root_item()) {
    return;
  }
  const auto ranges = params_table_->selectedRanges();
  if (ranges.isEmpty()) {
    return;
  }
  params_table_->removeRow(ranges.first().topRow());
  save_params_to_item();
  update_group_widget_for_kind(
      current_item_->data(0, kKindRole).toString());
  update_validation();
  refresh_preview();
}

void PropertyEditor::on_param_changed(int row, int column) {
  Q_UNUSED(row);
  Q_UNUSED(column);
  if (!current_item_ || is_root_item()) {
    return;
  }
  if (column == 0 && row >= 0 && params_table_) {
    auto* key_item = params_table_->item(row, 0);
    if (key_item) {
      const QString key = key_item->text().trimmed();
      for (int r = params_table_->rowCount() - 1; r >= 0; --r) {
        if (r == row) {
          continue;
        }
        auto* other_key = params_table_->item(r, 0);
        if (other_key && other_key->text().trimmed() == key && !key.isEmpty()) {
          params_table_->removeRow(r);
          if (r < row) {
            row--;
          }
        }
      }
    }
  }
  save_params_to_item();
  if (row >= 0 && params_table_->item(row, 0) &&
      params_table_->item(row, 0)->text().trimmed() == "block") {
    update_group_widget_for_kind(
        current_item_->data(0, kKindRole).toString());
  }
  refresh_preview();
  if (row >= 0 && (!sync_mode_ || sync_mode_->currentIndex() == 0)) {
    auto* key_item = params_table_->item(row, 0);
    auto* val_item = params_table_->item(row, 1);
    if (key_item && val_item) {
      const QString key = key_item->text().trimmed();
      if (form_widgets_.contains(key)) {
        form_updating_ = true;
        QString value = val_item->text();
        if (auto* edit = qobject_cast<QLineEdit*>(form_widgets_[key])) {
          if (edit->objectName() == "cdpYoungsModulusMpa") {
            // 高级表写回的是 SI 存储值（Pa），显示字段需要换回 MPa。
            bool stored_ok = false;
            const double stored = value.trimmed().toDouble(&stored_ok);
            if (stored_ok) {
              const QVariantMap params =
                  current_item_->data(0, kParamsRole).toMap();
              bool factor_ok = false;
              const double stored_factor = params.value("unit_factor_stress")
                                               .toString()
                                               .toDouble(&factor_ok);
              const double factor =
                  params.value("type").toString() == "AbaqusCDP" &&
                          factor_ok && stored_factor > 0.0
                      ? stored_factor
                      : display_unit_factor("pressure", 1e6);
              value = gmp::format_unit_display_value(stored, factor);
            }
          }
          edit->setText(value);
        } else if (auto* combo = qobject_cast<QComboBox*>(form_widgets_[key])) {
          const int idx = combo->findText(value);
          if (idx >= 0) {
            combo->setCurrentIndex(idx);
          } else if (!value.isEmpty()) {
            combo->addItem(value);
            combo->setCurrentText(value);
          }
        } else if (form_widgets_[key]) {
          if (auto* edit =
                  form_widgets_[key]->findChild<QLineEdit*>()) {
            edit->setText(value);
          }
        }
        form_updating_ = false;
      }
    }
  }
  update_validation();
}

void PropertyEditor::save_params_to_item() {
  if (!current_item_) {
    return;
  }
  QVariantMap params;
  for (int row = 0; row < params_table_->rowCount(); ++row) {
    auto* key_item = params_table_->item(row, 0);
    auto* val_item = params_table_->item(row, 1);
    if (!key_item) {
      continue;
    }
    const QString key = key_item->text().trimmed();
    if (key.isEmpty()) {
      continue;
    }
    const QString value = val_item ? val_item->text() : QString();
    params.insert(key, value);
  }
  write_item(current_item_->text(0), params);
  refresh_preview();
}

void PropertyEditor::set_write_callback(
    std::function<bool(QTreeWidgetItem*, const QString&,
                       const QVariantMap&)> callback) {
  write_callback_ = std::move(callback);
}

bool PropertyEditor::write_item(const QString& name,
                                const QVariantMap& params) {
  if (!current_item_) {
    return false;
  }
  if (write_callback_) {
    return write_callback_(current_item_, name, params);
  }
  current_item_->setText(0, name);
  current_item_->setData(0, kParamsRole, params);
  return true;
}

void PropertyEditor::on_apply_groups() {
  if (!current_item_ || is_root_item() || !groups_list_) {
    return;
  }
  const QString kind =
      current_item_->data(0, kKindRole).toString().isEmpty()
          ? current_item_->text(0)
          : current_item_->data(0, kKindRole).toString();
  if (kind != "BC" && kind != "Loads" && kind != "Materials" &&
      kind != "Sections" &&
      kind != "Physics" && kind != "Outputs") {
    return;
  }
  QStringList selected;
  const auto items = groups_list_->selectedItems();
  for (const auto* item : items) {
    if (item) {
      selected << item->text();
    }
  }
  if (selected.isEmpty() && kind != "Materials") {
    return;
  }
  QVariantMap params = current_item_->data(0, kParamsRole).toMap();
  if (kind == "BC") {
    params.insert("boundary", selected.join(" "));
  } else if (kind == "Loads" &&
             params.value("type").toString() == "Pressure") {
    params.insert("boundary", selected.join(" "));
  } else if (kind == "Outputs") {
    // W-03d：历史输出套餐的面组（反力/平均位移共用）。
    params.insert("hist_boundary", selected.join(" "));
  } else if (selected.isEmpty()) {
    // 材料可不限制 block；清空选择并应用会移除已有指派。
    params.remove("block");
  } else {
    // Materials/Loads/Sections/Physics：体组写入 block。
    params.insert("block", selected.join(" "));
  }
  write_item(current_item_->text(0), params);
  load_from_item();
  refresh_preview();
}

void PropertyEditor::update_group_widget_for_kind(const QString& kind) {
  if (!groups_box_ || !groups_list_) {
    return;
  }
  if (kind != "BC" && kind != "Loads" && kind != "Materials" &&
      kind != "Sections" &&
      kind != "Physics" && kind != "Outputs") {
    groups_box_->setVisible(false);
    return;
  }
  groups_box_->setVisible(true);
  groups_list_->clear();
  // W-03b：Physics block 用体组；W-03d：Outputs 历史输出面组用面组。
  const QVariantMap params =
      current_item_ ? current_item_->data(0, kParamsRole).toMap()
                    : QVariantMap();
  const bool use_boundary =
      (kind == "BC" || kind == "Outputs" ||
       (kind == "Loads" &&
        params.value("type").toString() == "Pressure"));
  QStringList source = use_boundary ? boundary_groups_ : volume_groups_;
  groups_list_->addItems(source);
  if (groups_hint_) {
    groups_hint_->setText(l10n::tr(
        use_boundary
            ? "Select one or more boundaries, then apply the selection."
            : (kind == "Sections" || kind == "Materials"
                   ? "Select one or more physical volumes below, then apply "
                     "the selection."
                   : "Select one or more volume groups, then apply the "
                     "selection.")));
  }
  if (groups_box_) {
    groups_box_->setTitle(l10n::tr(
        use_boundary ? "Available Boundary Groups"
                     : (kind == "Sections" || kind == "Materials"
                            ? "Available Physical Volumes"
                                            : "Available Volume Groups")));
  }
  if (apply_groups_btn_) {
    apply_groups_btn_->setText(l10n::tr(
        use_boundary ? "Apply Selected Boundaries"
                     : (kind == "Sections" || kind == "Materials"
                            ? "Apply Selected Volumes"
                                            : "Apply Selected Groups")));
  }
  const QString key =
      (kind == "BC" ||
       (kind == "Loads" && params.value("type").toString() == "Pressure"))
          ? "boundary"
          : (kind == "Outputs" ? "hist_boundary" : "block");
  const QString current = params.value(key).toString().trimmed();
  const QStringList selected =
      current.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
  for (int i = 0; i < groups_list_->count(); ++i) {
    auto* item = groups_list_->item(i);
    if (!item) {
      continue;
    }
    item->setSelected(selected.contains(item->text()));
  }
  if (apply_groups_btn_) {
    apply_groups_btn_->setEnabled(!source.isEmpty());
  }
  update_group_summary();
}

void PropertyEditor::update_group_summary() {
  if (!groups_summary_ || !groups_list_ || !groups_chips_layout_) {
    return;
  }
  QStringList selected;
  const auto items = groups_list_->selectedItems();
  for (const auto* item : items) {
    if (item) {
      selected << item->text();
    }
  }
  groups_summary_->setText(l10n::tr("Selection to apply:"));

  while (QLayoutItem* item = groups_chips_layout_->takeAt(0)) {
    if (auto* widget = item->widget()) {
      widget->deleteLater();
    }
    delete item;
  }

  if (selected.isEmpty()) {
    auto* none = new QLabel(l10n::tr("(none)"), groups_chips_container_);
    none->setStyleSheet("color: #666;");
    groups_chips_layout_->addWidget(none);
    groups_chips_layout_->addStretch(1);
    return;
  }

  for (const auto& name : selected) {
    auto* chip = new QLabel(name, groups_chips_container_);
    chip->setStyleSheet(
        "background: #d7e8ff; border: 1px solid #9bbcf2;"
        "border-radius: 8px; padding: 2px 8px;");
    groups_chips_layout_->addWidget(chip);
  }
  groups_chips_layout_->addStretch(1);
}

void PropertyEditor::update_advanced_visibility() {
  if (!params_container_ || !advanced_toggle_) {
    return;
  }
  const bool show = advanced_toggle_->isChecked() &&
                    advanced_toggle_->isEnabled();
  params_container_->setVisible(show);
}

void PropertyEditor::on_validate_model() {
  refresh_validation_summary();
}

void PropertyEditor::update_validation() {
  if (!validation_label_) {
    return;
  }
  if (!current_item_ || is_root_item()) {
    validation_label_->clear();
    return;
  }
  const QString kind =
      current_item_->data(0, kKindRole).toString().isEmpty()
          ? current_item_->text(0)
          : current_item_->data(0, kKindRole).toString();
  const QVariantMap params = current_item_->data(0, kParamsRole).toMap();
  QStringList missing = validate_params(kind, params);
  if (missing.isEmpty()) {
    validation_label_->clear();
  } else {
    validation_label_->setText(
        QString("Missing required fields: %1").arg(missing.join(", ")));
  }
  refresh_validation_summary();
}

void PropertyEditor::update_validation_table_height() {
  if (!validation_table_) {
    return;
  }
  // 表格高度 = 内容高度（表头 + 各行），按内容全部展开不设上限；
  // 弹窗场景由外层唯一滚动条承载，无问题时只剩摘要一行 + 筛选/按钮。
  int height = validation_table_->horizontalHeader()->height();
  for (int row = 0; row < validation_table_->rowCount(); ++row) {
    height += validation_table_->rowHeight(row);
  }
  height += validation_table_->frameWidth() * 2;
  validation_table_->setFixedHeight(height);
}

void PropertyEditor::refresh_validation_summary() {
  if (!validation_table_ || !validation_summary_label_ || !current_item_) {
    return;
  }
  auto* tree = current_item_->treeWidget();
  if (!tree) {
    return;
  }
  validation_table_->setRowCount(0);
  const QString current_kind =
      current_item_->data(0, kKindRole).toString().isEmpty()
          ? current_item_->text(0)
          : current_item_->data(0, kKindRole).toString();
  const bool filter_current =
      validation_filter_current_ && validation_filter_current_->isChecked();
  const bool only_issues =
      validation_only_with_issues_ ? validation_only_with_issues_->isChecked()
                                   : true;
  struct IssueRow {
    QString root;
    QString name;
    QString issues;
  };
  QVector<IssueRow> rows;
  for (int i = 0; i < tree->topLevelItemCount(); ++i) {
    auto* root = tree->topLevelItem(i);
    if (!root) {
      continue;
    }
    const QString kind = root->text(0);
    if (filter_current && kind != current_kind) {
      continue;
    }
    for (int j = 0; j < root->childCount(); ++j) {
      auto* child = root->child(j);
      if (!child) {
        continue;
      }
      const QVariantMap params =
          child->data(0, kParamsRole).toMap();
      const QStringList missing = validate_params(kind, params);
      if (!missing.isEmpty() || !only_issues) {
        rows.push_back({kind, child->text(0), missing.join(", ")});
      }
    }
  }
  validation_summary_label_->setText(
      rows.isEmpty()
          ? "No validation issues."
          : QString("%1 issue(s) found").arg(rows.size()));
  if (rows.isEmpty()) {
    validation_table_->setVisible(false);
    if (validation_goto_btn_) {
      validation_goto_btn_->setEnabled(false);
    }
    return;
  }
  validation_table_->setVisible(true);
  validation_table_->setRowCount(rows.size());
  for (int i = 0; i < rows.size(); ++i) {
    const auto& row = rows.at(i);
    auto* node_item = new QTableWidgetItem(
        QString("[%1] %2").arg(row.root, row.name));
    node_item->setData(Qt::UserRole, row.root);
    node_item->setData(Qt::UserRole + 1, row.name);
    validation_table_->setItem(i, 0, node_item);
    validation_table_->setItem(i, 1, new QTableWidgetItem(row.issues));
  }
  update_validation_table_height();
  if (validation_goto_btn_) {
    validation_goto_btn_->setEnabled(true);
  }
}

void PropertyEditor::select_validation_row(int row) {
  if (!validation_table_ || row < 0 ||
      row >= validation_table_->rowCount() || !current_item_) {
    return;
  }
  auto* item = validation_table_->item(row, 0);
  if (!item) {
    return;
  }
  const QString root_name = item->data(Qt::UserRole).toString();
  const QString child_name = item->data(Qt::UserRole + 1).toString();
  auto* tree = current_item_->treeWidget();
  if (!tree) {
    return;
  }
  QTreeWidgetItem* root_item = nullptr;
  for (int i = 0; i < tree->topLevelItemCount(); ++i) {
    auto* root = tree->topLevelItem(i);
    if (root && root->text(0) == root_name) {
      root_item = root;
      break;
    }
  }
  if (!root_item) {
    return;
  }
  root_item->setExpanded(true);
  for (int j = 0; j < root_item->childCount(); ++j) {
    auto* child = root_item->child(j);
    if (child && child->text(0) == child_name) {
      tree->setCurrentItem(child);
      break;
    }
  }
}

QStringList PropertyEditor::validate_params(const QString& kind,
                                            const QVariantMap& params) const {
  QStringList missing;
  auto require_key = [&missing, &params](const QString& key) {
    if (params.value(key).toString().trimmed().isEmpty()) {
      missing << key;
    }
  };
  if (kind == "Materials") {
    require_key("type");
    const QStringList blocks =
        params.value("block")
            .toString()
            .split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (const auto& block : blocks) {
      if (!volume_groups_.contains(block)) {
        missing << "block must reference an existing volume Physical Group";
        break;
      }
    }
    const QString type = params.value("type").toString();
    if (type == "GenericConstantMaterial") {
      require_key("prop_names");
      require_key("prop_values");
      const QStringList names = params.value("prop_names")
                                    .toString()
                                    .split(QRegularExpression("\\s+"),
                                           Qt::SkipEmptyParts);
      const QStringList values = params.value("prop_values")
                                     .toString()
                                     .split(QRegularExpression("\\s+"),
                                            Qt::SkipEmptyParts);
      if (!names.isEmpty() && !values.isEmpty() &&
          names.size() != values.size()) {
        missing << "prop_names/prop_values count mismatch";
      }
    } else if (type == "ParsedMaterial") {
      require_key("expression");
      require_key("property_name");
    } else if (type == "ComputeElasticityTensor") {
      require_key("C_ijkl");
    } else if (type == "ComputeIsotropicElasticityTensor") {
      require_key("youngs_modulus");
      require_key("poissons_ratio");
      const QString young_text =
          params.value("youngs_modulus").toString().trimmed();
      bool young_ok = false;
      const double young = young_text.toDouble(&young_ok);
      if (!young_text.isEmpty() && (!young_ok || young <= 0.0)) {
        missing << "youngs_modulus must be > 0";
      }
      const QString poisson_text =
          params.value("poissons_ratio").toString().trimmed();
      bool poisson_ok = false;
      const double poisson = poisson_text.toDouble(&poisson_ok);
      if (!poisson_text.isEmpty() &&
          (!poisson_ok || poisson <= -1.0 || poisson >= 0.5)) {
        missing << "poissons_ratio must be between -1 and 0.5";
      }
    } else if (type == "ComputeSmallStrain") {
      require_key("displacements");
    } else if (type == "ComputeThermalExpansionEigenstrain") {
      require_key("thermal_expansion_coeff");
      require_key("temperature");
    } else if (type == "AbaqusCDP") {
      // W-03a：CDP 三件套表单合同（v01 验收基线）。youngs_modulus 在
      // params 中为 SI 求解值（Pa），表单以 MPa 显示。
      require_key("youngs_modulus");
      require_key("poissons_ratio");
      require_key("dilation_angle");
      require_key("eccentricity");
      require_key("biaxial_to_uniaxial_compression_ratio");
      require_key("tensile_meridian_ratio");
      require_key("viscosity");
      require_key("tension_recovery");
      require_key("compression_recovery");
      require_key("maximum_substeps");
      require_key("compression_hardening_file");
      require_key("compression_damage_file");
      require_key("tension_stiffening_file");
      require_key("tension_damage_file");
    }
  } else if (kind == "Sections") {
    require_key("type");
    require_key("material");
    // W-01b：材料被删除/重命名后引用悬空，Section 标为有问题。
    const QString material = params.value("material").toString().trimmed();
    if (!material.isEmpty() &&
        !collect_model_names("Materials").contains(material)) {
      missing << "material reference";
    }
  } else if (kind == "Steps") {
    const QString type = params.value("type").toString();
    if (type.isEmpty()) {
      require_key("type");
    } else if (type == "Transient") {
      require_key("dt");
      require_key("end_time");
      bool ok_dt = false;
      const double dt = params.value("dt").toString().toDouble(&ok_dt);
      if (ok_dt && dt <= 0.0) {
        missing << "dt must be > 0";
      }
      bool ok_end = false;
      const double end_time =
          params.value("end_time").toString().toDouble(&ok_end);
      if (ok_end && end_time <= 0.0) {
        missing << "end_time must be > 0";
      }
    }
  } else if (kind == "Functions") {
    // W-03c：ParsedFunction 需 expression；PiecewiseLinear 需 x/y 数据对
    // 且个数一致（复载曲线）。
    const QString type = params.value("type").toString();
    if (type == "PiecewiseLinear") {
      require_key("x");
      require_key("y");
      const QStringList xs =
          params.value("x").toString().split(QRegularExpression("\\s+"),
                                             Qt::SkipEmptyParts);
      const QStringList ys =
          params.value("y").toString().split(QRegularExpression("\\s+"),
                                             Qt::SkipEmptyParts);
      if (!xs.isEmpty() && !ys.isEmpty() && xs.size() != ys.size()) {
        missing << "x/y count mismatch";
      }
    } else {
      require_key("expression");
    }
  } else if (kind == "BC") {
    const QString type = params.value("type").toString();
    require_key("variable");
    require_key("boundary");
    if (type == "FunctionDirichletBC") {
      require_key("function");
    } else {
      require_key("value");
    }
  } else if (kind == "Loads") {
    const QString type = params.value("type").toString();
    if (type != "TensorMechanics") {
      require_key("variable");
    }
    if (type == "BodyForce") {
      if (params.value("value").toString().trimmed().isEmpty() &&
          params.value("function").toString().trimmed().isEmpty()) {
        missing << "value or function";
      }
    } else if (type == "MatDiffusion") {
      require_key("diffusivity");
    } else if (type == "TensorMechanics") {
      require_key("displacements");
    } else if (type == "Pressure") {
      const QString variable = params.value("variable").toString().trimmed();
      if (!variable.isEmpty() &&
          !pressure_variable_candidates().contains(variable)) {
        missing << "variable must reference a displacement variable";
      }
      require_key("boundary");
      const QStringList boundaries =
          params.value("boundary")
              .toString()
              .split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
      for (const auto& boundary : boundaries) {
        if (!boundary_groups_.contains(boundary)) {
          missing << "boundary must reference an existing 2D Physical Group";
          break;
        }
      }
      if (params.value("factor").toString().trimmed().isEmpty() &&
          params.value("function").toString().trimmed().isEmpty()) {
        missing << "factor or function";
      }
      const QString component = params.value("component").toString().trimmed();
      if (!component.isEmpty()) {
        bool ok = false;
        const int value = component.toInt(&ok);
        if (!ok || value < 0 || value > 2) {
          missing << "component must be 0, 1, or 2";
        }
      }
    }
  } else if (kind == "Interactions") {
    const QString type = params.value("type").toString();
    require_key("type");
    if (type == "Contact") {
      require_key("model");
      require_key("formulation");
      require_key("primary");
      require_key("secondary");
      const QString primary = params.value("primary").toString().trimmed();
      const QString secondary = params.value("secondary").toString().trimmed();
      if (!primary.isEmpty() && !boundary_groups_.contains(primary)) {
        missing << "primary must reference an existing 2D Physical Group";
      }
      if (!secondary.isEmpty() && !boundary_groups_.contains(secondary)) {
        missing << "secondary must reference an existing 2D Physical Group";
      }
      if (!primary.isEmpty() && primary == secondary) {
        missing << "primary and secondary must differ";
      }
      const QString friction =
          params.value("friction_coefficient").toString().trimmed();
      if (params.value("model").toString() == "coulomb" &&
          friction.isEmpty()) {
        missing << "friction_coefficient";
      } else if (!friction.isEmpty()) {
        bool ok = false;
        const double value = friction.toDouble(&ok);
        if (!ok || value < 0.0) {
          missing << "friction_coefficient must be >= 0";
        }
      }
    }
  } else if (kind == "Physics") {
    // W-03b：Physics action 合同（v01）。block 允许暂空（生成侧警告），
    // action/strain 必填。
    require_key("action");
    require_key("strain");
  } else if (kind == "Outputs") {
    // W-03d：勾选历史输出套餐后面组必填；Times 勾选后间隔必填；
    // 极值套餐勾选后变量列表必填。
    const auto enabled = [&params](const QString& key) {
      return params.value(key).toString().trimmed() == "true";
    };
    if (enabled("hist_reaction_force") || enabled("hist_displacement_avg")) {
      require_key("hist_boundary");
    }
    if (enabled("times_enabled")) {
      require_key("times_interval");
    }
    if (enabled("hist_extremum")) {
      require_key("hist_extremum_variables");
    }
  } else if (kind == "Assembly") {
    require_key("part");
    const QString part = params.value("part").toString().trimmed();
    if (!part.isEmpty() && !collect_model_names("Parts").contains(part)) {
      missing << "part reference";
    }
    for (const QString& key : {"translate_x", "translate_y", "translate_z",
                               "rotate_x", "rotate_y", "rotate_z", "scale_x",
                               "scale_y", "scale_z", "order"}) {
      bool ok = false;
      const double value = params.value(key).toString().toDouble(&ok);
      if (!ok) {
        missing << key + " must be numeric";
      } else if (key.startsWith("scale_") && value <= 0.0) {
        missing << key + " must be > 0";
      }
    }
  }
  return missing;
}

QStringList PropertyEditor::collect_model_names(const QString& root_name) const {
  QStringList names;
  const QTreeWidget* tree =
      current_item_ ? current_item_->treeWidget() : nullptr;
  if (!tree) {
    tree = model_tree_.data();
  }
  if (!tree) {
    return names;
  }
  for (int i = 0; i < tree->topLevelItemCount(); ++i) {
    auto* root = tree->topLevelItem(i);
    if (!root || root->text(0) != root_name) {
      continue;
    }
    for (int j = 0; j < root->childCount(); ++j) {
      auto* child = root->child(j);
      if (child) {
        names << child->text(0);
      }
    }
    break;
  }
  return names;
}

QStringList PropertyEditor::pressure_variable_candidates() const {
  QStringList names = collect_model_names("Variables");
  // Physics/add_variables can create these at input-generation time, so they
  // need to be selectable even before the Variables tree has child nodes.
  for (const auto* displacement : {"disp_x", "disp_y", "disp_z"}) {
    if (!names.contains(QLatin1String(displacement))) {
      names << QLatin1String(displacement);
    }
  }
  return names;
}

QVariantMap PropertyEditor::build_type_template(const QString& kind,
                                                const QString& type) const {
  QVariantMap t;
  const QString var =
      current_variables_.isEmpty() ? "u" : current_variables_.first();
  const QString func =
      current_functions_.isEmpty() ? "func_1" : current_functions_.first();
  const QString mat =
      current_materials_.isEmpty() ? "material_1" : current_materials_.first();
  const QString bnd =
      boundary_groups_.isEmpty() ? "left" : boundary_groups_.first();
  const QString block =
      volume_groups_.isEmpty() ? "block_1" : volume_groups_.first();

  if (!type.isEmpty()) {
    t.insert("type", type);
  }
  if (kind == "Materials") {
    if (type == "GenericConstantMaterial") {
      t.insert("prop_names", "thermal_conductivity");
      t.insert("prop_values", "1.0");
    } else if (type == "ParsedMaterial") {
      t.insert("property_name", "thermal_conductivity");
      t.insert("expression", "1 + 0.01*T");
      t.insert("coupled_variables", "T");
    } else if (type == "ComputeElasticityTensor") {
      t.insert("fill_method", "symmetric_isotropic");
      t.insert("C_ijkl", "2.1e5 0.8e5");
    } else if (type == "ComputeSmallStrain") {
      t.insert("displacements", "disp_x disp_y");
    } else if (type == "ComputeThermalExpansionEigenstrain") {
      t.insert("thermal_expansion_coeff", "1e-5");
      t.insert("temperature", "T");
      t.insert("stress_free_temperature", "300");
      t.insert("eigenstrain_name", "eigenstrain");
    } else if (type == "AbaqusCDP") {
      // W-03a：v01 验收基线默认值（params 存 SI 求解值，E 为 Pa；
      // unit_factor_stress 记录表单显示→存储换算比例，MPa→Pa = 1e6）。
      t.insert("youngs_modulus", "29791500000");
      t.insert("poissons_ratio", "0.2");
      t.insert("dilation_angle", "36");
      t.insert("eccentricity", "0.1");
      t.insert("biaxial_to_uniaxial_compression_ratio", "1.16");
      t.insert("tensile_meridian_ratio", "0.667");
      t.insert("viscosity", "5e-4");
      t.insert("tension_recovery", "0");
      t.insert("compression_recovery", "1");
      t.insert("maximum_substeps", "256");
      t.insert("maximum_strain_increment", "2.5e-5");
      t.insert("enable_performance_diagnostics", "true");
      t.insert("unit_factor_stress", "1000000");
      t.insert("compression_hardening_file", "");
      t.insert("compression_damage_file", "");
      t.insert("tension_stiffening_file", "");
      t.insert("tension_damage_file", "");
    }
  } else if (kind == "Sections") {
    t.insert("type", type.isEmpty() ? "SolidSection" : type);
    t.insert("material", mat);
  } else if (kind == "Steps") {
    if (type == "Transient") {
      // W-03e：v01 验收基线默认值（与 default_params_for_kind 对齐）。
      t.insert("start_time", "0");
      t.insert("end_time", "1");
      t.insert("solve_type", "NEWTON");
      t.insert("line_search", "bt");
      t.insert("automatic_scaling", "true");
      t.insert("nl_rel_tol", "1e-9");
      t.insert("nl_abs_tol", "1e-8");
      t.insert("nl_max_its", "50");
      t.insert("num_steps", "100000");
      t.insert("dtmin", "1e-15");
      t.insert("dtmax", "1");
      t.insert("petsc_options_iname", "-pc_type -pc_factor_mat_solver_type");
      t.insert("petsc_options_value", "lu mumps");
      t.insert("timestepper_type", "IterationAdaptiveDT");
      t.insert("dt", "0.01");
      t.insert("optimal_iterations", "8");
      t.insert("iteration_window", "3");
      t.insert("growth_factor", "1.15");
      t.insert("cutback_factor", "0.5");
      t.insert("preconditioning_type", "SMP");
      t.insert("preconditioning_full", "true");
    } else if (type == "Steady") {
      t.insert("solve_type", "NEWTON");
    }
  } else if (kind == "Functions") {
    // W-03c：函数类型默认。PiecewiseLinear 给最小单调数据对占位。
    if (type == "PiecewiseLinear") {
      t.insert("x", "0 1");
      t.insert("y", "0 1");
    } else {
      t.insert("expression", "1.0");
    }
  } else if (kind == "BC") {
    t.insert("variable", var);
    t.insert("boundary", bnd);
    if (type == "FunctionDirichletBC") {
      t.insert("function", func);
    } else {
      t.insert("value", "0");
    }
  } else if (kind == "Loads") {
    if (type == "TensorMechanics") {
      t.insert("displacements", "disp_x disp_y");
      t.insert("block", block);
    } else {
      t.insert("variable", type == "Pressure" ? "disp_z" : var);
    }
    if (type == "BodyForce") {
      t.insert("value", "1.0");
    } else if (type == "MatDiffusion") {
      t.insert("diffusivity", "diff_u");
    } else if (type == "Pressure") {
      t.insert("boundary", bnd);
      t.insert("factor", "1.0");
      if (!current_functions_.isEmpty()) {
        t.insert("function", func);
      }
      t.insert("component", "2");
      t.insert("use_displaced_mesh", "true");
    }
  } else if (kind == "Interactions") {
    t.insert("type", type.isEmpty() ? "Contact" : type);
    t.insert("model", "coulomb");
    t.insert("formulation", "kinematic");
    if (!boundary_groups_.isEmpty()) {
      t.insert("primary", boundary_groups_.first());
      t.insert("secondary", boundary_groups_.size() > 1
                                ? boundary_groups_.at(1)
                                : boundary_groups_.first());
    }
    t.insert("friction_coefficient", "0.15");
    t.insert("tangential_tolerance", "5e-4");
    t.insert("penalty", "1e12");
    t.insert("normalize_penalty", "true");
  } else if (kind == "Physics") {
    // W-03b：v01 验收基线默认值（与 default_params_for_kind 对齐）；
    // block 不覆盖（由 Section 指派/chips 填入）。
    t.insert("action", "QuasiStatic");
    t.insert("volumetric_locking_correction", "true");
    t.insert("add_variables", "true");
    t.insert("incremental", "true");
    t.insert("strain", "SMALL");
    t.insert("generate_output", QLatin1String(kPhysicsGenerateOutputDefault));
    t.insert("save_in_resid", "true");
  }
  return t;
}

void PropertyEditor::apply_template_values(const QVariantMap& values,
                                           bool overwrite) {
  if (!current_item_) {
    return;
  }
  QVariantMap params = current_item_->data(0, kParamsRole).toMap();
  bool changed = false;
  const QString kind =
      current_item_->data(0, kKindRole).toString().isEmpty()
          ? current_item_->text(0)
          : current_item_->data(0, kKindRole).toString();
  // 材料模板切换时清掉其他材料类型的已知专属字段。普通材料的
  // prop_names/prop_values 若残留到 AbaqusCDP，会被当作无效 MOOSE
  // 参数写进 AbaqusCDPStressUpdate。
  if (kind == "Materials" && values.contains("type")) {
    const QString type = values.value("type").toString();
    const QSet<QString> known_type_keys = {
        "prop_names",
        "prop_values",
        "expression",
        "property_name",
        "coupled_variables",
        "fill_method",
        "C_ijkl",
        "thermal_expansion_coeff",
        "temperature",
        "stress_free_temperature",
        "eigenstrain_name",
        "displacements",
        "youngs_modulus",
        "poissons_ratio",
        "dilation_angle",
        "eccentricity",
        "biaxial_to_uniaxial_compression_ratio",
        "tensile_meridian_ratio",
        "viscosity",
        "tension_recovery",
        "compression_recovery",
        "maximum_substeps",
        "maximum_strain_increment",
        "enable_performance_diagnostics",
        "unit_factor_stress",
        "compression_hardening_file",
        "compression_damage_file",
        "tension_stiffening_file",
        "tension_damage_file"};
    QSet<QString> allowed;
    if (type == "GenericConstantMaterial") {
      allowed = {"prop_names", "prop_values"};
    } else if (type == "ParsedMaterial") {
      allowed = {"expression", "property_name", "coupled_variables"};
    } else if (type == "ComputeElasticityTensor") {
      allowed = {"fill_method", "C_ijkl"};
    } else if (type == "ComputeIsotropicElasticityTensor") {
      allowed = {"youngs_modulus", "poissons_ratio"};
    } else if (type == "ComputeSmallStrain") {
      allowed = {"displacements"};
    } else if (type == "ComputeThermalExpansionEigenstrain") {
      allowed = {"thermal_expansion_coeff", "temperature",
                 "stress_free_temperature", "eigenstrain_name"};
    } else if (type == "AbaqusCDP") {
      allowed = {"youngs_modulus",
                 "poissons_ratio",
                 "dilation_angle",
                 "eccentricity",
                 "biaxial_to_uniaxial_compression_ratio",
                 "tensile_meridian_ratio",
                 "viscosity",
                 "tension_recovery",
                 "compression_recovery",
                 "maximum_substeps",
                 "maximum_strain_increment",
                 "enable_performance_diagnostics",
                 "unit_factor_stress",
                 "compression_hardening_file",
                 "compression_damage_file",
                 "tension_stiffening_file",
                 "tension_damage_file"};
    }
    if (!allowed.isEmpty() || type == "ComputeLinearElasticStress") {
      for (const auto& key : known_type_keys) {
        if (!allowed.contains(key) && params.remove(key) > 0) {
          changed = true;
        }
      }
    }
  }
  for (auto it = values.begin(); it != values.end(); ++it) {
    if (!overwrite && !params.value(it.key()).toString().trimmed().isEmpty()) {
      continue;
    }
    if (params.value(it.key()) != it.value()) {
      params.insert(it.key(), it.value());
      changed = true;
    }
  }
  if (changed) {
    // 模板是一组参数，必须一次写入。逐字段 setData 会同步触发模型树刷新，
    // 中途重建本编辑器，导致后续字段（尤其 type）丢失。
    write_item(current_item_->text(0), params);
    load_from_item();
  }
  update_validation();
}

void PropertyEditor::on_apply_template() {
  if (!current_item_) {
    return;
  }
  const QString kind =
      current_item_->data(0, kKindRole).toString().isEmpty()
          ? current_item_->text(0)
          : current_item_->data(0, kKindRole).toString();
  // 显示文字会随界面语言翻译，模板查找必须使用稳定的内部键。
  // 否则中文的“CDP 混凝土 (Abaqus)”无法命中英文 preset key。
  QString choice = "Type Defaults";
  if (template_combo_) {
    choice = template_combo_->currentData().toString();
    if (choice.isEmpty()) {
      choice = template_combo_->currentText();
    }
  }
  QVariantMap values;
  if (choice == "Type Defaults") {
    QString type;
    if (form_widgets_.contains("type")) {
      if (auto* combo =
              qobject_cast<QComboBox*>(form_widgets_.value("type"))) {
        type = combo->currentText().trimmed();
      } else if (auto* edit =
                     qobject_cast<QLineEdit*>(form_widgets_.value("type"))) {
        type = edit->text().trimmed();
      }
    }
    values = build_type_template(kind, type);
  } else if (template_presets_.contains(choice)) {
    values = template_presets_.value(choice);
  }
  if (values.isEmpty()) {
    return;
  }
  apply_template_values(values, true);
  build_form_for_kind(kind);
  // 应用模板会重建整张表单。模板选择仅用于编辑器交互，不写入模型参数，
  // 因此重建后要按稳定内部键恢复当前项，避免界面看起来回退到“类型默认值”。
  if (template_combo_) {
    const int applied_index = template_combo_->findData(choice);
    if (applied_index >= 0) {
      template_combo_->setCurrentIndex(applied_index);
    }
  }
  update_group_widget_for_kind(kind);
  update_validation();
  refresh_preview();
}

void PropertyEditor::update_preview_text_height() {
  if (!preview_text_ || !preview_text_->document()) {
    return;
  }
  // 等宽预览区按内容行数给高、全部展开，内容短时不再被拉伸/裁掉末行。
  // NoWrap 下每个文本块即一行，blockCount 同步可得，不依赖异步重排。
  const int line_count = qMax(1, preview_text_->document()->blockCount());
  const int height = line_count * preview_text_->fontMetrics().lineSpacing() +
                     preview_text_->frameWidth() * 2 + 4;
  preview_text_->setFixedHeight(height);
}

void PropertyEditor::refresh_preview() {
  if (!preview_text_ || !preview_summary_label_) {
    return;
  }
  if (!current_item_) {
    preview_summary_label_->setText("Selected item preview:");
    preview_text_->setPlainText("Select a node in the model tree.");
    update_preview_text_height();
    return;
  }

  const bool root = is_root_item();
  const QString kind =
      current_item_->data(0, kKindRole).toString().isEmpty()
          ? current_item_->text(0)
          : current_item_->data(0, kKindRole).toString();
  const QVariantMap params = root ? QVariantMap()
                                 : current_item_->data(0, kParamsRole).toMap();
  const QString status = params.value("status").toString().isEmpty()
                             ? params.value("state").toString()
                             : params.value("status").toString();

  QStringList lines;
  lines << QString("Node: %1").arg(current_item_->text(0));
  lines << QString("Kind: %1").arg(kind);
  lines << QString("Status: %1").arg(status.isEmpty() ? "-" : status);

  if (root) {
    lines << "";
    lines << "[Root]";
    lines << "  Child nodes are configured in the corresponding module tab.";
    preview_summary_label_->setText(QString("Preview: %1 root").arg(kind));
    preview_text_->setPlainText(lines.join("\n"));
    update_preview_text_height();
    return;
  }

  const QString block =
      kind == "Materials" ? "Materials"
      : kind == "Sections" ? "Sections"
      : kind == "Steps" ? "Steps"
      : kind == "BC" ? "BCs"
      : kind == "Loads" ? "Loads"
      : kind == "Functions" ? "Functions"
      : kind == "Variables" ? "Variables"
      : QString();

  lines << "";
  lines << "[Input Preview]";
  if (!block.isEmpty()) {
    lines << QString("  [%1]").arg(block);
  }
  lines << QString("  # name: %1").arg(current_item_->text(0));
  QStringList keys = params.keys();
  keys.sort();
  for (const auto& key_name : keys) {
    if (key_name == "status" || key_name == "state") {
      continue;
    }
    lines << QString("  %1 = %2").arg(key_name,
                                       params.value(key_name).toString());
  }

  QStringList issues = validate_params(kind, params);
  lines << "";
  lines << "[Validation]";
  lines << (issues.isEmpty() ? "  No issues." : "  Missing: " +
                                              issues.join(", "));

  preview_summary_label_->setText(
      QString("Preview: %1 (%2) — node data, not MOOSE input")
          .arg(current_item_->text(0), kind));
  preview_text_->setPlainText(lines.join("\n"));
  update_preview_text_height();
}

QString PropertyEditor::build_node_summary(const QString& kind,
                                           const QVariantMap& params) const {
  if (kind == "Mesh") {
    QStringList lines;
    const QString summary = params.value("summary").toString().trimmed();
    if (!summary.isEmpty()) {
      lines << summary;
    }

    QStringList group_names =
        params.value("physical_group_names").toStringList();
    if (group_names.isEmpty()) {
      for (const QVariant& value :
           params.value("physical_group_names").toList()) {
        const QString name = value.toString().trimmed();
        if (!name.isEmpty()) {
          group_names << name;
        }
      }
    }
    if (!group_names.isEmpty()) {
      lines << QString("%1: %2")
                   .arg(l10n::tr("Physical Groups"), group_names.join(", "));
    }

    const QString sha256 = params.value("sha256").toString().trimmed();
    if (!sha256.isEmpty()) {
      lines << QString("SHA-256: %1").arg(sha256);
    }
    return lines.isEmpty() ? QString("-") : lines.join("\n");
  }
  if (kind == "Features") {
    // 特征 = 建模历史: 操作类型 + 来源草图 + 关键参数
    QStringList parts;
    if (!params.value("type").toString().isEmpty()) {
      parts << params.value("type").toString();
    }
    if (!params.value("sketch").toString().isEmpty()) {
      parts << QString("sketch: %1").arg(params.value("sketch").toString());
    }
    for (const char* k : {"distance", "angle_deg", "z2", "sketch2", "path"}) {
      if (params.contains(k)) {
        parts << QString("%1=%2").arg(QLatin1String(k),
                                      params.value(k).toString());
      }
    }
    return parts.isEmpty() ? QString("-") : parts.join(", ");
  }
  if (kind == "Parts") {
    // 部件 = 3D 产物: 来源草图 + 产出它的特征历史
    QStringList parts;
    if (!params.value("sketch").toString().isEmpty()) {
      parts << QString("sketch: %1").arg(params.value("sketch").toString());
    }
    if (!params.value("feature").toString().isEmpty()) {
      parts << QString("feature: %1").arg(params.value("feature").toString());
    }
    const QVariantList volume_tags = params.value("gmsh_volume_tags").toList();
    if (!volume_tags.isEmpty()) {
      QStringList labels;
      for (const QVariant& tag : volume_tags) {
        labels << tag.toString();
      }
      parts << QString("gmsh volumes %1").arg(labels.join(", "));
    } else if (!params.value("gmsh_volume_tag").toString().isEmpty()) {
      parts << QString("gmsh volume %1")
                   .arg(params.value("gmsh_volume_tag").toString());
    }
    return parts.isEmpty() ? QString("-") : parts.join(", ");
  }
  if (kind == "Assembly") {
    return QString("part: %1 | T=(%2, %3, %4) | R=(%5, %6, %7) | %8")
        .arg(params.value("part").toString(),
             params.value("translate_x").toString(),
             params.value("translate_y").toString(),
             params.value("translate_z").toString(),
             params.value("rotate_x").toString(),
             params.value("rotate_y").toString(),
             params.value("rotate_z").toString(),
             params.value("visible", "true").toString() == "false"
                 ? QString("hidden")
                 : QString("visible"));
  }
  if (kind == "Sketches") {
    const QString data = params.value("data").toString();
    if (data.trimmed().isEmpty()) {
      return QString("plane: XY, empty sketch");
    }
    SketchDocument doc;
    if (doc.from_yaml_string(data, nullptr)) {
      return QString("plane: XY, entities: %1, constraints: %2")
          .arg(doc.entity_count())
          .arg(doc.constraints().size());
    }
    return QString("plane: XY (data parse error)");
  }
  return QString("-");
}

void PropertyEditor::build_form_for_kind(const QString& kind) {
  clear_form();
  if (!form_box_ || !form_layout_) {
    return;
  }
  const QSet<QString> supported = {"Materials", "Sections", "Assembly",
                                   "Steps", "BC", "Loads", "Functions",
                                   "Physics", "Outputs", "Interactions"};
  if (!supported.contains(kind)) {
    form_box_->setVisible(false);
    return;
  }
  form_box_->setVisible(true);

  current_variables_ = collect_model_names("Variables");
  current_functions_ = collect_model_names("Functions");
  current_materials_ = collect_model_names("Materials");
  const QStringList variables = current_variables_;
  const QStringList functions = current_functions_;
  const QStringList materials = current_materials_;

  // Section 当前只有 SolidSection；Physics 当前也只有一套 QuasiStatic
  // 默认参数，因此不显示模板栏。Materials/BC/Loads 具有多个命名
  // 模板，使用下拉选择；Steps/Functions/Outputs 只有“类型默认值”，
  // 用直接按钮代替无选择意义的单项下拉，避免 macOS 弹层错位。
  if (kind != "Sections" && kind != "Physics" && kind != "Assembly") {
    const bool has_named_templates =
        kind == "Materials" || kind == "BC" || kind == "Loads";
    auto* template_row = new QWidget(form_box_);
    auto* template_layout = new QHBoxLayout(template_row);
    template_layout->setContentsMargins(0, 0, 0, 0);
    if (has_named_templates) {
      template_combo_ = new QComboBox(template_row);
      template_combo_->setObjectName("propertyTemplateSelector");
      install_combo_popup_fix(template_combo_);
      template_combo_->addItem("Type Defaults", "Type Defaults");
      template_layout->addWidget(template_combo_);
    }
    apply_template_btn_ =
        new QPushButton(has_named_templates ? "Apply Template"
                                            : "Restore Type Defaults",
                        template_row);
    apply_template_btn_->setIcon(gmp::icons::get("apply_template"));
    apply_template_btn_->setObjectName("propertyApplyTemplateButton");
    apply_template_btn_->setToolTip(
        l10n::tr("Overwrite template-controlled fields with their default "
                 "values; group assignments are kept."));
    template_layout->addWidget(apply_template_btn_);
    template_layout->addStretch(1);
    form_layout_->addRow("Template", template_row);
    connect(apply_template_btn_, &QPushButton::clicked, this,
            &PropertyEditor::on_apply_template);
  }
  template_presets_.clear();
  template_descriptions_.clear();

  auto add_line = [this](const QString& label, const QString& key,
                         const QString& object_name = QString()) {
    auto* edit = new QLineEdit(form_box_);
    if (!object_name.isEmpty()) {
      edit->setObjectName(object_name);
    }
    form_layout_->addRow(label, edit);
    form_widgets_.insert(key, edit);
    connect(edit, &QLineEdit::textChanged, this,
            [this, key](const QString& value) {
              set_param_value(key, value);
            });
  };

  // W-03a：文件选择行（编辑框 + Browse 按钮），params 存绝对路径；
  // 生成 .i 时由 MainWindow 归一化为 basename 相对引用。
  auto add_file_row = [this](const QString& label, const QString& key,
                             const QString& object_name) {
    auto* row = new QWidget(form_box_);
    auto* row_layout = new QHBoxLayout(row);
    row_layout->setContentsMargins(0, 0, 0, 0);
    row_layout->setSpacing(6);
    auto* edit = new QLineEdit(row);
    edit->setObjectName(object_name);
    auto* browse = new QPushButton("Browse...", row);
    browse->setObjectName(object_name + "Browse");
    row_layout->addWidget(edit, 1);
    row_layout->addWidget(browse);
    form_layout_->addRow(label, row);
    form_widgets_.insert(key, row);
    connect(edit, &QLineEdit::textChanged, this,
            [this, key](const QString& value) {
              set_param_value(key, value);
            });
    connect(browse, &QPushButton::clicked, this, [this, edit]() {
      const QString path = QFileDialog::getOpenFileName(
          this, "Select CSV file", edit->text(),
          "CSV Files (*.csv);;All Files (*)");
      if (!path.isEmpty()) {
        edit->setText(path);
      }
    });
  };

  auto add_combo = [this](const QString& label, const QString& key,
                          const QStringList& items,
                          const QString& object_name = QString()) {
    auto* combo = new QComboBox(form_box_);
    install_combo_popup_fix(combo);
    if (!object_name.isEmpty()) {
      combo->setObjectName(object_name);
    }
    combo->addItems(items);
    combo->setEditable(true);
    form_layout_->addRow(label, combo);
    form_widgets_.insert(key, combo);
    connect(combo, &QComboBox::currentTextChanged, this,
            [this, key](const QString& value) {
              set_param_value(key, value);
            });
  };

  // W-03e：分组小标题（占满整行），用于 Step 表单的基本/求解控制/
  // 时间步进/预处理分组。
  auto add_section = [this](const QString& text) {
    auto* label = new QLabel(text, form_box_);
    label->setStyleSheet("font-weight: 600; color: #333; padding-top: 6px;");
    form_layout_->addRow(label);
  };

  // W-03d：checkbox 多选组（param 值 = 选中项按候选顺序空格拼接）。
  // 每行 4 个，候选为 MOOSE 标识符（不翻译）。
  auto add_checkbox_group = [this](const QString& label, const QString& key,
                                   const QStringList& options,
                                   const QString& object_name) {
    auto* row = new QWidget(form_box_);
    auto* row_layout = new QVBoxLayout(row);
    row_layout->setContentsMargins(0, 0, 0, 0);
    row_layout->setSpacing(2);
    QHBoxLayout* line = nullptr;
    for (int i = 0; i < options.size(); ++i) {
      if (i % 4 == 0) {
        line = new QHBoxLayout();
        line->setContentsMargins(0, 0, 0, 0);
        line->setSpacing(8);
        row_layout->addLayout(line);
      }
      auto* box = new QCheckBox(options.at(i), row);
      box->setObjectName(object_name + "_" + options.at(i));
      line->addWidget(box);
      if (i % 4 == 3 || i == options.size() - 1) {
        line->addStretch(1);
      }
    }
    form_layout_->addRow(label, row);
    form_widgets_.insert(key, row);
    for (auto* box : row->findChildren<QCheckBox*>()) {
      connect(box, &QCheckBox::toggled, this,
              [this, key, row, options](bool) {
                if (form_updating_) {
                  return;
                }
                QStringList selected;
                const auto boxes = row->findChildren<QCheckBox*>();
                for (const auto& opt : options) {
                  for (auto* candidate : boxes) {
                    if (candidate->text() == opt && candidate->isChecked()) {
                      selected << opt;
                      break;
                    }
                  }
                }
                set_param_value(key, selected.join(" "));
              });
    }
  };

  if (kind == "Materials") {
    add_combo("Type", "type",
              {"GenericConstantMaterial", "ParsedMaterial",
               "ComputeElasticityTensor", "ComputeIsotropicElasticityTensor",
               "ComputeSmallStrain",
               "ComputeLinearElasticStress",
               "ComputeThermalExpansionEigenstrain", "AbaqusCDP"},
              "materialTypeCombo");
    add_line("Prop Names", "prop_names");
    add_line("Prop Values", "prop_values");
    add_line("Expression", "expression");
    add_line("Property Name", "property_name");
    add_line("Coupled Vars", "coupled_variables");
    add_line("fill_method", "fill_method");
    add_line("C_ijkl", "C_ijkl");
    add_line("Block", "block", "materialBlockEdit");
    if (auto* block = qobject_cast<QLineEdit*>(form_widgets_.value("block"))) {
      block->setReadOnly(true);
      block->setToolTip(l10n::tr(
          "Select physical volumes above and apply them to update this value."));
    }
    add_line("thermal_expansion_coeff", "thermal_expansion_coeff");
    add_line("temperature", "temperature");
    add_line("stress_free_temperature", "stress_free_temperature");
    add_line("eigenstrain_name", "eigenstrain_name");
    add_line("displacements", "displacements");
    // E/nu 同时服务于普通各向同性线弹性和 CDP；E 在表单按 MPa 显示，
    // params 存 SI（Pa）。仅 CDP 记录额外的 unit_factor_stress 元数据。
    {
      auto* young = new QLineEdit(form_box_);
      young->setObjectName("cdpYoungsModulusMpa");
      young->setToolTip(
          "Displayed in MPa; stored as SI (Pa) in node parameters.");
      form_layout_->addRow("Young's Modulus (MPa)", young);
      form_widgets_.insert("youngs_modulus", young);
      connect(young, &QLineEdit::textChanged, this,
              [this](const QString& value) {
                if (form_updating_) {
                  return;
                }
                const double factor = display_unit_factor("pressure", 1e6);
                bool ok = false;
                const double display = value.trimmed().toDouble(&ok);
                if (!value.trimmed().isEmpty() && ok) {
                  set_param_value("youngs_modulus",
                                  QString::number(display * factor, 'g', 17));
                } else {
                  // 非数值输入原样写入，由校验如实提示。
                  set_param_value("youngs_modulus", value);
                }
                if (current_item_ &&
                    current_item_->data(0, kParamsRole).toMap()
                            .value("type").toString() == "AbaqusCDP") {
                  set_param_value("unit_factor_stress",
                                  QString::number(factor, 'g', 17));
                }
              });
    }
    add_line("Poisson's Ratio", "poissons_ratio", "cdpPoissonsRatio");
    add_line("Dilation Angle (deg)", "dilation_angle", "cdpDilationAngle");
    add_line("Eccentricity", "eccentricity", "cdpEccentricity");
    add_line("fb0/fc0 Ratio", "biaxial_to_uniaxial_compression_ratio",
             "cdpBiaxialRatio");
    add_line("Tensile Meridian Ratio", "tensile_meridian_ratio",
             "cdpTensileMeridianRatio");
    add_line("Viscosity", "viscosity", "cdpViscosity");
    add_line("Tension Recovery", "tension_recovery", "cdpTensionRecovery");
    add_line("Compression Recovery", "compression_recovery",
             "cdpCompressionRecovery");
    add_line("Max Substeps", "maximum_substeps", "cdpMaximumSubsteps");
    add_file_row("Compression Hardening CSV", "compression_hardening_file",
                 "cdpCompressionHardeningFile");
    add_file_row("Compression Damage CSV", "compression_damage_file",
                 "cdpCompressionDamageFile");
    add_file_row("Tension Stiffening CSV", "tension_stiffening_file",
                 "cdpTensionStiffeningFile");
    add_file_row("Tension Damage CSV", "tension_damage_file",
                 "cdpTensionDamageFile");
  } else if (kind == "Sections") {
    // Section 当前只有 SolidSection 一种合法类型。只读显示比单选项下拉
    // 更准确，也规避 macOS 上下拉弹层按整行宽度展开的视觉异常。
    auto* section_type = new QLineEdit(form_box_);
    section_type->setObjectName("sectionType");
    section_type->setReadOnly(true);
    form_layout_->addRow("Type", section_type);
    form_widgets_.insert("type", section_type);
    add_combo("Material", "material", materials, "sectionMaterial");
    if (auto* material_combo =
            qobject_cast<QComboBox*>(form_widgets_.value("material"))) {
      // Material 是模型节点引用，只允许从现有材料中选择。按内容给一个
      // 紧凑宽度，避免 QFormLayout 将控件拉满整行后，macOS popup 也随之
      // 横跨整个工作窗。
      material_combo->setEditable(false);
      int content_width = 0;
      for (int i = 0; i < material_combo->count(); ++i) {
        content_width =
            qMax(content_width,
                 material_combo->fontMetrics().horizontalAdvance(
                     material_combo->itemText(i)));
      }
      const int compact_width = qBound(240, content_width + 64, 480);
      material_combo->setMinimumWidth(compact_width);
      material_combo->setMaximumWidth(compact_width);
      material_combo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    auto* assigned_volumes = new QLineEdit(form_box_);
    assigned_volumes->setObjectName("sectionBlock");
    assigned_volumes->setReadOnly(true);
    assigned_volumes->setPlaceholderText(
        "Choose from the available physical volumes below.");
    assigned_volumes->setToolTip(l10n::tr(
        "This value is updated by Apply Selected Volumes below."));
    form_layout_->addRow("Assigned Physical Volumes", assigned_volumes);
    form_widgets_.insert("block", assigned_volumes);
  } else if (kind == "Assembly") {
    const QStringList parts = collect_model_names("Parts");
    add_combo("Part", "part", parts, "assemblyPartCombo");
    if (auto* part_combo =
            qobject_cast<QComboBox*>(form_widgets_.value("part"))) {
      part_combo->setEditable(false);
    }
    add_section("Translation");
    add_line("X", "translate_x", "assemblyTranslateX");
    add_line("Y", "translate_y", "assemblyTranslateY");
    add_line("Z", "translate_z", "assemblyTranslateZ");
    add_section("Rotation (degrees, X → Y → Z)");
    add_line("X", "rotate_x", "assemblyRotateX");
    add_line("Y", "rotate_y", "assemblyRotateY");
    add_line("Z", "rotate_z", "assemblyRotateZ");
    add_section("Scale");
    add_line("X", "scale_x", "assemblyScaleX");
    add_line("Y", "scale_y", "assemblyScaleY");
    add_line("Z", "scale_z", "assemblyScaleZ");
    add_section("Instance");
    add_combo("Visible", "visible", {"true", "false"},
              "assemblyVisible");
    add_line("Order", "order", "assemblyOrder");
  } else if (kind == "Steps") {
    // W-03e：Step→Executioner/TimeStepper/Preconditioning 表单
    // （*Static 四参数语义），分组：基本/求解控制/时间步进/预处理。
    add_section("Basic");
    add_combo("Type", "type", {"Transient", "Steady"}, "stepType");
    add_line("Start Time (s)", "start_time", "stepStartTime");
    add_line("End Time (s)", "end_time", "stepEndTime");
    add_line("num_steps", "num_steps", "stepNumSteps");
    add_section("Solve Control");
    add_combo("solve_type", "solve_type", {"NEWTON", "PJFNK"},
              "stepSolveType");
    add_combo("line_search", "line_search",
              {"bt", "basic", "none", "cp", "l2", "shell", "default"},
              "stepLineSearch");
    add_combo("automatic_scaling", "automatic_scaling", {"true", "false"},
              "stepAutomaticScaling");
    add_line("nl_rel_tol", "nl_rel_tol", "stepNlRelTol");
    add_line("nl_abs_tol", "nl_abs_tol", "stepNlAbsTol");
    add_line("nl_max_its", "nl_max_its", "stepNlMaxIts");
    add_line("petsc_options_iname", "petsc_options_iname",
             "stepPetscOptionsIname");
    add_line("petsc_options_value", "petsc_options_value",
             "stepPetscOptionsValue");
    add_combo("scheme", "scheme", {"implicit-euler", "bdf2"}, "stepScheme");
    add_line("l_max_its", "l_max_its", "stepLMaxIts");
    add_line("l_tol", "l_tol", "stepLTol");
    add_section("Time Stepping");
    add_combo("timestepper_type", "timestepper_type", {"IterationAdaptiveDT"},
              "stepTimeStepperType");
    add_line("dt (s)", "dt", "stepDt");
    add_line("optimal_iterations", "optimal_iterations",
             "stepOptimalIterations");
    add_line("iteration_window", "iteration_window", "stepIterationWindow");
    add_line("growth_factor", "growth_factor", "stepGrowthFactor");
    add_line("cutback_factor", "cutback_factor", "stepCutbackFactor");
    add_line("dtmin (s)", "dtmin", "stepDtMin");
    add_line("dtmax (s)", "dtmax", "stepDtMax");
    add_section("Preconditioning");
    add_combo("preconditioning_type", "preconditioning_type", {"SMP"},
              "stepPreconditioningType");
    add_combo("preconditioning_full", "preconditioning_full",
              {"true", "false"}, "stepPreconditioningFull");
  } else if (kind == "Functions") {
    // W-03c：ParsedFunction（expression）/ PiecewiseLinear（x/y 数据对，
    // 空格分隔，个数需一致）。
    add_combo("Type", "type", {"ParsedFunction", "PiecewiseLinear"},
              "functionTypeCombo");
    add_line("Expression", "expression", "functionExpression");
    add_line("X Values", "x", "functionXValues");
    add_line("Y Values", "y", "functionYValues");
  } else if (kind == "BC") {
    // W-03c：variable 候选 = Variables 节点 ∪ 常见位移变量名；
    // boundary 由下方物理面组列表指派，上方字段只读回显；
    // FunctionDirichletBC 的 function 下拉引用 Functions 节点子项名称。
    QStringList variable_candidates = variables;
    for (const auto& candidate : {"disp_x", "disp_y", "disp_z", "u", "v"}) {
      if (!variable_candidates.contains(QLatin1String(candidate))) {
        variable_candidates << QLatin1String(candidate);
      }
    }
    add_combo("Type", "type", {"DirichletBC", "FunctionDirichletBC",
                               "NeumannBC"},
              "bcTypeCombo");
    add_combo("Variable", "variable", variable_candidates, "bcVariableCombo");
    add_line("Assigned Boundaries", "boundary", "bcBoundaryEdit");
    if (auto* boundary_edit =
            qobject_cast<QLineEdit*>(form_widgets_.value("boundary"))) {
      boundary_edit->setReadOnly(true);
      boundary_edit->setPlaceholderText(
          "Choose from the available boundary groups below.");
      boundary_edit->setToolTip(l10n::tr(
          "This value is updated by Apply Selected Boundaries below."));
    }
    add_line("Value", "value", "bcValueEdit");
    add_combo("Function", "function", functions, "bcFunctionCombo");
  } else if (kind == "Loads") {
    add_combo("Type", "type", load_type_options_, "loadTypeCombo");
    if (auto* type_combo =
            qobject_cast<QComboBox*>(form_widgets_.value("type"))) {
      type_combo->setEditable(false);
    }
    const bool pressure = current_item_ &&
        current_item_->data(0, kParamsRole).toMap().value("type").toString() ==
            "Pressure";
    add_combo("Variable", "variable",
              pressure ? pressure_variable_candidates() : variables,
              "loadVariableCombo");
    add_line("Assigned Boundaries", "boundary", "loadBoundaryEdit");
    if (auto* boundary_edit =
            qobject_cast<QLineEdit*>(form_widgets_.value("boundary"))) {
      boundary_edit->setReadOnly(true);
      boundary_edit->setPlaceholderText(
          "Choose from the available boundary groups below.");
    }
    add_line("Value", "value", "loadValueEdit");
    add_line("Factor", "factor", "loadFactorEdit");
    add_combo("Function", "function", functions, "loadFunctionCombo");
    add_line("Component", "component", "loadComponentEdit");
    add_combo("Use Displaced Mesh", "use_displaced_mesh",
              {"true", "false"}, "loadUseDisplacedMeshCombo");
    add_line("Diffusivity", "diffusivity", "loadDiffusivityEdit");
    add_line("Displacements", "displacements", "loadDisplacementsEdit");
  } else if (kind == "Interactions") {
    add_combo("Type", "type", interaction_type_options_,
              "interactionTypeCombo");
    if (auto* type_combo =
            qobject_cast<QComboBox*>(form_widgets_.value("type"))) {
      type_combo->setEditable(false);
    }
    add_combo("Model", "model", {"frictionless", "coulomb", "glued"},
              "interactionModelCombo");
    add_combo("Formulation", "formulation",
              {"kinematic", "penalty", "augmented_lagrange",
               "tangential_penalty", "mortar"},
              "interactionFormulationCombo");
    add_combo("Primary Surface", "primary", boundary_groups_,
              "interactionPrimaryCombo");
    add_combo("Secondary Surface", "secondary", boundary_groups_,
              "interactionSecondaryCombo");
    for (const auto& key : {"primary", "secondary"}) {
      if (auto* combo =
              qobject_cast<QComboBox*>(form_widgets_.value(key))) {
        combo->setEditable(false);
      }
    }
    add_line("Friction Coefficient", "friction_coefficient",
             "interactionFrictionCoefficient");
    add_line("Normal Smoothing Distance", "normal_smoothing_distance",
             "interactionNormalSmoothingDistance");
    add_line("Tangential Tolerance", "tangential_tolerance",
             "interactionTangentialTolerance");
    add_line("Penalty", "penalty", "interactionPenalty");
    add_combo("Normalize Penalty", "normalize_penalty", {"true", "false"},
              "interactionNormalizePenalty");
  } else if (kind == "Physics") {
    // W-03b：Physics action 快捷表单（v01 QuasiStatic 口径）。
    // block 行编辑 + 体组 chips（groups_box_）；CDPQuasiStatic 候选仅在
    // 档案 extra.physics_action 声明时由 MainWindow 注入。
    add_section("Physics Action");
    add_combo("Action", "action", physics_action_options_,
              "physicsActionCombo");
    add_line("Block", "block", "physicsBlockEdit");
    add_combo("Strain", "strain", {"SMALL", "FINITE"}, "physicsStrainCombo");
    add_combo("volumetric_locking_correction", "volumetric_locking_correction",
              {"true", "false"}, "physicsVolumetricLocking");
    add_combo("incremental", "incremental", {"true", "false"},
              "physicsIncremental");
    add_combo("add_variables", "add_variables", {"true", "false"},
              "physicsAddVariables");
    add_line("generate_output", "generate_output", "physicsGenerateOutput");
    add_combo("save_in_resid", "save_in_resid", {"true", "false"},
              "physicsSaveInResid");
  } else if (kind == "Outputs") {
    // W-03d：场/历史输出套餐快捷表单。场输出变量多选（8 个 CDP 诊断量
    // checkbox 组）；历史输出套餐勾选（反力/平均位移/极值，面组 chips
    // 写入 hist_boundary）；Times + Exodus/CSV 落盘开关。
    add_section("Field Output");
    add_checkbox_group("Field Variables", "field_outputs",
                       kCdpFieldOutputVariables, "outputsFieldOutputs");
    add_section("History Output");
    add_combo("History Preset", "history_profile",
              {"custom", "cdp_uniaxial_z"}, "outputsHistoryProfile");
    add_combo("Reaction Force", "hist_reaction_force", {"false", "true"},
              "outputsHistReactionForce");
    add_combo("Displacement Avg", "hist_displacement_avg", {"false", "true"},
              "outputsHistDisplacementAvg");
    add_combo("Extremum", "hist_extremum", {"false", "true"},
              "outputsHistExtremum");
    add_line("History Boundary", "hist_boundary", "outputsHistBoundary");
    add_line("Disp Variable", "hist_disp_variable", "outputsHistDispVariable");
    add_line("Extremum Variables", "hist_extremum_variables",
             "outputsHistExtremumVars");
    add_line("Extremum Types", "hist_extremum_types",
             "outputsHistExtremumTypes");
    add_section("Times");
    add_combo("Enable Times", "times_enabled", {"false", "true"},
              "outputsTimesEnabled");
    add_line("Times Name", "times_name", "outputsTimesName");
    add_line("start_time", "times_start", "outputsTimesStart");
    add_line("end_time", "times_end", "outputsTimesEnd");
    add_line("time_interval", "times_interval", "outputsTimesInterval");
    add_section("Output Files");
    add_combo("Exodus", "output_exodus", {"true", "false"},
              "outputsExodusEnabled");
    add_combo("CSV", "output_csv", {"true", "false"}, "outputsCsvEnabled");
    add_line("file_base", "file_base", "outputsFileBase");
  }

  const QString default_var = variables.isEmpty() ? "u" : variables.first();
  const QString default_func =
      functions.isEmpty() ? "func_1" : functions.first();
  const QString default_bnd =
      boundary_groups_.isEmpty() ? "left" : boundary_groups_.first();
  const QString default_block =
      volume_groups_.isEmpty() ? "block_1" : volume_groups_.first();

  if (kind == "Materials") {
    template_presets_.insert(
        "Generic Constant (k=1.0)",
        {{"type", "GenericConstantMaterial"},
         {"prop_names", "thermal_conductivity"},
         {"prop_values", "1.0"}});
    template_descriptions_.insert(
        "Generic Constant (k=1.0)",
        "Constant conductivity material (k = 1.0).");
    template_presets_.insert(
        "Parsed Conductivity k(T)",
        {{"type", "ParsedMaterial"},
         {"property_name", "thermal_conductivity"},
         {"expression", "1 + 0.01*T"},
         {"coupled_variables", "T"}});
    template_descriptions_.insert(
        "Parsed Conductivity k(T)",
        "Temperature-dependent conductivity k(T) = 1 + 0.01*T.");
    template_presets_.insert(
        "Linear Elastic (isotropic)",
        {{"type", "ComputeElasticityTensor"},
         {"fill_method", "symmetric_isotropic"},
         {"C_ijkl", "2.1e5 0.8e5"}});
    template_descriptions_.insert(
        "Linear Elastic (isotropic)",
        "Isotropic linear elastic tensor with sample C_ijkl.");
    template_presets_.insert(
        "Thermal Expansion",
        {{"type", "ComputeThermalExpansionEigenstrain"},
         {"thermal_expansion_coeff", "1e-5"},
         {"temperature", "T"},
         {"stress_free_temperature", "300"},
         {"eigenstrain_name", "eigenstrain"}});
    template_descriptions_.insert(
        "Thermal Expansion",
        "Thermal expansion eigenstrain with reference temperature 300.");
    template_presets_.insert(
        "CDP Concrete (Abaqus)",
        {{"type", "AbaqusCDP"},
         {"youngs_modulus", "29791500000"},
         {"poissons_ratio", "0.2"},
         {"dilation_angle", "36"},
         {"eccentricity", "0.1"},
         {"biaxial_to_uniaxial_compression_ratio", "1.16"},
         {"tensile_meridian_ratio", "0.667"},
         {"viscosity", "5e-4"},
         {"tension_recovery", "0"},
         {"compression_recovery", "1"},
         {"maximum_substeps", "256"},
         {"maximum_strain_increment", "2.5e-5"},
         {"enable_performance_diagnostics", "true"},
         {"unit_factor_stress", "1000000"}});
    template_descriptions_.insert(
        "CDP Concrete (Abaqus)",
        "Abaqus CDP concrete: generates ComputeIsotropicElasticityTensor + "
        "ComputeMultipleInelasticStress + AbaqusCDPStressUpdate; E is "
        "entered in MPa and stored as SI (Pa); pick the 4 CSV curves.");
  } else if (kind == "BC") {
    template_presets_.insert(
        "Fixed (Dirichlet 0)",
        {{"type", "DirichletBC"},
         {"variable", default_var},
         {"boundary", default_bnd},
         {"value", "0"}});
    template_descriptions_.insert(
        "Fixed (Dirichlet 0)",
        "Dirichlet BC fixing variable to 0 on boundary.");
    template_presets_.insert(
        "Prescribed Function",
        {{"type", "FunctionDirichletBC"},
         {"variable", default_var},
         {"boundary", default_bnd},
         {"function", default_func}});
    template_descriptions_.insert(
        "Prescribed Function",
        "Function-based Dirichlet boundary condition.");
    template_presets_.insert(
        "Neumann (traction)",
        {{"type", "NeumannBC"},
         {"variable", default_var},
         {"boundary", default_bnd},
         {"value", "1.0"}});
    template_descriptions_.insert(
        "Neumann (traction)",
        "Neumann traction/flux boundary condition.");
  } else if (kind == "Loads") {
    template_presets_.insert(
        "Body Force",
        {{"type", "BodyForce"},
         {"variable", default_var},
         {"value", "1.0"}});
    template_descriptions_.insert(
        "Body Force",
        "Constant body force on variable.");
    template_presets_.insert(
        "Body Force (Function)",
        {{"type", "BodyForce"},
         {"variable", default_var},
         {"function", default_func}});
    template_descriptions_.insert(
        "Body Force (Function)",
        "Function-driven body force.");
    template_presets_.insert(
        "MatDiffusion",
        {{"type", "MatDiffusion"},
         {"variable", default_var},
         {"diffusivity", "diff_u"}});
    template_descriptions_.insert(
        "MatDiffusion",
        "Material diffusion term using diffusivity property.");
    template_presets_.insert(
        "TensorMechanics",
        {{"type", "TensorMechanics"},
         {"displacements", "disp_x disp_y"},
         {"block", default_block}});
    template_descriptions_.insert(
        "TensorMechanics",
        "Tensor mechanics kernel using displacement variables.");
    if (load_type_options_.contains("Pressure")) {
      QVariantMap pressure_template{{"type", "Pressure"},
                                    {"variable", "disp_z"},
                                    {"boundary", default_bnd},
                                    {"factor", "1.0"},
                                    {"component", "2"},
                                    {"use_displaced_mesh", "true"}};
      if (!functions.isEmpty()) {
        pressure_template.insert("function", default_func);
      }
      template_presets_.insert("Surface Pressure", pressure_template);
      template_descriptions_.insert(
          "Surface Pressure",
          "Pressure BC using a named surface Physical Group and optional "
          "load function.");
    }
  }

  if (template_combo_) {
    for (const auto& key : template_presets_.keys()) {
      template_combo_->addItem(key, key);
    }
  }

  if (template_combo_ && !template_tabs_) {
    template_tabs_ = new QTabWidget(form_box_);
    template_preview_ = new QPlainTextEdit(template_tabs_);
    template_preview_->setReadOnly(true);
    template_tabs_->addTab(template_preview_, "Preview");
    // 模板说明只是只读的模板描述预览，限制高度避免喧宾夺主。
    template_tabs_->setMaximumHeight(72);
    form_layout_->addRow("Template Info", template_tabs_);
  }
  if (template_combo_) {
    connect(template_combo_, &QComboBox::currentIndexChanged, this,
            [this](int index) {
              if (!template_preview_) {
                return;
              }
              QString key = template_combo_->itemData(index).toString();
              if (key.isEmpty()) {
                key = template_combo_->itemText(index);
              }
              if (key == "Type Defaults") {
                template_preview_->setPlainText(
                    l10n::tr("Applies defaults for the selected type."));
                return;
              }
              const QString desc =
                  template_descriptions_.value(key, "No description.");
              template_preview_->setPlainText(l10n::tr(desc));
            });
    if (template_preview_) {
      template_preview_->setPlainText(
          l10n::tr("Applies defaults for the selected type."));
    }
  }

  auto set_row_visible = [this](const QString& key, bool visible) {
    if (!form_widgets_.contains(key)) {
      return;
    }
    QWidget* field = form_widgets_.value(key);
    if (!field) {
      return;
    }
    QWidget* label = form_layout_->labelForField(field);
    if (label) {
      label->setVisible(visible);
    }
    field->setVisible(visible);
  };

  auto update_visibility = [this, kind, set_row_visible]() {
    QString type;
    if (form_widgets_.contains("type")) {
      if (auto* combo =
              qobject_cast<QComboBox*>(form_widgets_.value("type"))) {
        type = combo->currentText().trimmed();
      } else if (auto* edit =
                     qobject_cast<QLineEdit*>(form_widgets_.value("type"))) {
        type = edit->text().trimmed();
      }
    }
    if (kind == "Materials") {
      set_row_visible("prop_names", true);
      set_row_visible("prop_values", true);
      set_row_visible("expression", true);
      set_row_visible("property_name", true);
      set_row_visible("coupled_variables", true);
      set_row_visible("fill_method", false);
      set_row_visible("C_ijkl", false);
      set_row_visible("block", false);
      set_row_visible("thermal_expansion_coeff", false);
      set_row_visible("temperature", false);
      set_row_visible("stress_free_temperature", false);
      set_row_visible("eigenstrain_name", false);
      set_row_visible("displacements", false);
      // 普通各向同性弹性和 CDP 共用 E/nu；CDP 其余字段只在 CDP 显示。
      const QStringList cdp_keys = {
          "youngs_modulus",
          "poissons_ratio",
          "dilation_angle",
          "eccentricity",
          "biaxial_to_uniaxial_compression_ratio",
          "tensile_meridian_ratio",
          "viscosity",
          "tension_recovery",
          "compression_recovery",
          "maximum_substeps",
          "compression_hardening_file",
          "compression_damage_file",
          "tension_stiffening_file",
          "tension_damage_file"};
      const bool is_cdp = (type == "AbaqusCDP");
      for (const auto& cdp_key : cdp_keys) {
        set_row_visible(cdp_key,
                        is_cdp || (type == "ComputeIsotropicElasticityTensor" &&
                                   (cdp_key == "youngs_modulus" ||
                                    cdp_key == "poissons_ratio")));
      }
      if (is_cdp) {
        set_row_visible("prop_names", false);
        set_row_visible("prop_values", false);
        set_row_visible("expression", false);
        set_row_visible("property_name", false);
        set_row_visible("coupled_variables", false);
      } else if (type == "GenericConstantMaterial") {
        set_row_visible("expression", false);
        set_row_visible("property_name", false);
        set_row_visible("coupled_variables", false);
      } else if (type == "ParsedMaterial") {
        set_row_visible("prop_names", false);
        set_row_visible("prop_values", false);
      } else if (type == "ComputeElasticityTensor") {
        set_row_visible("prop_names", false);
        set_row_visible("prop_values", false);
        set_row_visible("expression", false);
        set_row_visible("property_name", false);
        set_row_visible("coupled_variables", false);
        set_row_visible("fill_method", true);
        set_row_visible("C_ijkl", true);
      } else if (type == "ComputeIsotropicElasticityTensor" ||
                 type == "ComputeLinearElasticStress") {
        set_row_visible("prop_names", false);
        set_row_visible("prop_values", false);
        set_row_visible("expression", false);
        set_row_visible("property_name", false);
        set_row_visible("coupled_variables", false);
        set_row_visible("block", true);
      } else if (type == "ComputeSmallStrain") {
        set_row_visible("prop_names", false);
        set_row_visible("prop_values", false);
        set_row_visible("expression", false);
        set_row_visible("property_name", false);
        set_row_visible("coupled_variables", false);
        set_row_visible("displacements", true);
      } else if (type == "ComputeThermalExpansionEigenstrain") {
        set_row_visible("prop_names", false);
        set_row_visible("prop_values", false);
        set_row_visible("expression", false);
        set_row_visible("property_name", false);
        set_row_visible("coupled_variables", false);
        set_row_visible("thermal_expansion_coeff", true);
        set_row_visible("temperature", true);
        set_row_visible("stress_free_temperature", true);
        set_row_visible("eigenstrain_name", true);
      }
    } else if (kind == "Steps") {
      // W-03e：时间相关字段仅 Transient 显示；求解控制/预处理通用。
      const bool is_transient = (type != "Steady");
      const QStringList transient_keys = {
          "start_time",     "end_time",      "num_steps",
          "timestepper_type", "dt",          "optimal_iterations",
          "iteration_window", "growth_factor", "cutback_factor",
          "dtmin",          "dtmax"};
      for (const auto& key : transient_keys) {
        set_row_visible(key, is_transient);
      }
    } else if (kind == "Functions") {
      // W-03c：ParsedFunction ↔ expression；PiecewiseLinear ↔ x/y 数据对。
      const bool piecewise = (type == "PiecewiseLinear");
      set_row_visible("expression", !piecewise);
      set_row_visible("x", piecewise);
      set_row_visible("y", piecewise);
    } else if (kind == "BC") {
      const bool use_function = (type == "FunctionDirichletBC");
      set_row_visible("function", use_function);
      set_row_visible("value", !use_function);
    } else if (kind == "Loads") {
      const bool pressure = (type == "Pressure");
      set_row_visible("boundary", pressure);
      set_row_visible("factor", pressure);
      set_row_visible("component", pressure);
      set_row_visible("use_displaced_mesh", pressure);
      set_row_visible("diffusivity", type == "MatDiffusion");
      set_row_visible("displacements", type == "TensorMechanics");
      set_row_visible("value", type == "BodyForce");
      set_row_visible("function", type == "BodyForce" || pressure);
    } else if (kind == "Interactions") {
      const bool contact = (type == "Contact");
      for (const auto& key : {"model", "formulation", "primary",
                              "secondary", "normal_smoothing_distance",
                              "tangential_tolerance", "penalty",
                              "normalize_penalty"}) {
        set_row_visible(key, contact);
      }
      QString model;
      if (auto* combo =
              qobject_cast<QComboBox*>(form_widgets_.value("model"))) {
        model = combo->currentText();
      }
      set_row_visible("friction_coefficient", contact && model == "coulomb");
    }
  };

  auto apply_defaults = [this, kind](const QString& type) {
    if (kind == "Loads" && type == "Pressure" && current_item_) {
      // A BodyForce variable is not a meaningful Pressure default. On an
      // explicit type switch use the component-2 displacement convention;
      // subsequent user edits are left untouched.
      apply_template_values({{"variable", "disp_z"}}, true);
    }
    apply_template_values(build_type_template(kind, type), false);
  };

  const QVariantMap params =
      current_item_ ? current_item_->data(0, kParamsRole).toMap()
                    : QVariantMap();
  form_updating_ = true;
  for (auto it = form_widgets_.begin(); it != form_widgets_.end(); ++it) {
    const QString key = it.key();
    QString value = params.value(key).toString();
    if (key == "youngs_modulus" && kind == "Materials" &&
        (params.value("type").toString() == "AbaqusCDP" ||
         params.value("type").toString() ==
             "ComputeIsotropicElasticityTensor")) {
      // 存储值（SI，Pa）→ 表单显示值（MPa）；换算比例优先取 params 记录的
      // unit_factor_stress，缺省回落到当前档案单位合同/1e6。
      bool stored_ok = false;
      const double stored = value.trimmed().toDouble(&stored_ok);
      if (stored_ok) {
        bool factor_ok = false;
        double factor = params.value("unit_factor_stress")
                            .toString()
                            .toDouble(&factor_ok);
        if (!factor_ok || factor <= 0.0) {
          factor = display_unit_factor("pressure", 1e6);
        }
        value = gmp::format_unit_display_value(stored, factor);
      }
    }
    if (auto* edit = qobject_cast<QLineEdit*>(it.value())) {
      edit->setText(value);
    } else if (auto* combo = qobject_cast<QComboBox*>(it.value())) {
      const int idx = combo->findText(value);
      if (idx >= 0) {
        combo->setCurrentIndex(idx);
      } else if (!value.isEmpty()) {
        combo->addItem(value);
        combo->setCurrentText(value);
      } else {
        combo->setCurrentIndex(0);
      }
    } else if (it.value()) {
      // 文件选择行：容器内第一个 QLineEdit 承载参数值。
      if (auto* edit = it.value()->findChild<QLineEdit*>()) {
        edit->setText(value);
      } else {
        // W-03d checkbox 多选组：按空格分隔值回显选中态。
        const QStringList tokens = value.split(QRegularExpression("\\s+"),
                                               Qt::SkipEmptyParts);
        for (auto* box : it.value()->findChildren<QCheckBox*>()) {
          box->setChecked(tokens.contains(box->text()));
        }
      }
    }
  }
  form_updating_ = false;

  if (form_widgets_.contains("type")) {
    if (auto* combo =
            qobject_cast<QComboBox*>(form_widgets_.value("type"))) {
      connect(combo, &QComboBox::currentTextChanged, this,
              [this, kind, update_visibility,
               apply_defaults](const QString& value) {
                update_visibility();
                apply_defaults(value.trimmed());
                if (kind == "Loads") {
                  update_group_widget_for_kind(kind);
                }
              });
    }
  }
  if (kind == "Interactions") {
    if (auto* combo =
            qobject_cast<QComboBox*>(form_widgets_.value("model"))) {
      connect(combo, &QComboBox::currentTextChanged, this,
              [update_visibility](const QString&) { update_visibility(); });
    }
  }
  update_visibility();

  // 表单会在类型切换、应用模板和应用分组后动态重建；新建控件必须立即
  // 使用当前语言，否则弹窗会从中文退回英文。
  const bool was_updating = form_updating_;
  form_updating_ = true;
  l10n::apply(form_box_);
  form_updating_ = was_updating;
}

void PropertyEditor::set_param_value(const QString& key,
                                     const QString& value) {
  if (form_updating_ || !current_item_ || is_root_item()) {
    return;
  }
  if (!params_table_) {
    return;
  }
  params_table_->blockSignals(true);
  int target_row = -1;
  for (int row = 0; row < params_table_->rowCount(); ++row) {
    auto* key_item = params_table_->item(row, 0);
    if (key_item && key_item->text().trimmed() == key) {
      target_row = row;
      break;
    }
  }
  if (target_row < 0) {
    target_row = params_table_->rowCount();
    params_table_->insertRow(target_row);
    params_table_->setItem(target_row, 0, new QTableWidgetItem(key));
  }
  QTableWidgetItem* val_item = params_table_->item(target_row, 1);
  if (!val_item) {
    val_item = new QTableWidgetItem();
    params_table_->setItem(target_row, 1, val_item);
  }
  val_item->setText(value);
  const QString unit_tooltip = unit_tooltip_for_param(key, value);
  val_item->setToolTip(unit_tooltip);
  for (int row = params_table_->rowCount() - 1; row >= 0; --row) {
    if (row == target_row) {
      continue;
    }
    auto* key_item = params_table_->item(row, 0);
    if (key_item && key_item->text().trimmed() == key) {
      params_table_->removeRow(row);
      if (row < target_row) {
        target_row--;
      }
    }
  }
  params_table_->blockSignals(false);
  save_params_to_item();
  update_validation();
  refresh_preview();
}

void PropertyEditor::clear_form() {
  if (!form_layout_) {
    return;
  }
  form_widgets_.clear();
  template_presets_.clear();
  template_combo_ = nullptr;
  apply_template_btn_ = nullptr;
  // template_tabs_ / template_preview_ 曾作为 form_layout_ 的行被添加，
  // 下面的 deleteLater 会销毁它们，必须同步置空，否则下次
  // build_form_for_kind 会踩着悬垂指针调用 setPlainText 崩溃。
  template_tabs_ = nullptr;
  template_preview_ = nullptr;
  while (form_layout_->count() > 0) {
    QLayoutItem* item = form_layout_->takeAt(0);
    if (!item) {
      break;
    }
    if (auto* widget = item->widget()) {
      widget->deleteLater();
    }
    delete item;
  }
  if (form_box_) {
    form_box_->setVisible(false);
  }
}

}  // namespace gmp
