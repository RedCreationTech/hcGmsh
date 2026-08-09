#include "gmp/GmshPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include "gmp/ComboPopupFix.h"
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QVBoxLayout>
#include <algorithm>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef GMP_ENABLE_GMSH_GUI
#include <gmsh.h>
#endif

namespace {

void tune_gmsh_combo(QComboBox* combo, int min_width = 80, int view_min_width = 120) {
  if (!combo) {
    return;
  }
  combo->setMinimumWidth(min_width);
  combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  if (combo->view()) {
    combo->view()->setMinimumWidth(view_min_width);
  }
  gmp::install_combo_popup_fix(combo);
}

}  // namespace

namespace gmp {

GmshPanel::GmshPanel(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  auto* scroll = new QScrollArea();
  scroll->setWidgetResizable(true);
  auto* content = new QWidget();
  auto* content_layout = new QVBoxLayout(content);
  auto bind_entity_input_validation = [this](QLineEdit* edit, QComboBox* dim_combo,
                                            bool occ_only) {
    if (!edit) {
      return;
    }
    entity_inputs_.insert(edit);
    edit->installEventFilter(this);
    connect(edit, &QLineEdit::textChanged, this,
            [this, edit, dim_combo, occ_only]() {
              const int dim = dim_combo ? dim_combo->currentData().toInt() : -1;
              validate_entity_input(edit, dim, occ_only);
            });
  };
  auto tune_dim_combo = [](QComboBox* combo) {
    tune_gmsh_combo(combo, 70, 120);
  };

  auto* title = new QLabel("Gmsh Panel");
  content_layout->addWidget(title);

  auto* model_box = new QGroupBox("Model");
  auto* model_form = new QFormLayout(model_box);
  geo_path_ = new QLineEdit();
  geo_path_->setReadOnly(true);
  geo_path_->setPlaceholderText("No geometry loaded");
  auto* open_geo = new QPushButton("Open Geometry");
  connect(open_geo, &QPushButton::clicked, this, &GmshPanel::on_open_geometry);
  auto* clear_geo = new QPushButton("Clear Model");
  connect(clear_geo, &QPushButton::clicked, this, &GmshPanel::on_clear_model);
  model_form->addRow("Geometry", geo_path_);
  model_form->addRow(open_geo);
  model_form->addRow("", clear_geo);
  auto_mesh_on_import_ = new QCheckBox("Auto mesh after import");
  auto_mesh_on_import_->setChecked(true);
  model_form->addRow("", auto_mesh_on_import_);
  auto_reload_geometry_ = new QCheckBox("Auto reload geometry on project load");
  auto_reload_geometry_->setChecked(true);
  model_form->addRow("", auto_reload_geometry_);
  entity_summary_ = new QLabel("Entities: 0P / 0C / 0S / 0V");
  model_form->addRow("Summary", entity_summary_);
  content_layout->addWidget(model_box);

  auto* entities_box = new QGroupBox("Entities");
  auto* entities_layout = new QVBoxLayout(entities_box);
  entity_dim_ = new QComboBox();
  entity_dim_->addItem("All", -1);
  entity_dim_->addItem("0", 0);
  entity_dim_->addItem("1", 1);
  entity_dim_->addItem("2", 2);
  entity_dim_->addItem("3", 3);
  tune_dim_combo(entity_dim_);
  connect(entity_dim_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &GmshPanel::on_entity_dim_changed);
  auto* refresh_btn = new QPushButton("Refresh");
  connect(refresh_btn, &QPushButton::clicked, this, [this]() {
    update_entity_list();
  });
  entities_layout->addWidget(new QLabel("Dim"));
  entities_layout->addWidget(entity_dim_);
  entities_layout->addWidget(refresh_btn);
  entity_list_ = new QPlainTextEdit();
  entity_list_->setReadOnly(true);
  entity_list_->setMaximumHeight(120);
  entities_layout->addWidget(entity_list_);
  content_layout->addWidget(entities_box);

  auto* geo_box = new QGroupBox("Geometry");
  auto* geo_form = new QFormLayout(geo_box);

  use_sample_box_ = new QCheckBox("Use Sample Box");
  use_sample_box_->setChecked(true);
  connect(use_sample_box_, &QCheckBox::toggled, this,
          [this](bool) { update_geometry_controls(); });
  geo_form->addRow("", use_sample_box_);

  size_x_ = new QDoubleSpinBox();
  size_x_->setRange(0.01, 1000.0);
  size_x_->setValue(1.0);
  size_y_ = new QDoubleSpinBox();
  size_y_->setRange(0.01, 1000.0);
  size_y_->setValue(1.0);
  size_z_ = new QDoubleSpinBox();
  size_z_->setRange(0.01, 1000.0);
  size_z_->setValue(1.0);

  geo_form->addRow("Size X", size_x_);
  geo_form->addRow("Size Y", size_y_);
  geo_form->addRow("Size Z", size_z_);
  update_geometry_controls();
  content_layout->addWidget(geo_box);

  auto* prim_box = new QGroupBox("Primitives");
  auto* prim_form = new QFormLayout(prim_box);
  primitive_kind_ = new QComboBox();
  primitive_kind_->addItem("Box");
  primitive_kind_->addItem("Cylinder");
  primitive_kind_->addItem("Sphere");
  connect(primitive_kind_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { update_primitive_controls(); });
  prim_form->addRow("Type", primitive_kind_);

  prim_x_ = new QDoubleSpinBox();
  prim_y_ = new QDoubleSpinBox();
  prim_z_ = new QDoubleSpinBox();
  prim_x_->setRange(-1e6, 1e6);
  prim_y_->setRange(-1e6, 1e6);
  prim_z_->setRange(-1e6, 1e6);
  prim_form->addRow("Origin/Base x", prim_x_);
  prim_form->addRow("Origin/Base y", prim_y_);
  prim_form->addRow("Origin/Base z", prim_z_);

  prim_dx_ = new QDoubleSpinBox();
  prim_dy_ = new QDoubleSpinBox();
  prim_dz_ = new QDoubleSpinBox();
  prim_dx_->setRange(-1e6, 1e6);
  prim_dy_->setRange(-1e6, 1e6);
  prim_dz_->setRange(-1e6, 1e6);
  prim_dx_->setValue(1.0);
  prim_dy_->setValue(1.0);
  prim_dz_->setValue(1.0);
  prim_form->addRow("Size/Axis dx", prim_dx_);
  prim_form->addRow("Size/Axis dy", prim_dy_);
  prim_form->addRow("Size/Axis dz", prim_dz_);

  prim_radius_ = new QDoubleSpinBox();
  prim_radius_->setRange(0.0, 1e6);
  prim_radius_->setValue(0.5);
  prim_form->addRow("Radius", prim_radius_);

  prim_add_btn_ = new QPushButton("Add Primitive");
  connect(prim_add_btn_, &QPushButton::clicked, this,
          &GmshPanel::on_add_primitive);
  prim_form->addRow("", prim_add_btn_);
  update_primitive_controls();
  content_layout->addWidget(prim_box);

  auto* xform_box = new QGroupBox("Transform");
  auto* xform_form = new QFormLayout(xform_box);
  transform_dim_ = new QComboBox();
  transform_dim_->addItem("All", -1);
  transform_dim_->addItem("0", 0);
  transform_dim_->addItem("1", 1);
  transform_dim_->addItem("2", 2);
  transform_dim_->addItem("3", 3);
  tune_dim_combo(transform_dim_);
  transform_ids_ = new QLineEdit();
  transform_ids_->setPlaceholderText("IDs or dim:tag (e.g. 1,2 or 2:5). Empty = all.");
  auto* transform_pick = new QPushButton("Pick");
  connect(transform_pick, &QPushButton::clicked, this, [this]() {
    const int dim = transform_dim_ ? transform_dim_->currentData().toInt() : -1;
    active_entity_input_ = transform_ids_;
    transform_ids_->setText(
        pick_entities_dialog(dim, "Select Transform Entities",
                             transform_ids_->text()));
  });
  bind_entity_input_validation(transform_ids_, transform_dim_, true);
  xform_form->addRow("Dim", transform_dim_);
  xform_form->addRow("IDs", transform_ids_);
  transform_template_ = new QComboBox();
  transform_template_->setEditable(true);
  transform_template_->addItem("Templates");
  tune_gmsh_combo(transform_template_, 110, 160);
  xform_form->addRow("Template", transform_template_);
  xform_form->addRow(transform_pick);
  populate_transform_entity_templates(-1);
  connect(transform_dim_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this]() {
            const int dim = transform_dim_->currentData().toInt();
            populate_transform_entity_templates(dim);
            validate_entity_input(transform_ids_, dim, true);
          });
  connect(transform_template_, QOverload<int>::of(&QComboBox::activated), this,
          [this](int index) {
            if (!transform_template_ || index <= 0 || !transform_ids_) {
              return;
            }
            append_entity_template(transform_ids_,
                                  transform_template_->itemText(index));
            transform_template_->setCurrentIndex(0);
          });

  trans_dx_ = new QDoubleSpinBox();
  trans_dy_ = new QDoubleSpinBox();
  trans_dz_ = new QDoubleSpinBox();
  trans_dx_->setRange(-1e6, 1e6);
  trans_dy_->setRange(-1e6, 1e6);
  trans_dz_->setRange(-1e6, 1e6);
  auto* trans_btn = new QPushButton("Translate");
  connect(trans_btn, &QPushButton::clicked, this,
          &GmshPanel::on_apply_translate);
  xform_form->addRow("dx", trans_dx_);
  xform_form->addRow("dy", trans_dy_);
  xform_form->addRow("dz", trans_dz_);
  xform_form->addRow(trans_btn);

  rot_x_ = new QDoubleSpinBox();
  rot_y_ = new QDoubleSpinBox();
  rot_z_ = new QDoubleSpinBox();
  rot_x_->setRange(-1e6, 1e6);
  rot_y_->setRange(-1e6, 1e6);
  rot_z_->setRange(-1e6, 1e6);
  xform_form->addRow("Rotate Origin x", rot_x_);
  xform_form->addRow("Rotate Origin y", rot_y_);
  xform_form->addRow("Rotate Origin z", rot_z_);

  rot_ax_ = new QDoubleSpinBox();
  rot_ay_ = new QDoubleSpinBox();
  rot_az_ = new QDoubleSpinBox();
  rot_angle_ = new QDoubleSpinBox();
  rot_ax_->setRange(-1e6, 1e6);
  rot_ay_->setRange(-1e6, 1e6);
  rot_az_->setRange(-1e6, 1e6);
  rot_ax_->setValue(0.0);
  rot_ay_->setValue(0.0);
  rot_az_->setValue(1.0);
  rot_angle_->setRange(-360.0, 360.0);
  rot_angle_->setValue(0.0);
  auto* rot_btn = new QPushButton("Rotate");
  connect(rot_btn, &QPushButton::clicked, this,
          &GmshPanel::on_apply_rotate);
  xform_form->addRow("Rotate Axis ax", rot_ax_);
  xform_form->addRow("Rotate Axis ay", rot_ay_);
  xform_form->addRow("Rotate Axis az", rot_az_);
  xform_form->addRow("deg", rot_angle_);
  xform_form->addRow(rot_btn);

  scale_cx_ = new QDoubleSpinBox();
  scale_cy_ = new QDoubleSpinBox();
  scale_cz_ = new QDoubleSpinBox();
  scale_cx_->setRange(-1e6, 1e6);
  scale_cy_->setRange(-1e6, 1e6);
  scale_cz_->setRange(-1e6, 1e6);
  xform_form->addRow("Scale Center x", scale_cx_);
  xform_form->addRow("Scale Center y", scale_cy_);
  xform_form->addRow("Scale Center z", scale_cz_);

  scale_x_ = new QDoubleSpinBox();
  scale_y_ = new QDoubleSpinBox();
  scale_z_ = new QDoubleSpinBox();
  scale_x_->setRange(0.001, 1000.0);
  scale_y_->setRange(0.001, 1000.0);
  scale_z_->setRange(0.001, 1000.0);
  scale_x_->setValue(1.0);
  scale_y_->setValue(1.0);
  scale_z_->setValue(1.0);
  auto* scale_btn = new QPushButton("Scale");
  connect(scale_btn, &QPushButton::clicked, this,
          &GmshPanel::on_apply_scale);
  xform_form->addRow("sx", scale_x_);
  xform_form->addRow("sy", scale_y_);
  xform_form->addRow("sz", scale_z_);
  xform_form->addRow(scale_btn);
  content_layout->addWidget(xform_box);

  auto* bool_box = new QGroupBox("Boolean");
  auto* bool_form = new QFormLayout(bool_box);
  boolean_dim_ = new QComboBox();
  boolean_dim_->addItem("3", 3);
  boolean_dim_->addItem("2", 2);
  boolean_dim_->addItem("1", 1);
  boolean_dim_->addItem("0", 0);
  tune_dim_combo(boolean_dim_);
  bool_form->addRow("Dim", boolean_dim_);
  boolean_obj_ids_ = new QLineEdit();
  boolean_obj_ids_->setPlaceholderText("Object IDs or dim:tag (e.g. 1,2 or 3:4)");
  boolean_tool_ids_ = new QLineEdit();
  boolean_tool_ids_->setPlaceholderText("Tool IDs or dim:tag (e.g. 3 or 3:5)");
  auto* boolean_obj_pick = new QPushButton("Pick");
  connect(boolean_obj_pick, &QPushButton::clicked, this, [this]() {
    const int dim = boolean_dim_ ? boolean_dim_->currentData().toInt() : -1;
    active_entity_input_ = boolean_obj_ids_;
    boolean_obj_ids_->setText(
        pick_entities_dialog(dim, "Select Boolean Objects",
                             boolean_obj_ids_->text()));
  });
  auto* boolean_tool_pick = new QPushButton("Pick");
  connect(boolean_tool_pick, &QPushButton::clicked, this, [this]() {
    const int dim = boolean_dim_ ? boolean_dim_->currentData().toInt() : -1;
    active_entity_input_ = boolean_tool_ids_;
    boolean_tool_ids_->setText(
        pick_entities_dialog(dim, "Select Boolean Tools",
                             boolean_tool_ids_->text()));
  });
  bool_form->addRow("Obj", boolean_obj_ids_);
  bind_entity_input_validation(boolean_obj_ids_, boolean_dim_, true);
  boolean_obj_template_ = new QComboBox();
  boolean_obj_template_->setEditable(true);
  boolean_obj_template_->addItem("Templates");
  tune_gmsh_combo(boolean_obj_template_, 110, 160);
  bool_form->addRow("Template", boolean_obj_template_);
  bool_form->addRow(boolean_obj_pick);
  bool_form->addRow("Tool", boolean_tool_ids_);
  bind_entity_input_validation(boolean_tool_ids_, boolean_dim_, true);
  boolean_tool_template_ = new QComboBox();
  boolean_tool_template_->setEditable(true);
  boolean_tool_template_->addItem("Templates");
  tune_gmsh_combo(boolean_tool_template_, 110, 160);
  bool_form->addRow("Template", boolean_tool_template_);
  bool_form->addRow(boolean_tool_pick);
  connect(boolean_dim_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this]() {
            const int dim = boolean_dim_->currentData().toInt();
            populate_boolean_entity_templates(dim);
            validate_entity_input(boolean_obj_ids_, dim, true);
            validate_entity_input(boolean_tool_ids_, dim, true);
          });
  connect(boolean_obj_template_,
          QOverload<int>::of(&QComboBox::activated), this,
          [this](int index) {
            if (!boolean_obj_template_ || index <= 0 || !boolean_obj_ids_) {
              return;
            }
            append_entity_template(boolean_obj_ids_,
                                  boolean_obj_template_->itemText(index));
            boolean_obj_template_->setCurrentIndex(0);
          });
  connect(boolean_tool_template_,
          QOverload<int>::of(&QComboBox::activated), this,
          [this](int index) {
            if (!boolean_tool_template_ || index <= 0 || !boolean_tool_ids_) {
              return;
            }
            append_entity_template(boolean_tool_ids_,
                                  boolean_tool_template_->itemText(index));
            boolean_tool_template_->setCurrentIndex(0);
          });
  populate_boolean_entity_templates(3);

  boolean_remove_obj_ = new QCheckBox("Remove Object");
  boolean_remove_tool_ = new QCheckBox("Remove Tool");
  boolean_remove_obj_->setChecked(true);
  boolean_remove_tool_->setChecked(true);
  bool_form->addRow(boolean_remove_obj_);
  bool_form->addRow(boolean_remove_tool_);

  auto* fuse_btn = new QPushButton("Fuse");
  auto* cut_btn = new QPushButton("Cut");
  auto* intersect_btn = new QPushButton("Intersect");
  connect(fuse_btn, &QPushButton::clicked, this,
          &GmshPanel::on_apply_boolean_fuse);
  connect(cut_btn, &QPushButton::clicked, this,
          &GmshPanel::on_apply_boolean_cut);
  connect(intersect_btn, &QPushButton::clicked, this,
          &GmshPanel::on_apply_boolean_intersect);
  bool_form->addRow(fuse_btn);
  bool_form->addRow(cut_btn);
  bool_form->addRow(intersect_btn);
  content_layout->addWidget(bool_box);

  auto* phys_box = new QGroupBox("Physical Groups");
  auto* phys_form = new QFormLayout(phys_box);
  phys_group_list_ = new QComboBox();
  phys_group_list_->setMinimumWidth(0);
  connect(phys_group_list_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &GmshPanel::on_physical_group_selected);
  auto* phys_refresh = new QPushButton("Refresh");
  connect(phys_refresh, &QPushButton::clicked, this,
          &GmshPanel::on_physical_group_refresh);
  phys_form->addRow("Groups", phys_group_list_);
  phys_form->addRow(phys_refresh);

  phys_group_dim_ = new QComboBox();
  phys_group_dim_->addItem("0", 0);
  phys_group_dim_->addItem("1", 1);
  phys_group_dim_->addItem("2", 2);
  phys_group_dim_->addItem("3", 3);
  tune_dim_combo(phys_group_dim_);
  connect(phys_group_dim_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this]() {
            validate_entity_input(phys_group_entities_,
                                 phys_group_dim_ ? phys_group_dim_->currentData().toInt()
                                                : -1,
                                 false);
          });
  phys_group_name_ = new QLineEdit();
  phys_group_name_->setPlaceholderText("Name");
  phys_form->addRow("Dim", phys_group_dim_);
  phys_form->addRow("Name", phys_group_name_);

  phys_group_entities_ = new QLineEdit();
  phys_group_entities_->setPlaceholderText("Entity IDs or dim:tag list");
  auto* phys_entities_pick = new QPushButton("Pick");
  connect(phys_entities_pick, &QPushButton::clicked, this, [this]() {
    const int dim = phys_group_dim_ ? phys_group_dim_->currentData().toInt() : -1;
    active_entity_input_ = phys_group_entities_;
    phys_group_entities_->setText(
        pick_entities_dialog(dim, "Select Physical Group Entities",
                             phys_group_entities_->text()));
  });
  phys_form->addRow("Entities", phys_group_entities_);
  phys_form->addRow(phys_entities_pick);
  bind_entity_input_validation(phys_group_entities_, phys_group_dim_, false);

  phys_group_add_ = new QPushButton("Add");
  phys_group_update_ = new QPushButton("Update Selected");
  phys_group_delete_ = new QPushButton("Delete Selected");
  connect(phys_group_add_, &QPushButton::clicked, this,
          &GmshPanel::on_physical_group_add);
  connect(phys_group_update_, &QPushButton::clicked, this,
          &GmshPanel::on_physical_group_update);
  connect(phys_group_delete_, &QPushButton::clicked, this,
          &GmshPanel::on_physical_group_delete);
  phys_form->addRow(phys_group_add_);
  phys_form->addRow(phys_group_update_);
  phys_form->addRow(phys_group_delete_);

  phys_group_table_ = new QTableWidget();
  phys_group_table_->setColumnCount(5);
  phys_group_table_->setHorizontalHeaderLabels(
      {"Dim", "Tag", "Name", "Entities", "Elements"});
  phys_group_table_->horizontalHeader()->setStretchLastSection(true);
  // 5列表格不参与撑宽面板, 过窄时表格内部自行横向滚动
  phys_group_table_->setSizePolicy(QSizePolicy::Ignored,
                                   QSizePolicy::Preferred);
  phys_group_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  phys_group_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  phys_group_table_->setMinimumHeight(120);
  connect(phys_group_table_, &QTableWidget::itemSelectionChanged, this,
          [this]() {
            if (!phys_group_table_ || phys_group_table_->selectedItems().isEmpty()) {
              emit physical_group_selected(-1, -1);
              return;
            }
            const int row = phys_group_table_->currentRow();
            if (row < 0) {
              emit physical_group_selected(-1, -1);
              return;
            }
            bool ok_dim = false;
            bool ok_tag = false;
            const int dim = phys_group_table_->item(row, 0)->text().toInt(&ok_dim);
            const int tag = phys_group_table_->item(row, 1)->text().toInt(&ok_tag);
            if (ok_dim && ok_tag) {
              emit physical_group_selected(dim, tag);
            }
          });
  phys_form->addRow("Stats", phys_group_table_);
  content_layout->addWidget(phys_box);

  auto* field_box = new QGroupBox("Mesh Fields");
  auto* field_form = new QFormLayout(field_box);
  field_dim_ = new QComboBox();
  field_dim_->addItem("1", 1);
  field_dim_->addItem("2", 2);
  field_dim_->addItem("3", 3);
  tune_dim_combo(field_dim_);
  connect(field_dim_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this]() {
            validate_entity_input(field_entities_,
                                 field_dim_ ? field_dim_->currentData().toInt()
                                            : -1,
                                 false);
          });
  field_entities_ = new QLineEdit();
  field_entities_->setPlaceholderText("Entity IDs or dim:tag list");
  auto* field_entities_pick = new QPushButton("Pick");
  connect(field_entities_pick, &QPushButton::clicked, this, [this]() {
    const int dim = field_dim_ ? field_dim_->currentData().toInt() : -1;
    active_entity_input_ = field_entities_;
    field_entities_->setText(
        pick_entities_dialog(dim, "Select Field Entities",
                             field_entities_->text()));
  });
  field_form->addRow("Dim", field_dim_);
  field_form->addRow("Entities", field_entities_);
  field_form->addRow(field_entities_pick);
  bind_entity_input_validation(field_entities_, field_dim_, false);

  field_dist_min_ = new QDoubleSpinBox();
  field_dist_max_ = new QDoubleSpinBox();
  field_dist_min_->setRange(0.0, 1e6);
  field_dist_max_->setRange(0.0, 1e6);
  field_dist_min_->setValue(0.1);
  field_dist_max_->setValue(1.0);
  field_form->addRow("DistMin", field_dist_min_);
  field_form->addRow("DistMax", field_dist_max_);

  field_size_min_ = new QDoubleSpinBox();
  field_size_max_ = new QDoubleSpinBox();
  field_size_min_->setRange(0.0, 1e6);
  field_size_max_->setRange(0.0, 1e6);
  field_size_min_->setValue(0.05);
  field_size_max_->setValue(0.2);
  field_form->addRow("SizeMin", field_size_min_);
  field_form->addRow("SizeMax", field_size_max_);

  auto* field_apply = new QPushButton("Apply Field");
  auto* field_clear = new QPushButton("Clear Fields");
  auto* field_refresh = new QPushButton("Refresh");
  connect(field_apply, &QPushButton::clicked, this, &GmshPanel::on_field_apply);
  connect(field_clear, &QPushButton::clicked, this, &GmshPanel::on_field_clear);
  connect(field_refresh, &QPushButton::clicked, this,
          &GmshPanel::on_field_refresh);
  field_form->addRow(field_apply);
  field_form->addRow(field_clear);
  field_form->addRow(field_refresh);

  field_list_ = new QPlainTextEdit();
  field_list_->setReadOnly(true);
  field_list_->setMaximumHeight(100);
  field_form->addRow("Fields", field_list_);
  content_layout->addWidget(field_box);

  auto* mesh_box = new QGroupBox("Mesh");
  auto* mesh_form = new QFormLayout(mesh_box);

  mesh_size_ = new QDoubleSpinBox();
  mesh_size_->setRange(0.01, 1000.0);
  mesh_size_->setValue(0.2);
  mesh_size_->setSingleStep(0.05);
  mesh_form->addRow("Mesh Size", mesh_size_);

  mesh_dim_ = new QComboBox();
  mesh_dim_->addItem("Auto", -1);
  mesh_dim_->addItem("1D", 1);
  mesh_dim_->addItem("2D", 2);
  mesh_dim_->addItem("3D", 3);
  mesh_form->addRow("Generate Dim", mesh_dim_);

  entity_size_dim_ = new QComboBox();
  entity_size_dim_->addItem("0", 0);
  entity_size_dim_->addItem("1", 1);
  entity_size_dim_->addItem("2", 2);
  entity_size_dim_->addItem("3", 3);
  tune_dim_combo(entity_size_dim_);
  connect(entity_size_dim_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this]() {
            validate_entity_input(entity_size_ids_,
                                 entity_size_dim_
                                     ? entity_size_dim_->currentData().toInt()
                                     : -1,
                                 false);
          });
  entity_size_ids_ = new QLineEdit();
  entity_size_ids_->setPlaceholderText("IDs or dim:tag list");
  entity_size_value_ = new QDoubleSpinBox();
  entity_size_value_->setRange(0.0, 1e6);
  entity_size_value_->setValue(0.1);
  auto* entity_size_pick = new QPushButton("Pick");
  connect(entity_size_pick, &QPushButton::clicked, this, [this]() {
    const int dim = entity_size_dim_ ? entity_size_dim_->currentData().toInt() : -1;
    active_entity_input_ = entity_size_ids_;
    entity_size_ids_->setText(
        pick_entities_dialog(dim, "Select Entities for Size",
                             entity_size_ids_->text()));
  });
  entity_size_apply_ = new QPushButton("Apply");
  entity_size_clear_ = new QPushButton("Clear");
  connect(entity_size_apply_, &QPushButton::clicked, this,
          &GmshPanel::on_entity_size_apply);
  connect(entity_size_clear_, &QPushButton::clicked, this,
          &GmshPanel::on_entity_size_clear);
  mesh_form->addRow("Dim", entity_size_dim_);
  mesh_form->addRow("IDs", entity_size_ids_);
  mesh_form->addRow(entity_size_pick);
  bind_entity_input_validation(entity_size_ids_, entity_size_dim_, false);
  mesh_form->addRow("Size", entity_size_value_);
  mesh_form->addRow(entity_size_apply_);
  mesh_form->addRow(entity_size_clear_);

  elem_order_ = new QComboBox();
  elem_order_->addItem("Linear (1)", 1);
  elem_order_->addItem("Quadratic (2)", 2);
  elem_order_->addItem("Cubic (3)", 3);
  elem_order_->addItem("Quartic (4)", 4);
  mesh_form->addRow("Element Order", elem_order_);

  high_order_opt_ = new QComboBox();
  high_order_opt_->addItem("High-Order Optimize: Off", 0);
  high_order_opt_->addItem("High-Order Optimize: Simple", 1);
  high_order_opt_->addItem("High-Order Optimize: Elastic", 2);
  high_order_opt_->addItem("High-Order Optimize: Fast Curving", 3);
  mesh_form->addRow("High-Order", high_order_opt_);

  msh_version_ = new QComboBox();
  msh_version_->addItem("MSH 2.2", 2);
  msh_version_->addItem("MSH 4.1", 4);
  mesh_form->addRow("MSH Version", msh_version_);

  algo2d_ = new QComboBox();
  algo2d_->addItem("Automatic", 2);
  algo2d_->addItem("MeshAdapt", 1);
  algo2d_->addItem("Delaunay", 5);
  algo2d_->addItem("Frontal", 6);
  algo2d_->addItem("BAMG", 7);
  algo2d_->addItem("DelQuad", 8);
  mesh_form->addRow("Algorithm 2D", algo2d_);

  algo3d_ = new QComboBox();
  algo3d_->addItem("Delaunay", 1);
  algo3d_->addItem("Frontal", 4);
  algo3d_->addItem("HXT", 10);
  mesh_form->addRow("Algorithm 3D", algo3d_);

  recombine_ = new QCheckBox("Recombine (quad/hex)");
  recombine_->setChecked(false);
  mesh_form->addRow("", recombine_);

  smoothing_ = new QSpinBox();
  smoothing_->setRange(0, 100);
  smoothing_->setValue(10);
  mesh_form->addRow("Smoothing", smoothing_);

  optimize_ = new QCheckBox("Optimize Mesh");
  optimize_->setChecked(true);
  mesh_form->addRow("", optimize_);

  content_layout->addWidget(mesh_box);

  auto* form = new QFormLayout();
  output_path_ = new QLineEdit();
  output_path_->setPlaceholderText("Output mesh path (*.msh)");
  output_path_->setText(QDir::currentPath() + "/out/box.msh");

  auto* pick_btn = new QPushButton("Pick Output");
  connect(pick_btn, &QPushButton::clicked, this, &GmshPanel::on_pick_output);

  form->addRow("Mesh Output", output_path_);
  form->addRow(pick_btn);

  content_layout->addLayout(form);

  auto* export_btn = new QPushButton("Export Geometry");
  connect(export_btn, &QPushButton::clicked, this,
          &GmshPanel::on_export_geometry);
  content_layout->addWidget(export_btn);

  auto* generate_btn = new QPushButton("Generate Mesh");
  connect(generate_btn, &QPushButton::clicked, this, &GmshPanel::on_generate);

  auto* generate_2d_btn = new QPushButton("Generate 2D Mesh");
  auto* generate_3d_btn = new QPushButton("Generate 3D Mesh");
  connect(generate_2d_btn, &QPushButton::clicked, this, [this]() {
    if (mesh_dim_) {
      const int idx = mesh_dim_->findData(2);
      if (idx >= 0) {
        mesh_dim_->setCurrentIndex(idx);
      }
    }
    on_generate();
  });
  connect(generate_3d_btn, &QPushButton::clicked, this, [this]() {
    if (mesh_dim_) {
      const int idx = mesh_dim_->findData(3);
      if (idx >= 0) {
        mesh_dim_->setCurrentIndex(idx);
      }
    }
    on_generate();
  });

  content_layout->addWidget(generate_btn);
  content_layout->addWidget(generate_2d_btn);
  content_layout->addWidget(generate_3d_btn);

  log_ = new QPlainTextEdit();
  log_->setReadOnly(true);
  content_layout->addWidget(log_, 1);

  content_layout->addStretch(1);
  scroll->setWidget(content);
  layout->addWidget(scroll, 1);

  tune_gmsh_combo(primitive_kind_, 90, 180);
  tune_gmsh_combo(transform_dim_, 90, 120);
  tune_gmsh_combo(boolean_dim_, 90, 120);
  tune_gmsh_combo(phys_group_list_, 170, 200);
  tune_gmsh_combo(phys_group_dim_, 70, 120);
  tune_gmsh_combo(field_dim_, 70, 120);
  tune_gmsh_combo(entity_size_dim_, 70, 120);
  tune_gmsh_combo(elem_order_, 140, 160);
  tune_gmsh_combo(high_order_opt_, 170, 200);
  tune_gmsh_combo(msh_version_, 110, 140);
  tune_gmsh_combo(mesh_dim_, 90, 120);
  tune_gmsh_combo(algo2d_, 150, 180);
  tune_gmsh_combo(algo3d_, 150, 180);

  // 右侧边栏宽度有限: 表单标签换行到控件上方(垂直布局), 减少横向溢出
  for (auto* f : findChildren<QFormLayout*>()) {
    f->setRowWrapPolicy(QFormLayout::WrapAllRows);
  }

  append_log("Gmsh panel ready.");
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is disabled. Rebuild with -DGMP_ENABLE_GMSH_GUI=ON.");
#endif
  update_entity_list();
  update_physical_group_list();
  update_field_list();
}

bool GmshPanel::import_geometry(const QString& path, bool auto_mesh) {
#ifndef GMP_ENABLE_GMSH_GUI
  Q_UNUSED(path);
  Q_UNUSED(auto_mesh);
  return false;
#else
  if (path.isEmpty()) {
    return false;
  }
  ensure_gmsh();
  try {
    gmsh::option::setNumber("General.Terminal", 0);
    gmsh::logger::start();
    gmsh::clear();
    gmsh::model::add("imported");

    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "step" || ext == "stp" || ext == "iges" || ext == "igs" ||
        ext == "brep") {
      gmsh::vectorpair dim_tags;
      std::string format;
      if (ext == "brep") {
        format = "brep";
      } else if (ext == "iges" || ext == "igs") {
        format = "iges";
      } else {
        format = "step";
      }
      gmsh::model::occ::importShapes(path.toStdString(), dim_tags, true, format);
      gmsh::model::occ::synchronize();
    } else {
      gmsh::open(path.toStdString());
      try {
        gmsh::model::occ::synchronize();
      } catch (...) {
      }
      try {
        gmsh::model::geo::synchronize();
      } catch (...) {
      }
    }

    geo_path_->setText(path);
    model_loaded_ = true;
    use_sample_box_->setChecked(false);
    update_entity_summary();
    update_entity_list();
    update_physical_group_list();
    update_field_list();
    refresh_occ_entity_template_lists();

    std::vector<std::string> log;
    gmsh::logger::get(log);
    gmsh::logger::stop();
    for (const auto& line : log) {
      append_log(QString::fromStdString(line));
    }
    append_log("Geometry loaded: " + path);
    if (auto_mesh) {
      on_generate();
    }
    return true;
  } catch (const std::exception& ex) {
    append_log(QString("Gmsh error: %1").arg(ex.what()));
  }
  return false;
#endif
}

QVariantMap GmshPanel::gmsh_settings() const {
  QVariantMap map;
  map.insert("auto_mesh_on_import",
             auto_mesh_on_import_ && auto_mesh_on_import_->isChecked());
  map.insert("auto_reload_geometry",
             auto_reload_geometry_ && auto_reload_geometry_->isChecked());
  map.insert("use_sample_box", use_sample_box_ && use_sample_box_->isChecked());
  map.insert("size_x", size_x_ ? size_x_->value() : 1.0);
  map.insert("size_y", size_y_ ? size_y_->value() : 1.0);
  map.insert("size_z", size_z_ ? size_z_->value() : 1.0);
  map.insert("mesh_size", mesh_size_ ? mesh_size_->value() : 0.2);
  map.insert("mesh_dim", mesh_dim_ ? mesh_dim_->currentData().toInt() : -1);
  map.insert("elem_order",
             elem_order_ ? elem_order_->currentData().toInt() : 1);
  map.insert("msh_version",
             msh_version_ ? msh_version_->currentData().toInt() : 2);
  map.insert("optimize", optimize_ && optimize_->isChecked());
  map.insert("high_order_opt",
             high_order_opt_ ? high_order_opt_->currentData().toInt() : 0);
  map.insert("algo2d", algo2d_ ? algo2d_->currentData().toInt() : 2);
  map.insert("algo3d", algo3d_ ? algo3d_->currentData().toInt() : 1);
  map.insert("recombine", recombine_ && recombine_->isChecked());
  map.insert("smoothing", smoothing_ ? smoothing_->value() : 10);
  map.insert("output_path", output_path_ ? output_path_->text() : "");
  const QString geo_path = geo_path_ ? geo_path_->text() : "";
  if (!geo_path.isEmpty() && QFileInfo::exists(geo_path)) {
    map.insert("geometry_path", geo_path);
  }

  map.insert("entity_size_dim",
             entity_size_dim_ ? entity_size_dim_->currentData().toInt() : 0);
  map.insert("entity_size_ids",
             entity_size_ids_ ? entity_size_ids_->text() : "");
  map.insert("entity_size_value",
             entity_size_value_ ? entity_size_value_->value() : 0.1);

  map.insert("field_dim",
             field_dim_ ? field_dim_->currentData().toInt() : 2);
  map.insert("field_entities",
             field_entities_ ? field_entities_->text() : "");
  map.insert("field_dist_min",
             field_dist_min_ ? field_dist_min_->value() : 0.1);
  map.insert("field_dist_max",
             field_dist_max_ ? field_dist_max_->value() : 1.0);
  map.insert("field_size_min",
             field_size_min_ ? field_size_min_->value() : 0.05);
  map.insert("field_size_max",
             field_size_max_ ? field_size_max_->value() : 0.2);
  return map;
}

void GmshPanel::apply_gmsh_settings(const QVariantMap& settings) {
  auto set_combo_data = [](QComboBox* combo, int value) {
    if (!combo) {
      return;
    }
    const int idx = combo->findData(value);
    if (idx >= 0) {
      combo->setCurrentIndex(idx);
    }
  };

  if (auto_mesh_on_import_) {
    auto_mesh_on_import_->setChecked(
        settings.value("auto_mesh_on_import",
                       auto_mesh_on_import_->isChecked())
            .toBool());
  }
  if (auto_reload_geometry_) {
    auto_reload_geometry_->setChecked(
        settings.value("auto_reload_geometry",
                       auto_reload_geometry_->isChecked())
            .toBool());
  }
  if (use_sample_box_) {
    use_sample_box_->setChecked(
        settings.value("use_sample_box", use_sample_box_->isChecked())
            .toBool());
  }
  if (size_x_) {
    size_x_->setValue(settings.value("size_x", size_x_->value()).toDouble());
  }
  if (size_y_) {
    size_y_->setValue(settings.value("size_y", size_y_->value()).toDouble());
  }
  if (size_z_) {
    size_z_->setValue(settings.value("size_z", size_z_->value()).toDouble());
  }
  if (mesh_size_) {
    mesh_size_->setValue(
        settings.value("mesh_size", mesh_size_->value()).toDouble());
  }
  if (mesh_dim_) {
    set_combo_data(mesh_dim_, settings.value("mesh_dim", -1).toInt());
  }
  if (elem_order_) {
    set_combo_data(elem_order_,
                   settings.value("elem_order",
                                  elem_order_->currentData().toInt())
                       .toInt());
  }
  if (msh_version_) {
    set_combo_data(msh_version_,
                   settings.value("msh_version",
                                  msh_version_->currentData().toInt())
                       .toInt());
  }
  if (optimize_) {
    optimize_->setChecked(
        settings.value("optimize", optimize_->isChecked()).toBool());
  }
  if (high_order_opt_) {
    set_combo_data(high_order_opt_,
                   settings.value("high_order_opt",
                                  high_order_opt_->currentData().toInt())
                       .toInt());
  }
  if (algo2d_) {
    set_combo_data(algo2d_,
                   settings.value("algo2d", algo2d_->currentData().toInt())
                       .toInt());
  }
  if (algo3d_) {
    set_combo_data(algo3d_,
                   settings.value("algo3d", algo3d_->currentData().toInt())
                       .toInt());
  }
  if (recombine_) {
    recombine_->setChecked(
        settings.value("recombine", recombine_->isChecked()).toBool());
  }
  if (smoothing_) {
    smoothing_->setValue(
        settings.value("smoothing", smoothing_->value()).toInt());
  }
  if (output_path_) {
    output_path_->setText(
        settings.value("output_path", output_path_->text()).toString());
  }

  const QString geometry_path =
      settings.value("geometry_path", geo_path_ ? geo_path_->text() : "")
          .toString();
  if (!geometry_path.isEmpty() && auto_reload_geometry_ &&
      auto_reload_geometry_->isChecked() && QFileInfo::exists(geometry_path)) {
    import_geometry(geometry_path,
                    auto_mesh_on_import_ && auto_mesh_on_import_->isChecked());
  }

  if (entity_size_dim_) {
    set_combo_data(
        entity_size_dim_,
        settings.value("entity_size_dim",
                       entity_size_dim_->currentData().toInt())
            .toInt());
  }
  if (entity_size_ids_) {
    entity_size_ids_->setText(
        settings.value("entity_size_ids", entity_size_ids_->text()).toString());
  }
  if (entity_size_value_) {
    entity_size_value_->setValue(
        settings.value("entity_size_value", entity_size_value_->value())
            .toDouble());
  }

  if (field_dim_) {
    set_combo_data(field_dim_,
                   settings.value("field_dim", field_dim_->currentData().toInt())
                       .toInt());
  }
  if (field_entities_) {
    field_entities_->setText(
        settings.value("field_entities", field_entities_->text()).toString());
  }
  if (field_dist_min_) {
    field_dist_min_->setValue(
        settings.value("field_dist_min", field_dist_min_->value()).toDouble());
  }
  if (field_dist_max_) {
    field_dist_max_->setValue(
        settings.value("field_dist_max", field_dist_max_->value()).toDouble());
  }
  if (field_size_min_) {
    field_size_min_->setValue(
        settings.value("field_size_min", field_size_min_->value()).toDouble());
  }
  if (field_size_max_) {
    field_size_max_->setValue(
        settings.value("field_size_max", field_size_max_->value()).toDouble());
  }

  update_geometry_controls();
  update_primitive_controls();
  refresh_occ_entity_template_lists();
}

GmshPanel::~GmshPanel() {
#ifdef GMP_ENABLE_GMSH_GUI
  if (gmsh_ready_) {
    gmsh::finalize();
  }
#endif
}

void GmshPanel::generate_mesh() {
  on_generate();
}

void GmshPanel::set_mesh_generation_dim(int dim) {
#ifndef GMP_ENABLE_GMSH_GUI
  Q_UNUSED(dim);
  return;
#else
  if (!mesh_dim_) {
    return;
  }
  const int clamped = qBound(1, dim, 3);
  const int idx = mesh_dim_->findData(clamped);
  if (idx >= 0) {
    mesh_dim_->setCurrentIndex(idx);
    return;
  }
  const int auto_idx = mesh_dim_->findData(-1);
  if (auto_idx >= 0) {
    mesh_dim_->setCurrentIndex(auto_idx);
  }
  append_log(QString("Mesh dimension %1 not available in controls, fallback to Auto.").arg(dim));
#endif
}

bool GmshPanel::eventFilter(QObject* obj, QEvent* event) {
  if (event && event->type() == QEvent::FocusIn) {
    auto* edit = qobject_cast<QLineEdit*>(obj);
    if (edit && entity_inputs_.contains(edit)) {
      active_entity_input_ = edit;
    }
  }
  return QWidget::eventFilter(obj, event);
}

void GmshPanel::on_open_geometry() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  const QString path = QFileDialog::getOpenFileName(
      this, "Open Geometry", QDir::currentPath(),
      "Geometry (*.geo *.geo_unrolled *.step *.stp *.iges *.igs *.brep)");
  if (path.isEmpty()) {
    return;
  }
  import_geometry(path, auto_mesh_on_import_ && auto_mesh_on_import_->isChecked());
#endif
}

void GmshPanel::on_clear_model() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  ensure_gmsh();
  gmsh::clear();
  model_loaded_ = false;
  geo_path_->clear();
  update_entity_summary();
  update_entity_list();
  update_physical_group_list();
  update_field_list();
  use_sample_box_->setChecked(true);
  refresh_occ_entity_template_lists();
  append_log("Model cleared.");
#endif
}

void GmshPanel::on_pick_output() {
  const QString path =
      QFileDialog::getSaveFileName(this, "Select mesh output", output_path_->text(),
                                   "Gmsh Mesh (*.msh)");
  if (!path.isEmpty()) {
    output_path_->setText(path);
  }
}

void GmshPanel::on_entity_size_apply() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  try {
    ensure_gmsh();
    const int dim = entity_size_dim_->currentData().toInt();
    if (dim != 0) {
      append_log("Entity size: only dim=0 (points) supported. Use fields for surfaces/volumes.");
      return;
    }
    const auto tags = resolve_entity_tags(dim, entity_size_ids_->text());
    if (tags.empty()) {
      append_log("Entity size: no valid points.");
      return;
    }
    gmsh::vectorpair dim_tags;
    dim_tags.reserve(tags.size());
    for (int t : tags) {
      dim_tags.emplace_back(0, t);
    }
    gmsh::model::mesh::setSize(dim_tags, entity_size_value_->value());
    append_log(QString("Entity size applied to %1 points.").arg(tags.size()));
  } catch (const std::exception& ex) {
    append_log(QString("Entity size apply failed: %1").arg(ex.what()));
  }
#endif
}

void GmshPanel::on_entity_size_clear() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  try {
    ensure_gmsh();
    const int dim = entity_size_dim_->currentData().toInt();
    if (dim != 0) {
      append_log("Entity size clear: only dim=0 (points) supported.");
      return;
    }
    const auto tags = resolve_entity_tags(dim, entity_size_ids_->text());
    if (tags.empty()) {
      append_log("Entity size clear: no valid points.");
      return;
    }
    gmsh::vectorpair dim_tags;
    dim_tags.reserve(tags.size());
    for (int t : tags) {
      dim_tags.emplace_back(0, t);
    }
    gmsh::model::mesh::setSize(dim_tags, 0.0);
    append_log(QString("Entity size cleared for %1 points.").arg(tags.size()));
  } catch (const std::exception& ex) {
    append_log(QString("Entity size clear failed: %1").arg(ex.what()));
  }
#endif
}

void GmshPanel::on_export_geometry() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  ensure_gmsh();
  const QString path = QFileDialog::getSaveFileName(
      this, "Export Geometry", QDir::currentPath(),
      "BREP (*.brep);;GEO (*.geo);;GEO Unrolled (*.geo_unrolled)");
  if (path.isEmpty()) {
    return;
  }
  try {
    gmsh::write(path.toStdString());
    append_log("Geometry exported: " + path);
  } catch (const std::exception& ex) {
    append_log(QString("Export failed: %1").arg(ex.what()));
  }
#endif
}

void GmshPanel::on_generate() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  try {
    ensure_gmsh();

    gmsh::option::setNumber("General.Terminal", 0);
    gmsh::logger::start();

    const double dx = size_x_->value();
    const double dy = size_y_->value();
    const double dz = size_z_->value();
    const double lc = mesh_size_->value();
    const int order = elem_order_->currentData().toInt();
    const int msh_version = msh_version_->currentData().toInt();

    gmsh::option::setNumber("Mesh.CharacteristicLengthMin", lc);
    gmsh::option::setNumber("Mesh.CharacteristicLengthMax", lc);
    gmsh::option::setNumber("Mesh.ElementOrder", order);
    if (high_order_opt_) {
      const int opt = high_order_opt_->currentData().toInt();
      if (opt > 0 && order > 1) {
        gmsh::option::setNumber("Mesh.HighOrderOptimize", opt);
      } else {
        gmsh::option::setNumber("Mesh.HighOrderOptimize", 0);
      }
    }
    if (algo2d_) {
      gmsh::option::setNumber("Mesh.Algorithm",
                              algo2d_->currentData().toInt());
    }
    if (algo3d_) {
      gmsh::option::setNumber("Mesh.Algorithm3D",
                              algo3d_->currentData().toInt());
    }
    if (recombine_) {
      gmsh::option::setNumber("Mesh.RecombineAll",
                              recombine_->isChecked() ? 1 : 0);
    }
    if (smoothing_) {
      gmsh::option::setNumber("Mesh.Smoothing", smoothing_->value());
    }
    gmsh::option::setNumber("Mesh.Optimize", optimize_->isChecked() ? 1 : 0);
    gmsh::option::setNumber("Mesh.MshFileVersion", msh_version == 2 ? 2.2 : 4.1);

    if (!model_loaded_ || (use_sample_box_ && use_sample_box_->isChecked())) {
      gmsh::clear();
      gmsh::model::add("box_model");
      const int box = gmsh::model::occ::addBox(0, 0, 0, dx, dy, dz);
      gmsh::model::occ::synchronize();

      const int phys = gmsh::model::addPhysicalGroup(3, {box});
      gmsh::model::setPhysicalName(3, phys, "solid");
      std::vector<std::pair<int, int>> faces;
      gmsh::model::getEntities(faces, 2);
      if (!faces.empty()) {
        std::vector<int> face_tags;
        face_tags.reserve(faces.size());
        for (const auto& f : faces) {
          face_tags.push_back(f.second);
        }
        const int bnd = gmsh::model::addPhysicalGroup(2, face_tags);
        gmsh::model::setPhysicalName(2, bnd, "boundary");
      }
      model_loaded_ = true;
      geo_path_->setText("sample: box");
      use_sample_box_->setChecked(true);
    } else {
      gmsh::model::mesh::clear();
    }

    int dim = infer_mesh_dim();
    if (mesh_dim_) {
      const int configured = mesh_dim_->currentData().toInt();
      if (configured >= 1 && configured <= 3) {
        if (configured <= dim) {
          dim = configured;
        } else {
          append_log(QString("Requested mesh dim %1 exceeds geometry dim %2, "
                            "fallback to %2.")
                         .arg(configured)
                         .arg(dim));
        }
      }
    }
    gmsh::model::mesh::generate(dim);
    const int boundary_dim = std::max(0, dim - 1);

    const QString out_path = output_path_->text();
    QDir().mkpath(QFileInfo(out_path).absolutePath());
    gmsh::write(out_path.toStdString());

    std::vector<std::pair<int, int>> phys_groups;
    gmsh::model::getPhysicalGroups(phys_groups);
    QStringList boundary_names;
    for (const auto& p : phys_groups) {
      if (p.first != boundary_dim) {
        continue;
      }
      std::string name;
      gmsh::model::getPhysicalName(p.first, p.second, name);
      if (name.empty()) {
        name = "boundary_" + std::to_string(p.second);
      }
      boundary_names << QString::fromStdString(name);
    }
    emit boundary_groups(boundary_names);

    QStringList volume_names;
    std::vector<std::pair<int, int>> vol_groups;
    gmsh::model::getPhysicalGroups(vol_groups, dim);
    for (const auto& g : vol_groups) {
      std::string name;
      gmsh::model::getPhysicalName(g.first, g.second, name);
      if (name.empty()) {
        name = "volume_" + std::to_string(g.second);
      }
      volume_names << QString::fromStdString(name);
    }
    emit volume_groups(volume_names);

    std::vector<std::string> log;
    gmsh::logger::get(log);
    gmsh::logger::stop();
    for (const auto& line : log) {
      append_log(QString::fromStdString(line));
    }

    append_log("Mesh written: " + out_path);
    emit mesh_written(out_path);

    std::vector<std::size_t> node_tags;
    std::vector<double> node_coords;
    std::vector<double> node_params;
    gmsh::model::mesh::getNodes(node_tags, node_coords, node_params);

    std::vector<int> element_types;
    std::vector<std::vector<std::size_t>> element_tags;
    std::vector<std::vector<std::size_t>> element_nodes;
    gmsh::model::mesh::getElements(element_types, element_tags, element_nodes);
    std::size_t elem_count = 0;
    std::vector<std::size_t> all_element_tags;
    for (const auto& tags : element_tags) {
      elem_count += tags.size();
      all_element_tags.insert(all_element_tags.end(), tags.begin(), tags.end());
    }

    append_log(QString("Nodes: %1, Elements: %2").arg(node_tags.size()).arg(elem_count));
    update_entity_summary();
    update_entity_list();
    update_physical_group_list();
    update_field_list();

    if (!all_element_tags.empty()) {
      try {
        std::vector<double> qualities;
        qualities.resize(all_element_tags.size());
        gmsh::model::mesh::getElementQualities(all_element_tags, qualities,
                                               "minSICN");
        double qmin = qualities.front();
        double qmax = qualities.front();
        double qsum = 0.0;
        for (double q : qualities) {
          qmin = std::min(qmin, q);
          qmax = std::max(qmax, q);
          qsum += q;
        }
        const double qmean = qsum / static_cast<double>(qualities.size());
        append_log(QString("Quality (minSICN) min=%1 mean=%2 max=%3")
                       .arg(qmin, 0, 'g', 6)
                       .arg(qmean, 0, 'g', 6)
                       .arg(qmax, 0, 'g', 6));
      } catch (const std::exception& ex) {
        append_log(QString("Quality report failed: %1").arg(ex.what()));
      }
    }

    try {
      std::vector<std::pair<int, int>> groups;
      gmsh::model::getPhysicalGroups(groups);
      if (!groups.empty()) {
        append_log("Physical group element counts:");
      }
      for (const auto& g : groups) {
        std::string name;
        gmsh::model::getPhysicalName(g.first, g.second, name);
        std::vector<int> ent_tags;
        gmsh::model::getEntitiesForPhysicalGroup(g.first, g.second, ent_tags);
        std::size_t count = 0;
        for (int ent : ent_tags) {
          std::vector<int> etypes;
          std::vector<std::vector<std::size_t>> etags;
          std::vector<std::vector<std::size_t>> enodes;
          gmsh::model::mesh::getElements(etypes, etags, enodes, g.first, ent);
          for (const auto& tags : etags) {
            count += tags.size();
          }
        }
        const QString label = name.empty()
                                  ? QString("%1:%2").arg(g.first).arg(g.second)
                                  : QString("%1:%2 %3")
                                        .arg(g.first)
                                        .arg(g.second)
                                        .arg(QString::fromStdString(name));
        append_log(QString("  %1 -> %2 elems").arg(label).arg(count));
      }
    } catch (const std::exception& ex) {
      append_log(QString("Physical group count failed: %1").arg(ex.what()));
    }
  } catch (const std::exception& ex) {
    append_log(QString("Gmsh error: %1").arg(ex.what()));
  }
#endif
}

void GmshPanel::on_add_primitive() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  try {
    ensure_gmsh();
    gmsh::logger::start();
    const QString kind = primitive_kind_->currentText();
    const double x = prim_x_->value();
    const double y = prim_y_->value();
    const double z = prim_z_->value();
    if (kind == "Box") {
      gmsh::model::occ::addBox(x, y, z, prim_dx_->value(), prim_dy_->value(),
                               prim_dz_->value());
    } else if (kind == "Cylinder") {
      gmsh::model::occ::addCylinder(x, y, z, prim_dx_->value(),
                                    prim_dy_->value(), prim_dz_->value(),
                                    prim_radius_->value());
    } else {
      gmsh::model::occ::addSphere(x, y, z, prim_radius_->value());
    }
    gmsh::model::occ::synchronize();
    model_loaded_ = true;
    use_sample_box_->setChecked(false);
    if (geo_path_->text().isEmpty() || geo_path_->text().startsWith("sample")) {
      geo_path_->setText("custom: primitives");
    }
    update_entity_summary();
    update_entity_list();
    refresh_occ_entity_template_lists();

    std::vector<std::string> log;
    gmsh::logger::get(log);
    gmsh::logger::stop();
    for (const auto& line : log) {
      append_log(QString::fromStdString(line));
    }
    append_log(QString("Primitive added: %1").arg(kind));
  } catch (const std::exception& ex) {
    append_log(QString("Gmsh error: %1").arg(ex.what()));
  }
#endif
}

void GmshPanel::on_apply_translate() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  try {
    ensure_gmsh();
    const int dim = transform_dim_->currentData().toInt();
    const auto tokens = parse_dim_tag_tokens(transform_ids_->text());
    auto tags = resolve_occ_dim_tags(dim, tokens);
    if (tags.empty()) {
      if (tokens.empty()) {
        append_log("Translate: no selectable entities for current dimension.");
      } else {
        append_log("Translate: no valid OCC entities in selection.");
      }
      return;
    }
    gmsh::model::occ::translate(tags, trans_dx_->value(), trans_dy_->value(),
                                trans_dz_->value());
    gmsh::model::occ::synchronize();
    update_entity_summary();
    update_entity_list();
    refresh_occ_entity_template_lists();
    append_log(QString("Translated %1 entities.").arg(tags.size()));
  } catch (const std::exception& ex) {
    append_log(QString("Translate failed: %1").arg(ex.what()));
  }
#endif
}

void GmshPanel::on_apply_rotate() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  try {
    ensure_gmsh();
    const int dim = transform_dim_->currentData().toInt();
    const auto tokens = parse_dim_tag_tokens(transform_ids_->text());
    auto tags = resolve_occ_dim_tags(dim, tokens);
    if (tags.empty()) {
      if (tokens.empty()) {
        append_log("Rotate: no selectable entities for current dimension.");
      } else {
        append_log("Rotate: no valid OCC entities in selection.");
      }
      return;
    }
    const double angle = rot_angle_->value() * 3.141592653589793 / 180.0;
    gmsh::model::occ::rotate(tags, rot_x_->value(), rot_y_->value(),
                             rot_z_->value(), rot_ax_->value(), rot_ay_->value(),
                             rot_az_->value(), angle);
    gmsh::model::occ::synchronize();
    update_entity_summary();
    update_entity_list();
    refresh_occ_entity_template_lists();
    append_log(QString("Rotated %1 entities.").arg(tags.size()));
  } catch (const std::exception& ex) {
    append_log(QString("Rotate failed: %1").arg(ex.what()));
  }
#endif
}

void GmshPanel::on_apply_scale() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  try {
    ensure_gmsh();
    const int dim = transform_dim_->currentData().toInt();
    const auto tokens = parse_dim_tag_tokens(transform_ids_->text());
    auto tags = resolve_occ_dim_tags(dim, tokens);
    if (tags.empty()) {
      if (tokens.empty()) {
        append_log("Scale: no selectable entities for current dimension.");
      } else {
        append_log("Scale: no valid OCC entities in selection.");
      }
      return;
    }
    gmsh::model::occ::dilate(tags, scale_cx_->value(), scale_cy_->value(),
                             scale_cz_->value(), scale_x_->value(),
                             scale_y_->value(), scale_z_->value());
    gmsh::model::occ::synchronize();
    update_entity_summary();
    update_entity_list();
    refresh_occ_entity_template_lists();
    append_log(QString("Scaled %1 entities.").arg(tags.size()));
  } catch (const std::exception& ex) {
    append_log(QString("Scale failed: %1").arg(ex.what()));
  }
#endif
}

void GmshPanel::on_apply_boolean_fuse() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  try {
    ensure_gmsh();
    const int dim = boolean_dim_->currentData().toInt();
    const auto obj_tokens = parse_dim_tag_tokens(boolean_obj_ids_->text());
    const auto tool_tokens = parse_dim_tag_tokens(boolean_tool_ids_->text());
    if (obj_tokens.empty() || tool_tokens.empty()) {
      append_log("Fuse: object/tool IDs required.");
      return;
    }
    auto obj_tags = resolve_occ_dim_tags(dim, obj_tokens);
    auto tool_tags = resolve_occ_dim_tags(dim, tool_tokens);
    if (obj_tags.empty() || tool_tags.empty()) {
      if (obj_tags.empty()) {
        append_log("Fuse: no valid OCC object entities.");
      }
      if (tool_tags.empty()) {
        append_log("Fuse: no valid OCC tool entities.");
      }
      return;
    }
    gmsh::vectorpair out_tags;
    std::vector<gmsh::vectorpair> out_map;
    gmsh::model::occ::fuse(obj_tags, tool_tags, out_tags, out_map, -1,
                           boolean_remove_obj_->isChecked(),
                           boolean_remove_tool_->isChecked());
    gmsh::model::occ::synchronize();
    update_entity_summary();
    update_entity_list();
    refresh_occ_entity_template_lists();
    append_log(QString("Fuse result: %1 entities.").arg(out_tags.size()));
  } catch (const std::exception& ex) {
    append_log(QString("Fuse failed: %1").arg(ex.what()));
  }
#endif
}

void GmshPanel::on_apply_boolean_cut() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  try {
    ensure_gmsh();
    const int dim = boolean_dim_->currentData().toInt();
    const auto obj_tokens = parse_dim_tag_tokens(boolean_obj_ids_->text());
    const auto tool_tokens = parse_dim_tag_tokens(boolean_tool_ids_->text());
    if (obj_tokens.empty() || tool_tokens.empty()) {
      append_log("Cut: object/tool IDs required.");
      return;
    }
    auto obj_tags = resolve_occ_dim_tags(dim, obj_tokens);
    auto tool_tags = resolve_occ_dim_tags(dim, tool_tokens);
    if (obj_tags.empty() || tool_tags.empty()) {
      if (obj_tags.empty()) {
        append_log("Cut: no valid OCC object entities.");
      }
      if (tool_tags.empty()) {
        append_log("Cut: no valid OCC tool entities.");
      }
      return;
    }
    gmsh::vectorpair out_tags;
    std::vector<gmsh::vectorpair> out_map;
    gmsh::model::occ::cut(obj_tags, tool_tags, out_tags, out_map, -1,
                          boolean_remove_obj_->isChecked(),
                          boolean_remove_tool_->isChecked());
    gmsh::model::occ::synchronize();
    update_entity_summary();
    update_entity_list();
    refresh_occ_entity_template_lists();
    append_log(QString("Cut result: %1 entities.").arg(out_tags.size()));
  } catch (const std::exception& ex) {
    append_log(QString("Cut failed: %1").arg(ex.what()));
  }
#endif
}

void GmshPanel::on_apply_boolean_intersect() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  try {
    ensure_gmsh();
    const int dim = boolean_dim_->currentData().toInt();
    const auto obj_tokens = parse_dim_tag_tokens(boolean_obj_ids_->text());
    const auto tool_tokens = parse_dim_tag_tokens(boolean_tool_ids_->text());
    if (obj_tokens.empty() || tool_tokens.empty()) {
      append_log("Intersect: object/tool IDs required.");
      return;
    }
    auto obj_tags = resolve_occ_dim_tags(dim, obj_tokens);
    auto tool_tags = resolve_occ_dim_tags(dim, tool_tokens);
    if (obj_tags.empty() || tool_tags.empty()) {
      if (obj_tags.empty()) {
        append_log("Intersect: no valid OCC object entities.");
      }
      if (tool_tags.empty()) {
        append_log("Intersect: no valid OCC tool entities.");
      }
      return;
    }
    gmsh::vectorpair out_tags;
    std::vector<gmsh::vectorpair> out_map;
    gmsh::model::occ::intersect(obj_tags, tool_tags, out_tags, out_map, -1,
                                boolean_remove_obj_->isChecked(),
                                boolean_remove_tool_->isChecked());
    gmsh::model::occ::synchronize();
    update_entity_summary();
    update_entity_list();
    refresh_occ_entity_template_lists();
    append_log(QString("Intersect result: %1 entities.").arg(out_tags.size()));
  } catch (const std::exception& ex) {
    append_log(QString("Intersect failed: %1").arg(ex.what()));
  }
#endif
}

void GmshPanel::on_entity_dim_changed(int) {
  update_entity_list();
}

void GmshPanel::on_physical_group_selected(int) {
#ifndef GMP_ENABLE_GMSH_GUI
  return;
#else
  ensure_gmsh();
  if (!gmsh_ready_) {
    return;
  }
  if (!phys_group_list_) {
    return;
  }
  const QString key = phys_group_list_->currentData().toString();
  if (key.isEmpty()) {
    phys_group_name_->clear();
    phys_group_entities_->clear();
    emit physical_group_selected(-1, -1);
    return;
  }
  const QStringList parts = key.split(":");
  if (parts.size() != 2) {
    return;
  }
  bool ok_dim = false;
  bool ok_tag = false;
  const int dim = parts[0].toInt(&ok_dim);
  const int tag = parts[1].toInt(&ok_tag);
  if (!ok_dim || !ok_tag) {
    return;
  }
  emit physical_group_selected(dim, tag);
  if (phys_group_table_) {
    for (int row = 0; row < phys_group_table_->rowCount(); ++row) {
      bool row_ok_dim = false;
      bool row_ok_tag = false;
      const int row_dim =
          phys_group_table_->item(row, 0)->text().toInt(&row_ok_dim);
      const int row_tag =
          phys_group_table_->item(row, 1)->text().toInt(&row_ok_tag);
      if (row_ok_dim && row_ok_tag && row_dim == dim && row_tag == tag) {
        phys_group_table_->setCurrentCell(row, 0);
        break;
      }
    }
  }
  if (phys_group_dim_) {
    const int idx = phys_group_dim_->findData(dim);
    if (idx >= 0) {
      phys_group_dim_->setCurrentIndex(idx);
    }
  }
  std::string name;
  gmsh::model::getPhysicalName(dim, tag, name);
  if (phys_group_name_) {
    phys_group_name_->setText(QString::fromStdString(name));
  }
  std::vector<int> ent_tags;
  gmsh::model::getEntitiesForPhysicalGroup(dim, tag, ent_tags);
  QStringList ids;
  for (int t : ent_tags) {
    ids << QString::number(t);
  }
  if (phys_group_entities_) {
    phys_group_entities_->setText(ids.join(", "));
  }
#endif
}

void GmshPanel::select_physical_group(int dim, int tag) {
#ifndef GMP_ENABLE_GMSH_GUI
  Q_UNUSED(dim);
  Q_UNUSED(tag);
  return;
#else
  if (!phys_group_list_) {
    return;
  }
  const QString key = QString("%1:%2").arg(dim).arg(tag);
  const int idx = phys_group_list_->findData(key);
  if (idx >= 0) {
    phys_group_list_->setCurrentIndex(idx);
  } else {
    phys_group_list_->setCurrentIndex(0);
  }
#endif
}

void GmshPanel::apply_entity_pick(int dim, int tag) {
#ifndef GMP_ENABLE_GMSH_GUI
  Q_UNUSED(dim);
  Q_UNUSED(tag);
  return;
#else
  if (!active_entity_input_) {
    append_log("Pick: focus an entity input field first.");
    return;
  }
  int dim_filter = -1;
  if (active_entity_input_ == transform_ids_ && transform_dim_) {
    dim_filter = transform_dim_->currentData().toInt();
  } else if (active_entity_input_ == boolean_obj_ids_ && boolean_dim_) {
    dim_filter = boolean_dim_->currentData().toInt();
  } else if (active_entity_input_ == boolean_tool_ids_ && boolean_dim_) {
    dim_filter = boolean_dim_->currentData().toInt();
  } else if (active_entity_input_ == phys_group_entities_ && phys_group_dim_) {
    dim_filter = phys_group_dim_->currentData().toInt();
  } else if (active_entity_input_ == field_entities_ && field_dim_) {
    dim_filter = field_dim_->currentData().toInt();
  } else if (active_entity_input_ == entity_size_ids_ && entity_size_dim_) {
    dim_filter = entity_size_dim_->currentData().toInt();
  }

  const bool same_dim = dim_filter >= 0 && dim_filter == dim;
  const QString token = same_dim ? QString::number(tag)
                                 : QString("%1:%2").arg(dim).arg(tag);
  const QString text = active_entity_input_->text();
  QStringList parts =
      text.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
  QSet<QString> existing;
  for (const auto& part : parts) {
    if (part.contains(":")) {
      existing.insert(part);
    } else if (dim_filter >= 0) {
      existing.insert(QString("%1:%2").arg(dim_filter).arg(part));
    } else {
      existing.insert(part);
    }
  }
  const QString key = same_dim
                          ? QString("%1:%2").arg(dim_filter).arg(tag)
                          : QString("%1:%2").arg(dim).arg(tag);
  if (!existing.contains(key)) {
    parts << token;
  }
  active_entity_input_->setText(parts.join(", "));
  active_entity_input_->setFocus();
#endif
}

void GmshPanel::on_physical_group_add() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  try {
    ensure_gmsh();
    const int dim = phys_group_dim_->currentData().toInt();
    const auto tags = resolve_entity_tags(dim, phys_group_entities_->text());
    if (tags.empty()) {
      append_log("Physical group add: no valid entities.");
      return;
    }
    const std::string name = phys_group_name_->text().toStdString();
    const int group_tag = gmsh::model::addPhysicalGroup(dim, tags, -1, name);
    if (!name.empty()) {
      gmsh::model::setPhysicalName(dim, group_tag, name);
    }
    update_physical_group_list();
    append_log(QString("Physical group added: %1:%2").arg(dim).arg(group_tag));
  } catch (const std::exception& ex) {
    append_log(QString("Physical group add failed: %1").arg(ex.what()));
  }
#endif
}

void GmshPanel::on_physical_group_update() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  try {
    ensure_gmsh();
    const QString key = phys_group_list_->currentData().toString();
    const QStringList parts = key.split(":");
    if (parts.size() != 2) {
      append_log("Update: select a physical group.");
      return;
    }
    bool ok_dim = false;
    bool ok_tag = false;
    const int dim = parts[0].toInt(&ok_dim);
    const int tag = parts[1].toInt(&ok_tag);
    if (!ok_dim || !ok_tag) {
      append_log("Update: invalid group selection.");
      return;
    }
    const auto tags = resolve_entity_tags(dim, phys_group_entities_->text());
    if (tags.empty()) {
      append_log("Update: no valid entities.");
      return;
    }
    gmsh::model::removePhysicalGroups({{dim, tag}});
    const std::string name = phys_group_name_->text().toStdString();
    gmsh::model::addPhysicalGroup(dim, tags, tag, name);
    if (!name.empty()) {
      gmsh::model::setPhysicalName(dim, tag, name);
    }
    update_physical_group_list();
    append_log(QString("Physical group updated: %1:%2").arg(dim).arg(tag));
  } catch (const std::exception& ex) {
    append_log(QString("Physical group update failed: %1").arg(ex.what()));
  }
#endif
}

void GmshPanel::on_physical_group_delete() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  try {
    ensure_gmsh();
    const QString key = phys_group_list_->currentData().toString();
    const QStringList parts = key.split(":");
    if (parts.size() != 2) {
      append_log("Delete: select a physical group.");
      return;
    }
    bool ok_dim = false;
    bool ok_tag = false;
    const int dim = parts[0].toInt(&ok_dim);
    const int tag = parts[1].toInt(&ok_tag);
    if (!ok_dim || !ok_tag) {
      append_log("Delete: invalid group selection.");
      return;
    }
    gmsh::model::removePhysicalGroups({{dim, tag}});
    update_physical_group_list();
    phys_group_name_->clear();
    phys_group_entities_->clear();
    append_log(QString("Physical group deleted: %1:%2").arg(dim).arg(tag));
  } catch (const std::exception& ex) {
    append_log(QString("Physical group delete failed: %1").arg(ex.what()));
  }
#endif
}

void GmshPanel::on_physical_group_refresh() {
  update_physical_group_list();
}

void GmshPanel::on_field_apply() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  try {
    ensure_gmsh();
    const int dim = field_dim_->currentData().toInt();
    const auto tags = resolve_entity_tags(dim, field_entities_->text());
    if (tags.empty()) {
      append_log("Field: no valid entities.");
      return;
    }
    std::vector<double> list;
    list.reserve(tags.size());
    for (int t : tags) {
      list.push_back(static_cast<double>(t));
    }

    const int dist = gmsh::model::mesh::field::add("Distance");
    if (dim == 1) {
      gmsh::model::mesh::field::setNumbers(dist, "EdgesList", list);
    } else if (dim == 2) {
      gmsh::model::mesh::field::setNumbers(dist, "FacesList", list);
    } else {
      gmsh::model::mesh::field::setNumbers(dist, "VolumesList", list);
    }

    const int thr = gmsh::model::mesh::field::add("Threshold");
    gmsh::model::mesh::field::setNumber(thr, "InField", dist);
    gmsh::model::mesh::field::setNumber(thr, "SizeMin",
                                        field_size_min_->value());
    gmsh::model::mesh::field::setNumber(thr, "SizeMax",
                                        field_size_max_->value());
    gmsh::model::mesh::field::setNumber(thr, "DistMin",
                                        field_dist_min_->value());
    gmsh::model::mesh::field::setNumber(thr, "DistMax",
                                        field_dist_max_->value());
    gmsh::model::mesh::field::setAsBackgroundMesh(thr);

    update_field_list();
    append_log(QString("Field applied: Distance=%1 Threshold=%2")
                   .arg(dist)
                   .arg(thr));
  } catch (const std::exception& ex) {
    append_log(QString("Field apply failed: %1").arg(ex.what()));
  }
#endif
}

void GmshPanel::on_field_clear() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  try {
    ensure_gmsh();
    std::vector<int> tags;
    gmsh::model::mesh::field::list(tags);
    for (int tag : tags) {
      gmsh::model::mesh::field::remove(tag);
    }
    update_field_list();
    append_log("All mesh fields cleared.");
  } catch (const std::exception& ex) {
    append_log(QString("Field clear failed: %1").arg(ex.what()));
  }
#endif
}

void GmshPanel::on_field_refresh() {
  update_field_list();
}

void GmshPanel::ensure_gmsh() {
#ifdef GMP_ENABLE_GMSH_GUI
  if (!gmsh_ready_) {
    gmsh::initialize();
    gmsh_ready_ = true;
    append_log("Gmsh initialized.");
  }
#endif
}

void GmshPanel::update_entity_summary() {
#ifdef GMP_ENABLE_GMSH_GUI
  if (!entity_summary_) {
    return;
  }
  if (!gmsh_ready_) {
    entity_summary_->setText("Entities: 0P / 0C / 0S / 0V");
    return;
  }
  std::vector<std::pair<int, int>> entities;
  gmsh::model::getEntities(entities);
  int counts[4] = {0, 0, 0, 0};
  for (const auto& e : entities) {
    if (e.first >= 0 && e.first < 4) {
      counts[e.first] += 1;
    }
  }
  entity_summary_->setText(
      QString("Entities: %1P / %2C / %3S / %4V")
          .arg(counts[0])
          .arg(counts[1])
          .arg(counts[2])
          .arg(counts[3]));
#else
  if (entity_summary_) {
    entity_summary_->setText("Entities: 0P / 0C / 0S / 0V");
  }
#endif
}

void GmshPanel::update_entity_list() {
#ifdef GMP_ENABLE_GMSH_GUI
  if (!entity_list_) {
    return;
  }
  if (!gmsh_ready_) {
    entity_list_->setPlainText("No model.");
    return;
  }
  const int dim_filter = entity_dim_ ? entity_dim_->currentData().toInt() : -1;
  std::vector<std::pair<int, int>> entities;
  if (dim_filter >= 0) {
    gmsh::model::getEntities(entities, dim_filter);
  } else {
    gmsh::model::getEntities(entities);
  }
  std::vector<int> by_dim[4];
  for (const auto& e : entities) {
    if (e.first >= 0 && e.first < 4) {
      by_dim[e.first].push_back(e.second);
    }
  }
  for (int d = 0; d < 4; ++d) {
    std::sort(by_dim[d].begin(), by_dim[d].end());
  }

  QStringList lines;
  auto format_list = [](const std::vector<int>& ids) {
    QStringList out;
    for (int id : ids) {
      out << QString::number(id);
    }
    return out.join(", ");
  };
  if (dim_filter >= 0) {
    lines << QString("dim %1: %2")
                 .arg(dim_filter)
                 .arg(format_list(by_dim[dim_filter]));
  } else {
    for (int d = 0; d < 4; ++d) {
      lines << QString("dim %1: %2").arg(d).arg(format_list(by_dim[d]));
    }
  }
  entity_list_->setPlainText(lines.join("\n"));
#else
  if (entity_list_) {
    entity_list_->setPlainText("No model.");
  }
#endif
}

void GmshPanel::update_physical_group_list() {
#ifdef GMP_ENABLE_GMSH_GUI
  if (!phys_group_list_) {
    return;
  }
  if (!gmsh_ready_) {
    phys_group_list_->clear();
    phys_group_list_->addItem("New", "");
    return;
  }
  const QString current = phys_group_list_->currentData().toString();
  phys_group_list_->blockSignals(true);
  phys_group_list_->clear();
  phys_group_list_->addItem("New", "");
  std::vector<std::pair<int, int>> groups;
  gmsh::model::getPhysicalGroups(groups);
  for (const auto& g : groups) {
    std::string name;
    gmsh::model::getPhysicalName(g.first, g.second, name);
    const QString label = name.empty()
                              ? QString("%1:%2").arg(g.first).arg(g.second)
                              : QString("%1:%2 %3")
                                    .arg(g.first)
                                    .arg(g.second)
                                    .arg(QString::fromStdString(name));
    const QString key = QString("%1:%2").arg(g.first).arg(g.second);
    phys_group_list_->addItem(label, key);
  }
  int idx = phys_group_list_->findData(current);
  if (idx < 0) {
    idx = 0;
  }
  phys_group_list_->setCurrentIndex(idx);
  phys_group_list_->blockSignals(false);

  QStringList boundary_names;
  std::vector<std::pair<int, int>> bnd_groups;
  const int boundary_dim = std::max(0, infer_mesh_dim() - 1);
  gmsh::model::getPhysicalGroups(bnd_groups, boundary_dim);
  for (const auto& g : bnd_groups) {
    std::string name;
    gmsh::model::getPhysicalName(g.first, g.second, name);
    if (name.empty()) {
      name = "boundary_" + std::to_string(g.second);
    }
    boundary_names << QString::fromStdString(name);
  }
  emit boundary_groups(boundary_names);

  QStringList volume_names;
  std::vector<std::pair<int, int>> vol_groups;
  const int volume_dim = std::max(0, infer_mesh_dim());
  gmsh::model::getPhysicalGroups(vol_groups, volume_dim);
  for (const auto& g : vol_groups) {
    std::string name;
    gmsh::model::getPhysicalName(g.first, g.second, name);
    if (name.empty()) {
      name = "volume_" + std::to_string(g.second);
    }
    volume_names << QString::fromStdString(name);
  }
  emit volume_groups(volume_names);

  update_physical_group_table();
#else
  if (phys_group_list_) {
    phys_group_list_->clear();
    phys_group_list_->addItem("New", "");
  }
#endif
}

void GmshPanel::update_physical_group_table() {
#ifdef GMP_ENABLE_GMSH_GUI
  if (!phys_group_table_) {
    return;
  }
  if (!gmsh_ready_) {
    phys_group_table_->setRowCount(0);
    return;
  }
  std::vector<std::pair<int, int>> groups;
  try {
    gmsh::model::getPhysicalGroups(groups);
  } catch (const std::exception& ex) {
    append_log(QString("Physical group list failed: %1").arg(ex.what()));
    phys_group_table_->setRowCount(0);
    return;
  }

  const QString current =
      phys_group_list_ ? phys_group_list_->currentData().toString() : QString();
  int selected_row = -1;

  phys_group_table_->blockSignals(true);
  phys_group_table_->setRowCount(static_cast<int>(groups.size()));
  for (size_t i = 0; i < groups.size(); ++i) {
    const auto& g = groups[i];
    std::string name;
    gmsh::model::getPhysicalName(g.first, g.second, name);
    std::vector<int> ent_tags;
    gmsh::model::getEntitiesForPhysicalGroup(g.first, g.second, ent_tags);
    std::size_t elem_count = 0;
    for (int ent : ent_tags) {
      std::vector<int> etypes;
      std::vector<std::vector<std::size_t>> etags;
      std::vector<std::vector<std::size_t>> enodes;
      gmsh::model::mesh::getElements(etypes, etags, enodes, g.first, ent);
      for (const auto& tags : etags) {
        elem_count += tags.size();
      }
    }

    const int row = static_cast<int>(i);
    phys_group_table_->setItem(row, 0,
                               new QTableWidgetItem(QString::number(g.first)));
    phys_group_table_->setItem(row, 1,
                               new QTableWidgetItem(QString::number(g.second)));
    const QString name_text = name.empty()
                                  ? QString("(unnamed)")
                                  : QString::fromStdString(name);
    phys_group_table_->setItem(row, 2, new QTableWidgetItem(name_text));
    phys_group_table_->setItem(
        row, 3, new QTableWidgetItem(QString::number(ent_tags.size())));
    phys_group_table_->setItem(
        row, 4, new QTableWidgetItem(QString::number(elem_count)));

    const QString key = QString("%1:%2").arg(g.first).arg(g.second);
    if (!current.isEmpty() && key == current) {
      selected_row = row;
    }
  }

  if (selected_row >= 0) {
    phys_group_table_->setCurrentCell(selected_row, 0);
  }
  phys_group_table_->resizeColumnsToContents();
  phys_group_table_->blockSignals(false);
#else
  if (phys_group_table_) {
    phys_group_table_->setRowCount(0);
  }
#endif
}

void GmshPanel::update_field_list() {
#ifdef GMP_ENABLE_GMSH_GUI
  if (!field_list_) {
    return;
  }
  if (!gmsh_ready_) {
    field_list_->setPlainText("No model.");
    return;
  }
  std::vector<int> tags;
  gmsh::model::mesh::field::list(tags);
  QStringList lines;
  for (int tag : tags) {
    std::string type;
    gmsh::model::mesh::field::getType(tag, type);
    lines << QString("%1: %2")
                 .arg(tag)
                 .arg(QString::fromStdString(type));
  }
  if (lines.isEmpty()) {
    field_list_->setPlainText("No fields.");
  } else {
    field_list_->setPlainText(lines.join("\n"));
  }
#else
  if (field_list_) {
    field_list_->setPlainText("No fields.");
  }
#endif
}

void GmshPanel::update_geometry_controls() {
  const bool use_sample = use_sample_box_ && use_sample_box_->isChecked();
  size_x_->setEnabled(use_sample);
  size_y_->setEnabled(use_sample);
  size_z_->setEnabled(use_sample);
}

void GmshPanel::update_primitive_controls() {
  const QString kind = primitive_kind_ ? primitive_kind_->currentText() : "Box";
  const bool box = kind == "Box";
  const bool cyl = kind == "Cylinder";
  const bool sph = kind == "Sphere";
  if (prim_dx_) {
    prim_dx_->setEnabled(box || cyl);
  }
  if (prim_dy_) {
    prim_dy_->setEnabled(box || cyl);
  }
  if (prim_dz_) {
    prim_dz_->setEnabled(box || cyl);
  }
  if (prim_radius_) {
    prim_radius_->setEnabled(cyl || sph);
  }
}

int GmshPanel::infer_mesh_dim() const {
#ifdef GMP_ENABLE_GMSH_GUI
  std::vector<std::pair<int, int>> ents;
  gmsh::model::getEntities(ents);
  int max_dim = 0;
  for (const auto& e : ents) {
    if (e.first > max_dim) {
      max_dim = e.first;
    }
  }
  if (max_dim < 1) {
    max_dim = 1;
  }
  if (max_dim > 3) {
    max_dim = 3;
  }
  return max_dim;
#else
  return 3;
#endif
}

void GmshPanel::append_entity_template(QLineEdit* target,
                                       const QString& token) {
  if (!target) {
    return;
  }
  const QString trimmed = token.trimmed();
  if (trimmed.isEmpty() || trimmed == "Templates") {
    return;
  }
  QStringList values = target->text().split(QRegularExpression("[,\\s]+"),
                                           Qt::SkipEmptyParts);
  if (values.isEmpty()) {
    target->setText(trimmed);
    return;
  }
  if (!values.contains(trimmed)) {
    values << trimmed;
  }
  target->setText(values.join(", "));
  target->setFocus();
}

QStringList GmshPanel::invalid_entity_tokens(const QString& text, int dim_filter,
                                            bool occ_only) const {
  QStringList invalid;
#ifdef GMP_ENABLE_GMSH_GUI
  if (!gmsh_ready_) {
    return invalid;
  }
  const QString normalized = text.trimmed();
  if (normalized.isEmpty()) {
    return invalid;
  }

  std::vector<std::pair<int, int>> entities;
  if (occ_only) {
    if (dim_filter >= 0) {
      gmsh::model::occ::getEntities(entities, dim_filter);
    } else {
      for (int dim = 0; dim <= 3; ++dim) {
        std::vector<std::pair<int, int>> ents;
        gmsh::model::occ::getEntities(ents, dim);
        entities.insert(entities.end(), ents.begin(), ents.end());
      }
    }
  } else {
    if (dim_filter >= 0) {
      gmsh::model::getEntities(entities, dim_filter);
    } else {
      gmsh::model::getEntities(entities);
    }
  }

  std::unordered_set<long long> existing;
  existing.reserve(entities.size());
  for (const auto& e : entities) {
    const long long key =
        (static_cast<long long>(e.first) << 32) |
        static_cast<unsigned int>(e.second);
    existing.insert(key);
  }

  const QStringList parts =
      normalized.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
  for (const auto& part : parts) {
    const QString token = part.trimmed();
    if (token.isEmpty()) {
      continue;
    }
    const int colon = token.indexOf(':');
    bool ok = true;
    int dim = dim_filter;
    int tag = 0;
    if (colon >= 0) {
      const int second_colon = token.indexOf(':', colon + 1);
      if (second_colon >= 0) {
        ok = false;
      } else {
        const QString dim_text = token.left(colon).trimmed();
        const QString tag_text = token.mid(colon + 1).trimmed();
        if (dim_text.isEmpty() || tag_text.isEmpty()) {
          ok = false;
        } else {
          bool ok_dim = false;
          bool ok_tag = false;
          dim = dim_text.toInt(&ok_dim);
          tag = tag_text.toInt(&ok_tag);
          if (!ok_dim || !ok_tag || dim < 0 || dim > 3 || tag <= 0) {
            ok = false;
          }
        }
      }
    } else {
      bool ok_tag = false;
      tag = token.toInt(&ok_tag);
      if (!ok_tag || tag <= 0) {
        ok = false;
      }
    }
    if (!ok) {
      invalid << token;
      continue;
    }
    if (!existing.count((static_cast<long long>(dim) << 32) |
                       static_cast<unsigned int>(tag))) {
      invalid << token;
    }
  }
#endif
  return invalid;
}

void GmshPanel::validate_entity_input(QLineEdit* input, int dim_filter,
                                     bool occ_only) {
  if (!input) {
    return;
  }
  const QStringList invalid = invalid_entity_tokens(input->text(), dim_filter,
                                                  occ_only);
  if (invalid.isEmpty()) {
    input->setStyleSheet(QString());
    input->setToolTip(QString());
    return;
  }
  input->setStyleSheet(
      "QLineEdit{border: 1px solid #d32f2f; background: #fff5f6;}");
  input->setToolTip(
      QString("Invalid entities: %1").arg(invalid.join(", ")));
}

static void fill_entity_template_combo(QComboBox* combo, int dim_filter,
                                      int max_items = 20) {
  if (!combo) {
    return;
  }
  combo->clear();
  combo->addItem("Templates");

#ifdef GMP_ENABLE_GMSH_GUI
  try {
    std::vector<std::pair<int, int>> entities;
    if (dim_filter >= 0) {
      gmsh::model::occ::getEntities(entities, dim_filter);
    } else {
      for (int dim = 0; dim <= 3; ++dim) {
        std::vector<std::pair<int, int>> ents;
        gmsh::model::occ::getEntities(ents, dim);
        entities.insert(entities.end(), ents.begin(), ents.end());
      }
    }
    if (!entities.empty()) {
      std::sort(entities.begin(), entities.end(),
                [](const std::pair<int, int>& a,
                   const std::pair<int, int>& b) {
                  if (a.first != b.first) {
                    return a.first < b.first;
                  }
                  return a.second < b.second;
                });
      int added = 0;
      for (const auto& e : entities) {
        if (added >= max_items) {
          break;
        }
        combo->addItem(QString("%1:%2").arg(e.first).arg(e.second));
        ++added;
      }
      if (!entities.empty()) {
        return;
      }
    }
  } catch (...) {
    // Fall through to defaults.
  }
#endif

  combo->addItem("1:1");
  combo->addItem("2:1");
  combo->addItem("3:1");
  combo->addItem("2:1, 3:1");
  combo->addItem("0:1");
  combo->addItem("1:1,1:2");
}


void GmshPanel::populate_transform_entity_templates(int dim_filter) {
  fill_entity_template_combo(transform_template_, dim_filter, 24);
}

void GmshPanel::populate_boolean_entity_templates(int dim_filter) {
  fill_entity_template_combo(boolean_obj_template_, dim_filter, 24);
  fill_entity_template_combo(boolean_tool_template_, dim_filter, 24);
}

void GmshPanel::refresh_occ_entity_template_lists() {
  const int transform_dim =
      transform_dim_ ? transform_dim_->currentData().toInt() : -1;
  const int boolean_dim =
      boolean_dim_ ? boolean_dim_->currentData().toInt() : 3;
  populate_transform_entity_templates(transform_dim);
  populate_boolean_entity_templates(boolean_dim);
  validate_entity_input(transform_ids_, transform_dim, true);
  validate_entity_input(boolean_obj_ids_, boolean_dim, true);
  validate_entity_input(boolean_tool_ids_, boolean_dim, true);
  validate_entity_input(phys_group_entities_,
                        phys_group_dim_ ? phys_group_dim_->currentData().toInt()
                                       : -1,
                        false);
  validate_entity_input(field_entities_,
                        field_dim_ ? field_dim_->currentData().toInt() : -1,
                        false);
  validate_entity_input(entity_size_ids_,
                        entity_size_dim_ ? entity_size_dim_->currentData().toInt()
                                        : -1,
                        false);
}

std::vector<int> GmshPanel::resolve_entity_tags(int dim_filter,
                                                const QString& text) const {
  std::vector<int> tags;
#ifdef GMP_ENABLE_GMSH_GUI
  const auto tokens = parse_dim_tag_tokens(text);
  auto pairs = resolve_dim_tags(dim_filter, tokens);
  if (pairs.empty() && dim_filter >= 0 && tokens.empty()) {
    gmsh::vectorpair ents;
    gmsh::model::getEntities(ents, dim_filter);
    pairs = ents;
  }
  std::set<int> unique;
  for (const auto& p : pairs) {
    if (dim_filter < 0 || p.first == dim_filter) {
      unique.insert(p.second);
    }
  }
  tags.assign(unique.begin(), unique.end());
#else
  (void)dim_filter;
  (void)text;
#endif
  return tags;
}

QString GmshPanel::pick_entities_dialog(int dim_filter,
                                        const QString& title,
                                        const QString& current_text) {
#ifdef GMP_ENABLE_GMSH_GUI
  if (!gmsh_ready_) {
    return current_text;
  }
  ensure_gmsh();
  QDialog dialog(this);
  dialog.setWindowTitle(title);
  dialog.resize(420, 360);
  auto* layout = new QVBoxLayout(&dialog);

  auto* list = new QListWidget();
  list->setSelectionMode(QAbstractItemView::NoSelection);
  layout->addWidget(list, 1);

  QSet<QString> preselect;
  const auto tokens = parse_dim_tag_tokens(current_text);
  const auto pairs = resolve_dim_tags(dim_filter, tokens);
  for (const auto& p : pairs) {
    preselect.insert(QString("%1:%2").arg(p.first).arg(p.second));
  }

  std::vector<std::pair<int, int>> entities;
  if (dim_filter >= 0) {
    gmsh::model::getEntities(entities, dim_filter);
  } else {
    gmsh::model::getEntities(entities);
  }
  std::sort(entities.begin(), entities.end());
  for (const auto& e : entities) {
    const QString key = QString("%1:%2").arg(e.first).arg(e.second);
    auto* item = new QListWidgetItem(key, list);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(preselect.contains(key) ? Qt::Checked
                                                : Qt::Unchecked);
  }

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                       QDialogButtonBox::Cancel);
  auto* select_all = new QPushButton("All");
  auto* clear_all = new QPushButton("Clear");
  buttons->addButton(select_all, QDialogButtonBox::ActionRole);
  buttons->addButton(clear_all, QDialogButtonBox::ActionRole);
  layout->addWidget(buttons);

  connect(select_all, &QPushButton::clicked, list, [list]() {
    for (int i = 0; i < list->count(); ++i) {
      list->item(i)->setCheckState(Qt::Checked);
    }
  });
  connect(clear_all, &QPushButton::clicked, list, [list]() {
    for (int i = 0; i < list->count(); ++i) {
      list->item(i)->setCheckState(Qt::Unchecked);
    }
  });
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted) {
    return current_text;
  }

  QStringList selected;
  for (int i = 0; i < list->count(); ++i) {
    auto* item = list->item(i);
    if (item->checkState() != Qt::Checked) {
      continue;
    }
    const QString key = item->text();
    if (dim_filter >= 0) {
      const int colon = key.indexOf(':');
      if (colon > 0) {
        selected << key.mid(colon + 1);
      }
    } else {
      selected << key;
    }
  }
  return selected.join(", ");
#else
  Q_UNUSED(dim_filter);
  Q_UNUSED(title);
  return current_text;
#endif
}

std::vector<GmshPanel::DimTagToken> GmshPanel::parse_dim_tag_tokens(
    const QString& text) const {
  QString cleaned = text;
  cleaned.replace(",", " ");
  const QStringList parts =
      cleaned.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
  std::vector<DimTagToken> tokens;
  tokens.reserve(parts.size());
  for (const auto& part : parts) {
    const int colon = part.indexOf(':');
    if (colon > 0) {
      bool ok_dim = false;
      bool ok_tag = false;
      const int dim = part.left(colon).toInt(&ok_dim);
      const int tag = part.mid(colon + 1).toInt(&ok_tag);
      if (ok_dim && ok_tag) {
        tokens.push_back({dim, tag, true});
      }
    } else {
      bool ok = false;
      const int tag = part.toInt(&ok);
      if (ok) {
        tokens.push_back({-1, tag, false});
      }
    }
  }
  return tokens;
}

std::vector<std::pair<int, int>> GmshPanel::resolve_dim_tags(
    int dim_filter, const std::vector<DimTagToken>& tokens) const {
  std::vector<std::pair<int, int>> tags;
#ifdef GMP_ENABLE_GMSH_GUI
  if (!gmsh_ready_) {
    return tags;
  }
  std::vector<std::pair<int, int>> entities;
  gmsh::model::getEntities(entities);
  std::unordered_map<int, std::vector<int>> tag_dims;
  std::unordered_set<long long> existing;
  tag_dims.reserve(entities.size());
  existing.reserve(entities.size());
  for (const auto& e : entities) {
    tag_dims[e.second].push_back(e.first);
    const long long key =
        (static_cast<long long>(e.first) << 32) | static_cast<unsigned int>(e.second);
    existing.insert(key);
  }

  if (tokens.empty()) {
    if (dim_filter >= 0) {
      gmsh::model::getEntities(tags, dim_filter);
    } else {
      tags = entities;
    }
    return tags;
  }

  std::set<std::pair<int, int>> out;
  for (const auto& token : tokens) {
    if (token.has_dim) {
      const long long key =
          (static_cast<long long>(token.dim) << 32) | static_cast<unsigned int>(token.tag);
      if (existing.count(key)) {
        out.emplace(token.dim, token.tag);
      }
      continue;
    }

    if (dim_filter >= 0) {
      const long long key =
          (static_cast<long long>(dim_filter) << 32) | static_cast<unsigned int>(token.tag);
      if (existing.count(key)) {
        out.emplace(dim_filter, token.tag);
      }
      continue;
    }

    auto it = tag_dims.find(token.tag);
    if (it != tag_dims.end()) {
      for (int dim : it->second) {
        out.emplace(dim, token.tag);
      }
    }
  }

  tags.assign(out.begin(), out.end());
  return tags;
#else
  (void)dim_filter;
  (void)tokens;
#endif
  return tags;
}

std::vector<std::pair<int, int>> GmshPanel::resolve_occ_dim_tags(
    int dim_filter, const std::vector<DimTagToken>& tokens) const {
#ifdef GMP_ENABLE_GMSH_GUI
  std::vector<std::pair<int, int>> tags;
  if (!gmsh_ready_) {
    return tags;
  }

  auto make_key = [](int dim, int tag) -> long long {
    return (static_cast<long long>(dim) << 32) |
           static_cast<unsigned int>(tag);
  };

  std::vector<std::pair<int, int>> occ_entities;
  if (dim_filter >= 0) {
    gmsh::model::occ::getEntities(occ_entities, dim_filter);
  } else {
    for (int dim = 0; dim <= 3; ++dim) {
      std::vector<std::pair<int, int>> ents;
      gmsh::model::occ::getEntities(ents, dim);
      occ_entities.insert(occ_entities.end(), ents.begin(), ents.end());
    }
  }

  std::unordered_set<long long> occ_keys;
  occ_keys.reserve(occ_entities.size());
  for (const auto& e : occ_entities) {
    occ_keys.insert(make_key(e.first, e.second));
  }

  if (tokens.empty()) {
    return occ_entities;
  }

  std::set<std::pair<int, int>> out;
  auto resolved = resolve_dim_tags(dim_filter, tokens);
  for (const auto& p : resolved) {
    if (occ_keys.count(make_key(p.first, p.second)) > 0) {
      out.emplace(p.first, p.second);
    }
  }

  tags.assign(out.begin(), out.end());
  return tags;
#else
  (void)dim_filter;
  (void)tokens;
  return {};
#endif
}

void GmshPanel::append_log(const QString& text) {
  if (log_) {
    log_->appendPlainText(text);
  }
}

}  // namespace gmp
