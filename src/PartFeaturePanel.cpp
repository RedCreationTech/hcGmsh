#include "gmp/PartFeaturePanel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

// WS3 实装: 部件特征面板 (拉伸/旋转/放样/扫掠)。
// 面板不直接调 OccBridge, 只发信号, 由 MainWindow 统一接线。

namespace gmp {

namespace {

// 刷新下拉框内容并尽量保留原选择
void refresh_combo(QComboBox* combo, const QStringList& names) {
  const QString prev = combo->currentText();
  combo->blockSignals(true);
  combo->clear();
  combo->addItems(names);
  const int idx = combo->findText(prev);
  if (idx >= 0)
    combo->setCurrentIndex(idx);
  combo->blockSignals(false);
}

QDoubleSpinBox* make_spin(double value, double min, double max,
                          const QString& suffix, QWidget* parent) {
  auto* spin = new QDoubleSpinBox(parent);
  spin->setRange(min, max);
  spin->setDecimals(3);
  spin->setValue(value);
  spin->setSuffix(suffix);
  return spin;
}

}  // namespace

PartFeaturePanel::PartFeaturePanel(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(10, 10, 10, 10);
  layout->setSpacing(6);

  auto* heading = new QLabel("Part Features", this);
  QFont hfont = heading->font();
  hfont.setPointSize(hfont.pointSize() + 3);
  hfont.setBold(true);
  heading->setFont(hfont);
  layout->addWidget(heading);

  auto* desc = new QLabel(
      "Create 3D features from closed sketch profiles: extrude, revolve, "
      "loft and sweep. The result is imported into the mesh model and can be "
      "written to a BREP file.",
      this);
  desc->setWordWrap(true);
  layout->addWidget(desc);

  auto* feature_tabs = new QTabWidget(this);
  feature_tabs->setObjectName("partFeatureTabs");
  feature_tabs->setDocumentMode(true);

  // ---- 拉伸 ----
  auto* extrude_group = new QGroupBox(this);
  auto* extrude_form = new QFormLayout(extrude_group);
  extrude_form->setRowWrapPolicy(QFormLayout::DontWrapRows);
  extrude_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  sketch_combo_ = new QComboBox(extrude_group);
  sketch_combo_->setToolTip("Profile sketch (also used by Revolve).");
  extrude_distance_ = make_spin(10.0, -1e6, 1e6, " mm", extrude_group);
  auto* extrude_btn = new QPushButton("Extrude along +Z", extrude_group);
  extrude_btn->setToolTip(
      "Extrude the closed profile along +Z; a negative distance reverses "
      "the direction.");
  extrude_form->addRow("Sketch:", sketch_combo_);
  extrude_form->addRow("Distance:", extrude_distance_);
  extrude_form->addRow(extrude_btn);
  feature_tabs->addTab(extrude_group, "Extrude");

  // ---- 旋转 ----
  auto* revolve_group = new QGroupBox(this);
  auto* revolve_form = new QFormLayout(revolve_group);
  revolve_form->setRowWrapPolicy(QFormLayout::DontWrapRows);
  revolve_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  revolve_angle_ = make_spin(90.0, -360.0, 360.0, " deg", revolve_group);
  auto* revolve_btn = new QPushButton("Revolve about Y axis", revolve_group);
  revolve_btn->setToolTip(
      "Revolve the profile about the Y axis through the origin. The profile "
      "must not intersect the axis.");
  revolve_form->addRow("Angle:", revolve_angle_);
  revolve_form->addRow(revolve_btn);
  feature_tabs->addTab(revolve_group, "Revolve");

  // ---- 放样 (v1: 固定两个截面) ----
  auto* loft_group = new QGroupBox(this);
  auto* loft_form = new QFormLayout(loft_group);
  loft_form->setRowWrapPolicy(QFormLayout::DontWrapRows);
  loft_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  loft_first_ = new QComboBox(loft_group);
  loft_second_ = new QComboBox(loft_group);
  loft_z_ = make_spin(10.0, -1e6, 1e6, " mm", loft_group);
  loft_z_->setToolTip("Z lift applied to the second section.");
  auto* loft_btn = new QPushButton("Loft (solid)", loft_group);
  loft_btn->setToolTip(
      "Loft a solid between two sections; the second section is lifted by "
      "the Z offset.");
  loft_form->addRow("First section:", loft_first_);
  loft_form->addRow("Second section:", loft_second_);
  loft_form->addRow("Z lift:", loft_z_);
  loft_form->addRow(loft_btn);
  feature_tabs->addTab(loft_group, "Loft");

  // ---- 扫掠 ----
  auto* sweep_group = new QGroupBox(this);
  auto* sweep_form = new QFormLayout(sweep_group);
  sweep_form->setRowWrapPolicy(QFormLayout::DontWrapRows);
  sweep_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  sweep_profile_ = new QComboBox(sweep_group);
  sweep_path_ = new QComboBox(sweep_group);
  sweep_path_->setToolTip(
      "Path sketch must be a single connected chain of lines/arcs.");
  auto* sweep_btn = new QPushButton("Sweep profile along path", sweep_group);
  sweep_form->addRow("Profile sketch:", sweep_profile_);
  sweep_form->addRow("Path sketch:", sweep_path_);
  sweep_form->addRow(sweep_btn);
  feature_tabs->addTab(sweep_group, "Sweep");
  layout->addWidget(feature_tabs);

  connect(extrude_btn, &QPushButton::clicked, this, [this] {
    const QString name = selected_sketch();
    if (!name.isEmpty())
      emit extrude_requested(name, extrude_distance_->value());
  });
  connect(revolve_btn, &QPushButton::clicked, this, [this] {
    const QString name = selected_sketch();
    if (!name.isEmpty())
      emit revolve_requested(name, revolve_angle_->value());
  });
  connect(loft_btn, &QPushButton::clicked, this, [this] {
    const QString first = loft_first_->currentText();
    const QString second = loft_second_->currentText();
    if (!first.isEmpty() && !second.isEmpty())
      emit loft_requested({first, second});
  });
  connect(sweep_btn, &QPushButton::clicked, this, [this] {
    const QString profile = sweep_profile_->currentText();
    const QString path = sweep_path_->currentText();
    if (!profile.isEmpty() && !path.isEmpty())
      emit sweep_requested(profile, path);
  });
}

void PartFeaturePanel::set_sketch_names(const QStringList& names) {
  refresh_combo(sketch_combo_, names);
  refresh_combo(loft_first_, names);
  refresh_combo(loft_second_, names);
  refresh_combo(sweep_profile_, names);
  refresh_combo(sweep_path_, names);
}

QString PartFeaturePanel::selected_sketch() const {
  return sketch_combo_->currentText();
}

double PartFeaturePanel::loft_second_z() const { return loft_z_->value(); }

}  // namespace gmp
