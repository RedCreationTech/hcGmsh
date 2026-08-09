#include "gmp/PropertyEditor.h"

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
#include <QDialog>
#include <QPlainTextEdit>
#include <QFont>

#include "gmp/ComboPopupFix.h"

namespace gmp {

PropertyEditor::PropertyEditor(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  header_label_ = new QLabel("No Selection", this);
  header_label_->setStyleSheet("font-weight: 600; padding: 2px 0;");
  layout->addWidget(header_label_);

  tabs_ = new QTabWidget(this);
  layout->addWidget(tabs_, 1);

  general_tab_ = new QWidget(this);
  auto* general_layout = new QFormLayout(general_tab_);
  kind_label_ = new QLabel("-", general_tab_);
  status_label_ = new QLabel("-", general_tab_);
  name_edit_ = new QLineEdit(general_tab_);
  general_layout->addRow("Kind", kind_label_);
  general_layout->addRow("Status", status_label_);
  general_layout->addRow("Name", name_edit_);
  tabs_->addTab(general_tab_, "General");

  params_tab_ = new QWidget(this);
  auto* params_layout = new QVBoxLayout(params_tab_);

  form_box_ = new QGroupBox("Quick Parameters", params_tab_);
  form_layout_ = new QFormLayout(form_box_);
  params_layout->addWidget(form_box_);

  groups_box_ = new QGroupBox("Groups", params_tab_);
  auto* groups_layout = new QVBoxLayout(groups_box_);
  groups_hint_ = new QLabel("Select physical groups to apply.", groups_box_);
  groups_hint_->setStyleSheet("color: #444;");
  groups_list_ = new QListWidget(groups_box_);
  groups_list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  groups_list_->setMaximumHeight(120);
  groups_summary_ = new QLabel("Selected:", groups_box_);
  groups_summary_->setStyleSheet("color: #333;");
  groups_chips_container_ = new QWidget(groups_box_);
  groups_chips_layout_ = new QHBoxLayout(groups_chips_container_);
  groups_chips_layout_->setContentsMargins(0, 0, 0, 0);
  groups_chips_layout_->setSpacing(6);
  apply_groups_btn_ = new QPushButton("Apply Groups", groups_box_);
  groups_layout->addWidget(groups_hint_);
  groups_layout->addWidget(groups_list_, 1);
  groups_layout->addWidget(groups_summary_);
  groups_layout->addWidget(groups_chips_container_);
  groups_layout->addWidget(apply_groups_btn_);
  params_layout->addWidget(groups_box_);

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
  remove_param_btn_ = new QPushButton("Remove Param", params_container_);
  buttons->addWidget(add_param_btn_);
  buttons->addWidget(remove_param_btn_);
  buttons->addStretch(1);
  params_container_layout->addWidget(params_buttons_container_);
  params_layout->addWidget(params_container_);

  validation_label_ = new QLabel(params_tab_);
  validation_label_->setStyleSheet("color: #b00020;");
  validation_label_->setWordWrap(true);
  params_layout->addWidget(validation_label_);

  validation_box_ = new QGroupBox("Validation Summary", params_tab_);
  auto* validation_layout = new QVBoxLayout(validation_box_);
  validation_summary_label_ = new QLabel("No issues.", validation_box_);
  validation_summary_label_->setStyleSheet("font-weight: 600;");
  validation_layout->addWidget(validation_summary_label_);
  validation_table_ = new QTableWidget(validation_box_);
  validation_table_->setColumnCount(2);
  validation_table_->setHorizontalHeaderLabels({"Node", "Issues"});
  validation_table_->horizontalHeader()->setStretchLastSection(true);
  validation_table_->verticalHeader()->setVisible(false);
  validation_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  validation_table_->setSelectionMode(QAbstractItemView::SingleSelection);
  validation_layout->addWidget(validation_table_, 1);
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
  validation_goto_btn_ = new QPushButton("Go To Node", validation_box_);
  validation_actions->addWidget(validation_refresh_btn_);
  validation_actions->addWidget(validation_goto_btn_);
  validation_actions->addStretch(1);
  validation_layout->addLayout(validation_actions);
  params_layout->addWidget(validation_box_);
  tabs_->addTab(params_tab_, "Parameters");

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
  preview_layout->addWidget(preview_text_, 1);
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

  // 右侧边栏宽度有限: 表单标签换行到控件上方(垂直布局), 减少横向溢出
  for (auto* f : findChildren<QFormLayout*>()) {
    f->setRowWrapPolicy(QFormLayout::WrapAllRows);
  }

  set_item(nullptr);
}

void PropertyEditor::set_item(QTreeWidgetItem* item) {
  current_item_ = item;
  load_from_item();
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
  int row = 0;
  for (auto it = params.begin(); it != params.end(); ++it) {
    params_table_->insertRow(row);
    params_table_->setItem(row, 0, new QTableWidgetItem(it.key()));
    params_table_->setItem(row, 1,
                           new QTableWidgetItem(it.value().toString()));
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
  current_item_->setText(0, value);
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
  refresh_preview();
  if (row >= 0 && (!sync_mode_ || sync_mode_->currentIndex() == 0)) {
    auto* key_item = params_table_->item(row, 0);
    auto* val_item = params_table_->item(row, 1);
    if (key_item && val_item) {
      const QString key = key_item->text().trimmed();
      if (form_widgets_.contains(key)) {
        form_updating_ = true;
        const QString value = val_item->text();
        if (auto* edit = qobject_cast<QLineEdit*>(form_widgets_[key])) {
          edit->setText(value);
        } else if (auto* combo = qobject_cast<QComboBox*>(form_widgets_[key])) {
          const int idx = combo->findText(value);
          if (idx >= 0) {
            combo->setCurrentIndex(idx);
          } else if (!value.isEmpty()) {
            combo->addItem(value);
            combo->setCurrentText(value);
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
  current_item_->setData(0, kParamsRole, params);
  refresh_preview();
}

void PropertyEditor::on_apply_groups() {
  if (!current_item_ || is_root_item() || !groups_list_) {
    return;
  }
  const QString kind =
      current_item_->data(0, kKindRole).toString().isEmpty()
          ? current_item_->text(0)
          : current_item_->data(0, kKindRole).toString();
  if (kind != "BC" && kind != "Loads") {
    return;
  }
  QStringList selected;
  const auto items = groups_list_->selectedItems();
  for (const auto* item : items) {
    if (item) {
      selected << item->text();
    }
  }
  if (selected.isEmpty()) {
    return;
  }
  QVariantMap params = current_item_->data(0, kParamsRole).toMap();
  if (kind == "BC") {
    params.insert("boundary", selected.join(" "));
  } else {
    params.insert("block", selected.join(" "));
  }
  current_item_->setData(0, kParamsRole, params);
  load_from_item();
  refresh_preview();
}

void PropertyEditor::update_group_widget_for_kind(const QString& kind) {
  if (!groups_box_ || !groups_list_) {
    return;
  }
  if (kind != "BC" && kind != "Loads") {
    groups_box_->setVisible(false);
    return;
  }
  groups_box_->setVisible(true);
  groups_list_->clear();
  QStringList source =
      kind == "BC" ? boundary_groups_ : volume_groups_;
  groups_list_->addItems(source);
  if (groups_hint_) {
    groups_hint_->setText(kind == "BC"
                              ? "Apply selection to boundary."
                              : "Apply selection to block.");
  }
  if (groups_box_) {
    groups_box_->setTitle(kind == "BC" ? "Boundary Groups" : "Volume Groups");
  }
  const QVariantMap params =
      current_item_ ? current_item_->data(0, kParamsRole).toMap()
                    : QVariantMap();
  const QString key = kind == "BC" ? "boundary" : "block";
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
  groups_summary_->setText("Selected:");

  while (QLayoutItem* item = groups_chips_layout_->takeAt(0)) {
    if (auto* widget = item->widget()) {
      widget->deleteLater();
    }
    delete item;
  }

  if (selected.isEmpty()) {
    auto* none = new QLabel("(none)", groups_chips_container_);
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
    } else if (type == "ComputeSmallStrain") {
      require_key("displacements");
    } else if (type == "ComputeThermalExpansionEigenstrain") {
      require_key("thermal_expansion_coeff");
      require_key("temperature");
    }
  } else if (kind == "Sections") {
    require_key("type");
    require_key("material");
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
    }
  }
  return missing;
}

QStringList PropertyEditor::collect_model_names(const QString& root_name) const {
  QStringList names;
  if (!current_item_) {
    return names;
  }
  auto* tree = current_item_->treeWidget();
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
    }
  } else if (kind == "Sections") {
    t.insert("type", type.isEmpty() ? "SolidSection" : type);
    t.insert("material", mat);
  } else if (kind == "Steps") {
    if (type == "Transient") {
      t.insert("dt", "0.1");
      t.insert("end_time", "1.0");
      t.insert("scheme", "bdf2");
      t.insert("solve_type", "NEWTON");
    } else if (type == "Steady") {
      t.insert("solve_type", "NEWTON");
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
      t.insert("variable", var);
    }
    if (type == "BodyForce") {
      t.insert("value", "1.0");
    } else if (type == "MatDiffusion") {
      t.insert("diffusivity", "diff_u");
    }
  }
  return t;
}

void PropertyEditor::apply_template_values(const QVariantMap& values,
                                           bool overwrite) {
  if (!current_item_) {
    return;
  }
  const QVariantMap params = current_item_->data(0, kParamsRole).toMap();
  for (auto it = values.begin(); it != values.end(); ++it) {
    if (!overwrite && !params.value(it.key()).toString().trimmed().isEmpty()) {
      continue;
    }
    set_param_value(it.key(), it.value().toString());
  }
  update_validation();
}

void PropertyEditor::on_apply_template() {
  if (!current_item_ || !template_combo_) {
    return;
  }
  const QString kind =
      current_item_->data(0, kKindRole).toString().isEmpty()
          ? current_item_->text(0)
          : current_item_->data(0, kKindRole).toString();
  const QString choice = template_combo_->currentText();
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
  update_group_widget_for_kind(kind);
  update_validation();
  refresh_preview();
}

void PropertyEditor::refresh_preview() {
  if (!preview_text_ || !preview_summary_label_) {
    return;
  }
  if (!current_item_) {
    preview_summary_label_->setText("Selected item preview:");
    preview_text_->setPlainText("Select a node in the model tree.");
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
      QString("Preview: %1 (%2)").arg(current_item_->text(0), kind));
  preview_text_->setPlainText(lines.join("\n"));
}

void PropertyEditor::build_form_for_kind(const QString& kind) {
  clear_form();
  if (!form_box_ || !form_layout_) {
    return;
  }
  const QSet<QString> supported = {"Materials", "Sections", "Steps", "BC",
                                   "Loads"};
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

  auto* template_row = new QWidget(form_box_);
  auto* template_layout = new QHBoxLayout(template_row);
  template_layout->setContentsMargins(0, 0, 0, 0);
  template_combo_ = new QComboBox(template_row);
  install_combo_popup_fix(template_combo_);
  template_combo_->addItem("Type Defaults");
  apply_template_btn_ = new QPushButton("Apply Template", template_row);
  template_layout->addWidget(template_combo_);
  template_layout->addWidget(apply_template_btn_);
  template_layout->addStretch(1);
  form_layout_->addRow("Template", template_row);
  connect(apply_template_btn_, &QPushButton::clicked, this,
          &PropertyEditor::on_apply_template);
  template_presets_.clear();
  template_descriptions_.clear();

  auto add_line = [this](const QString& label, const QString& key) {
    auto* edit = new QLineEdit(form_box_);
    form_layout_->addRow(label, edit);
    form_widgets_.insert(key, edit);
    connect(edit, &QLineEdit::textChanged, this,
            [this, key](const QString& value) {
              set_param_value(key, value);
            });
  };

  auto add_combo = [this](const QString& label, const QString& key,
                          const QStringList& items) {
    auto* combo = new QComboBox(form_box_);
    install_combo_popup_fix(combo);
    combo->addItems(items);
    combo->setEditable(true);
    form_layout_->addRow(label, combo);
    form_widgets_.insert(key, combo);
    connect(combo, &QComboBox::currentTextChanged, this,
            [this, key](const QString& value) {
              set_param_value(key, value);
            });
  };

  if (kind == "Materials") {
    add_combo("Type", "type",
              {"GenericConstantMaterial", "ParsedMaterial",
               "ComputeElasticityTensor", "ComputeSmallStrain",
               "ComputeLinearElasticStress",
               "ComputeThermalExpansionEigenstrain"});
    add_line("Prop Names", "prop_names");
    add_line("Prop Values", "prop_values");
    add_line("Expression", "expression");
    add_line("Property Name", "property_name");
    add_line("Coupled Vars", "coupled_variables");
    add_line("fill_method", "fill_method");
    add_line("C_ijkl", "C_ijkl");
    add_line("thermal_expansion_coeff", "thermal_expansion_coeff");
    add_line("temperature", "temperature");
    add_line("stress_free_temperature", "stress_free_temperature");
    add_line("eigenstrain_name", "eigenstrain_name");
    add_line("displacements", "displacements");
  } else if (kind == "Sections") {
    add_combo("Type", "type", {"SolidSection"});
    add_combo("Material", "material", materials);
  } else if (kind == "Steps") {
    add_combo("Type", "type", {"Transient", "Steady"});
    add_line("dt", "dt");
    add_line("end_time", "end_time");
    add_combo("solve_type", "solve_type", {"NEWTON", "PJFNK"});
    add_combo("scheme", "scheme", {"bdf2", "implicit-euler"});
    add_line("nl_max_its", "nl_max_its");
    add_line("l_max_its", "l_max_its");
    add_line("nl_abs_tol", "nl_abs_tol");
    add_line("l_tol", "l_tol");
  } else if (kind == "BC") {
    add_combo("Type", "type", {"DirichletBC", "FunctionDirichletBC",
                               "NeumannBC"});
    add_combo("Variable", "variable", variables);
    add_line("Boundary", "boundary");
    add_line("Value", "value");
    add_combo("Function", "function", functions);
  } else if (kind == "Loads") {
    add_combo("Type", "type",
              {"BodyForce", "TimeDerivative", "MatDiffusion",
               "HeatConduction", "TensorMechanics"});
    add_combo("Variable", "variable", variables);
    add_line("Value", "value");
    add_combo("Function", "function", functions);
    add_line("Diffusivity", "diffusivity");
    add_line("Displacements", "displacements");
  }

  const QString default_var = variables.isEmpty() ? "u" : variables.first();
  const QString default_func =
      functions.isEmpty() ? "func_1" : functions.first();
  const QString default_mat =
      materials.isEmpty() ? "material_1" : materials.first();
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
  } else if (kind == "Sections") {
    template_presets_.insert("Solid Section",
                             {{"type", "SolidSection"},
                              {"material", default_mat}});
    template_descriptions_.insert(
        "Solid Section",
        "Solid section assigning material.");
  }

  if (template_combo_) {
    for (const auto& key : template_presets_.keys()) {
      template_combo_->addItem(key);
    }
  }

  if (!template_tabs_) {
    template_tabs_ = new QTabWidget(form_box_);
    template_preview_ = new QPlainTextEdit(template_tabs_);
    template_preview_->setReadOnly(true);
    template_tabs_->addTab(template_preview_, "Preview");
    form_layout_->addRow("Template Info", template_tabs_);
  }
  if (template_combo_) {
    connect(template_combo_, &QComboBox::currentTextChanged, this,
            [this](const QString& key) {
              if (!template_preview_) {
                return;
              }
              if (key == "Type Defaults") {
                template_preview_->setPlainText(
                    "Applies defaults for the selected type.");
                return;
              }
              const QString desc =
                  template_descriptions_.value(key, "No description.");
              template_preview_->setPlainText(desc);
            });
    if (template_preview_) {
      template_preview_->setPlainText("Applies defaults for the selected type.");
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
      set_row_visible("thermal_expansion_coeff", false);
      set_row_visible("temperature", false);
      set_row_visible("stress_free_temperature", false);
      set_row_visible("eigenstrain_name", false);
      set_row_visible("displacements", false);
      if (type == "GenericConstantMaterial") {
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
      const bool is_transient = (type != "Steady");
      set_row_visible("dt", is_transient);
      set_row_visible("end_time", is_transient);
    } else if (kind == "BC") {
      const bool use_function = (type == "FunctionDirichletBC");
      set_row_visible("function", use_function);
      set_row_visible("value", !use_function);
    } else if (kind == "Loads") {
      set_row_visible("diffusivity", type == "MatDiffusion");
      set_row_visible("displacements", type == "TensorMechanics");
      if (type == "TimeDerivative") {
        set_row_visible("value", false);
        set_row_visible("function", false);
      }
    }
  };

  auto apply_defaults = [this, kind](const QString& type) {
    apply_template_values(build_type_template(kind, type), false);
  };

  const QVariantMap params =
      current_item_ ? current_item_->data(0, kParamsRole).toMap()
                    : QVariantMap();
  form_updating_ = true;
  for (auto it = form_widgets_.begin(); it != form_widgets_.end(); ++it) {
    const QString key = it.key();
    const QString value = params.value(key).toString();
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
    }
  }
  form_updating_ = false;

  if (form_widgets_.contains("type")) {
    if (auto* combo =
            qobject_cast<QComboBox*>(form_widgets_.value("type"))) {
      connect(combo, &QComboBox::currentTextChanged, this,
              [update_visibility, apply_defaults](const QString& value) {
                update_visibility();
                apply_defaults(value.trimmed());
              });
    }
  }
  update_visibility();
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
