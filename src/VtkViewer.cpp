#include "gmp/VtkViewer.h"

#include "gmp/MeshViewport.h"
#include "gmp/PhysicalGroupManifest.h"
#include "gmp/ResultViewport.h"
#include "gmp/SketchViewport.h"
#include "ViewportInternal.h"
#include "gmp/ViewportCamera.h"
#include "gmp/ViewportSelection.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <QStringList>
#include <QtCore/Qt>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>

#include "gmp/ComboPopupFix.h"
#include "gmp/SketchSolver.h"

#ifdef GMP_ENABLE_VTK_VIEWER
#include <QVTKOpenGLNativeWidget.h>
#include <vtkActor.h>
#include <vtkCellData.h>
#include <vtkCompositeDataGeometryFilter.h>
#include <vtkCompositeDataIterator.h>
#include <vtkCompositeDataSet.h>
#include <vtkDataArray.h>
#include <vtkDataSet.h>
#include <vtkDataObject.h>
#include <vtkDataSet.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkDataSetMapper.h>
#include <vtkExodusIIReader.h>
#include <vtkFieldData.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkMultiBlockDataSetAlgorithm.h>
#include <vtkObjectFactory.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkCamera.h>
#include <vtkInformation.h>
#include <vtkLookupTable.h>
#include <vtkMeshQuality.h>
#include <vtkOutlineFilter.h>
#include <vtkAxesActor.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkTextProperty.h>
#include <vtkWarpVector.h>
#include <vtkPlane.h>
#include <vtkCutter.h>
#include <vtkShrinkFilter.h>
#include <vtkThreshold.h>
#include <vtkProperty.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkIntArray.h>
#include <vtkUnstructuredGrid.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkCellType.h>
#include <vtkVertexGlyphFilter.h>
#include <vtkSphereSource.h>
#include <vtkTubeFilter.h>
#include <vtkRenderer.h>
#include <vtkScalarBarActor.h>
#include <vtkWindowToImageFilter.h>
#include <vtkPNGWriter.h>
#include <vtkStreamingDemandDrivenPipeline.h>
#include <vtkCellPicker.h>
#include <vtkCallbackCommand.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyle.h>
#include <vtkInteractorStyleImage.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkCommand.h>
#endif

#ifdef GMP_ENABLE_GMSH_GUI
#include <gmsh.h>
#endif

namespace gmp {

#ifdef GMP_ENABLE_VTK_VIEWER
namespace {

}  // namespace
#endif

VtkViewer::~VtkViewer() = default;

VtkViewer::VtkViewer(QWidget* parent) : QWidget(parent) {
  sketch_viewport_ = std::make_unique<SketchViewport>(this);
  mesh_viewport_ = std::make_unique<MeshViewport>(this);
  result_viewport_ = std::make_unique<ResultViewport>(this);
  auto* layout = new QVBoxLayout(this);

  // 顶部文件/输出行改为独立容器 top_bar_, 由 MainWindow 迁移到右侧边栏,
  // 中央区域只保留 3D 场景与变量列表
  top_bar_ = new QWidget();
  auto* top_bar_layout = new QVBoxLayout(top_bar_);
  top_bar_layout->setContentsMargins(0, 0, 0, 0);
  top_bar_layout->setSpacing(4);
  file_label_ = new QLabel("No file loaded");
  // 长路径不撑宽边栏, 超出部分直接裁剪
  file_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  open_btn_ = new QPushButton("Open");
  reload_btn_ = new QPushButton("Reload");
  connect(open_btn_, &QPushButton::clicked, this, &VtkViewer::on_open_file);
  connect(reload_btn_, &QPushButton::clicked, this, &VtkViewer::on_reload);
  top_bar_layout->addWidget(file_label_);
  {
    auto* btn_row = new QHBoxLayout();
    btn_row->setContentsMargins(0, 0, 0, 0);
    btn_row->setSpacing(6);
    btn_row->addWidget(open_btn_);
    btn_row->addWidget(reload_btn_);
    btn_row->addStretch(1);
    top_bar_layout->addLayout(btn_row);
  }

  output_combo_ = new QComboBox();
  AttachComboPopupFix(output_combo_);
  output_pick_ = new QPushButton("Load Selected");
  connect(output_pick_, &QPushButton::clicked, this, [this]() {
    const QString path = output_combo_->currentData().toString();
    if (!path.isEmpty()) {
      set_exodus_file(path);
    }
  });
  {
    // 下拉框自带语义, 不再单独放 "Outputs" 标签行
    auto* output_row = new QHBoxLayout();
    output_row->setContentsMargins(0, 0, 0, 0);
    output_row->setSpacing(6);
    output_row->addWidget(output_combo_, 1);
    output_row->addWidget(output_pick_);
    top_bar_layout->addLayout(output_row);
  }

  auto* main_split = new QSplitter(Qt::Horizontal, this);
  main_split->setChildrenCollapsible(false);
  main_split->setStretchFactor(0, 0);
  main_split->setStretchFactor(1, 1);
  layout->addWidget(main_split, 1);

  auto* left_panel = new QWidget(main_split);
  left_panel->setMaximumWidth(220);  // Variables 列表限宽, 让 3D 场景占更大比例
  auto* left_layout = new QVBoxLayout(left_panel);
  left_layout->setContentsMargins(0, 0, 0, 0);
  left_layout->setSpacing(6);
  left_layout->addWidget(new QLabel("Variables"));
  array_filter_ = new QComboBox(left_panel);
  array_filter_->addItem("All", "all");
  array_filter_->addItem("Point", "P");
  array_filter_->addItem("Cell", "C");
  AttachComboPopupFix(array_filter_);
  connect(array_filter_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { update_array_list(); });
  left_layout->addWidget(array_filter_);
  array_list_ = new QListWidget(left_panel);
  array_list_->setSelectionMode(QAbstractItemView::SingleSelection);
  connect(array_list_, &QListWidget::currentItemChanged, this,
          [this](QListWidgetItem* current, QListWidgetItem*) {
            if (!current || !array_combo_) {
              return;
            }
            const QString key = current->data(Qt::UserRole).toString();
            const int idx = array_combo_->findData(key);
            if (idx >= 0 && array_combo_->currentIndex() != idx) {
              array_combo_->setCurrentIndex(idx);
            }
          });
  left_layout->addWidget(array_list_, 1);

  auto* right_panel = new QWidget(main_split);
  auto* right_layout = new QVBoxLayout(right_panel);
  right_layout->setContentsMargins(0, 0, 0, 0);
  right_layout->setSpacing(6);

  // 窄侧边栏里页签条会超出宽度(出现滚动箭头), 改为"下拉选择器+堆叠页"
  auto* control_host = new QWidget(right_panel);
  auto* control_host_layout = new QVBoxLayout(control_host);
  control_host_layout->setContentsMargins(0, 0, 0, 0);
  control_host_layout->setSpacing(4);
  control_nav_ = new QComboBox(control_host);
  AttachComboPopupFix(control_nav_);
  control_host_layout->addWidget(control_nav_);
  control_stack_ = new QStackedWidget(control_host);
  control_host_layout->addWidget(control_stack_, 1);
  control_tabs_ = control_host;
  right_layout->addWidget(control_host);
  connect(control_nav_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          control_stack_, &QStackedWidget::setCurrentIndex);
  auto make_tab = [this](const QString& name) {
    auto* tab = new QWidget(control_stack_);
    // 页内容不参与撑宽堆叠区, 各页按边栏宽度显示, 宽内容自行内部滚动
    tab->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto* tab_page_layout = new QVBoxLayout(tab);
    tab_page_layout->setContentsMargins(0, 0, 0, 0);
    tab_page_layout->setSpacing(0);
    // 窗体高度统一处理：页内容包一层滚动区（widgetResizable + NoFrame），
    // 空间不足时页内滚动，不再压扁控件；滚动只此一层。
    auto* scroll = new QScrollArea(tab);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scroll);
    auto* tab_layout = new QVBoxLayout(content);
    tab_layout->setContentsMargins(0, 0, 0, 0);
    tab_layout->setSpacing(4);
    scroll->setWidget(content);
    tab_page_layout->addWidget(scroll);
    control_stack_->addWidget(tab);
    control_nav_->addItem(name);
    return tab_layout;
  };

  // 纵向堆叠控件, 适配窄侧边栏; 行内说明文字(QLabel)独占一行位于控件上方
  auto vadd = [](QVBoxLayout* layout, std::initializer_list<QWidget*> ws) {
    for (auto* w : ws) {
      layout->addWidget(w);
    }
  };
  // 窄控件紧凑同行(复选框/窄下拉/按钮等), 行尾自动补 stretch
  auto hrow = [](QVBoxLayout* layout, std::initializer_list<QWidget*> ws) {
    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);
    for (auto* w : ws) {
      row->addWidget(w);
    }
    row->addStretch(1);
    layout->addLayout(row);
  };

  auto* scalar_layout = make_tab("Scalar");

  array_combo_ = new QComboBox();
  AttachComboPopupFix(array_combo_);
  connect(array_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &VtkViewer::on_array_changed);
  preset_combo_ = new QComboBox();
  preset_combo_->addItem("Blue-Red");
  preset_combo_->addItem("Grayscale");
  preset_combo_->addItem("Rainbow");
  AttachComboPopupFix(preset_combo_);
  connect(preset_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &VtkViewer::on_preset_changed);
  repr_combo_ = new QComboBox();
  repr_combo_->addItem("Surface");
  repr_combo_->addItem("Wireframe");
  repr_combo_->addItem("Surface + Edges");
  AttachComboPopupFix(repr_combo_);
  connect(repr_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &VtkViewer::on_repr_changed);
  scalar_bar_pos_combo_ = new QComboBox();
  scalar_bar_pos_combo_->addItem("Right-Bottom", 0);
  scalar_bar_pos_combo_->addItem("Right-Top", 1);
  scalar_bar_pos_combo_->addItem("Left-Bottom", 2);
  scalar_bar_pos_combo_->addItem("Left-Top", 3);
  AttachComboPopupFix(scalar_bar_pos_combo_);
  connect(scalar_bar_pos_combo_,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int index) {
            scalar_bar_pos_ = scalar_bar_pos_combo_->itemData(index).toInt();
            apply_scalar_bar_pos();
            if (render_window_) {
              render_window_->Render();
            }
          });

  auto_range_ = new QCheckBox("Auto Range");
  auto_range_->setChecked(true);
  connect(auto_range_, &QCheckBox::toggled, this, &VtkViewer::on_apply_range);
  range_min_ = new QDoubleSpinBox();
  range_max_ = new QDoubleSpinBox();
  range_min_->setDecimals(6);
  range_max_->setDecimals(6);
  range_min_->setRange(-1e12, 1e12);
  range_max_->setRange(-1e12, 1e12);
  range_min_->setEnabled(false);
  range_max_->setEnabled(false);
  connect(range_min_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &VtkViewer::on_apply_range);
  connect(range_max_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &VtkViewer::on_apply_range);

  // 标签—控件同行的紧凑表单(对标 Abaqus 弹窗范式), Auto Range 行保持一行
  auto* scalar_form = new QFormLayout();
  scalar_form->setContentsMargins(0, 0, 0, 0);
  scalar_form->setSpacing(4);
  scalar_form->addRow("Scalar", array_combo_);
  scalar_form->addRow("Preset", preset_combo_);
  scalar_form->addRow("Repr", repr_combo_);
  scalar_form->addRow("Bar Pos", scalar_bar_pos_combo_);
  scalar_layout->addLayout(scalar_form);
  hrow(scalar_layout, {auto_range_, range_min_, range_max_});
  scalar_layout->addStretch(1);

  auto* mesh_layout = make_tab("Mesh");
  show_faces_ = new QCheckBox("Faces");
  show_faces_->setChecked(true);
  connect(show_faces_, &QCheckBox::toggled, this,
          [this](bool) { apply_mesh_visuals(); });
  show_edges_ = new QCheckBox("Edges");
  show_edges_->setChecked(true);
  connect(show_edges_, &QCheckBox::toggled, this,
          [this](bool) { apply_mesh_visuals(); });
  show_shell_ = new QCheckBox("Shell");
  show_shell_->setChecked(true);
  connect(show_shell_, &QCheckBox::toggled, this,
          [this](bool) { update_mesh_pipeline(); });
  show_nodes_ = new QCheckBox("Nodes");
  connect(show_nodes_, &QCheckBox::toggled, this,
          [this](bool) { update_nodes_visibility(); });
  show_quality_ = new QCheckBox("Quality");
  connect(show_quality_, &QCheckBox::toggled, this, [this](bool checked) {
    update_pipeline();
    const QString target = checked ? "C:Quality" : "C:phys_id";
    const int idx = array_combo_->findData(target);
    if (idx >= 0) {
      array_combo_->setCurrentIndex(idx);
    }
  });
  mesh_dim_ = new QComboBox();
  mesh_dim_->setObjectName("meshDimensionCombo");
  mesh_dim_->addItem("All", -1);
  mesh_dim_->addItem("0", 0);
  mesh_dim_->addItem("1", 1);
  mesh_dim_->addItem("2", 2);
  mesh_dim_->addItem("3", 3);
  AttachComboPopupFix(mesh_dim_);
  connect(mesh_dim_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { update_mesh_pipeline(); });
  mesh_group_ = new QComboBox();
  AttachComboPopupFix(mesh_group_);
  connect(mesh_group_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { update_mesh_pipeline(); });
  hrow(mesh_layout, {new QLabel("Mesh"), show_faces_, show_edges_, show_shell_});
  hrow(mesh_layout, {show_nodes_, show_quality_});
  vadd(mesh_layout,
       {new QLabel("Dim"), mesh_dim_, new QLabel("Group"), mesh_group_});

  mesh_entity_ = new QComboBox();
  AttachComboPopupFix(mesh_entity_);
  connect(mesh_entity_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { update_mesh_pipeline(); });
  vadd(mesh_layout, {new QLabel("Entity"), mesh_entity_});

  mesh_type_ = new QComboBox();
  AttachComboPopupFix(mesh_type_);
  connect(mesh_type_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { update_mesh_pipeline(); });
  mesh_opacity_ = new QDoubleSpinBox();
  mesh_opacity_->setRange(0.05, 1.0);
  mesh_opacity_->setSingleStep(0.05);
  mesh_opacity_->setValue(1.0);
  connect(mesh_opacity_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, [this](double) { apply_mesh_visuals(); });
  mesh_shrink_ = new QDoubleSpinBox();
  mesh_shrink_->setRange(0.0, 1.0);
  mesh_shrink_->setSingleStep(0.05);
  mesh_shrink_->setValue(1.0);
  connect(mesh_shrink_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, [this](double) { update_mesh_pipeline(); });
  mesh_scalar_bar_ = new QCheckBox("Scalar Bar");
  mesh_scalar_bar_->setChecked(true);
  connect(mesh_scalar_bar_, &QCheckBox::toggled, this,
          [this](bool) { apply_mesh_visuals(); });
  pick_enable_ = new QCheckBox("Pick");
  pick_enable_->setChecked(false);
  connect(pick_enable_, &QCheckBox::toggled, this, [this](bool enabled) {
    if (pick_info_) {
      pick_info_->setText(enabled ? "Pick: click to inspect"
                                  : "Pick: disabled");
    }
    update_selection_pipeline();
    emit stage_picking_changed(enabled);
  });
  pick_mode_ = new QComboBox();
  pick_mode_->addItem("Group", 0);
  pick_mode_->addItem("Entity", 1);
  pick_mode_->addItem("Cell", 2);
  AttachComboPopupFix(pick_mode_);
  connect(pick_mode_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { update_selection_pipeline(); });
  pick_clear_ = new QPushButton("Clear");
  connect(pick_clear_, &QPushButton::clicked, this, [this]() {
    selection_.group_dim_ = -1;
    selection_.group_id_ = -1;
    selection_.entity_dim_ = -1;
    selection_.entity_tag_ = -1;
    selection_.cell_id_ = -1;
    update_selection_pipeline();
    if (pick_info_) {
      pick_info_->setText("Pick: cleared");
    }
  });
  vadd(mesh_layout,
       {new QLabel("Type"), mesh_type_});
  hrow(mesh_layout, {new QLabel("Opacity"), mesh_opacity_, new QLabel("Shrink"),
                     mesh_shrink_});
  hrow(mesh_layout, {mesh_scalar_bar_, pick_enable_, pick_mode_, pick_clear_});

  // 原 Probe 控制页并入本页底部作为分组
  auto* probe_header = new QLabel("Probe");
  probe_header->setStyleSheet("color: #666;");
  mesh_layout->addWidget(probe_header);
  probe_enable_ = new QCheckBox("Enable Probe");
  probe_mode_ = new QComboBox();
  probe_mode_->addItem("Point", 0);
  probe_mode_->addItem("Cell", 1);
  AttachComboPopupFix(probe_mode_);
  probe_clear_ = new QPushButton("Clear");
  hrow(mesh_layout, {probe_enable_, new QLabel("Mode"), probe_mode_,
                     probe_clear_});
  probe_info_ = new QLabel("Probe: disabled");
  probe_info_->setWordWrap(true);
  mesh_layout->addWidget(probe_info_);
  mesh_layout->addStretch(1);
  auto update_probe_status = [this]() {
    if (!probe_info_) {
      return;
    }
    if (!probe_enable_ || !probe_enable_->isChecked()) {
      probe_info_->setText("Probe: disabled");
      return;
    }
    const QString mode =
        probe_mode_ ? probe_mode_->currentText().toLower() : "point";
    probe_info_->setText(QString("Probe: enabled (%1 mode)").arg(mode));
  };
  connect(probe_enable_, &QCheckBox::toggled, this,
          [this, update_probe_status](bool enabled) {
            update_probe_status();
            if (probe_enable_ && probe_enable_->isEnabled()) {
              emit stage_picking_changed(enabled);
            }
          });
  connect(probe_mode_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [update_probe_status](int) { update_probe_status(); });
  connect(probe_clear_, &QPushButton::clicked, this, [this]() {
    if (probe_info_) {
      probe_info_->setText("Probe: cleared");
    }
  });

  auto* view_layout = make_tab("View");
  view_combo_ = new QComboBox();
  view_combo_->addItem("Reset", 0);
  view_combo_->addItem("Front", 1);
  view_combo_->addItem("Right", 2);
  view_combo_->addItem("Top", 3);
  view_combo_->addItem("Iso", 4);
  AttachComboPopupFix(view_combo_);
  view_apply_ = new QPushButton("Apply View");
  connect(view_apply_, &QPushButton::clicked, this, [this]() {
    apply_view_preset(view_combo_ ? view_combo_->currentData().toInt() : 0);
  });
  show_axes_ = new QCheckBox("Axes");
  show_axes_->setChecked(true);
  connect(show_axes_, &QCheckBox::toggled, this,
          [this](bool) { update_scene_extras(); });
  show_outline_ = new QCheckBox("Outline");
  show_outline_->setChecked(false);
  connect(show_outline_, &QCheckBox::toggled, this,
          [this](bool) { update_scene_extras(); });
  auto* reset_filters = new QPushButton("Reset Filters");
  connect(reset_filters, &QPushButton::clicked, this, [this]() {
    if (mesh_dim_) {
      const int idx = mesh_dim_->findData(-1);
      if (idx >= 0) {
        mesh_dim_->setCurrentIndex(idx);
      }
    }
    if (mesh_group_) {
      const int idx = mesh_group_->findData(-1);
      if (idx >= 0) {
        mesh_group_->setCurrentIndex(idx);
      }
    }
    if (mesh_entity_) {
      const int idx = mesh_entity_->findData(-1);
      if (idx >= 0) {
        mesh_entity_->setCurrentIndex(idx);
      }
    }
    if (mesh_type_) {
      const int idx = mesh_type_->findData(-1);
      if (idx >= 0) {
        mesh_type_->setCurrentIndex(idx);
      }
    }
    if (slice_enable_) {
      slice_enable_->setChecked(false);
    }
    selection_.group_dim_ = -1;
    selection_.group_id_ = -1;
    selection_.entity_dim_ = -1;
    selection_.entity_tag_ = -1;
    selection_.preview_dim_ = -1;
    selection_.preview_tag_ = -1;
    preview_visual_active_ = false;
    selection_.cell_id_ = -1;
    update_mesh_pipeline();
  });
  hrow(view_layout, {view_combo_, view_apply_});
  hrow(view_layout, {show_axes_, show_outline_, reset_filters});

  pick_info_ = new QLabel("Pick: disabled");
  view_layout->addWidget(pick_info_);

  mesh_legend_ = new QLabel();
  mesh_legend_->setWordWrap(true);
  mesh_legend_->setText("Groups: none");
  view_layout->addWidget(mesh_legend_);

  // 原 Slice 控制页并入本页底部作为分组(复选+轴向+滑块同行)
  slice_enable_ = new QCheckBox("Slice");
  slice_axis_ = new QComboBox();
  slice_axis_->addItems({"X", "Y", "Z"});
  AttachComboPopupFix(slice_axis_);
  slice_slider_ = new QSlider(Qt::Horizontal);
  slice_slider_->setRange(0, 100);
  slice_slider_->setValue(50);
  connect(slice_enable_, &QCheckBox::toggled, this,
          [this](bool enabled) {
            update_mesh_pipeline();
            emit stage_slice_changed(enabled);
          });
  connect(slice_axis_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { update_mesh_pipeline(); });
  connect(slice_slider_, &QSlider::valueChanged, this,
          [this](int) { update_mesh_pipeline(); });
  {
    auto* slice_row = new QHBoxLayout();
    slice_row->setContentsMargins(0, 0, 0, 0);
    slice_row->setSpacing(6);
    slice_row->addWidget(slice_enable_);
    slice_row->addWidget(slice_axis_);
    slice_row->addWidget(slice_slider_, 1);
    view_layout->addLayout(slice_row);
  }
  view_layout->addStretch(1);

  auto* time_layout = make_tab("Time");
  auto_refresh_ = new QCheckBox("Auto Refresh");
  refresh_ms_ = new QSpinBox();
  refresh_ms_->setRange(250, 10000);
  refresh_ms_->setSingleStep(250);
  refresh_ms_->setValue(1000);
  refresh_timer_ = new QTimer(this);
  connect(auto_refresh_, &QCheckBox::toggled, this,
          &VtkViewer::on_auto_refresh_toggled);
  connect(refresh_timer_, &QTimer::timeout, this,
          &VtkViewer::on_auto_refresh_tick);
  connect(refresh_ms_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int) {
            if (refresh_timer_->isActive()) {
              refresh_timer_->start(refresh_ms_->value());
            }
          });
  debounce_timer_ = new QTimer(this);
  debounce_timer_->setSingleShot(true);
  connect(debounce_timer_, &QTimer::timeout, this,
          &VtkViewer::on_auto_refresh_tick);
  hrow(time_layout, {auto_refresh_, refresh_ms_, new QLabel("ms")});

  time_slider_ = new QSlider(Qt::Horizontal);
  time_slider_->setRange(0, 0);
  time_label_ = new QLabel("t=0");
  connect(time_slider_, &QSlider::valueChanged, this, &VtkViewer::on_time_changed);
  {
    auto* time_row = new QHBoxLayout();
    time_row->setContentsMargins(0, 0, 0, 0);
    time_row->setSpacing(6);
    time_row->addWidget(new QLabel("Time"));
    time_row->addWidget(time_slider_, 1);
    time_row->addWidget(time_label_);
    time_layout->addLayout(time_row);
  }
  time_layout->addStretch(1);

  auto* deform_layout = make_tab("Deformation");
  deform_enable_ = new QCheckBox("Enable Deformation");
  deform_vector_ = new QComboBox();
  AttachComboPopupFix(deform_vector_);
  deform_scale_ = new QDoubleSpinBox();
  deform_scale_->setRange(0.0, 1000.0);
  deform_scale_->setSingleStep(0.1);
  deform_scale_->setValue(1.0);
  vadd(deform_layout, {deform_enable_, new QLabel("Vector"), deform_vector_});
  hrow(deform_layout, {new QLabel("Scale"), deform_scale_});
  auto* deform_hint = new QLabel(
      "Applies warping using the selected vector array.", control_stack_);
  deform_hint->setStyleSheet("color: #666;");
  deform_layout->addWidget(deform_hint);

  // 原 Vector 控制页并入本页作为分组(向量数组/自动同步/Apply)
  auto* vector_header = new QLabel("Vector Arrays");
  vector_header->setStyleSheet("color: #666;");
  deform_layout->addWidget(vector_header);
  vector_array_combo_ = new QComboBox();
  AttachComboPopupFix(vector_array_combo_);
  vector_auto_sync_deform_ = new QCheckBox("Auto-sync deformation vector");
  vector_auto_sync_deform_->setChecked(true);
  vector_apply_to_deform_ = new QPushButton("Apply to Deform");
  vector_info_ = new QLabel("No vector data loaded");
  vector_info_->setWordWrap(true);
  vadd(deform_layout, {vector_array_combo_});
  hrow(deform_layout, {vector_auto_sync_deform_, vector_apply_to_deform_});
  deform_layout->addWidget(vector_info_);
  deform_layout->addStretch(1);
  connect(deform_enable_, &QCheckBox::toggled, this, [this](bool) {
    update_deformation_pipeline();
    update_scene_extras();
    if (render_window_) {
      render_window_->Render();
    }
  });
  connect(deform_vector_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) {
            update_deformation_pipeline();
            update_scene_extras();
            if (render_window_) {
              render_window_->Render();
            }
          });
  connect(deform_scale_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, [this](double) {
            update_deformation_pipeline();
            update_scene_extras();
            if (render_window_) {
              render_window_->Render();
            }
          });
  connect(vector_array_combo_,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) {
            update_vector_tab();
          });
  if (vector_auto_sync_deform_) {
    connect(vector_auto_sync_deform_, &QCheckBox::toggled, this,
            [this](bool) {
              if (vector_auto_sync_deform_ && vector_auto_sync_deform_->isChecked()) {
                update_vector_tab();
              }
            });
  }
  connect(vector_apply_to_deform_, &QPushButton::clicked, this, [this]() {
    const QString key = vector_array_combo_
                            ? vector_array_combo_->currentData().toString()
                            : QString();
    if (key.isEmpty() || !deform_vector_) {
      return;
    }
    const int idx = deform_vector_->findData(key);
    if (idx >= 0) {
      deform_vector_->blockSignals(true);
      deform_vector_->setCurrentIndex(idx);
      deform_vector_->blockSignals(false);
    }
    if (deform_enable_ && !deform_enable_->isChecked()) {
      deform_enable_->setChecked(true);
    }
    if (!key.isEmpty()) {
      if (vector_auto_sync_deform_) {
        vector_auto_sync_deform_->setChecked(true);
      }
    }
    update_deformation_pipeline();
    update_scene_extras();
    if (render_window_) {
      render_window_->Render();
    }
  });

#ifdef GMP_ENABLE_VTK_VIEWER
  vtk_widget_ = new QVTKOpenGLNativeWidget(right_panel);
  vtk_widget_->setMinimumSize(240, 160);
  right_layout->addWidget(vtk_widget_, 1);
  QTimer::singleShot(0, this, [this]() { init_vtk(); });
#else
  auto* label =
      new QLabel("VTK Viewer Disabled\n(Rebuild with GMP_ENABLE_VTK_VIEWER=ON)");
  label->setAlignment(Qt::AlignCenter);
  right_layout->addWidget(label, 1);
  reload_btn_->setEnabled(false);
  open_btn_->setEnabled(false);
  array_combo_->setEnabled(false);
  preset_combo_->setEnabled(false);
  repr_combo_->setEnabled(false);
  auto_range_->setEnabled(false);
  range_min_->setEnabled(false);
  range_max_->setEnabled(false);
  output_combo_->setEnabled(false);
  output_pick_->setEnabled(false);
  auto_refresh_->setEnabled(false);
  refresh_ms_->setEnabled(false);
  time_slider_->setEnabled(false);
  show_nodes_->setEnabled(false);
  show_quality_->setEnabled(false);
  show_faces_->setEnabled(false);
  show_edges_->setEnabled(false);
  show_shell_->setEnabled(false);
  mesh_dim_->setEnabled(false);
  mesh_group_->setEnabled(false);
  slice_enable_->setEnabled(false);
  slice_axis_->setEnabled(false);
  slice_slider_->setEnabled(false);
  mesh_legend_->setEnabled(false);
#endif
}

void VtkViewer::clear_stage_data() {
  current_file_.clear();
  pending_reload_ = false;
  setup_watcher(QString());
  if (debounce_timer_) {
    debounce_timer_->stop();
  }
  if (file_label_) {
    file_label_->setText("No file loaded");
  }

#ifdef GMP_ENABLE_VTK_VIEWER
  pipeline_ready_ = false;
  first_render_ = true;
  mode_ = DataMode::None;
  mesh_grid_ = nullptr;
  mesh_quality_ready_ = false;
  mesh_groups_.clear();
  mesh_elem_types_.clear();
  mesh_entities_.clear();
  time_steps_.clear();
  selection_.group_dim_ = -1;
  selection_.group_id_ = -1;
  selection_.cell_id_ = -1;
  selection_.entity_dim_ = -1;
  selection_.entity_tag_ = -1;
  selection_.preview_dim_ = -1;
  selection_.preview_tag_ = -1;
  mesh_select_occ_geometry_ = nullptr;
  preview_visual_active_ = false;

  for (vtkActor* actor : {actor_.GetPointer(), nodes_actor_.GetPointer(),
                          outline_actor_.GetPointer(),
                          mesh_select_actor_.GetPointer()}) {
    if (actor) {
      actor->SetVisibility(0);
    }
  }
  if (scalar_bar_) {
    scalar_bar_->SetVisibility(0);
  }
  if (slice_enable_) {
    const QSignalBlocker blocker(slice_enable_);
    slice_enable_->setChecked(false);
  }
  if (pick_enable_) {
    const QSignalBlocker blocker(pick_enable_);
    pick_enable_->setChecked(false);
  }
  if (probe_enable_) {
    const QSignalBlocker blocker(probe_enable_);
    probe_enable_->setChecked(false);
  }
  if (array_combo_) {
    const QSignalBlocker blocker(array_combo_);
    array_combo_->clear();
  }
  if (array_list_) {
    array_list_->clear();
  }
  if (time_slider_) {
    const QSignalBlocker blocker(time_slider_);
    time_slider_->setRange(0, 0);
    time_slider_->setValue(0);
    time_slider_->setEnabled(false);
  }
  if (time_label_) {
    time_label_->setText("t=0");
  }
  update_mesh_controls();
  update_plot_view();
  update_table_view();
  if (renderer_) {
    renderer_->ResetCamera();
  }
  if (render_window_) {
    render_window_->Render();
  }
#endif

  emit stage_picking_changed(false);
  emit stage_slice_changed(false);
  emit stage_command_feedback("已清空舞台中的网格/结果显示。");
}











QVariantMap VtkViewer::viewer_settings() const {
  QVariantMap map;
  map.insert("current_file", current_file_);
  map.insert("scalar_bar_pos", scalar_bar_pos_);
  map.insert("array_key",
             array_combo_ ? array_combo_->currentData().toString() : "");
  map.insert("preset",
             preset_combo_ ? preset_combo_->currentText() : "");
  map.insert("repr", repr_combo_ ? repr_combo_->currentIndex() : 0);
  map.insert("auto_range", auto_range_ && auto_range_->isChecked());
  map.insert("range_min", range_min_ ? range_min_->value() : 0.0);
  map.insert("range_max", range_max_ ? range_max_->value() : 1.0);
  map.insert("auto_refresh", auto_refresh_ && auto_refresh_->isChecked());
  map.insert("refresh_ms", refresh_ms_ ? refresh_ms_->value() : 1000);
  map.insert("show_faces", show_faces_ && show_faces_->isChecked());
  map.insert("show_edges", show_edges_ && show_edges_->isChecked());
  map.insert("show_shell", show_shell_ && show_shell_->isChecked());
  map.insert("show_nodes", show_nodes_ && show_nodes_->isChecked());
  map.insert("show_quality", show_quality_ && show_quality_->isChecked());
  map.insert("mesh_dim", mesh_dim_ ? mesh_dim_->currentData().toInt() : -1);
  map.insert("mesh_type", mesh_type_ ? mesh_type_->currentData().toInt() : -1);
  map.insert("mesh_opacity",
             mesh_opacity_ ? mesh_opacity_->value() : 1.0);
  map.insert("mesh_shrink", mesh_shrink_ ? mesh_shrink_->value() : 1.0);
  map.insert("mesh_scalar_bar",
             mesh_scalar_bar_ && mesh_scalar_bar_->isChecked());
  map.insert("pick_enable", pick_enable_ && pick_enable_->isChecked());
  map.insert("pick_mode", pick_mode_ ? pick_mode_->currentData().toInt() : 0);
  map.insert("slice_enable", slice_enable_ && slice_enable_->isChecked());
  map.insert("slice_axis", slice_axis_ ? slice_axis_->currentIndex() : 0);
  map.insert("slice_value", slice_slider_ ? slice_slider_->value() : 50);
  map.insert("show_axes", show_axes_ && show_axes_->isChecked());
  map.insert("show_outline", show_outline_ && show_outline_->isChecked());
  map.insert("view_preset", view_combo_ ? view_combo_->currentData().toInt() : 0);
  map.insert("output_selected",
             output_combo_ ? output_combo_->currentData().toString() : "");
  map.insert("array_filter",
             array_filter_ ? array_filter_->currentData().toString() : "");
  map.insert("probe_enable", probe_enable_ && probe_enable_->isChecked());
  map.insert("probe_mode",
             probe_mode_ ? probe_mode_->currentData().toInt() : 0);
  map.insert("deform_enable", deform_enable_ && deform_enable_->isChecked());
  map.insert("deform_vector",
             deform_vector_ ? deform_vector_->currentData().toString() : "");
  map.insert("deform_scale",
             deform_scale_ ? deform_scale_->value() : 1.0);
  map.insert("vector_auto_sync_deform",
             vector_auto_sync_deform_ && vector_auto_sync_deform_->isChecked());
  map.insert("vector_array", vector_array_combo_ ? vector_array_combo_->currentData().toString() : "");
  map.insert("table_rows", table_rows_);

#ifdef GMP_ENABLE_VTK_VIEWER
  if (mesh_group_) {
    const int idx = mesh_group_->currentData().toInt();
    if (idx >= 0 && idx < static_cast<int>(mesh_groups_.size())) {
      const auto& g = mesh_groups_[idx];
      map.insert("mesh_group_dim", g.dim);
      map.insert("mesh_group_id", g.id);
    }
  }
  if (mesh_entity_) {
    const int idx = mesh_entity_->currentData().toInt();
    if (idx >= 0 && idx < static_cast<int>(mesh_entities_.size())) {
      const auto& e = mesh_entities_[idx];
      map.insert("mesh_entity_dim", e.dim);
      map.insert("mesh_entity_tag", e.tag);
    }
  }
#endif

  return map;
}

void VtkViewer::apply_viewer_settings(const QVariantMap& settings) {
  const QString file = settings.value("current_file").toString();
  if (!file.isEmpty()) {
    load_file(file);
  }
  if (settings.contains("scalar_bar_pos")) {
    scalar_bar_pos_ = settings.value("scalar_bar_pos").toInt();
    if (scalar_bar_pos_combo_) {
      const int idx = scalar_bar_pos_combo_->findData(scalar_bar_pos_);
      if (idx >= 0) {
        scalar_bar_pos_combo_->setCurrentIndex(idx);
      }
    }
    apply_scalar_bar_pos();
  }
  if (show_faces_) {
    show_faces_->setChecked(
        settings.value("show_faces", show_faces_->isChecked()).toBool());
  }
  if (show_edges_) {
    show_edges_->setChecked(
        settings.value("show_edges", show_edges_->isChecked()).toBool());
  }
  if (show_shell_) {
    show_shell_->setChecked(
        settings.value("show_shell", show_shell_->isChecked()).toBool());
  }
  if (show_nodes_) {
    show_nodes_->setChecked(
        settings.value("show_nodes", show_nodes_->isChecked()).toBool());
  }
  if (show_quality_) {
    show_quality_->setChecked(
        settings.value("show_quality", show_quality_->isChecked()).toBool());
  }
  if (mesh_dim_) {
    const int dim_val = settings.value("mesh_dim", -1).toInt();
    const int idx = mesh_dim_->findData(dim_val);
    if (idx >= 0) {
      mesh_dim_->setCurrentIndex(idx);
    }
  }
  if (mesh_type_) {
    const int type_val = settings.value("mesh_type", -1).toInt();
    const int idx = mesh_type_->findData(type_val);
    if (idx >= 0) {
      mesh_type_->setCurrentIndex(idx);
    }
  }
  if (mesh_opacity_) {
    mesh_opacity_->setValue(
        settings.value("mesh_opacity", mesh_opacity_->value()).toDouble());
  }
  if (mesh_shrink_) {
    mesh_shrink_->setValue(
        settings.value("mesh_shrink", mesh_shrink_->value()).toDouble());
  }
  if (mesh_scalar_bar_) {
    mesh_scalar_bar_->setChecked(
        settings.value("mesh_scalar_bar", mesh_scalar_bar_->isChecked()).toBool());
  }
  if (pick_enable_) {
    pick_enable_->setChecked(
        settings.value("pick_enable", pick_enable_->isChecked()).toBool());
  }
  if (pick_mode_) {
    const int mode_val = settings.value("pick_mode", pick_mode_->currentData().toInt()).toInt();
    const int idx = pick_mode_->findData(mode_val);
    if (idx >= 0) {
      pick_mode_->setCurrentIndex(idx);
    }
  }
  if (slice_enable_) {
    slice_enable_->setChecked(
        settings.value("slice_enable", slice_enable_->isChecked()).toBool());
  }
  if (slice_axis_) {
    slice_axis_->setCurrentIndex(
        settings.value("slice_axis", slice_axis_->currentIndex()).toInt());
  }
  if (slice_slider_) {
    slice_slider_->setValue(
        settings.value("slice_value", slice_slider_->value()).toInt());
  }
  if (show_axes_) {
    show_axes_->setChecked(
        settings.value("show_axes", show_axes_->isChecked()).toBool());
  }
  if (show_outline_) {
    show_outline_->setChecked(
        settings.value("show_outline", show_outline_->isChecked()).toBool());
  }
  if (view_combo_) {
    const int preset = settings.value("view_preset", view_combo_->currentData().toInt()).toInt();
    const int idx = view_combo_->findData(preset);
    if (idx >= 0) {
      view_combo_->setCurrentIndex(idx);
    }
  }

  if (auto_range_) {
    auto_range_->setChecked(
        settings.value("auto_range", auto_range_->isChecked()).toBool());
  }
  if (range_min_) {
    range_min_->setValue(
        settings.value("range_min", range_min_->value()).toDouble());
  }
  if (range_max_) {
    range_max_->setValue(
        settings.value("range_max", range_max_->value()).toDouble());
  }
  if (preset_combo_) {
    const QString preset = settings.value("preset").toString();
    if (!preset.isEmpty()) {
      const int idx = preset_combo_->findText(preset);
      if (idx >= 0) {
        preset_combo_->setCurrentIndex(idx);
      }
    }
  }
  if (repr_combo_) {
    repr_combo_->setCurrentIndex(
        settings.value("repr", repr_combo_->currentIndex()).toInt());
  }
  if (array_combo_) {
    const QString key = settings.value("array_key").toString();
    if (!key.isEmpty()) {
      const int idx = array_combo_->findData(key);
      if (idx >= 0) {
        array_combo_->setCurrentIndex(idx);
      }
    }
  }
  if (auto_refresh_) {
    auto_refresh_->setChecked(
        settings.value("auto_refresh", auto_refresh_->isChecked()).toBool());
  }
  if (refresh_ms_) {
    refresh_ms_->setValue(
        settings.value("refresh_ms", refresh_ms_->value()).toInt());
  }
  if (output_combo_) {
    const QString output = settings.value("output_selected").toString();
    if (!output.isEmpty()) {
      const int idx = output_combo_->findData(output);
      if (idx >= 0) {
        output_combo_->setCurrentIndex(idx);
      }
    }
  }
  if (array_filter_) {
    const QString filter = settings.value("array_filter").toString();
    if (!filter.isEmpty()) {
      const int idx = array_filter_->findData(filter);
      if (idx >= 0) {
        array_filter_->setCurrentIndex(idx);
      }
    }
  }
  if (probe_enable_) {
    probe_enable_->setChecked(
        settings.value("probe_enable", probe_enable_->isChecked()).toBool());
  }
  if (probe_mode_) {
    const int mode_val =
        settings.value("probe_mode", probe_mode_->currentData().toInt()).toInt();
    const int idx = probe_mode_->findData(mode_val);
    if (idx >= 0) {
      probe_mode_->setCurrentIndex(idx);
    }
  }
  if (deform_enable_) {
    deform_enable_->setChecked(
        settings.value("deform_enable", deform_enable_->isChecked()).toBool());
  }
  if (deform_scale_) {
    deform_scale_->setValue(
        settings.value("deform_scale", deform_scale_->value()).toDouble());
  }
  if (deform_vector_) {
    const QString vec = settings.value("deform_vector").toString();
    if (!vec.isEmpty()) {
      const int idx = deform_vector_->findData(vec);
      if (idx >= 0) {
        deform_vector_->setCurrentIndex(idx);
      }
    }
  }
  if (vector_auto_sync_deform_) {
    vector_auto_sync_deform_->setChecked(
        settings.value("vector_auto_sync_deform", vector_auto_sync_deform_->isChecked()).toBool());
  }
  if (vector_array_combo_) {
    const QString vec = settings.value("vector_array").toString();
    if (!vec.isEmpty()) {
      const int idx = vector_array_combo_->findData(vec);
      if (idx >= 0) {
        vector_array_combo_->setCurrentIndex(idx);
      }
    }
  }
  table_rows_ = qBound(10, settings.value("table_rows", table_rows_).toInt(), 5000);

  if (settings.contains("mesh_group_dim") && settings.contains("mesh_group_id")) {
    set_mesh_group_filter(settings.value("mesh_group_dim").toInt(),
                          settings.value("mesh_group_id").toInt());
  }
  if (settings.contains("mesh_entity_dim") &&
      settings.contains("mesh_entity_tag")) {
    set_mesh_entity_filter(settings.value("mesh_entity_dim").toInt(),
                           settings.value("mesh_entity_tag").toInt());
  }

  apply_mesh_visuals();
  update_pipeline();
  update_vector_tab();
  update_plot_view();
  update_table_view();
}


bool VtkViewer::save_screenshot(const QString& path) {
  if (path.isEmpty()) {
    return false;
  }
#ifdef GMP_ENABLE_VTK_VIEWER
  if (render_window_) {
    auto w2i = vtkSmartPointer<vtkWindowToImageFilter>::New();
    w2i->SetInput(render_window_);
    w2i->ReadFrontBufferOff();
    w2i->Update();
    auto writer = vtkSmartPointer<vtkPNGWriter>::New();
    writer->SetFileName(path.toUtf8().constData());
    writer->SetInputConnection(w2i->GetOutputPort());
    writer->Write();
    return QFileInfo::exists(path);
  }
#endif
  const QPixmap pix = grab();
  return pix.save(path);
}









void VtkViewer::init_vtk() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!vtk_widget_) {
    return;
  }
  render_window_ = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
  renderer_ = vtkSmartPointer<vtkRenderer>::New();
  render_window_->AddRenderer(renderer_);
  vtk_widget_->setRenderWindow(render_window_);

  renderer_->SetBackground(0.12, 0.12, 0.12);

  auto* interactor = render_window_->GetInteractor();
  if (!interactor) {
    auto new_interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    render_window_->SetInteractor(new_interactor);
    interactor = new_interactor;
  }
  if (interactor && !style_3d_) {
    style_3d_ = vtkSmartPointer<StageInteractorStyle>::New();
    interactor->SetInteractorStyle(style_3d_);
  }
  if (interactor && !pick_callback_) {
    pick_callback_ = vtkSmartPointer<vtkCallbackCommand>::New();
    pick_callback_->SetClientData(this);
    pick_callback_->SetCallback([](vtkObject* caller, unsigned long,
                                   void* client_data, void*) {
      auto* self = static_cast<VtkViewer*>(client_data);
      auto* iren = vtkRenderWindowInteractor::SafeDownCast(caller);
      if (!self || !iren) {
        return;
      }
      // 草图选择/绘制时由草图逻辑接管左键；二维平移/缩放时不终止事件，
      // 让当前 SketchInteractorStyle 完成相机操作。
      if (self->sketch_doc_) {
        if (self->sketch_navigation_mode_ < 0) {
          int spos[2] = {0, 0};
          iren->GetEventPosition(spos);
          SketchPoint2d wpt;
          if (self->sketch_display_to_world(spos[0], spos[1], &wpt)) {
            self->sketch_press(wpt, iren->GetShiftKey() != 0,
                               iren->GetAltKey() != 0);
          }
          if (self->pick_callback_) {
            self->pick_callback_->AbortFlagOn();
          }
        }
        return;
      }
      if ((self->pick_enable_ && self->pick_enable_->isChecked()) ||
          (self->probe_enable_ && self->probe_enable_->isChecked())) {
        int pos[2] = {0, 0};
        iren->GetEventPosition(pos);
        self->handle_pick(pos[0], pos[1]);
        // 拾取期间不允许同一次左键事件继续触发相机旋转；此前手动再次
        // 调用 OnLeftButtonDown 会与交互样式自己的观察器重复分发事件。
        if (self->pick_callback_) {
          self->pick_callback_->AbortFlagOn();
        }
      }
    });
    interactor->AddObserver(vtkCommand::LeftButtonPressEvent, pick_callback_,
                            1.0f);
  }

  // 草图编辑: 鼠标移动 (橡皮筋预览 + 世界坐标上报) 与 Delete 键删除选中图元。
  // 仅在草图会话 (sketch_doc_ 非空) 中生效, 不影响 3D 视图交互。
  if (interactor && !sketch_move_callback_) {
    sketch_move_callback_ = vtkSmartPointer<vtkCallbackCommand>::New();
    sketch_move_callback_->SetClientData(this);
    sketch_move_callback_->SetCallback([](vtkObject* caller, unsigned long,
                                          void* client_data, void*) {
      auto* self = static_cast<VtkViewer*>(client_data);
      auto* iren = vtkRenderWindowInteractor::SafeDownCast(caller);
      if (!self || !iren || !self->sketch_doc_ ||
          self->sketch_preview_only_ || self->sketch_navigation_mode_ >= 0) {
        return;
      }
      int pos[2] = {0, 0};
      iren->GetEventPosition(pos);
      SketchPoint2d wpt;
      if (self->sketch_display_to_world(pos[0], pos[1], &wpt)) {
        self->sketch_move(wpt);
      }
    });
    interactor->AddObserver(vtkCommand::MouseMoveEvent, sketch_move_callback_);
  }
  if (interactor && !sketch_release_callback_) {
    sketch_release_callback_ = vtkSmartPointer<vtkCallbackCommand>::New();
    sketch_release_callback_->SetClientData(this);
    sketch_release_callback_->SetCallback(
        [](vtkObject*, unsigned long, void* client_data, void*) {
          auto* self = static_cast<VtkViewer*>(client_data);
          if (!self || !self->sketch_doc_ || !self->sketch_dragging_) {
            return;
          }
          self->sketch_release();
          if (self->sketch_release_callback_) {
            self->sketch_release_callback_->AbortFlagOn();
          }
        });
    interactor->AddObserver(vtkCommand::LeftButtonReleaseEvent,
                            sketch_release_callback_, 1.0f);
  }
  if (interactor && !sketch_key_callback_) {
    sketch_key_callback_ = vtkSmartPointer<vtkCallbackCommand>::New();
    sketch_key_callback_->SetClientData(this);
    sketch_key_callback_->SetCallback([](vtkObject* caller, unsigned long,
                                         void* client_data, void*) {
      auto* self = static_cast<VtkViewer*>(client_data);
      auto* iren = vtkRenderWindowInteractor::SafeDownCast(caller);
      if (!self || !iren || !self->sketch_doc_ ||
          self->sketch_preview_only_) {
        return;
      }
      const char* sym = iren->GetKeySym();
      if (sym && (strcmp(sym, "Delete") == 0 || strcmp(sym, "BackSpace") == 0)) {
        self->sketch_delete_selected();
      }
    });
    interactor->AddObserver(vtkCommand::KeyPressEvent, sketch_key_callback_);
  }

  // 左下角 XYZ 方向指示（vtkOrientationMarkerWidget），由 View 页签 Axes 开关控制
  if (interactor && !axes_marker_) {
    axes_actor_ = vtkSmartPointer<vtkAxesActor>::New();
    axes_actor_->SetPickable(0);
    axes_marker_ = vtkSmartPointer<vtkOrientationMarkerWidget>::New();
    axes_marker_->SetOrientationMarker(axes_actor_);
    axes_marker_->SetInteractor(interactor);
    axes_marker_->SetViewport(0.0, 0.0, 0.15, 0.15);
    axes_marker_->InteractiveOff();
    axes_marker_->SetEnabled(show_axes_ && show_axes_->isChecked() ? 1 : 0);
  }
#endif
}

void VtkViewer::update_pipeline() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!render_window_) {
    return;
  }
  if (current_file_.isEmpty()) {
    return;
  }
  if (!pipeline_ready_) {
    return;
  }
  if (mode_ == DataMode::Exodus && reader_ && geom_) {
    if (!time_steps_.empty()) {
      vtkInformation* info = reader_->GetOutputInformation(0);
      if (info) {
        const int idx = time_slider_->value();
        const double t = time_steps_[idx];
        info->Set(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP(), t);
      }
    }
    reader_->Update();
    // reader 输出对象被原地复用，下游执行器检测不到时间步变化；
    // 显式标脏中间过滤器，强制整条链路按新时间步重新执行。
    if (block_pad_) {
      block_pad_->Modified();
    }
    geom_->Update();
    update_deformation_pipeline();
  } else if (mode_ == DataMode::Mesh) {
    update_mesh_pipeline();
  }
  populate_arrays();
  update_vector_list();
  update_vector_tab();
  update_scene_extras();
  update_plot_view();
  update_table_view();
  if (first_render_) {
    renderer_->ResetCamera();
    first_render_ = false;
  }
  render_window_->Render();
#endif
}









void VtkViewer::on_preset_changed(int index) {
  Q_UNUSED(index);
  apply_lookup_table();
}

void VtkViewer::on_repr_changed(int index) {
  Q_UNUSED(index);
  apply_representation();
}


void VtkViewer::apply_representation() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!actor_) {
    return;
  }
  if (mode_ == DataMode::Mesh) {
    apply_mesh_visuals();
    return;
  }
  const QString mode = repr_combo_->currentText();
  if (mode == "Wireframe") {
    actor_->GetProperty()->SetRepresentationToWireframe();
    actor_->GetProperty()->SetEdgeVisibility(0);
  } else if (mode == "Surface + Edges") {
    actor_->GetProperty()->SetRepresentationToSurface();
    actor_->GetProperty()->SetEdgeVisibility(1);
  } else {
    actor_->GetProperty()->SetRepresentationToSurface();
    actor_->GetProperty()->SetEdgeVisibility(0);
  }
  if (render_window_) {
    render_window_->Render();
  }
#endif
}

void VtkViewer::apply_lookup_table() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!lut_ || !mapper_) {
    return;
  }
  const QString preset = preset_combo_->currentText();
  if (preset == "Grayscale") {
    lut_->SetHueRange(0.0, 0.0);
    lut_->SetSaturationRange(0.0, 0.0);
  } else if (preset == "Rainbow") {
    lut_->SetHueRange(0.666, 0.0);
    lut_->SetSaturationRange(1.0, 1.0);
  } else {  // Blue-Red
    lut_->SetHueRange(0.666, 0.0);
    lut_->SetSaturationRange(1.0, 1.0);
  }
  lut_->Build();
  mapper_->SetLookupTable(lut_);
  if (scalar_bar_) {
    scalar_bar_->SetLookupTable(lut_);
  }
  if (render_window_) {
    render_window_->Render();
  }
#endif
}













void VtkViewer::apply_scalar_bar_pos() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (scalar_bar_) {
    position_scalar_bar(scalar_bar_, scalar_bar_pos_);
  }
#endif
}

void VtkViewer::update_scene_extras() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!renderer_) {
    return;
  }
  vtkDataSet* data = nullptr;
  if (mode_ == DataMode::Mesh && mesh_grid_) {
    data = mesh_grid_;
  } else if (mode_ == DataMode::Exodus && mapper_) {
    data = vtkDataSet::SafeDownCast(mapper_->GetInput());
    if (!data && geom_) {
      geom_->Update();
      data = geom_->GetOutput();
    }
  }
  if (show_axes_ && axes_marker_) {
    axes_marker_->SetEnabled(show_axes_->isChecked() ? 1 : 0);
  }

  if (show_outline_) {
    if (!outline_filter_) {
      outline_filter_ = vtkSmartPointer<vtkOutlineFilter>::New();
      outline_mapper_ = vtkSmartPointer<vtkPolyDataMapper>::New();
      outline_actor_ = vtkSmartPointer<vtkActor>::New();
      outline_actor_->SetMapper(outline_mapper_);
      outline_actor_->GetProperty()->SetColor(0.8, 0.8, 0.8);
      outline_actor_->GetProperty()->SetLineWidth(1.5);
      renderer_->AddActor(outline_actor_);
    }
    if (data) {
      outline_filter_->SetInputData(data);
      outline_mapper_->SetInputConnection(outline_filter_->GetOutputPort());
    }
    outline_actor_->SetVisibility(show_outline_->isChecked() ? 1 : 0);
  }
#endif
}

void VtkViewer::apply_view_preset(int preset) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (mode_2d_) {
    return;  // 2D 草图模式下视角预设不生效
  }
  if (!renderer_) {
    return;
  }
  vtkDataSet* data = nullptr;
  if (mode_ == DataMode::Mesh && mesh_grid_) {
    data = mesh_grid_;
  } else if (mode_ == DataMode::Exodus && mapper_) {
    data = vtkDataSet::SafeDownCast(mapper_->GetInput());
    if (!data && geom_) {
      geom_->Update();
      data = geom_->GetOutput();
    }
  }
  if (!data) {
    return;
  }
  double bounds[6] = {0, 0, 0, 0, 0, 0};
  data->GetBounds(bounds);
  auto* cam = renderer_->GetActiveCamera();
  if (!cam) {
    return;
  }
  const gmp::CameraPresetResult preset_cam =
      gmp::view_preset_camera(bounds, preset);
  if (preset_cam.fit) {
    renderer_->ResetCamera();
    render_window_->Render();
    return;
  }
  cam->SetPosition(preset_cam.position[0], preset_cam.position[1],
                   preset_cam.position[2]);
  cam->SetViewUp(preset_cam.view_up[0], preset_cam.view_up[1],
                 preset_cam.view_up[2]);
  cam->SetFocalPoint(preset_cam.focal[0], preset_cam.focal[1],
                     preset_cam.focal[2]);
  renderer_->ResetCameraClippingRange();
  render_window_->Render();
#else
  Q_UNUSED(preset);
#endif
}

void VtkViewer::set_stage_interaction_mode(int mode) {
#ifdef GMP_ENABLE_VTK_VIEWER
  mode = qBound(0, mode, 2);
  if (sketch_doc_) {
    // 二维草图不提供旋转；“旋转”按钮回到当前草图工具，平移/缩放则
    // 显式放行左键事件给二维交互样式。
    sketch_navigation_mode_ = mode == 0 ? -1 : mode;
    if (auto* style = SketchInteractorStyle::SafeDownCast(style_2d_)) {
      style->SetStageMode(mode);
    }
    emit stage_picking_changed(false);
    if (vtk_widget_) {
      vtk_widget_->setCursor(mode == 1   ? Qt::SizeAllCursor
                             : mode == 2 ? Qt::SizeVerCursor
                                         : Qt::ArrowCursor);
      vtk_widget_->setFocus();
    }
    if (mode == 0) {
      emit stage_command_feedback(
          "二维草图不支持旋转；已回到当前草图选择/绘制工具。");
    } else {
      const QStringList names = {"", "平移", "缩放"};
      emit stage_command_feedback(
          QString("二维草图%1模式：按住左键拖动。").arg(names.at(mode)));
    }
    return;
  }
  if (auto* style = StageInteractorStyle::SafeDownCast(style_3d_)) {
    style->SetStageMode(mode);
  }
  if (vtk_widget_) {
    vtk_widget_->setCursor(mode == 1   ? Qt::SizeAllCursor
                           : mode == 2 ? Qt::SizeVerCursor
                                       : Qt::OpenHandCursor);
    vtk_widget_->setFocus();
  }
  if (mode_ == DataMode::None && !sketch_doc_) {
    emit stage_command_feedback(
        "舞台暂无可交互对象；请先加载 .msh/.e 或进入草图编辑。");
  } else {
    const QStringList names = {"旋转", "平移", "缩放"};
    emit stage_command_feedback(
        QString("舞台交互模式：%1（按住左键拖动）")
            .arg(names.value(qBound(0, mode, 2))));
  }
#else
  Q_UNUSED(mode);
#endif
}

void VtkViewer::apply_stage_view(int preset) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (sketch_doc_) {
    if (preset != 0 && preset != 3) {
      emit stage_command_feedback(
          "二维草图仅支持适配窗口和顶视图。");
      return;
    }
    if (renderer_ && render_window_) {
      if (auto* cam = renderer_->GetActiveCamera()) {
        cam->SetParallelProjection(true);
        cam->SetPosition(0.0, 0.0, 1.0);
        cam->SetFocalPoint(0.0, 0.0, 0.0);
        cam->SetViewUp(0.0, 1.0, 0.0);
      }
      renderer_->ResetCamera();
      renderer_->ResetCameraClippingRange();
      render_window_->Render();
    }
    emit stage_command_feedback(
        preset == 0 ? "草图已适配窗口。" : "草图已恢复顶视图。");
    return;
  }
  if (mode_ == DataMode::None || (!mesh_grid_ && !mapper_)) {
    emit stage_command_feedback(
        "当前没有可取景的数据；请先加载 .msh 或 .e 文件。");
    return;
  }
#endif
  if (view_combo_) {
    const int index = view_combo_->findData(preset);
    if (index >= 0) {
      view_combo_->setCurrentIndex(index);
    }
  }
  apply_view_preset(preset);
  const QStringList names = {"适配窗口", "前视图", "右视图", "顶视图",
                             "轴测图"};
  emit stage_command_feedback(
      QString("已应用：%1").arg(names.value(qBound(0, preset, 4))));
}

void VtkViewer::set_stage_picking(bool enabled) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (sketch_doc_) {
    if (enabled) {
      set_sketch_tool(SketchToolSelect);
      emit stage_command_feedback(
          sketch_preview_only_ ? "草图预览选择已启用：单击图元。"
                               : "草图选择已启用：单击图元，Shift 可多选。");
    } else {
      // Select 是草图中的一个互斥工具而非可独立关闭的开关。若顶部 Pick
      // 动作在 Select 状态下被再次点击，恢复其选中态，避免按钮显示“关闭”
      // 而视口实际上仍在执行选择。
      emit sketch_tool_changed(sketch_tool_);
      emit stage_picking_changed(sketch_tool_ == SketchToolSelect);
    }
    return;
  }
  if (mode_ == DataMode::Mesh && pick_enable_) {
    if (probe_enable_ && probe_enable_->isChecked()) {
      probe_enable_->setChecked(false);
    }
    pick_enable_->setChecked(enabled);
    emit stage_command_feedback(enabled ? "网格拾取已启用：单击舞台对象。"
                                        : "网格拾取已关闭。");
    return;
  }
  if (mode_ == DataMode::Exodus && probe_enable_) {
    if (pick_enable_ && pick_enable_->isChecked()) {
      pick_enable_->setChecked(false);
    }
    probe_enable_->setChecked(enabled);
    emit stage_command_feedback(enabled ? "结果探针已启用：单击舞台对象。"
                                        : "结果探针已关闭。");
    return;
  }
#endif
  if (pick_enable_) {
    pick_enable_->setChecked(false);
  }
  if (probe_enable_) {
    probe_enable_->setChecked(false);
  }
  emit stage_picking_changed(false);
  if (enabled) {
    emit stage_command_feedback(
        "拾取不可用：请先加载 .msh 网格或 .e 结果文件。");
  }
}

void VtkViewer::clear_stage_selection() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (sketch_doc_) {
    set_sketch_selection({});
    emit stage_command_feedback("已清除草图选择。");
  } else if (mode_ == DataMode::Exodus && probe_clear_) {
    probe_clear_->click();
    emit stage_command_feedback("已清除结果探针信息。");
  } else if (mode_ == DataMode::Mesh && pick_clear_) {
    pick_clear_->click();
    emit stage_command_feedback("已清除网格选择。");
  } else {
    emit stage_command_feedback("当前舞台没有可清除的选择。");
  }
#else
  emit stage_command_feedback("当前构建未启用 VTK 舞台。");
#endif
}

void VtkViewer::set_stage_slice(bool enabled) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (mode_ != DataMode::Mesh || !mesh_grid_) {
    if (slice_enable_) {
      slice_enable_->setChecked(false);
    }
    emit stage_slice_changed(false);
    if (enabled) {
      emit stage_command_feedback("剖切仅对已加载的 .msh 网格可用。");
    }
    return;
  }
#endif
  if (slice_enable_) {
    slice_enable_->setChecked(enabled);
  }
  emit stage_command_feedback(enabled ? "网格剖切已启用。"
                                      : "网格剖切已关闭。");
}

void VtkViewer::cycle_stage_representation() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (mode_ == DataMode::Mesh && mesh_grid_ && show_faces_ && show_edges_) {
    const bool faces = show_faces_->isChecked();
    const bool edges = show_edges_->isChecked();
    int next = 0;
    if (faces && !edges) {
      next = 1;  // wireframe
    } else if (!faces && edges) {
      next = 2;  // surface + edges
    } else {
      next = 0;  // surface
    }
    const QSignalBlocker faces_blocker(show_faces_);
    const QSignalBlocker edges_blocker(show_edges_);
    show_faces_->setChecked(next != 1);
    show_edges_->setChecked(next != 0);
    apply_mesh_visuals();
    const QStringList names = {"实体表面", "线框", "表面 + 边线"};
    emit stage_command_feedback("网格显示方式：" + names.at(next));
    return;
  }
  if (mode_ == DataMode::Exodus && repr_combo_ && repr_combo_->count() > 0) {
    const int next = (repr_combo_->currentIndex() + 1) % repr_combo_->count();
    repr_combo_->setCurrentIndex(next);
    emit stage_command_feedback("结果显示方式：" + repr_combo_->currentText());
    return;
  }
#endif
  emit stage_command_feedback(
      "显示方式不可用：请先加载 .msh 网格或 .e 结果文件。");
}



















// ---- TASK-V02-050 facade forwarders：VtkViewer 外观层，职责委托给三个视口 ----
void VtkViewer::set_exodus_file(const QString& path) {
  result_viewport_->set_exodus_file(path);
}
bool VtkViewer::stage_data_visible() const {
  return mesh_viewport_->stage_data_visible();
}
int VtkViewer::visible_mesh_entity_count(int dim) const {
  return mesh_viewport_->visible_mesh_entity_count(dim);
}
int VtkViewer::current_mesh_dimension() const {
  return mesh_viewport_->current_mesh_dimension();
}
void VtkViewer::set_mesh_file(const QString& path) {
  mesh_viewport_->set_mesh_file(path);
}
void VtkViewer::set_mesh_file_from_current_model(const QString& path) {
  mesh_viewport_->set_mesh_file_from_current_model(path);
}
void VtkViewer::set_mesh_file_impl(const QString& path,
                                   bool use_current_gmsh_model) {
  mesh_viewport_->set_mesh_file_impl(path, use_current_gmsh_model);
}
void VtkViewer::set_mesh_group_filter(int dim, int tag) {
  mesh_viewport_->set_mesh_group_filter(dim, tag);
}
void VtkViewer::set_mesh_entity_filter(int dim, int tag) {
  mesh_viewport_->set_mesh_entity_filter(dim, tag);
}
void VtkViewer::preview_mesh_entity(int dim, int tag, double view_x,
                                    double view_y, double view_z) {
  mesh_viewport_->preview_mesh_entity(dim, tag, view_x, view_y, view_z);
}
bool VtkViewer::is_mesh_entity_previewed(int dim, int tag) const {
  return mesh_viewport_->is_mesh_entity_previewed(dim, tag);
}
bool VtkViewer::is_mesh_entity_preview_visible(int dim, int tag) const {
  return mesh_viewport_->is_mesh_entity_preview_visible(dim, tag);
}
void VtkViewer::set_exodus_history(const QStringList& paths) {
  result_viewport_->set_exodus_history(paths);
}
PhysicalGroupManifest VtkViewer::read_exodus_mesh_manifest(
    const QString& path) const {
  return result_viewport_->read_exodus_mesh_manifest(path);
}
QStringList VtkViewer::read_exodus_side_set_names(const QString& path) const {
  return result_viewport_->read_exodus_side_set_names(path);
}
QString VtkViewer::plot_snapshot_text() const {
  return result_viewport_->plot_snapshot_text();
}
QString VtkViewer::plot_stats_snapshot() const {
  return result_viewport_->plot_stats_snapshot();
}
QString VtkViewer::table_snapshot_text() const {
  return result_viewport_->table_snapshot_text();
}
QString VtkViewer::table_stats_snapshot() const {
  return result_viewport_->table_stats_snapshot();
}
void VtkViewer::on_reload() {
  result_viewport_->on_reload();
}
int VtkViewer::current_time_step_index() const {
  return result_viewport_->current_time_step_index();
}
void VtkViewer::set_time_step_index(int index) {
  result_viewport_->set_time_step_index(index);
}
void VtkViewer::on_time_changed(int index) {
  result_viewport_->on_time_changed(index);
}
void VtkViewer::on_array_changed(int index) {
  result_viewport_->on_array_changed(index);
}
void VtkViewer::populate_arrays() {
  result_viewport_->populate_arrays();
}
void VtkViewer::update_array_list() {
  result_viewport_->update_array_list();
}
void VtkViewer::update_vector_tab() {
  result_viewport_->update_vector_tab();
}
void VtkViewer::update_plot_view() {
  result_viewport_->update_plot_view();
}
void VtkViewer::update_table_view() {
  result_viewport_->update_table_view();
}
void VtkViewer::update_vector_list() {
  result_viewport_->update_vector_list();
}
void VtkViewer::update_deformation_pipeline() {
  result_viewport_->update_deformation_pipeline();
}
void VtkViewer::on_open_file() {
  result_viewport_->on_open_file();
}
void VtkViewer::on_apply_range() {
  result_viewport_->on_apply_range();
}
void VtkViewer::on_auto_refresh_toggled(bool enabled) {
  result_viewport_->on_auto_refresh_toggled(enabled);
}
void VtkViewer::on_auto_refresh_tick() {
  result_viewport_->on_auto_refresh_tick();
}
void VtkViewer::set_refresh_enabled(bool enabled) {
  result_viewport_->set_refresh_enabled(enabled);
}
void VtkViewer::setup_watcher(const QString& file_path) {
  result_viewport_->setup_watcher(file_path);
}
void VtkViewer::schedule_reload() {
  result_viewport_->schedule_reload();
}
void VtkViewer::refresh_from_disk() {
  result_viewport_->refresh_from_disk();
}
void VtkViewer::refresh_time_only() {
  result_viewport_->refresh_time_only();
}
void VtkViewer::update_time_steps_from_reader(bool keep_index) {
  result_viewport_->update_time_steps_from_reader(keep_index);
}
void VtkViewer::load_file(const QString& path) {
  result_viewport_->load_file(path);
}
void VtkViewer::update_mesh_controls() {
  mesh_viewport_->update_mesh_controls();
}
void VtkViewer::update_mesh_pipeline() {
  mesh_viewport_->update_mesh_pipeline();
}
void VtkViewer::apply_mesh_visuals() {
  mesh_viewport_->apply_mesh_visuals();
}
void VtkViewer::update_nodes_visibility() {
  mesh_viewport_->update_nodes_visibility();
}
void VtkViewer::handle_pick(int x, int y) {
  mesh_viewport_->handle_pick(x, y);
}
void VtkViewer::update_selection_pipeline() {
  mesh_viewport_->update_selection_pipeline();
}
void VtkViewer::set_2d_mode(bool on) {
  sketch_viewport_->set_2d_mode(on);
}
void VtkViewer::set_sketch_document(SketchDocument* doc) {
  sketch_viewport_->set_sketch_document(doc);
}
void VtkViewer::set_sketch_preview(const SketchDocument* doc) {
  sketch_viewport_->set_sketch_preview(doc);
}
void VtkViewer::set_sketch_tool(int tool) {
  sketch_viewport_->set_sketch_tool(tool);
}
void VtkViewer::refresh_sketch() {
  sketch_viewport_->refresh_sketch();
}
bool VtkViewer::sketch_display_to_world(int x, int y, SketchPoint2d* out) const {
  return sketch_viewport_->sketch_display_to_world(x, y, out);
}
double VtkViewer::sketch_pick_tol() const {
  return sketch_viewport_->sketch_pick_tol();
}
SketchPoint2d VtkViewer::sketch_snap(const SketchPoint2d& pt) const {
  return sketch_viewport_->sketch_snap(pt);
}
void VtkViewer::set_sketch_selection(const QList<int>& ids) {
  sketch_viewport_->set_sketch_selection(ids);
}
void VtkViewer::rebuild_sketch_actors() {
  sketch_viewport_->rebuild_sketch_actors();
}
void VtkViewer::update_sketch_preview() {
  sketch_viewport_->update_sketch_preview();
}
void VtkViewer::sketch_press(const SketchPoint2d& pt, bool shift,
                             bool subentity) {
  sketch_viewport_->sketch_press(pt, shift, subentity);
}
void VtkViewer::sketch_move(const SketchPoint2d& pt) {
  sketch_viewport_->sketch_move(pt);
}
void VtkViewer::sketch_release() {
  sketch_viewport_->sketch_release();
}
void VtkViewer::sketch_delete_selected() {
  sketch_viewport_->sketch_delete_selected();
}
void VtkViewer::solve_sketch_and_refresh() {
  sketch_viewport_->solve_sketch_and_refresh();
}
void VtkViewer::add_constraint_for_selection(int type) {
  sketch_viewport_->add_constraint_for_selection(type);
}
void VtkViewer::add_dimension_for_selection(int type, double value) {
  sketch_viewport_->add_dimension_for_selection(type, value);
}

}  // namespace gmp
