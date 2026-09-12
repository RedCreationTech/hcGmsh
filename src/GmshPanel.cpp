#include "gmp/GmshPanel.h"
#include "gmp/L10n.h"
#include "gmp/OperationLog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include "gmp/ComboPopupFix.h"
#include <QEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QApplication>
#include <QEventLoop>
#include <QModelIndex>
#include <QObject>
#include <QMessageBox>
#include <QProgressDialog>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
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

#ifdef GMP_ENABLE_GMSH_GUI
std::atomic<unsigned long long> generated_mesh_model_sequence{0};

struct StructuredMeshEligibility {
  bool eligible = false;
  QString reason;
  std::map<int, int> curve_nodes;
  std::set<int> surface_tags;
  std::set<int> volume_tags;
};

// TransfiniteAutomatic only creates a genuine mapped quad/brick mesh for
// quadrangular surfaces and six-sided volumes.  Do this topology check before
// setting constraints: tetrahedron subdivision also reports "Hexahedron", but
// produces the highly skewed honeycomb-like cells that users do not mean by a
// structured brick mesh.
StructuredMeshEligibility CheckStructuredMeshEligibility(
    int dim, double target_size,
    const std::vector<std::pair<int, int>>& top_entities) {
  StructuredMeshEligibility result;
  const bool chinese =
      gmp::l10n::current_language() == gmp::l10n::Language::Chinese;
  if (dim != 2 && dim != 3) {
    result.reason = chinese
                        ? QString::fromUtf8("结构化四边形/砖形网格仅适用于 2D/3D")
                        : QString("structured quad/brick meshing is only "
                                  "available in 2D/3D");
    return result;
  }
  if (top_entities.empty()) {
    result.reason = chinese ? QString::fromUtf8("模型没有最高维几何实体")
                            : QString("the model has no top-dimensional entities");
    return result;
  }

  for (const auto& entity : top_entities) {
    if (dim == 3) {
      result.volume_tags.insert(entity.second);
    } else {
      result.surface_tags.insert(entity.second);
    }
    std::vector<std::pair<int, int>> boundary;
    gmsh::model::getBoundary({entity}, boundary, false, false, false);
    const std::size_t expected = dim == 3 ? 6u : 4u;
    if (boundary.size() != expected) {
      if (chinese) {
        result.reason =
            dim == 3
                ? QString::fromUtf8(
                      "体 %1 有 %2 个面（映射砖形需要 6 个；带孔几何"
                      "通常需要先分区）")
                      .arg(entity.second)
                      .arg(boundary.size())
                : QString::fromUtf8(
                      "面 %1 有 %2 条边界曲线（映射四边形需要 4 条）")
                      .arg(entity.second)
                      .arg(boundary.size());
      } else {
        result.reason =
            dim == 3
                ? QString("volume %1 has %2 faces (a mapped brick requires 6; "
                          "holes usually require geometry partitioning first)")
                      .arg(entity.second)
                      .arg(boundary.size())
                : QString("surface %1 has %2 boundary curves (a mapped quad "
                          "requires 4)")
                      .arg(entity.second)
                      .arg(boundary.size());
      }
      return result;
    }
    if (dim == 3) {
      for (const auto& face : boundary) {
        result.surface_tags.insert(face.second);
        std::vector<std::pair<int, int>> curves;
        gmsh::model::getBoundary({face}, curves, false, false, false);
        if (curves.size() != 4u) {
          result.reason = chinese
                              ? QString::fromUtf8(
                                    "面 %1 有 %2 条边界曲线（映射砖形的每个面"
                                    "需要 4 条）")
                                    .arg(face.second)
                                    .arg(curves.size())
                              : QString("face %1 has %2 boundary curves (a "
                                        "mapped brick face requires 4)")
                                    .arg(face.second)
                                    .arg(curves.size());
          return result;
        }
        std::vector<int> face_node_counts;
        for (const auto& curve : curves) {
          double length = 0.0;
          gmsh::model::occ::getMass(1, curve.second, length);
          const int nodes =
              std::max(2, static_cast<int>(std::ceil(length / target_size)) +
                              1);
          result.curve_nodes[curve.second] = nodes;
          face_node_counts.push_back(nodes);
        }
        std::sort(face_node_counts.begin(), face_node_counts.end());
        if (face_node_counts[0] != face_node_counts[1] ||
            face_node_counts[2] != face_node_counts[3]) {
          result.reason =
              chinese
                  ? QString::fromUtf8(
                        "按当前网格尺寸计算后，面 %1 的对边分段数不兼容；"
                        "请调整尺寸或对几何分区")
                        .arg(face.second)
                  : QString("face %1 has incompatible opposite-edge divisions "
                            "at the requested size; partition or adjust the "
                            "geometry")
                        .arg(face.second);
          return result;
        }
      }
    } else {
      std::vector<int> face_node_counts;
      for (const auto& curve : boundary) {
        double length = 0.0;
        gmsh::model::occ::getMass(1, curve.second, length);
        const int nodes =
            std::max(2, static_cast<int>(std::ceil(length / target_size)) + 1);
        result.curve_nodes[curve.second] = nodes;
        face_node_counts.push_back(nodes);
      }
      std::sort(face_node_counts.begin(), face_node_counts.end());
      if (face_node_counts[0] != face_node_counts[1] ||
          face_node_counts[2] != face_node_counts[3]) {
        result.reason = chinese
                            ? QString::fromUtf8(
                                  "按当前网格尺寸计算后，面 %1 的对边分段数"
                                  "不兼容")
                                  .arg(entity.second)
                            : QString("surface %1 has incompatible opposite-"
                                      "edge divisions at the requested size")
                                  .arg(entity.second);
        return result;
      }
    }
  }
  result.eligible = true;
  return result;
}

// 在独立 model 中读取子进程生成的网格，收集舞台/质量/manifest 数据后
// 恢复原 OCC model。建模状态与文件预览状态必须隔离。
class ScopedGeneratedMeshModel {
 public:
  ScopedGeneratedMeshModel(const QString& path, const QString& restore_path)
      : restore_path_(restore_path.toStdString()) {
    try {
      // Gmsh 的文件模型注册表及解析状态在长会话里会残留同名/空模型。
      // 回读前重建干净会话，以保证 open() 读取的就是本轮离散网格；
      // 析构时再重建会话并从未附加 worker 指令的快照恢复 OCC 几何。
      reset_gmsh_session();
      gmsh::open(path.toStdString());
      select_opened_model(path);
    } catch (...) {
      restore();
      throw;
    }
  }

  ScopedGeneratedMeshModel(const ScopedGeneratedMeshModel&) = delete;
  ScopedGeneratedMeshModel& operator=(const ScopedGeneratedMeshModel&) = delete;
  ~ScopedGeneratedMeshModel() { restore(); }

 private:
  void restore() noexcept {
    try {
      reset_gmsh_session();
      if (!restore_path_.empty()) {
        gmsh::open(restore_path_);
        select_opened_model(QString::fromStdString(restore_path_));
      }
    } catch (...) {
    }
  }

  static void reset_gmsh_session() {
    if (gmsh::isInitialized()) {
      gmsh::finalize();
    }
    gmsh::initialize(0, nullptr, false, false);
    gmsh::option::setNumber("General.Terminal", 0);
  }

  static void select_opened_model(const QString& path) {
    std::vector<std::string> model_names;
    gmsh::model::list(model_names);
    if (model_names.empty()) {
      throw std::runtime_error("Gmsh opened a file without creating a model.");
    }
    const std::string expected =
        QFileInfo(path).completeBaseName().toStdString();
    const auto match =
        std::find(model_names.begin(), model_names.end(), expected);
    gmsh::model::setCurrent(match != model_names.end() ? *match
                                                       : model_names.back());
  }

  std::string restore_path_;
};
#endif

}  // namespace

namespace gmp {

GmshPanel::GmshPanel(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  auto* workspace_tabs = new QTabWidget();
  workspace_tabs->setObjectName("gmshWorkspaceTabs");
  layout->addWidget(workspace_tabs, 1);

  auto make_scrolled_page = [workspace_tabs](const QString& title,
                                               const QString& object_name) {
    auto* scroll = new QScrollArea();
    scroll->setObjectName(object_name);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget();
    auto* page_layout = new QVBoxLayout(content);
    page_layout->setContentsMargins(8, 8, 8, 8);
    page_layout->setSpacing(8);
    scroll->setWidget(content);
    workspace_tabs->addTab(scroll, title);
    return page_layout;
  };

  auto* model_layout = make_scrolled_page("Model", "gmshModelPage");

  auto* geometry_page = new QWidget();
  auto* geometry_layout = new QVBoxLayout(geometry_page);
  geometry_layout->setContentsMargins(8, 8, 8, 8);
  auto* geometry_tabs = new QTabWidget(geometry_page);
  geometry_tabs->setObjectName("gmshGeometryTabs");
  geometry_layout->addWidget(geometry_tabs, 1);
  workspace_tabs->addTab(geometry_page, "Geometry");

  auto* groups_page = new QWidget();
  auto* groups_layout = new QVBoxLayout(groups_page);
  groups_layout->setContentsMargins(8, 8, 8, 8);
  auto* groups_tabs = new QTabWidget(groups_page);
  groups_tabs->setObjectName("gmshGroupsTabs");
  groups_layout->addWidget(groups_tabs, 1);
  workspace_tabs->addTab(groups_page, "Groups & Fields");

  auto* mesh_layout = make_scrolled_page("Mesh", "gmshMeshPage");
  // 弹窗精简：Gmsh 运行日志统一外移到主窗口 Console（append_log 实时
  // 镜像），面板不再内嵌日志页签；log_ 保留为隐藏存储供调试使用。

  auto add_scrolled_tool_tab = [](QTabWidget* tabs, QWidget* content,
                                  const QString& title,
                                  const QString& object_name) {
    auto* scroll = new QScrollArea();
    scroll->setObjectName(object_name);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    tabs->addTab(scroll, title);
  };
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

  auto* model_box = new QGroupBox("Model");
  auto* model_form = new QFormLayout(model_box);
  model_selector_ = new QComboBox();
  model_selector_->setObjectName("gmshModelSelector");
  model_selector_->setPlaceholderText("No geometry loaded");
  tune_gmsh_combo(model_selector_, 180, 240);
  geo_path_ = new QLineEdit(this);  // 持久化导入路径，不再作为只读展示控件。
  geo_path_->hide();
  connect(model_selector_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int index) {
            activate_model(index);
          });
  auto* open_geo = new QPushButton("Open Geometry");
  connect(open_geo, &QPushButton::clicked, this, &GmshPanel::on_open_geometry);
  auto* clear_geo = new QPushButton("Clear Model");
  connect(clear_geo, &QPushButton::clicked, this, &GmshPanel::on_clear_model);
  model_form->addRow("Geometry", model_selector_);
  auto* model_actions = new QHBoxLayout();
  model_actions->addWidget(open_geo);
  model_actions->addWidget(clear_geo);
  model_actions->addStretch(1);
  auto* model_actions_container = new QWidget();
  model_actions_container->setLayout(model_actions);
  model_form->addRow("", model_actions_container);
  auto_mesh_on_import_ = new QCheckBox("Auto mesh after import");
  auto_mesh_on_import_->setChecked(true);
  model_form->addRow("", auto_mesh_on_import_);
  auto_reload_geometry_ = new QCheckBox("Auto reload geometry on project load");
  auto_reload_geometry_->setChecked(true);
  model_form->addRow("", auto_reload_geometry_);
  entity_summary_ = new QLabel("Entities: 0P / 0C / 0S / 0V");
  model_form->addRow("Summary", entity_summary_);
  model_layout->addWidget(model_box);

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
  auto* entity_actions = new QHBoxLayout();
  entity_actions->addWidget(new QLabel("Dim"));
  entity_actions->addWidget(entity_dim_);
  entity_actions->addWidget(refresh_btn);
  entity_actions->addStretch(1);
  entities_layout->addLayout(entity_actions);
  entity_list_ = new QPlainTextEdit();
  entity_list_->setReadOnly(true);
  entity_list_->setMaximumHeight(120);
  entities_layout->addWidget(entity_list_);
  model_layout->addWidget(entities_box);

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
  model_layout->addWidget(geo_box);
  model_layout->addStretch(1);

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
  add_scrolled_tool_tab(geometry_tabs, prim_box, "Primitives",
                        "gmshPrimitivesPage");

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
  add_scrolled_tool_tab(geometry_tabs, xform_box, "Transform",
                        "gmshTransformPage");

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
  auto* boolean_actions = new QHBoxLayout();
  boolean_actions->addWidget(fuse_btn);
  boolean_actions->addWidget(cut_btn);
  boolean_actions->addWidget(intersect_btn);
  boolean_actions->addStretch(1);
  auto* boolean_actions_container = new QWidget();
  boolean_actions_container->setLayout(boolean_actions);
  bool_form->addRow("", boolean_actions_container);
  add_scrolled_tool_tab(geometry_tabs, bool_box, "Boolean",
                        "gmshBooleanPage");

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
  auto* phys_entities_row = new QHBoxLayout();
  phys_entities_row->addWidget(phys_group_entities_);
  phys_entities_row->addWidget(phys_entities_pick);
  auto* phys_entities_container = new QWidget();
  phys_entities_container->setLayout(phys_entities_row);
  phys_form->addRow("Entities", phys_entities_container);
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
  auto* phys_group_actions = new QHBoxLayout();
  phys_group_actions->addWidget(phys_group_add_);
  phys_group_actions->addWidget(phys_group_update_);
  phys_group_actions->addWidget(phys_group_delete_);
  phys_group_actions->addStretch(1);
  auto* phys_group_actions_container = new QWidget();
  phys_group_actions_container->setLayout(phys_group_actions);
  phys_form->addRow("", phys_group_actions_container);

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
  phys_group_table_->setMinimumHeight(90);
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
  add_scrolled_tool_tab(groups_tabs, phys_box, "Physical Groups",
                        "gmshPhysicalGroupsPage");

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
  auto* field_entities_row = new QHBoxLayout();
  field_entities_row->addWidget(field_entities_);
  field_entities_row->addWidget(field_entities_pick);
  auto* field_entities_container = new QWidget();
  field_entities_container->setLayout(field_entities_row);
  field_form->addRow("Entities", field_entities_container);
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
  auto* field_actions = new QHBoxLayout();
  field_actions->addWidget(field_apply);
  field_actions->addWidget(field_clear);
  field_actions->addWidget(field_refresh);
  field_actions->addStretch(1);
  auto* field_actions_container = new QWidget();
  field_actions_container->setLayout(field_actions);
  field_form->addRow("", field_actions_container);

  field_list_ = new QPlainTextEdit();
  field_list_->setReadOnly(true);
  field_list_->setMaximumHeight(100);
  field_form->addRow("Fields", field_list_);
  add_scrolled_tool_tab(groups_tabs, field_box, "Mesh Fields",
                        "gmshMeshFieldsPage");

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
  auto* entity_size_row = new QHBoxLayout();
  entity_size_row->addWidget(entity_size_ids_);
  entity_size_row->addWidget(entity_size_pick);
  auto* entity_size_container = new QWidget();
  entity_size_container->setLayout(entity_size_row);
  mesh_form->addRow("IDs", entity_size_container);
  bind_entity_input_validation(entity_size_ids_, entity_size_dim_, false);
  mesh_form->addRow("Size", entity_size_value_);
  auto* entity_size_actions = new QHBoxLayout();
  entity_size_actions->addWidget(entity_size_apply_);
  entity_size_actions->addWidget(entity_size_clear_);
  entity_size_actions->addStretch(1);
  auto* entity_size_actions_container = new QWidget();
  entity_size_actions_container->setLayout(entity_size_actions);
  mesh_form->addRow("", entity_size_actions_container);

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
  algo2d_->setObjectName("meshAlgorithm2dComboBox");
  algo2d_->addItem("Automatic", 2);
  algo2d_->addItem("MeshAdapt", 1);
  algo2d_->addItem("Delaunay", 5);
  algo2d_->addItem("Frontal", 6);
  algo2d_->addItem("BAMG", 7);
  algo2d_->addItem("DelQuad", 8);
  mesh_form->addRow("Algorithm 2D", algo2d_);

  algo3d_ = new QComboBox();
  algo3d_->setObjectName("meshAlgorithm3dComboBox");
  algo3d_->addItem("Delaunay", 1);
  algo3d_->addItem("Frontal", 4);
  algo3d_->addItem("HXT", 10);
  mesh_form->addRow("Algorithm 3D", algo3d_);

  mesh_topology_mode_ = new QComboBox();
  mesh_topology_mode_->setObjectName("meshTopologyModeComboBox");
  mesh_topology_mode_->addItem("Automatic (structured when eligible)", 0);
  mesh_topology_mode_->addItem("Triangles / Tetrahedra (general)", 1);
  mesh_topology_mode_->addItem("Structured Quads / Hexahedra (strict)", 2);
  mesh_topology_mode_->setToolTip(
      "Automatic may fall back to triangles/tetrahedra. Strict never falls "
      "back and requires four-sided surfaces or six-faced mapped blocks with "
      "compatible edge divisions. Extruded profiles with holes require "
      "geometry partitioning or a dedicated swept all-quad source mesh.");
  tune_gmsh_combo(mesh_topology_mode_, 220, 360);
  connect(mesh_topology_mode_,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) {
            const bool strict = mesh_topology_mode_ &&
                                mesh_topology_mode_->currentData().toInt() == 2;
            if (algo2d_) {
              algo2d_->setEnabled(!strict);
              algo2d_->setToolTip(
                  strict ? "Ignored in strict structured mode; mapped surface "
                           "constraints select the algorithm."
                         : QString());
            }
            if (algo3d_) {
              algo3d_->setEnabled(!strict);
              algo3d_->setToolTip(
                  strict ? "Ignored in strict structured mode; mapped volume "
                           "constraints select the algorithm."
                         : QString());
            }
          });
  mesh_form->addRow("Element Topology", mesh_topology_mode_);

  smoothing_ = new QSpinBox();
  smoothing_->setRange(0, 100);
  smoothing_->setValue(10);
  mesh_form->addRow("Smoothing", smoothing_);

  optimize_ = new QCheckBox("Optimize Mesh");
  optimize_->setChecked(true);
  mesh_form->addRow("", optimize_);

  mesh_layout->addWidget(mesh_box);

  auto* form = new QFormLayout();
  output_path_ = new QLineEdit();
  output_path_->setPlaceholderText("Output mesh path (*.msh)");
  output_path_->setText(QDir::currentPath() + "/out/box.msh");

  auto* pick_btn = new QPushButton("Pick Output");
  connect(pick_btn, &QPushButton::clicked, this, &GmshPanel::on_pick_output);

  form->addRow("Mesh Output", output_path_);
  form->addRow(pick_btn);

  mesh_layout->addLayout(form);

  auto* export_btn = new QPushButton("Export Geometry");
  connect(export_btn, &QPushButton::clicked, this,
          &GmshPanel::on_export_geometry);

  auto* generate_btn = new QPushButton("Generate Mesh");
  generate_btn->setObjectName("generateMeshButton");
  connect(generate_btn, &QPushButton::clicked, this, &GmshPanel::on_generate);

  auto* generate_2d_btn = new QPushButton("Generate 2D Mesh");
  auto* generate_3d_btn = new QPushButton("Generate 3D Mesh");
  generate_2d_btn->setObjectName("generate2dMeshButton");
  generate_3d_btn->setObjectName("generate3dMeshButton");
  mesh_generate_buttons_ = {generate_btn, generate_2d_btn, generate_3d_btn};
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

  auto* generate_actions = new QHBoxLayout();
  generate_actions->addWidget(export_btn);
  generate_actions->addWidget(generate_btn);
  generate_actions->addWidget(generate_2d_btn);
  generate_actions->addWidget(generate_3d_btn);
  generate_actions->addStretch(1);
  mesh_layout->addLayout(generate_actions);
  mesh_layout->addStretch(1);

  log_ = new QPlainTextEdit(this);
  log_->setReadOnly(true);
  log_->hide();

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

  // The mesh workspace is a wide task dialog; keep labels and fields aligned.
  for (auto* f : findChildren<QFormLayout*>()) {
    f->setRowWrapPolicy(QFormLayout::DontWrapRows);
    f->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
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
  last_import_error_ = "Gmsh is not enabled in this build.";
  return false;
#else
  if (path.isEmpty()) {
    return false;
  }
  ensure_gmsh();
  last_import_error_.clear();
  gmp::log_operation("geometry",
                     "Geometry import started: " + path +
                         (auto_mesh ? " (auto mesh on)" : ""));
  // API 初始化默认 AbortOnError=2：.geo 脚本错误会抛异常，且异常路径
  // 不释放 gmsh 的 busy 标志——之后所有 gmsh::open 都被 “I'm busy” 静默
  // 吞掉，整个会话无法再导入任何几何。导入期间临时改为 0：错误只进
  // logger，由本函数扫描后统一按失败处理，会话保持可用。
  double abort_on_error = 2;
  try {
    gmsh::option::getNumber("General.AbortOnError", abort_on_error);
  } catch (...) {
  }
  auto restore_abort_on_error = [abort_on_error]() {
    try {
      gmsh::option::setNumber("General.AbortOnError", abort_on_error);
    } catch (...) {
    }
  };
  try {
    gmsh::option::setNumber("General.Terminal", 0);
    gmsh::option::setNumber("General.AbortOnError", 0);
    gmsh::logger::start();
    gmsh::clear();
    external_model_name_.clear();
    model_selector_->clear();
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

    std::vector<std::string> log;
    gmsh::logger::get(log);
    gmsh::logger::stop();
    QStringList error_lines;
    for (const auto& line : log) {
      append_log(QString::fromStdString(line));
      if (line.rfind("Error", 0) == 0) {
        error_lines << QString::fromStdString(line);
      }
    }

    // 脚本错误（AbortOnError=0 下 gmsh 会继续执行并留下残缺模型）与
    // “成功”但不产生实体的坏文件，统一按失败处理，不得进入已加载状态。
    if (!error_lines.isEmpty()) {
      throw std::runtime_error(error_lines.join("\n").toStdString());
    }
    std::vector<std::pair<int, int>> entities;
    gmsh::model::getEntities(entities);
    if (entities.empty()) {
      throw std::runtime_error(
          "Geometry file produced no entities (check the Gmsh script for "
          "errors).");
    }
    // 退化实体检查：错误的旋转/布尔可能产生零体积的“体”（例如把 xy 平面
    // 的矩形直接绕轴旋转）。它能通过实体计数，但永远无法体网格化，
    // 会在生成阶段才以难懂的 overlapping facets 报错。这里提前拦截。
    std::vector<std::pair<int, int>> volumes;
    gmsh::model::getEntities(volumes, 3);
    for (const auto& v : volumes) {
      double mass = 0;
      try {
        gmsh::model::occ::getMass(3, v.second, mass);
      } catch (...) {
        continue;  // 无法计算时交给后续网格阶段判断
      }
      if (mass <= 1e-12) {
        throw std::runtime_error(
            "Degenerate solid with zero volume detected (check the geometry "
            "script, e.g. a rectangle revolved while lying in the wrong "
            "plane).");
      }
    }
    gmp::log_operation(
        "geometry",
        QString("Geometry import succeeded: %1 (%2 entities)")
            .arg(path)
            .arg(entities.size()));

    note_external_model_loaded(path);
    restore_abort_on_error();

    append_log("Geometry loaded: " + path);
    if (auto_mesh) {
      on_generate();
    }
    return true;
  } catch (const std::exception& ex) {
    last_import_error_ = QString::fromUtf8(ex.what());
    append_log("Gmsh error: " + last_import_error_);
    gmp::log_operation("geometry", QString("Geometry import FAILED: %1 | %2")
                                       .arg(path, last_import_error_));
  }
  restore_abort_on_error();
  // 导入失败：丢弃残缺模型并让面板如实反映空状态，
  // 不得保留旧路径或旧实体摘要造成“加载成功”的假象。
  try {
    gmsh::clear();
  } catch (...) {
  }
  external_model_name_.clear();
  model_selector_->clear();
  model_loaded_ = false;
  if (geo_path_) {
    geo_path_->clear();
  }
  update_entity_summary();
  update_entity_list();
  update_physical_group_list();
  update_field_list();
  refresh_occ_entity_template_lists();
  return false;
#endif
}

bool GmshPanel::activate_model(int index) {
  if (!model_selector_ || index < 0 || index >= model_selector_->count()) {
    return false;
  }
  external_model_name_ = model_selector_->itemData(index).toString();
  geo_path_->setText(model_selector_->itemText(index));
#ifdef GMP_ENABLE_GMSH_GUI
  try {
    std::vector<std::string> model_names;
    gmsh::model::list(model_names);
    const std::string expected = external_model_name_.toStdString();
    if (std::find(model_names.begin(), model_names.end(), expected) ==
        model_names.end()) {
      const QString source_path =
          model_selector_->itemData(index, Qt::UserRole + 1).toString();
      if (source_path.isEmpty() || !QFileInfo::exists(source_path)) {
        return false;
      }
      gmsh::model::add(expected);
      gmsh::vectorpair imported;
      gmsh::model::occ::importShapes(source_path.toStdString(), imported, true,
                                     "brep");
      gmsh::model::occ::synchronize();
    }
    gmsh::model::setCurrent(expected);
  } catch (...) {
    return false;
  }
#endif
  model_loaded_ = true;
  update_entity_summary();
  update_entity_list();
  update_physical_group_list();
  update_field_list();
  refresh_occ_entity_template_lists();
  return true;
}

void GmshPanel::note_external_model_loaded(const QString& label,
                                           const QString& source_path) {
  // 部件特征等外部通道已把几何放进 Gmsh 模型；同步面板状态，
  // 避免“生成网格”按空模型清空模型改画示例盒（on_generate 1469 守卫）。
  model_loaded_ = true;
#ifdef GMP_ENABLE_GMSH_GUI
  try {
    std::string model_name;
    gmsh::model::getCurrent(model_name);
    external_model_name_ = QString::fromStdString(model_name);
  } catch (...) {
    external_model_name_.clear();
  }
#endif
  if (use_sample_box_ && use_sample_box_->isChecked()) {
    use_sample_box_->setChecked(false);
  }
  if (geo_path_ && !label.isEmpty()) {
    geo_path_->setText(label);
  }
  if (model_selector_ && !external_model_name_.isEmpty()) {
    int index = model_selector_->findText(label);
    if (index < 0) {
      model_selector_->addItem(label, external_model_name_);
      index = model_selector_->count() - 1;
    } else {
      model_selector_->setItemData(index, external_model_name_);
    }
    if (!source_path.isEmpty()) {
      model_selector_->setItemData(index, source_path, Qt::UserRole + 1);
    }
    model_selector_->setCurrentIndex(index);
    activate_model(index);  // 同一项被再次选择时 currentIndexChanged 不会发出。
  }
  update_entity_summary();
  update_entity_list();
  update_physical_group_list();
  update_field_list();
  refresh_occ_entity_template_lists();
}

void GmshPanel::restore_external_models(const QVariantMap& sources,
                                        const QString& selected_label) {
  if (!model_selector_) {
    return;
  }
#ifdef GMP_ENABLE_GMSH_GUI
  ensure_gmsh();
  gmsh::clear();
#endif
  external_model_name_.clear();
  model_loaded_ = false;
  geo_path_->clear();
  const QSignalBlocker blocker(model_selector_);
  model_selector_->clear();
  for (auto it = sources.cbegin(); it != sources.cend(); ++it) {
    model_selector_->addItem(it.key(), it.key());
    model_selector_->setItemData(model_selector_->count() - 1,
                                 it.value().toString(), Qt::UserRole + 1);
  }
  int index = model_selector_->findText(selected_label);
  if (index < 0 && model_selector_->count() > 0) {
    index = 0;
  }
  model_selector_->setCurrentIndex(index);
  if (index < 0 || !activate_model(index)) {
    update_entity_summary();
    update_entity_list();
    update_physical_group_list();
    update_field_list();
    refresh_occ_entity_template_lists();
  }
}

void GmshPanel::select_external_model(const QString& label) {
  if (!model_selector_) {
    return;
  }
  const int index = model_selector_->findText(label);
  if (index >= 0) {
    model_selector_->setCurrentIndex(index);
    activate_model(index);
  }
}

void GmshPanel::set_mesh_output_path(const QString& path) {
  if (output_path_ && !path.isEmpty()) {
    output_path_->setText(path);
  }
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
  const int topology_mode = mesh_topology_mode_
                                ? mesh_topology_mode_->currentData().toInt()
                                : 0;
  map.insert("mesh_topology_mode", topology_mode);
  // Legacy compatibility: old projects only know the boolean recombine key.
  map.insert("recombine", topology_mode != 1);
  map.insert("smoothing", smoothing_ ? smoothing_->value() : 10);
  map.insert("output_path", output_path_ ? output_path_->text() : "");
  map.insert("model_source",
             model_selector_ ? model_selector_->currentText() : "");
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
  if (mesh_topology_mode_) {
    int topology_mode = 0;
    if (settings.contains("mesh_topology_mode")) {
      topology_mode = settings.value("mesh_topology_mode").toInt();
    } else if (settings.contains("recombine")) {
      topology_mode = settings.value("recombine").toBool() ? 0 : 1;
    }
    set_combo_data(mesh_topology_mode_, topology_mode);
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

void GmshPanel::set_external_busy(bool busy) {
  mesh_external_busy_ = busy;
  for (auto* button : mesh_generate_buttons_) {
    if (!button) {
      continue;
    }
    button->setEnabled(!mesh_generation_running_ && !mesh_external_busy_);
    if (mesh_external_busy_) {
      button->setToolTip(
          "Wait for the active Job to finish before generating a mesh.");
    } else if (!mesh_generation_running_) {
      button->setToolTip(QString());
    }
  }
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
  if (!import_geometry(path, auto_mesh_on_import_ &&
                                auto_mesh_on_import_->isChecked())) {
    // 导入失败必须显性反馈：错误不能只留在“运行日志”页签里。
    QMessageBox::warning(
        this, "Open Geometry",
        QString("Failed to load geometry:\n%1\n\nThe file was not loaded; "
                "check the script for errors (details are in the log tab).")
            .arg(last_import_error_.isEmpty() ? path : last_import_error_));
  }
#endif
}

void GmshPanel::on_clear_model() {
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  return;
#else
  ensure_gmsh();
  gmsh::clear();
  external_model_name_.clear();
  model_selector_->clear();
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
  if (mesh_generation_running_ || mesh_external_busy_) {
    append_log(mesh_generation_running_
                   ? "Mesh generation is already running."
                   : "Mesh generation is unavailable while a Job is running.");
    return;
  }
  set_mesh_generation_running(true);
  emit mesh_generation_started();
  // 先刷新运行态；实际耗时剖分会在独立 Gmsh 子进程执行，主线程通过嵌套事件
  // 循环维持进度窗和窗口重绘，同时应用模态窗阻止其他 Gmsh 操作并发进入。
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
#ifndef GMP_ENABLE_GMSH_GUI
  append_log("Gmsh is not enabled in this build.");
  set_mesh_generation_running(false);
  emit mesh_generation_finished(false, "Gmsh is not enabled in this build.");
  return;
#else
  bool success = false;
  bool logger_started = false;
  bool size_guard_rejected = false;
  bool structured_mode_rejected = false;
  QString completion_message;
  QString structured_fallback_reason;
  try {
    ensure_gmsh();

    // 舞台文件读取和前一次生成结果回读可能暂时改变 current model；
    // 外部部件已明确登记时，以登记的模型为网格输入源。
    if (!external_model_name_.isEmpty()) {
      std::vector<std::string> model_names;
      gmsh::model::list(model_names);
      const std::string expected = external_model_name_.toStdString();
      if (std::find(model_names.begin(), model_names.end(), expected) !=
          model_names.end()) {
        gmsh::model::setCurrent(expected);
      } else {
        throw std::runtime_error(
            "The selected part model is no longer available. Select it again "
            "or rebuild the part before generating the mesh.");
      }
    }

    gmsh::option::setNumber("General.Terminal", 0);
    gmsh::logger::start();
    logger_started = true;

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
    // 禁用全局重组和四面体细分。后者虽会在文件中报告 Hexahedron，
    // 却会生成继承四面体方向的扭曲蜂窝单元，不是用户所指的砖形网格。
    gmsh::option::setNumber("Mesh.RecombineAll", 0);
    gmsh::option::setNumber("Mesh.SubdivisionAlgorithm", 0);
    if (smoothing_) {
      gmsh::option::setNumber("Mesh.Smoothing", smoothing_->value());
    }
    gmsh::option::setNumber("Mesh.Optimize", optimize_->isChecked() ? 1 : 0);
    gmsh::option::setNumber("Mesh.MshFileVersion", msh_version == 2 ? 2.2 : 4.1);

    // 是否用示例盒兜底只取决于“当前模型是否真的没有几何实体”，
    // 不看 model_loaded_ 标志或示例盒勾选状态——部件特征等外部通道
    // 导入的几何绝不允许被清空换盒（历史缺陷：勾选框/面板状态把用户
    // 的部件几何误清，产出 12 节点的退化网格）。
    std::vector<std::pair<int, int>> existing_entities;
    gmsh::model::getEntities(existing_entities, -1);
    const bool model_empty = existing_entities.empty();
    if (model_empty) {
      if (!use_sample_box_ || !use_sample_box_->isChecked()) {
        throw std::runtime_error(
            "No geometry in the current model. Import geometry, create a "
            "part feature, or enable the sample box before generating.");
      }
      gmsh::clear();
      external_model_name_.clear();
      model_selector_->clear();
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
      model_selector_->addItem("sample: box", "box_model");
      model_selector_->setCurrentIndex(0);
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
    const int topology_mode = mesh_topology_mode_
                                  ? mesh_topology_mode_->currentData().toInt()
                                  : 0;
    const bool prefer_structured = topology_mode != 1;
    const bool require_structured = topology_mode == 2;
    // 规模预检：草图/OCC 使用模型坐标原值（通常为 mm）。沿用面向单位盒的
    // 默认 lc=0.2 到百毫米级实体时，单元数会按 lc^-dim 爆炸，表现为应用
    // “卡死”。优先用 OCC 的真实长度/面积/体积，其他内核回退到包围盒估算。
    std::vector<std::pair<int, int>> top_entities;
    gmsh::model::getEntities(top_entities, dim);
    const StructuredMeshEligibility structured =
        prefer_structured
            ? CheckStructuredMeshEligibility(dim, lc, top_entities)
            : StructuredMeshEligibility{};
    const bool use_structured_mesh = prefer_structured && structured.eligible;
    if (require_structured && !structured.eligible) {
      structured_mode_rejected = true;
      completion_message =
          gmp::l10n::current_language() == gmp::l10n::Language::Chinese
              ? QString::fromUtf8(
                    "严格结构化网格未生成：%1。该模式不会回退为四面体；请先"
                    "将几何划分为可映射/可扫掠块体，或改选“自动”或“通用"
                    "四面体”。")
                    .arg(structured.reason)
              : QString("Strict structured mesh was not generated: %1. This "
                        "mode never falls back; partition the geometry into "
                        "mapped/sweepable blocks, or select Automatic or the "
                        "general simplex mode.")
                    .arg(structured.reason);
      throw std::runtime_error(completion_message.toStdString());
    }
    if (use_structured_mesh) {
      append_log(dim == 3
                     ? "Structured brick mesh enabled: mapped constraints "
                       "applied to eligible six-faced volume(s)."
                     : "Structured quadrilateral mesh enabled: mapped "
                       "constraints applied to four-sided surface(s).");
    } else if (prefer_structured) {
      structured_fallback_reason = structured.reason;
      append_log(QString("Structured mesh unavailable: %1. Falling back to "
                         "%2; partition the geometry into sweepable blocks "
                         "to obtain a conforming structured mesh.")
                     .arg(structured.reason)
                     .arg(dim == 3 ? "tetrahedra" : "triangles"));
    }
    double occ_measure = 0.0;
    bool has_occ_measure = false;
    double bbox_measure = 0.0;
    for (const auto& entity : top_entities) {
      try {
        double mass = 0.0;
        gmsh::model::occ::getMass(dim, entity.second, mass);
        if (std::isfinite(mass) && mass > 0.0) {
          occ_measure += mass;
          has_occ_measure = true;
        }
      } catch (...) {
      }

      try {
        double xmin = 0.0;
        double ymin = 0.0;
        double zmin = 0.0;
        double xmax = 0.0;
        double ymax = 0.0;
        double zmax = 0.0;
        gmsh::model::getBoundingBox(dim, entity.second, xmin, ymin, zmin,
                                    xmax, ymax, zmax);
        std::vector<double> spans = {std::max(0.0, xmax - xmin),
                                     std::max(0.0, ymax - ymin),
                                     std::max(0.0, zmax - zmin)};
        std::sort(spans.begin(), spans.end(), std::greater<double>());
        if (dim == 3) {
          bbox_measure += spans[0] * spans[1] * spans[2];
        } else if (dim == 2) {
          bbox_measure += spans[0] * spans[1];
        } else if (dim == 1) {
          bbox_measure += spans[0];
        }
      } catch (...) {
      }
    }
    const double measure = has_occ_measure ? occ_measure : bbox_measure;
    const double simplex_factor = dim == 3 ? 6.0 : (dim == 2 ? 2.0 : 1.0);
    // 映射砖形网格约为 measure/lc^dim；保留 1.5 的保守余量。
    // 回退时按单纯形网格估算，不再为已删除的自动细分乘 3/4 倍。
    const double element_factor = use_structured_mesh ? 1.5 : simplex_factor;
    const double estimated_elements =
        (lc > 0.0 && measure > 0.0)
            ? element_factor * measure / std::pow(lc, dim)
            : 0.0;
    bool max_ok = false;
    const qlonglong configured_max =
        qEnvironmentVariable("GMP_MESH_MAX_ESTIMATED_ELEMENTS")
            .toLongLong(&max_ok);
    const double max_estimated_elements =
        max_ok && configured_max > 0 ? static_cast<double>(configured_max)
                                     : 2000000.0;
    if (estimated_elements > max_estimated_elements) {
      const double recommended_lc =
          1.1 * std::pow(element_factor * measure / max_estimated_elements,
                         1.0 / static_cast<double>(dim));
      const bool chinese =
          gmp::l10n::current_language() == gmp::l10n::Language::Chinese;
      const double estimated_millions = estimated_elements / 1000000.0;
      const double limit_millions = max_estimated_elements / 1000000.0;
      completion_message =
          chinese
              ? QString::fromUtf8(
                    "网格尺寸过细：预计约 %1 百万个 %2D 单元，超过交互式生成"
                    "上限 %3 百万个。请将全局网格尺寸调至至少 %4（建议从 %5 "
                    "开始），再重新生成。")
                    .arg(estimated_millions, 0, 'f', 1)
                    .arg(dim)
                    .arg(limit_millions, 0, 'f', 1)
                    .arg(recommended_lc, 0, 'g', 4)
                    .arg(std::max(recommended_lc, lc * 2.0), 0, 'g', 4)
              : QString(
                    "Mesh size is too fine: about %1 million %2D elements are "
                    "estimated, above the interactive limit of %3 million. "
                    "Increase Global Mesh Size to at least %4 (start with %5) "
                    "and generate again.")
                    .arg(estimated_millions, 0, 'f', 1)
                    .arg(dim)
                    .arg(limit_millions, 0, 'f', 1)
                    .arg(recommended_lc, 0, 'g', 4)
                    .arg(std::max(recommended_lc, lc * 2.0), 0, 'g', 4);
      size_guard_rejected = true;
      throw std::runtime_error(completion_message.toStdString());
    }
    append_log(QString("Mesh size preflight: estimated %1 top-dimensional "
                       "elements (limit %2, lc=%3).")
                   .arg(estimated_elements, 0, 'g', 6)
                   .arg(max_estimated_elements, 0, 'g', 6)
                   .arg(lc, 0, 'g', 6));

    // libgmsh 是进程级全局状态，不能安全地从 Qt 后台线程调用。把当前
    // model 序列化后交给独立 gmsh 进程：GUI 可响应、stdout 提供真实阶段/
    // 百分比，取消也只终止子进程，不会损坏应用内 OCC model。
    QString gmsh_executable = QStandardPaths::findExecutable("gmsh");
#ifdef Q_OS_MACOS
    if (gmsh_executable.isEmpty()) {
      const QString app_executable =
          "/Applications/Gmsh.app/Contents/MacOS/gmsh";
      if (QFileInfo::exists(app_executable)) {
        gmsh_executable = app_executable;
      }
    }
#endif
    if (gmsh_executable.isEmpty()) {
      throw std::runtime_error(
          "The gmsh executable was not found; background mesh generation "
          "requires the Gmsh CLI on PATH.");
    }
    QTemporaryDir mesh_temp_dir(
        QDir::tempPath() + "/gmp_mesh_generation_XXXXXX");
    if (!mesh_temp_dir.isValid()) {
      throw std::runtime_error(
          "Could not create a temporary directory for mesh generation.");
    }
    const QString source_path =
        mesh_temp_dir.filePath("current_model.geo_unrolled");
    QString restore_model_name = external_model_name_.isEmpty()
                                     ? QString("current_model_restore")
                                     : external_model_name_;
    restore_model_name.replace(QRegularExpression("[^A-Za-z0-9_.-]"), "_");
    const QString restore_path =
        mesh_temp_dir.filePath(restore_model_name + ".geo_unrolled");
    const auto generated_sequence = generated_mesh_model_sequence.fetch_add(
        1, std::memory_order_relaxed);
    const QString generated_path = mesh_temp_dir.filePath(
        QString("generated_%1.msh").arg(generated_sequence));
    gmsh::write(source_path.toStdString());
    if (!QFile::copy(source_path, restore_path)) {
      throw std::runtime_error(
          "Could not snapshot the current OCC model for restoration.");
    }
    QStringList worker_directives;
    auto tag_list = [](const auto& tags) {
      QStringList values;
      for (const auto tag : tags) {
        values << QString::number(tag);
      }
      return values.join(", ");
    };

    // Gmsh writes only entities covered by physical groups as soon as any
    // physical group exists. A newly created Part can therefore disappear if
    // older groups are present but do not yet cover the new top-dimensional
    // entity. Add a worker-only group for uncovered entities; the in-memory
    // OCC model and all existing user groups stay untouched.
    std::vector<std::pair<int, int>> top_physical_groups;
    gmsh::model::getPhysicalGroups(top_physical_groups, dim);
    std::set<int> grouped_top_entities;
    int next_physical_tag = 1;
    for (const auto& group : top_physical_groups) {
      next_physical_tag = std::max(next_physical_tag, group.second + 1);
      std::vector<int> entity_tags;
      gmsh::model::getEntitiesForPhysicalGroup(dim, group.second, entity_tags);
      grouped_top_entities.insert(entity_tags.begin(), entity_tags.end());
    }
    std::vector<int> ungrouped_top_entities;
    for (const auto& entity : top_entities) {
      if (!grouped_top_entities.count(entity.second)) {
        ungrouped_top_entities.push_back(entity.second);
      }
    }
    if (!ungrouped_top_entities.empty()) {
      const QString physical_kind =
          dim == 3 ? "Volume" : (dim == 2 ? "Surface" : "Curve");
      worker_directives
          << QString("Physical %1(\"unassigned_%2d\", %3) = {%4};")
                 .arg(physical_kind)
                 .arg(dim)
                 .arg(next_physical_tag)
                 .arg(tag_list(ungrouped_top_entities));
      append_log(QString("Worker mesh group added for %1 unassigned %2D "
                         "entity/entities.")
                     .arg(ungrouped_top_entities.size())
                     .arg(dim));
    }
    if (use_structured_mesh) {
      // OCC models are written as a .geo_unrolled file that only Merge-s an
      // accompanying XAO file; mesh attributes set through the API are not
      // serialized. Append explicit mapped-mesh directives for the worker.
      for (const auto& [curve_tag, node_count] : structured.curve_nodes) {
        worker_directives
            << QString("Transfinite Curve {%1} = %2 Using Progression 1;")
                   .arg(curve_tag)
                   .arg(node_count);
      }
      if (!structured.surface_tags.empty()) {
        const QString surfaces = tag_list(structured.surface_tags);
        worker_directives
            << QString("Transfinite Surface {%1};").arg(surfaces)
            << QString("Recombine Surface {%1};").arg(surfaces);
      }
      if (!structured.volume_tags.empty()) {
        worker_directives
            << QString("Transfinite Volume {%1};")
                   .arg(tag_list(structured.volume_tags));
      }
    }
    if (!worker_directives.empty()) {
      QFile source_file(source_path);
      if (!source_file.open(QIODevice::Append | QIODevice::Text) ||
          source_file.write(
              ("\n" + worker_directives.join("\n") + "\n").toUtf8()) < 0) {
        throw std::runtime_error(
            "Could not serialize worker mesh constraints.");
      }
    }

    if (logger_started) {
      std::vector<std::string> setup_log;
      gmsh::logger::get(setup_log);
      gmsh::logger::stop();
      logger_started = false;
      for (const auto& line : setup_log) {
        append_log(QString::fromStdString(line));
      }
    }

    QStringList process_args;
    process_args << source_path << QString("-%1").arg(dim) << "-nopopup"
                 << "-v" << "4" << "-o" << generated_path << "-format"
                 << (msh_version == 2 ? "msh2" : "msh4") << "-clmin"
                 << QString::number(lc, 'g', 16) << "-clmax"
                 << QString::number(lc, 'g', 16) << "-order"
                 << QString::number(order) << "-setnumber" << "Mesh.Algorithm"
                 << QString::number(algo2d_ ? algo2d_->currentData().toInt() : 2)
                 << "-setnumber" << "Mesh.Algorithm3D"
                 << QString::number(algo3d_ ? algo3d_->currentData().toInt() : 1)
                 << "-setnumber" << "Mesh.RecombineAll"
                 << "0"
                 << "-setnumber" << "Mesh.SubdivisionAlgorithm"
                 << "0"
                 << "-setnumber" << "Mesh.Smoothing"
                 << QString::number(smoothing_ ? smoothing_->value() : 0)
                 << "-setnumber" << "Mesh.HighOrderOptimize"
                 << QString::number(high_order_opt_
                                        ? high_order_opt_->currentData().toInt()
                                        : 0)
                 << "-setnumber" << "Mesh.Optimize"
                 << QString::number(optimize_ && optimize_->isChecked() ? 1
                                                                        : 0);

    QEventLoop wait_loop;
    QElapsedTimer elapsed;
    elapsed.start();
    const bool automated_tour =
        qEnvironmentVariableIsSet("GMP_SCREENSHOT_DIR");
    std::unique_ptr<QProgressDialog> progress;
    if (!automated_tour) {
      progress = std::make_unique<QProgressDialog>(this);
      progress->setObjectName("meshGenerationProgressDialog");
      progress->setWindowTitle(
          gmp::l10n::current_language() == gmp::l10n::Language::Chinese
              ? QString::fromUtf8("正在生成网格")
              : QString("Generating Mesh"));
      progress->setLabelText(
          gmp::l10n::current_language() == gmp::l10n::Language::Chinese
              ? QString::fromUtf8("正在生成 %1D 网格…").arg(dim)
              : QString("Generating %1D mesh...").arg(dim));
      progress->setRange(0, 100);
      progress->setValue(0);
      progress->setCancelButtonText(
          gmp::l10n::current_language() == gmp::l10n::Language::Chinese
              ? QString::fromUtf8("取消")
              : QString("Cancel"));
      progress->setMinimumDuration(0);
      progress->setAutoClose(false);
      progress->setAutoReset(false);
      progress->setWindowModality(Qt::ApplicationModal);
      progress->show();
    }

    QProcess mesh_process;
    mesh_process.setProcessChannelMode(QProcess::MergedChannels);
    QString process_output;
    int reported_percent = 0;
    int progress_base = 0;
    int progress_span = 3;
    int reported_step = 1;
    int optimization_passes = 0;
    // ponytail: Gmsh CLI 不给节点插入总数；用预检单元数/8 估算，仅用于
    // 阶段内反馈。若 CLI 后续提供总量，直接替换该估算。
    const double estimated_insertions =
        std::max(1.0, estimated_elements / 8.0);
    QString reported_phase =
        gmp::l10n::current_language() == gmp::l10n::Language::Chinese
            ? QString::fromUtf8("准备几何")
            : QString("Preparing geometry");
    const QRegularExpression percent_pattern("\\[\\s*(\\d+)%\\]");
    const QRegularExpression iteration_pattern("\\bIt\\.\\s*(\\d+)\\s*-");
    auto consume_process_output = [&]() {
      process_output += QString::fromLocal8Bit(mesh_process.readAll());
      process_output.replace('\r', '\n');
      int newline = -1;
      while ((newline = process_output.indexOf('\n')) >= 0) {
        const QString line = process_output.left(newline).trimmed();
        process_output.remove(0, newline + 1);
        if (line.isEmpty()) {
          continue;
        }
        append_log(line);
        if (line.contains("Done meshing 1D", Qt::CaseInsensitive)) {
          reported_percent = std::max(reported_percent, 3);
        } else if (line.contains("Done meshing 2D", Qt::CaseInsensitive)) {
          reported_percent = std::max(reported_percent, 33);
        } else if (line.contains("Done meshing 3D", Qt::CaseInsensitive)) {
          reported_percent = std::max(reported_percent, 67);
        } else if (line.contains("Done optimizing mesh",
                                 Qt::CaseInsensitive)) {
          reported_percent = 99;
        } else if (line.contains("Meshing 1D", Qt::CaseInsensitive)) {
          reported_percent = 0;
          progress_base = 0;
          progress_span = 3;
          reported_step = 1;
          reported_phase =
              gmp::l10n::current_language() == gmp::l10n::Language::Chinese
                  ? QString::fromUtf8("生成 1D 边网格")
                  : QString("Generating 1D edge mesh");
        } else if (line.contains("Meshing 2D", Qt::CaseInsensitive)) {
          reported_percent = 3;
          progress_base = 3;
          progress_span = 30;
          reported_step = 1;
          reported_phase =
              gmp::l10n::current_language() == gmp::l10n::Language::Chinese
                  ? QString::fromUtf8("生成 2D 面网格")
                  : QString("Generating 2D surface mesh");
        } else if (line.contains("Meshing 3D", Qt::CaseInsensitive)) {
          reported_percent = 33;
          progress_base = 33;
          progress_span = 34;
          reported_step = 2;
          reported_phase =
              gmp::l10n::current_language() == gmp::l10n::Language::Chinese
                  ? QString::fromUtf8("生成 3D 体网格")
                  : QString("Generating 3D volume mesh");
        } else if (line.contains("Optimizing mesh", Qt::CaseInsensitive)) {
          reported_percent = 67;
          progress_base = 67;
          progress_span = 32;
          reported_step = 3;
          reported_phase =
              gmp::l10n::current_language() == gmp::l10n::Language::Chinese
                  ? QString::fromUtf8("优化网格质量")
                  : QString("Optimizing mesh quality");
        }
        const auto match = percent_pattern.match(line);
        if (match.hasMatch()) {
          reported_percent = std::max(
              reported_percent,
              std::min(99, progress_base +
                               match.captured(1).toInt() * progress_span / 100));
        }
        const auto iteration = iteration_pattern.match(line);
        if (reported_step == 2 && iteration.hasMatch()) {
          const double fraction = std::min(
              1.0, iteration.captured(1).toDouble() / estimated_insertions);
          reported_percent = std::max(
              reported_percent, 48 + static_cast<int>(17.0 * fraction));
        } else if (reported_step == 2) {
          if (line.contains("Done tetrahedrizing", Qt::CaseInsensitive)) {
            reported_percent = std::max(reported_percent, 38);
          } else if (line.contains("Tetrahedrizing", Qt::CaseInsensitive)) {
            reported_percent = std::max(reported_percent, 35);
          } else if (line.contains("Done reconstructing mesh",
                                   Qt::CaseInsensitive)) {
            reported_percent = std::max(reported_percent, 47);
          } else if (line.contains("Reconstructing mesh",
                                   Qt::CaseInsensitive)) {
            reported_percent = std::max(reported_percent, 40);
          } else if (line.contains("Creating surface mesh",
                                   Qt::CaseInsensitive)) {
            reported_percent = std::max(reported_percent, 42);
          } else if (line.contains("Identifying boundary",
                                   Qt::CaseInsensitive)) {
            reported_percent = std::max(reported_percent, 44);
          } else if (line.contains("Recovering boundary",
                                   Qt::CaseInsensitive)) {
            reported_percent = std::max(reported_percent, 45);
          } else if (line.contains("Found volume", Qt::CaseInsensitive)) {
            reported_percent = std::max(reported_percent, 48);
          } else if (line.contains("3D refinement terminated",
                                   Qt::CaseInsensitive)) {
            reported_percent = std::max(reported_percent, 66);
          }
        } else if (reported_step == 3) {
          if (line.contains("Optimization starts", Qt::CaseInsensitive)) {
            reported_percent = std::max(reported_percent, 72);
          } else if (line.contains("edge swaps", Qt::CaseInsensitive)) {
            ++optimization_passes;
            reported_percent = std::max(
                reported_percent, std::min(97, 74 + optimization_passes * 7));
          } else if (line.contains("No ill-shaped", Qt::CaseInsensitive)) {
            reported_percent = std::max(reported_percent, 98);
          }
        }
      }
    };
    connect(&mesh_process, &QProcess::readyReadStandardOutput, &wait_loop,
            consume_process_output);
    connect(&mesh_process, &QProcess::finished, &wait_loop,
            [&wait_loop](int, QProcess::ExitStatus) { wait_loop.quit(); });
    if (progress) {
      connect(progress.get(), &QProgressDialog::canceled, &mesh_process,
              [&mesh_process]() {
                if (mesh_process.state() != QProcess::NotRunning) {
                  mesh_process.kill();
                }
              });
    }
    QTimer progress_timer;
    progress_timer.setInterval(250);
    connect(&progress_timer, &QTimer::timeout, &wait_loop,
            [&elapsed, &progress, &reported_phase, &reported_percent,
             &reported_step]() {
              if (progress) {
                const qint64 seconds = elapsed.elapsed() / 1000;
                const bool chinese =
                    gmp::l10n::current_language() ==
                    gmp::l10n::Language::Chinese;
                progress->setLabelText(
                    chinese
                        ? QString::fromUtf8("第 %1/3 步 · %2\n已用时 %3 秒")
                              .arg(reported_step)
                              .arg(reported_phase)
                              .arg(seconds)
                        : QString("Step %1/3 · %2\nElapsed %3 s")
                              .arg(reported_step)
                              .arg(reported_phase)
                              .arg(seconds));
                progress->setRange(0, 100);
                progress->setValue(reported_percent);
              }
            });
    progress_timer.start();
    mesh_process.start(gmsh_executable, process_args);
    if (!mesh_process.waitForStarted(3000)) {
      throw std::runtime_error(
          QString("Failed to start Gmsh: %1")
              .arg(mesh_process.errorString())
              .toStdString());
    }
    wait_loop.exec();
    progress_timer.stop();
    consume_process_output();
    if (!process_output.trimmed().isEmpty()) {
      append_log(process_output.trimmed());
    }
    const bool canceled =
        progress && progress->wasCanceled();
    if (canceled) {
      throw std::runtime_error("Mesh generation canceled by the user.");
    }
    if (mesh_process.exitStatus() != QProcess::NormalExit ||
        mesh_process.exitCode() != 0 || !QFileInfo::exists(generated_path)) {
      throw std::runtime_error(
          QString("Gmsh process failed (exit code %1).")
              .arg(mesh_process.exitCode())
              .toStdString());
    }
    if (progress) {
      progress->setValue(100);
      QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    progress.reset();

    // 后处理暂时切到生成网格 model；作用域结束自动恢复原 OCC model。
    ScopedGeneratedMeshModel generated_model(generated_path, restore_path);
    const int boundary_dim = std::max(0, dim - 1);

    // 空结果保护：生成不出任何节点时不得写文件或报告成功——空网格既无
    // 意义，又会覆盖上一个有效输出并造成“已生成但舞台为空”的假象。
    std::vector<std::size_t> node_tags;
    std::vector<double> node_coords;
    std::vector<double> node_params;
    gmsh::model::mesh::getNodes(node_tags, node_coords, node_params);
    if (node_tags.empty()) {
      throw std::runtime_error(
          "Mesh generation produced no nodes; refusing to write an empty "
          "mesh file.");
    }
    if (use_structured_mesh) {
      std::vector<int> generated_types;
      std::vector<std::vector<std::size_t>> generated_tags;
      std::vector<std::vector<std::size_t>> generated_nodes;
      gmsh::model::mesh::getElements(generated_types, generated_tags,
                                     generated_nodes, dim);
      const QString expected = dim == 3 ? "Hexahedron" : "Quadrilateral";
      for (const int element_type : generated_types) {
        std::string type_name;
        int type_dim = 0;
        int type_order = 0;
        int type_nodes = 0;
        int type_primary_nodes = 0;
        std::vector<double> local_coords;
        gmsh::model::mesh::getElementProperties(
            element_type, type_name, type_dim, type_order, type_nodes,
            local_coords, type_primary_nodes);
        if (!QString::fromStdString(type_name).contains(expected,
                                                        Qt::CaseInsensitive)) {
          throw std::runtime_error(
              QString("Mapped mesh validation failed: expected only %1 "
                      "cells, got %2. The previous output was preserved.")
                  .arg(expected)
                  .arg(QString::fromStdString(type_name))
                  .toStdString());
        }
      }
    }

    const QString out_path = output_path_->text();
    QDir().mkpath(QFileInfo(out_path).absolutePath());
    QFile generated_file(generated_path);
    if (!generated_file.open(QIODevice::ReadOnly)) {
      throw std::runtime_error(
          QString("Could not read generated mesh: %1")
              .arg(generated_file.errorString())
              .toStdString());
    }
    QSaveFile output_file(out_path);
    if (!output_file.open(QIODevice::WriteOnly)) {
      throw std::runtime_error(
          QString("Could not open mesh output: %1")
              .arg(output_file.errorString())
              .toStdString());
    }
    while (!generated_file.atEnd()) {
      const QByteArray chunk = generated_file.read(1024 * 1024);
      if (chunk.isEmpty() && generated_file.error() != QFile::NoError) {
        output_file.cancelWriting();
        throw std::runtime_error(
            QString("Could not read generated mesh: %1")
                .arg(generated_file.errorString())
                .toStdString());
      }
      if (output_file.write(chunk) != chunk.size()) {
        output_file.cancelWriting();
        throw std::runtime_error(
            QString("Could not write mesh output: %1")
                .arg(output_file.errorString())
                .toStdString());
      }
    }
    if (!output_file.commit()) {
      throw std::runtime_error(
          QString("Could not commit mesh output: %1")
              .arg(output_file.errorString())
              .toStdString());
    }

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

    append_log("Mesh written: " + out_path);
    emit mesh_written(out_path);

    std::vector<int> element_types;
    std::vector<std::vector<std::size_t>> element_tags;
    std::vector<std::vector<std::size_t>> element_nodes;
    gmsh::model::mesh::getElements(element_types, element_tags, element_nodes);
    std::size_t elem_count = 0;
    for (const auto& tags : element_tags) {
      elem_count += tags.size();
    }

    append_log(QString("Nodes: %1, Elements: %2").arg(node_tags.size()).arg(elem_count));
    update_entity_summary();
    update_entity_list();
    update_physical_group_list();
    update_field_list();

    double quality_min = 0.0;
    double quality_mean = 0.0;
    double quality_max = 0.0;
    bool has_quality = false;
    // minSICN 只对最高维单元有意义。把点/线等低维单元混入查询会让
    // Gmsh 抛出 "Unknown element type for ordered monomials: 1"，从而把
    // 一个有效的 3D 网格误报成质量统计失败。
    std::vector<int> quality_element_types;
    std::vector<std::vector<std::size_t>> quality_element_tag_groups;
    std::vector<std::vector<std::size_t>> quality_element_nodes;
    gmsh::model::mesh::getElements(quality_element_types,
                                   quality_element_tag_groups,
                                   quality_element_nodes, dim);
    std::vector<std::size_t> quality_element_tags;
    QStringList top_element_composition;
    for (const auto& tags : quality_element_tag_groups) {
      quality_element_tags.insert(quality_element_tags.end(), tags.begin(),
                                  tags.end());
    }
    for (std::size_t i = 0; i < quality_element_types.size(); ++i) {
      std::string type_name;
      int type_dim = 0;
      int type_order = 0;
      int type_nodes = 0;
      int type_primary_nodes = 0;
      std::vector<double> local_coords;
      gmsh::model::mesh::getElementProperties(
          quality_element_types[i], type_name, type_dim, type_order,
          type_nodes, local_coords, type_primary_nodes);
      const std::size_t count = i < quality_element_tag_groups.size()
                                    ? quality_element_tag_groups[i].size()
                                    : 0;
      top_element_composition <<
          QString("%1=%2").arg(QString::fromStdString(type_name)).arg(count);
    }
    if (!top_element_composition.isEmpty()) {
      append_log("Top-dimensional element composition: " +
                 top_element_composition.join(", "));
    }
    if (dim >= 2 && !quality_element_tags.empty()) {
      try {
        std::vector<double> qualities;
        qualities.resize(quality_element_tags.size());
        gmsh::model::mesh::getElementQualities(quality_element_tags, qualities,
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
        quality_min = qmin;
        quality_mean = qmean;
        quality_max = qmax;
        has_quality = true;
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

    // W-00b：回读物理组清单 + 网格摘要 + 文件 SHA-256，作为
    // PhysicalGroupManifest 的生产者。读取失败仅降级为警告，不阻断生成。
    try {
      QVariantMap manifest;
      manifest.insert("mesh_path", out_path);
      QFile mesh_file(out_path);
      if (mesh_file.open(QIODevice::ReadOnly)) {
        manifest.insert(
            "mesh_sha256",
            QString::fromLatin1(
                QCryptographicHash::hash(mesh_file.readAll(),
                                         QCryptographicHash::Sha256)
                    .toHex()));
      }
      QVariantList group_list;
      std::vector<std::pair<int, int>> manifest_groups;
      gmsh::model::getPhysicalGroups(manifest_groups);
      for (const auto& g : manifest_groups) {
        std::string gname;
        gmsh::model::getPhysicalName(g.first, g.second, gname);
        QVariantMap entry;
        entry.insert("name",
                     gname.empty()
                         ? QString("group_%1_%2").arg(g.first).arg(g.second)
                         : QString::fromStdString(gname));
        entry.insert("dim", g.first);
        entry.insert("tags", QVariantList{g.second});
        std::vector<int> ent_tags;
        gmsh::model::getEntitiesForPhysicalGroup(g.first, g.second, ent_tags);
        entry.insert("entity_count", static_cast<int>(ent_tags.size()));
        std::size_t group_elements = 0;
        for (int ent : ent_tags) {
          std::vector<int> etypes;
          std::vector<std::vector<std::size_t>> etags;
          std::vector<std::vector<std::size_t>> enodes;
          gmsh::model::mesh::getElements(etypes, etags, enodes, g.first, ent);
          for (const auto& tags : etags) {
            group_elements += tags.size();
          }
        }
        entry.insert("element_count", static_cast<int>(group_elements));
        group_list.append(entry);
      }
      manifest.insert("physical_groups", group_list);
      manifest.insert("mesh_dim", dim);
      manifest.insert("node_count", static_cast<int>(node_tags.size()));
      manifest.insert("element_count", static_cast<int>(elem_count));
      QString dominant_type;
      std::size_t dominant_count = 0;
      for (std::size_t t = 0; t < quality_element_types.size(); ++t) {
        if (t < quality_element_tag_groups.size() &&
            quality_element_tag_groups[t].size() > dominant_count) {
          dominant_count = quality_element_tag_groups[t].size();
          std::string type_name;
          int etype_dim = 0;
          int etype_order = 0;
          int num_nodes = 0;
          int num_primary_nodes = 0;
          std::vector<double> local_coords;
          gmsh::model::mesh::getElementProperties(
              quality_element_types[t], type_name, etype_dim, etype_order,
              num_nodes,
              local_coords, num_primary_nodes);
          dominant_type = QString::fromStdString(type_name);
        }
      }
      manifest.insert("element_type", dominant_type);
      if (has_quality) {
        manifest.insert("quality_summary",
                        QVariantMap{{"quality_min", quality_min},
                                    {"quality_avg", quality_mean},
                                    {"quality_max", quality_max}});
      }
      emit mesh_manifest(manifest);
    } catch (const std::exception& ex) {
      append_log(
          QString("Physical group manifest collection failed: %1").arg(ex.what()));
    }
    success = true;
    if (!structured_fallback_reason.isEmpty()) {
      completion_message =
          gmp::l10n::current_language() == gmp::l10n::Language::Chinese
              ? QString::fromUtf8(
                    "网格已生成（已回退为%1）：%2。若要结构化砖形网格，请先将"
                    "几何划分为可扫掠六面块。")
                    .arg(dim == 3 ? QString::fromUtf8("四面体")
                                  : QString::fromUtf8("三角形"))
                    .arg(structured_fallback_reason)
              : QString("Mesh generated with %1 fallback: %2. Partition the "
                        "geometry into sweepable blocks for a structured mesh.")
                    .arg(dim == 3 ? "tetrahedral" : "triangular")
                    .arg(structured_fallback_reason);
    } else {
      completion_message = "Mesh generated.";
    }
  } catch (const std::exception& ex) {
    if (logger_started) {
      try {
        std::vector<std::string> log;
        gmsh::logger::get(log);
        gmsh::logger::stop();
        logger_started = false;
        for (const auto& line : log) {
          append_log(QString::fromStdString(line));
        }
      } catch (...) {
      }
    }
    if (!size_guard_rejected && !structured_mode_rejected) {
      completion_message = QString("Gmsh error: %1").arg(ex.what());
    }
    append_log(completion_message);
    if ((size_guard_rejected || structured_mode_rejected) &&
        !qEnvironmentVariableIsSet("GMP_SCREENSHOT_DIR")) {
      QMessageBox::warning(
          this,
          structured_mode_rejected
              ? (gmp::l10n::current_language() ==
                         gmp::l10n::Language::Chinese
                     ? QString::fromUtf8("无法生成严格结构化网格")
                     : QString("Strict Structured Mesh Unavailable"))
              : (gmp::l10n::current_language() ==
                         gmp::l10n::Language::Chinese
                     ? QString::fromUtf8("网格尺寸过细")
                     : QString("Mesh Size Too Fine")),
          completion_message);
    }
  }
  // ScopedGeneratedMeshModel 已销毁后再做一次显式恢复，确保同步的
  // mesh_generation_finished 观察者看到的是部件 OCC 模型而非离散模型。
  if (!external_model_name_.isEmpty()) {
    try {
      std::vector<std::string> model_names;
      gmsh::model::list(model_names);
      const std::string expected = external_model_name_.toStdString();
      if (std::find(model_names.begin(), model_names.end(), expected) !=
          model_names.end()) {
        gmsh::model::setCurrent(expected);
      } else {
        std::string restored_model;
        gmsh::model::getCurrent(restored_model);
        external_model_name_ = QString::fromStdString(restored_model);
        if (model_selector_ && model_selector_->currentIndex() >= 0) {
          model_selector_->setItemData(model_selector_->currentIndex(),
                                       external_model_name_);
        }
      }
    } catch (...) {
    }
  }
  set_mesh_generation_running(false);
  emit mesh_generation_finished(success, completion_message);
#endif
}

void GmshPanel::set_mesh_generation_running(bool running) {
  mesh_generation_running_ = running;
  for (auto* button : mesh_generate_buttons_) {
    if (!button) {
      continue;
    }
    if (running) {
      if (!button->property("idleText").isValid()) {
        button->setProperty("idleText", button->text());
      }
      button->setText(
          gmp::l10n::current_language() == gmp::l10n::Language::Chinese
              ? QString::fromUtf8("正在生成...")
              : QString("Generating..."));
      button->setToolTip(
          "Mesh generation is running; duplicate submission is disabled.");
    } else {
      button->setText(button->property("idleText").toString());
      button->setToolTip(QString());
    }
    button->setEnabled(!running && !mesh_external_busy_);
  }
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

bool GmshPanel::validate_physical_group_input(int dim, int exclude_tag,
                                              QString* error) const {
#ifdef GMP_ENABLE_GMSH_GUI
  const bool zh =
      gmp::l10n::current_language() == gmp::l10n::Language::Chinese;
  auto fail = [error, zh](const QString& en, const QString& zh_msg) {
    if (error) {
      *error = zh ? zh_msg : en;
    }
    return false;
  };
  if (dim < 0 || dim > 3) {
    return fail(QString("invalid dimension %1 (expected 0-3).").arg(dim),
                QString::fromUtf8("维度 %1 非法（应为 0~3）。").arg(dim));
  }
  const QString name =
      phys_group_name_ ? phys_group_name_->text().trimmed() : QString();
  if (name.isEmpty()) {
    return fail("name is empty; please enter a group name.",
                QString::fromUtf8("名称为空，请输入物理组名称。"));
  }
  const QString entities_text =
      phys_group_entities_ ? phys_group_entities_->text() : QString();
  if (entities_text.trimmed().isEmpty()) {
    return fail("no entities selected; pick at least one entity.",
                QString::fromUtf8("未选择实体，请至少选择一个实体。"));
  }
  const auto tokens = parse_dim_tag_tokens(entities_text);
  if (tokens.empty()) {
    return fail("Entities contains no valid entity IDs.",
                QString::fromUtf8("实体列表中没有合法的实体编号。"));
  }
  for (const auto& token : tokens) {
    if (token.has_dim && token.dim != dim) {
      return fail(QString("entity %1:%2 does not match group dimension %3.")
                      .arg(token.dim)
                      .arg(token.tag)
                      .arg(dim),
                  QString::fromUtf8("实体 %1:%2 的维度与物理组维度 %3 不一致。")
                      .arg(token.dim)
                      .arg(token.tag)
                      .arg(dim));
    }
  }
  if (resolve_entity_tags(dim, entities_text).empty()) {
    return fail(
        QString("selection has no existing entities of dimension %1.").arg(dim),
        QString::fromUtf8("所选内容中没有维度 %1 的现存实体。").arg(dim));
  }
  std::vector<std::pair<int, int>> groups;
  gmsh::model::getPhysicalGroups(groups, dim);
  for (const auto& g : groups) {
    if (g.second == exclude_tag) {
      continue;
    }
    std::string existing;
    gmsh::model::getPhysicalName(dim, g.second, existing);
    if (QString::fromStdString(existing).trimmed() == name) {
      return fail(QString("name \"%1\" is already used by physical group "
                          "%2:%3.")
                      .arg(name)
                      .arg(dim)
                      .arg(g.second),
                  QString::fromUtf8("名称“%1”已被物理组 %2:%3 使用。")
                      .arg(name)
                      .arg(dim)
                      .arg(g.second));
    }
  }
  return true;
#else
  Q_UNUSED(dim);
  Q_UNUSED(exclude_tag);
  Q_UNUSED(error);
  return false;
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
    QString error;
    if (!validate_physical_group_input(dim, -1, &error)) {
      append_log(QString("Physical group add rejected: %1").arg(error));
      return;
    }
    const auto tags = resolve_entity_tags(dim, phys_group_entities_->text());
    const std::string name = phys_group_name_->text().trimmed().toStdString();
    const int group_tag = gmsh::model::addPhysicalGroup(dim, tags, -1, name);
    gmsh::model::setPhysicalName(dim, group_tag, name);
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
    QString error;
    if (!validate_physical_group_input(dim, tag, &error)) {
      append_log(QString("Physical group update rejected: %1").arg(error));
      return;
    }
    const auto tags = resolve_entity_tags(dim, phys_group_entities_->text());
    gmsh::model::removePhysicalGroups({{dim, tag}});
    const std::string name = phys_group_name_->text().trimmed().toStdString();
    gmsh::model::addPhysicalGroup(dim, tags, tag, name);
    gmsh::model::setPhysicalName(dim, tag, name);
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
  // 日志统一出口：实时镜像到主窗口 Console 与操作日志文件。
  gmp::log_operation("gmsh", text);
}

}  // namespace gmp
