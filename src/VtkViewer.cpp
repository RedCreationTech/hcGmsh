#include "gmp/VtkViewer.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSlider>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QStringList>
#include <QtCore/Qt>
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

#include "gmp/ComboPopupFix.h"

#ifdef GMP_ENABLE_VTK_VIEWER
#include <QVTKOpenGLNativeWidget.h>
#include <vtkActor.h>
#include <vtkCellData.h>
#include <vtkCompositeDataGeometryFilter.h>
#include <vtkDataArray.h>
#include <vtkDataObject.h>
#include <vtkDataSet.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkDataSetMapper.h>
#include <vtkExodusIIReader.h>
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
#include <vtkCellType.h>
#include <vtkVertexGlyphFilter.h>
#include <vtkRenderer.h>
#include <vtkScalarBarActor.h>
#include <vtkWindowToImageFilter.h>
#include <vtkPNGWriter.h>
#include <vtkStreamingDemandDrivenPipeline.h>
#include <vtkCellPicker.h>
#include <vtkCallbackCommand.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyle.h>
#include <vtkCommand.h>
#endif

#ifdef GMP_ENABLE_GMSH_GUI
#include <gmsh.h>
#endif

namespace gmp {

#ifdef GMP_ENABLE_VTK_VIEWER
namespace {

// 统一约束色标条样式：尺寸按视口比例自适应（随黑色显示区缩放），
// 必须先关闭 VTK 默认的约束字体模式，否则它会自动放大字体填满色标条。
void style_scalar_bar(vtkScalarBarActor* bar) {
  if (!bar) {
    return;
  }
  bar->SetUnconstrainedFontSize(1);
  bar->SetWidth(0.08);
  bar->SetHeight(0.55);
  bar->SetNumberOfLabels(5);
  bar->SetBarRatio(0.35);
  if (auto* title = bar->GetTitleTextProperty()) {
    title->SetFontSize(12);
  }
  if (auto* label = bar->GetLabelTextProperty()) {
    label->SetFontSize(11);
  }
}

// 把色标条摆到视口的某个角（corner: 0=右下 1=右上 2=左下 3=左上），边距收紧
void position_scalar_bar(vtkScalarBarActor* bar, int corner) {
  if (!bar) {
    return;
  }
  const double w = bar->GetWidth();
  const double h = bar->GetHeight();
  const double mx = 0.015;
  const double my = 0.02;
  const double x = (corner == 0 || corner == 1) ? 1.0 - w - mx : mx;
  const double y = (corner == 0 || corner == 2) ? my : 1.0 - h - my;
  bar->SetPosition(x, y);
}

struct VectorStats {
  bool has_data = false;
  int components = 0;
  vtkIdType tuples = 0;
  double min_mag = 0.0;
  double max_mag = 0.0;
  double mean_mag = 0.0;
  double rms_mag = 0.0;
};

double ComputeMagnitude(vtkDataArray* arr, vtkIdType idx) {
  if (!arr || idx < 0 || idx >= arr->GetNumberOfTuples()) {
    return 0.0;
  }
  const int comps = arr->GetNumberOfComponents();
  double sum_sq = 0.0;
  for (int c = 0; c < comps; ++c) {
    const double v = arr->GetComponent(idx, c);
    sum_sq += v * v;
  }
  return std::sqrt(sum_sq);
}

VectorStats AnalyzeVectorArray(vtkDataArray* arr) {
  VectorStats stats;
  if (!arr) {
    return stats;
  }
  const int comps = arr->GetNumberOfComponents();
  if (comps < 2) {
    return stats;
  }
  const vtkIdType tuples = arr->GetNumberOfTuples();
  if (tuples <= 0) {
    return stats;
  }

  stats.has_data = true;
  stats.components = comps;
  stats.tuples = tuples;

  double min_mag = std::numeric_limits<double>::infinity();
  double max_mag = -std::numeric_limits<double>::infinity();
  double sum_mag = 0.0;
  double sum_sq = 0.0;
  for (vtkIdType i = 0; i < tuples; ++i) {
    const double m = ComputeMagnitude(arr, i);
    min_mag = std::min(min_mag, m);
    max_mag = std::max(max_mag, m);
    sum_mag += m;
    sum_sq += m * m;
  }
  stats.min_mag = min_mag;
  stats.max_mag = max_mag;
  stats.mean_mag = sum_mag / static_cast<double>(tuples);
  stats.rms_mag = std::sqrt(sum_sq / static_cast<double>(tuples));
  return stats;
}

QString ArrayValueSample(vtkDataArray* arr, vtkIdType idx) {
  if (!arr || idx < 0 || idx >= arr->GetNumberOfTuples()) {
    return "n/a";
  }
  const int comps = arr->GetNumberOfComponents();
  if (comps <= 1) {
    return QString::number(arr->GetComponent(idx, 0), 'g', 6);
  }

  QStringList parts;
  for (int c = 0; c < comps; ++c) {
    parts << QString::number(arr->GetComponent(idx, c), 'g', 5);
  }
  return QString("(%1)").arg(parts.join(", "));
}

QString FormatVectorStatsText(const VectorStats& stats) {
  if (!stats.has_data) {
    return "No compatible vector data";
  }
  return QString(
             "components=%1, tuples=%2, |v| min=%3, max=%4, mean=%5, rms=%6")
      .arg(stats.components)
      .arg(stats.tuples)
      .arg(stats.min_mag, 0, 'g', 6)
      .arg(stats.max_mag, 0, 'g', 6)
      .arg(stats.mean_mag, 0, 'g', 6)
      .arg(stats.rms_mag, 0, 'g', 6);
}

int VtkCellFromDimAndNodes(int dim, int num_primary) {
  if (dim == 0) {
    return VTK_VERTEX;
  }
  if (dim == 1) {
    return VTK_LINE;
  }
  if (dim == 2) {
    if (num_primary >= 4) {
      return VTK_QUAD;
    }
    return VTK_TRIANGLE;
  }
  if (dim == 3) {
    if (num_primary == 4) {
      return VTK_TETRA;
    }
    if (num_primary == 5) {
      return VTK_PYRAMID;
    }
    if (num_primary == 6) {
      return VTK_WEDGE;
    }
    if (num_primary == 8) {
      return VTK_HEXAHEDRON;
    }
  }
  return VTK_EMPTY_CELL;
}

QString ElementTypeLabel(int element_type) {
#ifdef GMP_ENABLE_GMSH_GUI
  try {
    std::string name;
    int dim = 0;
    int order = 0;
    int num_nodes = 0;
    int num_primary = 0;
    std::vector<double> local;
    gmsh::model::mesh::getElementProperties(element_type, name, dim, order,
                                            num_nodes, local, num_primary);
    if (!name.empty()) {
      return QString::fromStdString(name);
    }
  } catch (...) {
  }
#endif
  return QString("Type %1").arg(element_type);
}

#ifdef GMP_ENABLE_GMSH_GUI
vtkSmartPointer<vtkUnstructuredGrid> BuildGridFromGmsh(
    const QString& path) {
  try {
    if (!gmsh::isInitialized()) {
      gmsh::initialize(0, nullptr, false, false);
    }
    gmsh::option::setNumber("General.Terminal", 0);
    gmsh::clear();
    gmsh::open(path.toStdString());

    std::vector<std::size_t> node_tags;
    std::vector<double> coords;
    std::vector<double> params;
    gmsh::model::mesh::getNodes(node_tags, coords, params);
    if (node_tags.empty() || coords.size() < node_tags.size() * 3) {
      return nullptr;
    }

    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetNumberOfPoints(static_cast<vtkIdType>(node_tags.size()));
    std::unordered_map<std::size_t, vtkIdType> id_map;
    id_map.reserve(node_tags.size());

    auto node_tags_arr = vtkSmartPointer<vtkIntArray>::New();
    node_tags_arr->SetName("node_tag");
    node_tags_arr->SetNumberOfValues(
        static_cast<vtkIdType>(node_tags.size()));
    for (size_t i = 0; i < node_tags.size(); ++i) {
      id_map[node_tags[i]] = static_cast<vtkIdType>(i);
      points->SetPoint(static_cast<vtkIdType>(i), coords[3 * i],
                       coords[3 * i + 1], coords[3 * i + 2]);
      node_tags_arr->SetValue(static_cast<vtkIdType>(i),
                              static_cast<int>(node_tags[i]));
    }

    auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    grid->SetPoints(points);
    grid->GetPointData()->AddArray(node_tags_arr);

    std::unordered_map<std::size_t, int> elem_phys;
    std::unordered_map<std::size_t, int> elem_phys_dim;
    std::unordered_map<std::size_t, int> elem_ent_tag;
    std::unordered_map<std::size_t, int> elem_ent_dim;
    std::vector<std::pair<int, int>> phys_groups;
    gmsh::model::getPhysicalGroups(phys_groups);
    for (const auto& pg : phys_groups) {
      std::vector<int> entities;
      gmsh::model::getEntitiesForPhysicalGroup(pg.first, pg.second, entities);
      for (const auto ent : entities) {
        std::vector<int> etypes;
        std::vector<std::vector<std::size_t>> etags;
        std::vector<std::vector<std::size_t>> enodes;
        gmsh::model::mesh::getElements(etypes, etags, enodes, pg.first, ent);
        for (const auto& list : etags) {
          for (const auto tag : list) {
            elem_phys[tag] = pg.second;
            elem_phys_dim[tag] = pg.first;
          }
        }
      }
    }

    std::vector<std::pair<int, int>> entities;
    gmsh::model::getEntities(entities);
    for (const auto& ent : entities) {
      std::vector<int> etypes;
      std::vector<std::vector<std::size_t>> etags;
      std::vector<std::vector<std::size_t>> enodes;
      gmsh::model::mesh::getElements(etypes, etags, enodes, ent.first, ent.second);
      for (const auto& list : etags) {
        for (const auto tag : list) {
          elem_ent_tag[tag] = ent.second;
          elem_ent_dim[tag] = ent.first;
        }
      }
    }

    auto phys_id_arr = vtkSmartPointer<vtkIntArray>::New();
    phys_id_arr->SetName("phys_id");
    auto phys_dim_arr = vtkSmartPointer<vtkIntArray>::New();
    phys_dim_arr->SetName("phys_dim");
    auto elem_type_arr = vtkSmartPointer<vtkIntArray>::New();
    elem_type_arr->SetName("elem_type");
    auto elem_tag_arr = vtkSmartPointer<vtkIntArray>::New();
    elem_tag_arr->SetName("elem_tag");
    auto cell_id_arr = vtkSmartPointer<vtkIntArray>::New();
    cell_id_arr->SetName("cell_id");
    auto ent_dim_arr = vtkSmartPointer<vtkIntArray>::New();
    ent_dim_arr->SetName("entity_dim");
    auto ent_tag_arr = vtkSmartPointer<vtkIntArray>::New();
    ent_tag_arr->SetName("entity_tag");

    std::vector<int> element_types;
    std::vector<std::vector<std::size_t>> element_tags;
    std::vector<std::vector<std::size_t>> element_node_tags;
    gmsh::model::mesh::getElements(element_types, element_tags,
                                   element_node_tags);

    int cell_index = 0;
    for (size_t k = 0; k < element_types.size(); ++k) {
      int dim = 0;
      int order = 0;
      int num_nodes = 0;
      int num_primary = 0;
      std::string name;
      std::vector<double> local;
      gmsh::model::mesh::getElementProperties(element_types[k], name, dim,
                                              order, num_nodes, local,
                                              num_primary);
      if (num_nodes <= 0 || num_primary <= 0) {
        continue;
      }
      const int cell_type = VtkCellFromDimAndNodes(dim, num_primary);
      if (cell_type == VTK_EMPTY_CELL) {
        continue;
      }

      const auto& nodes = element_node_tags[k];
      const size_t elem_count = nodes.size() / static_cast<size_t>(num_nodes);
      std::vector<vtkIdType> ids(static_cast<size_t>(num_primary));
      for (size_t e = 0; e < elem_count; ++e) {
        const size_t base = e * static_cast<size_t>(num_nodes);
        for (int j = 0; j < num_primary; ++j) {
          const std::size_t tag = nodes[base + static_cast<size_t>(j)];
          auto it = id_map.find(tag);
          ids[static_cast<size_t>(j)] =
              it == id_map.end() ? 0 : it->second;
        }
        grid->InsertNextCell(cell_type, num_primary, ids.data());
        const std::size_t elem_tag = element_tags[k][e];
        const auto phys_it = elem_phys.find(elem_tag);
        const int phys_id =
            phys_it == elem_phys.end() ? 0 : phys_it->second;
        const int phys_dim =
            phys_it == elem_phys.end() ? dim : elem_phys_dim[elem_tag];
        const auto ent_it = elem_ent_tag.find(elem_tag);
        const int ent_tag = ent_it == elem_ent_tag.end() ? 0 : ent_it->second;
        const int ent_dim =
            ent_it == elem_ent_tag.end() ? dim : elem_ent_dim[elem_tag];
        phys_id_arr->InsertNextValue(phys_id);
        phys_dim_arr->InsertNextValue(phys_dim);
        elem_type_arr->InsertNextValue(element_types[k]);
        elem_tag_arr->InsertNextValue(static_cast<int>(elem_tag));
        cell_id_arr->InsertNextValue(cell_index);
        ent_dim_arr->InsertNextValue(ent_dim);
        ent_tag_arr->InsertNextValue(ent_tag);
        ++cell_index;
      }
    }

    grid->GetCellData()->AddArray(phys_id_arr);
    grid->GetCellData()->AddArray(phys_dim_arr);
    grid->GetCellData()->AddArray(elem_type_arr);
    grid->GetCellData()->AddArray(elem_tag_arr);
    grid->GetCellData()->AddArray(cell_id_arr);
    grid->GetCellData()->AddArray(ent_dim_arr);
    grid->GetCellData()->AddArray(ent_tag_arr);
    grid->GetCellData()->SetScalars(phys_id_arr);

    return grid;
  } catch (...) {
    return nullptr;
  }
}
#endif

void AttachComboPopupFix(QComboBox* combo) {
  gmp::install_combo_popup_fix(combo);
}

}  // namespace
#endif

VtkViewer::VtkViewer(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);

  auto* header = new QHBoxLayout();
  file_label_ = new QLabel("No file loaded");
  open_btn_ = new QPushButton("Open");
  reload_btn_ = new QPushButton("Reload");
  connect(open_btn_, &QPushButton::clicked, this, &VtkViewer::on_open_file);
  connect(reload_btn_, &QPushButton::clicked, this, &VtkViewer::on_reload);
  header->addWidget(file_label_, 1);
  header->addWidget(open_btn_);
  header->addWidget(reload_btn_);
  layout->addLayout(header);

  auto* output_row = new QHBoxLayout();
  output_label_ = new QLabel("Outputs");
  output_combo_ = new QComboBox();
  output_combo_->setMinimumWidth(240);
  AttachComboPopupFix(output_combo_);
  output_pick_ = new QPushButton("Load Selected");
  connect(output_pick_, &QPushButton::clicked, this, [this]() {
    const QString path = output_combo_->currentData().toString();
    if (!path.isEmpty()) {
      set_exodus_file(path);
    }
  });
  output_row->addWidget(output_label_);
  output_row->addWidget(output_combo_, 1);
  output_row->addWidget(output_pick_);
  layout->addLayout(output_row);

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

  auto* control_tabs = new QTabWidget(right_panel);
  control_tabs->setObjectName("controlTabs");
  control_tabs_ = control_tabs;
  right_layout->addWidget(control_tabs);
  auto make_tab = [control_tabs](const QString& name) {
    auto* tab = new QWidget(control_tabs);
    auto* tab_layout = new QVBoxLayout(tab);
    tab_layout->setContentsMargins(0, 0, 0, 0);
    tab_layout->setSpacing(4);
    control_tabs->addTab(tab, name);
    return tab_layout;
  };

  auto* scalar_layout = make_tab("Scalar");

  auto* scalar_row = new QHBoxLayout();
  array_combo_ = new QComboBox();
  array_combo_->setMinimumWidth(220);
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

  scalar_row->addWidget(new QLabel("Scalar"));
  scalar_row->addWidget(array_combo_, 2);
  scalar_row->addWidget(new QLabel("Preset"));
  scalar_row->addWidget(preset_combo_);
  scalar_row->addWidget(new QLabel("Repr"));
  scalar_row->addWidget(repr_combo_);
  scalar_row->addWidget(new QLabel("Bar Pos"));
  scalar_row->addWidget(scalar_bar_pos_combo_);
  scalar_row->addWidget(auto_range_);
  scalar_row->addWidget(range_min_);
  scalar_row->addWidget(range_max_);
  scalar_layout->addLayout(scalar_row);

  auto* mesh_layout = make_tab("Mesh");
  auto* mesh_row = new QHBoxLayout();
  mesh_row->addWidget(new QLabel("Mesh"));
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
  mesh_dim_->addItem("All", -1);
  mesh_dim_->addItem("0", 0);
  mesh_dim_->addItem("1", 1);
  mesh_dim_->addItem("2", 2);
  mesh_dim_->addItem("3", 3);
  AttachComboPopupFix(mesh_dim_);
  connect(mesh_dim_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { update_mesh_pipeline(); });
  mesh_group_ = new QComboBox();
  mesh_group_->setMinimumWidth(180);
  AttachComboPopupFix(mesh_group_);
  connect(mesh_group_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { update_mesh_pipeline(); });
  mesh_row->addWidget(show_faces_);
  mesh_row->addWidget(show_edges_);
  mesh_row->addWidget(show_shell_);
  mesh_row->addWidget(show_nodes_);
  mesh_row->addWidget(show_quality_);
  mesh_row->addWidget(new QLabel("Dim"));
  mesh_row->addWidget(mesh_dim_);
  mesh_row->addWidget(new QLabel("Group"));
  mesh_row->addWidget(mesh_group_, 1);
  mesh_row->addStretch(1);
  mesh_layout->addLayout(mesh_row);

  auto* entity_row = new QHBoxLayout();
  mesh_entity_ = new QComboBox();
  mesh_entity_->setMinimumWidth(180);
  AttachComboPopupFix(mesh_entity_);
  connect(mesh_entity_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { update_mesh_pipeline(); });
  entity_row->addWidget(new QLabel("Entity"));
  entity_row->addWidget(mesh_entity_, 1);
  entity_row->addStretch(1);
  mesh_layout->addLayout(entity_row);

  auto* mesh_opts = new QHBoxLayout();
  mesh_type_ = new QComboBox();
  mesh_type_->setMinimumWidth(160);
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
    selected_group_dim_ = -1;
    selected_group_id_ = -1;
    selected_cell_id_ = -1;
    update_selection_pipeline();
    if (pick_info_) {
      pick_info_->setText("Pick: cleared");
    }
  });
  mesh_opts->addWidget(new QLabel("Type"));
  mesh_opts->addWidget(mesh_type_, 1);
  mesh_opts->addWidget(new QLabel("Opacity"));
  mesh_opts->addWidget(mesh_opacity_);
  mesh_opts->addWidget(new QLabel("Shrink"));
  mesh_opts->addWidget(mesh_shrink_);
  mesh_opts->addWidget(mesh_scalar_bar_);
  mesh_opts->addWidget(pick_enable_);
  mesh_opts->addWidget(pick_mode_);
  mesh_opts->addWidget(pick_clear_);
  mesh_opts->addStretch(1);
  mesh_layout->addLayout(mesh_opts);

  auto* view_layout = make_tab("View");
  auto* view_row = new QHBoxLayout();
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
    selected_group_dim_ = -1;
    selected_group_id_ = -1;
    selected_entity_dim_ = -1;
    selected_entity_tag_ = -1;
    selected_cell_id_ = -1;
    update_mesh_pipeline();
  });
  view_row->addWidget(view_combo_);
  view_row->addWidget(view_apply_);
  view_row->addWidget(show_axes_);
  view_row->addWidget(show_outline_);
  view_row->addWidget(reset_filters);
  view_row->addStretch(1);
  view_layout->addLayout(view_row);

  pick_info_ = new QLabel("Pick: disabled");
  view_layout->addWidget(pick_info_);

  auto* slice_layout = make_tab("Slice");
  auto* slice_row = new QHBoxLayout();
  slice_enable_ = new QCheckBox("Slice");
  slice_axis_ = new QComboBox();
  slice_axis_->addItems({"X", "Y", "Z"});
  AttachComboPopupFix(slice_axis_);
  slice_slider_ = new QSlider(Qt::Horizontal);
  slice_slider_->setRange(0, 100);
  slice_slider_->setValue(50);
  connect(slice_enable_, &QCheckBox::toggled, this,
          [this](bool) { update_mesh_pipeline(); });
  connect(slice_axis_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { update_mesh_pipeline(); });
  connect(slice_slider_, &QSlider::valueChanged, this,
          [this](int) { update_mesh_pipeline(); });
  slice_row->addWidget(slice_enable_);
  slice_row->addWidget(slice_axis_);
  slice_row->addWidget(slice_slider_, 1);
  slice_layout->addLayout(slice_row);

  mesh_legend_ = new QLabel();
  mesh_legend_->setWordWrap(true);
  mesh_legend_->setText("Groups: none");
  view_layout->addWidget(mesh_legend_);

  auto* time_layout = make_tab("Time");
  auto* refresh_row = new QHBoxLayout();
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
  refresh_row->addWidget(auto_refresh_);
  refresh_row->addWidget(new QLabel("ms"));
  refresh_row->addWidget(refresh_ms_);
  refresh_row->addStretch(1);
  time_layout->addLayout(refresh_row);

  auto* time_row = new QHBoxLayout();
  time_slider_ = new QSlider(Qt::Horizontal);
  time_slider_->setRange(0, 0);
  time_label_ = new QLabel("t=0");
  connect(time_slider_, &QSlider::valueChanged, this, &VtkViewer::on_time_changed);
  time_row->addWidget(new QLabel("Time"));
  time_row->addWidget(time_slider_, 1);
  time_row->addWidget(time_label_);
  time_layout->addLayout(time_row);

  auto* vector_layout = make_tab("Vector");
  vector_array_combo_ = new QComboBox();
  vector_array_combo_->setMinimumWidth(240);
  AttachComboPopupFix(vector_array_combo_);
  vector_auto_sync_deform_ = new QCheckBox("Auto-sync deformation vector");
  vector_auto_sync_deform_->setChecked(true);
  vector_apply_to_deform_ = new QPushButton("Apply to Deform");
  vector_info_ = new QLabel("No vector data loaded");
  vector_info_->setWordWrap(true);
  auto* vector_row = new QHBoxLayout();
  vector_row->addWidget(new QLabel("Vector"));
  vector_row->addWidget(vector_array_combo_, 1);
  vector_row->addWidget(vector_auto_sync_deform_);
  vector_row->addWidget(vector_apply_to_deform_);
  vector_layout->addLayout(vector_row);
  vector_layout->addWidget(vector_info_);
  vector_layout->addStretch(1);
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

  auto* deform_layout = make_tab("Deformation");
  deform_enable_ = new QCheckBox("Enable Deformation");
  deform_vector_ = new QComboBox();
  deform_vector_->setMinimumWidth(200);
  AttachComboPopupFix(deform_vector_);
  deform_scale_ = new QDoubleSpinBox();
  deform_scale_->setRange(0.0, 1000.0);
  deform_scale_->setSingleStep(0.1);
  deform_scale_->setValue(1.0);
  auto* deform_row = new QHBoxLayout();
  deform_row->addWidget(deform_enable_);
  deform_row->addWidget(new QLabel("Vector"));
  deform_row->addWidget(deform_vector_, 1);
  deform_row->addWidget(new QLabel("Scale"));
  deform_row->addWidget(deform_scale_);
  deform_layout->addLayout(deform_row);
  auto* deform_hint = new QLabel(
      "Applies warping using the selected vector array.", control_tabs);
  deform_hint->setStyleSheet("color: #666;");
  deform_layout->addWidget(deform_hint);
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

  auto* probe_layout = make_tab("Probe");
  probe_enable_ = new QCheckBox("Enable Probe");
  probe_mode_ = new QComboBox();
  probe_mode_->addItem("Point", 0);
  probe_mode_->addItem("Cell", 1);
  AttachComboPopupFix(probe_mode_);
  probe_clear_ = new QPushButton("Clear");
  auto* probe_row = new QHBoxLayout();
  probe_row->addWidget(probe_enable_);
  probe_row->addWidget(new QLabel("Mode"));
  probe_row->addWidget(probe_mode_);
  probe_row->addWidget(probe_clear_);
  probe_row->addStretch(1);
  probe_layout->addLayout(probe_row);
  probe_info_ = new QLabel("Probe: disabled");
  probe_info_->setWordWrap(true);
  probe_layout->addWidget(probe_info_);
  probe_layout->addStretch(1);
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
          [update_probe_status](bool) { update_probe_status(); });
  connect(probe_mode_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [update_probe_status](int) { update_probe_status(); });
  connect(probe_clear_, &QPushButton::clicked, this, [this]() {
    if (probe_info_) {
      probe_info_->setText("Probe: cleared");
    }
  });

  auto* plot_layout = make_tab("Plot");
  auto* plot_row = new QHBoxLayout();
  plot_refresh_btn_ = new QPushButton("Refresh");
  plot_stats_ = new QLabel("No data");
  plot_row->addWidget(plot_refresh_btn_);
  plot_row->addWidget(plot_stats_);
  plot_row->addStretch(1);
  plot_layout->addLayout(plot_row);
  plot_view_ = new QPlainTextEdit();
  plot_view_->setReadOnly(true);
  plot_view_->setLineWrapMode(QPlainTextEdit::NoWrap);
  QFont plot_font;
  plot_font.setFamilies({"SFMono-Regular", "Monaco", "Consolas", "Menlo"});
  plot_font.setStyleHint(QFont::Monospace);
  plot_font.setPointSize(10);
  plot_view_->setFont(plot_font);
  plot_view_->setMinimumHeight(96);
  plot_layout->addWidget(plot_view_, 1);
  connect(plot_refresh_btn_, &QPushButton::clicked, this,
          &VtkViewer::update_plot_view);

  auto* table_layout = make_tab("Table");
  auto* table_row = new QHBoxLayout();
  table_row->addWidget(new QLabel("Rows"));
  table_rows_spin_ = new QSpinBox();
  table_rows_spin_->setRange(10, 5000);
  table_rows_spin_->setSingleStep(50);
  table_rows_spin_->setValue(100);
  table_refresh_btn_ = new QPushButton("Refresh");
  table_stats_ = new QLabel("No data");
  table_row->addWidget(table_rows_spin_);
  table_row->addWidget(table_refresh_btn_);
  table_row->addWidget(table_stats_);
  table_row->addStretch(1);
  table_layout->addLayout(table_row);
  table_view_ = new QTableWidget();
  table_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_view_->setSelectionMode(QAbstractItemView::SingleSelection);
  table_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_view_->setAlternatingRowColors(true);
  table_view_->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  table_view_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  table_view_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  table_view_->setMinimumHeight(96);
  table_layout->addWidget(table_view_, 1);
  connect(table_rows_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &VtkViewer::update_table_view);
  connect(table_refresh_btn_, &QPushButton::clicked, this,
          &VtkViewer::update_table_view);

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

void VtkViewer::set_exodus_file(const QString& path) {
  current_file_ = path;
  if (!file_label_) {
    return;
  }
  file_label_->setText(path.isEmpty() ? "No file loaded" : path);
#ifdef GMP_ENABLE_VTK_VIEWER
  if (path.isEmpty()) {
    return;
  }

  if (!reader_) {
    reader_ = vtkSmartPointer<vtkExodusIIReader>::New();
  }
  if (!geom_) {
    geom_ = vtkSmartPointer<vtkCompositeDataGeometryFilter>::New();
  }
  if (!mapper_) {
    mapper_ = vtkSmartPointer<vtkDataSetMapper>::New();
  }
  if (!actor_) {
    actor_ = vtkSmartPointer<vtkActor>::New();
  }
  if (!lut_) {
    lut_ = vtkSmartPointer<vtkLookupTable>::New();
    lut_->SetNumberOfTableValues(256);
    lut_->Build();
  }
  if (!scalar_bar_) {
    scalar_bar_ = vtkSmartPointer<vtkScalarBarActor>::New();
    style_scalar_bar(scalar_bar_);
    position_scalar_bar(scalar_bar_, scalar_bar_pos_);
  }
  mapper_->SetLookupTable(lut_);
  geom_->SetInputConnection(reader_->GetOutputPort());
  mapper_->SetInputConnection(geom_->GetOutputPort());
  actor_->SetMapper(mapper_);
  if (renderer_ && !actor_added_) {
    renderer_->AddActor(actor_);
    scalar_bar_->SetLookupTable(mapper_->GetLookupTable());
    renderer_->AddViewProp(scalar_bar_);
    actor_added_ = true;
  }
  pipeline_ready_ = true;

  first_render_ = true;
  mode_ = DataMode::Exodus;
  reader_->SetFileName(path.toUtf8().constData());
  reader_->UpdateInformation();
  reader_->SetAllArrayStatus(vtkExodusIIReader::NODAL, 1);
  reader_->SetAllArrayStatus(vtkExodusIIReader::ELEM_BLOCK, 1);
  reader_->SetAllArrayStatus(vtkExodusIIReader::GLOBAL, 1);
  const int n_points = reader_->GetNumberOfPointResultArrays();
  for (int i = 0; i < n_points; ++i) {
    reader_->SetPointResultArrayStatus(
        reader_->GetPointResultArrayName(i), 1);
  }
  const int n_elems = reader_->GetNumberOfElementResultArrays();
  for (int i = 0; i < n_elems; ++i) {
    reader_->SetElementResultArrayStatus(
        reader_->GetElementResultArrayName(i), 1);
  }
  update_time_steps_from_reader(false);
  update_mesh_controls();
  setup_watcher(path);
  update_pipeline();
#else
  Q_UNUSED(path);
#endif
}

void VtkViewer::set_mesh_file(const QString& path) {
  current_file_ = path;
  if (!file_label_) {
    return;
  }
  file_label_->setText(path.isEmpty() ? "No file loaded" : path);
#ifdef GMP_ENABLE_VTK_VIEWER
  if (path.isEmpty()) {
    return;
  }
  if (!renderer_) {
    return;
  }
  if (!mapper_) {
    mapper_ = vtkSmartPointer<vtkDataSetMapper>::New();
    actor_ = vtkSmartPointer<vtkActor>::New();
    lut_ = vtkSmartPointer<vtkLookupTable>::New();
    lut_->SetNumberOfTableValues(256);
    lut_->Build();
    mapper_->SetLookupTable(lut_);
    scalar_bar_ = vtkSmartPointer<vtkScalarBarActor>::New();
    style_scalar_bar(scalar_bar_);
    position_scalar_bar(scalar_bar_, scalar_bar_pos_);
  }
  if (!mesh_geom_) {
    mesh_geom_ = vtkSmartPointer<vtkDataSetSurfaceFilter>::New();
  }

#ifdef GMP_ENABLE_GMSH_GUI
  mesh_quality_ready_ = false;
  mesh_groups_.clear();
  mesh_elem_types_.clear();
  mesh_entities_.clear();
  mesh_grid_ = BuildGridFromGmsh(path);
  if (!mesh_grid_) {
    file_label_->setText("Failed to load mesh");
    return;
  }
  mesh_grid_->GetBounds(mesh_bounds_);
  try {
    std::vector<std::pair<int, int>> groups;
    gmsh::model::getPhysicalGroups(groups);
    for (const auto& pg : groups) {
      std::string name;
      gmsh::model::getPhysicalName(pg.first, pg.second, name);
      mesh_groups_.push_back(
          {pg.first, pg.second, QString::fromStdString(name)});
    }
    std::vector<int> element_types;
    std::vector<std::vector<std::size_t>> element_tags;
    std::vector<std::vector<std::size_t>> element_nodes;
    gmsh::model::mesh::getElements(element_types, element_tags, element_nodes);
    std::sort(element_types.begin(), element_types.end());
    element_types.erase(std::unique(element_types.begin(), element_types.end()),
                        element_types.end());
    for (int t : element_types) {
      mesh_elem_types_.push_back(t);
    }

    std::vector<std::pair<int, int>> entities;
    gmsh::model::getEntities(entities);
    std::sort(entities.begin(), entities.end());
    for (const auto& ent : entities) {
      mesh_entities_.push_back({ent.first, ent.second});
    }
  } catch (...) {
    // Ignore physical group name errors.
  }
  mesh_geom_->SetInputData(mesh_grid_);
#else
  file_label_->setText("Mesh preview requires libgmsh");
  return;
#endif

  mapper_->SetInputConnection(mesh_geom_->GetOutputPort());
  actor_->SetMapper(mapper_);
  if (renderer_ && !actor_added_) {
    renderer_->AddActor(actor_);
    scalar_bar_->SetLookupTable(mapper_->GetLookupTable());
    renderer_->AddViewProp(scalar_bar_);
    actor_added_ = true;
  }
  pipeline_ready_ = true;
  first_render_ = true;
  mode_ = DataMode::Mesh;
  time_steps_.clear();
  time_slider_->setEnabled(false);
  time_slider_->setRange(0, 0);
  time_label_->setText("t=0");
  if (repr_combo_ && repr_combo_->currentIndex() == 0) {
    repr_combo_->setCurrentIndex(2);
  }
  update_mesh_controls();

  setup_watcher(path);
  update_pipeline();
  update_nodes_visibility();
#else
  Q_UNUSED(path);
#endif
}

void VtkViewer::set_mesh_group_filter(int dim, int tag) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!mesh_group_ || mesh_group_->count() == 0) {
    return;
  }
  selected_group_dim_ = dim;
  selected_group_id_ = tag;
  selected_cell_id_ = -1;
  int target_index = 0;
  if (dim >= 0 && tag >= 0) {
    for (size_t i = 0; i < mesh_groups_.size(); ++i) {
      const auto& g = mesh_groups_[i];
      if (g.dim == dim && g.id == tag) {
        const int idx = mesh_group_->findData(static_cast<int>(i));
        if (idx >= 0) {
          target_index = idx;
        }
        break;
      }
    }
  }
  mesh_group_->blockSignals(true);
  mesh_group_->setCurrentIndex(target_index);
  mesh_group_->blockSignals(false);
  update_mesh_pipeline();
  update_selection_pipeline();
#else
  Q_UNUSED(dim);
  Q_UNUSED(tag);
#endif
}

void VtkViewer::set_mesh_entity_filter(int dim, int tag) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!mesh_entity_ || mesh_entity_->count() == 0) {
    return;
  }
  selected_entity_dim_ = dim;
  selected_entity_tag_ = tag;
  selected_cell_id_ = -1;
  int target_index = 0;
  if (dim >= 0 && tag >= 0) {
    for (size_t i = 0; i < mesh_entities_.size(); ++i) {
      const auto& ent = mesh_entities_[i];
      if (ent.dim == dim && ent.tag == tag) {
        const int idx = mesh_entity_->findData(static_cast<int>(i));
        if (idx >= 0) {
          target_index = idx;
        }
        break;
      }
    }
  }
  mesh_entity_->blockSignals(true);
  mesh_entity_->setCurrentIndex(target_index);
  mesh_entity_->blockSignals(false);
  update_mesh_pipeline();
  update_selection_pipeline();
#else
  Q_UNUSED(dim);
  Q_UNUSED(tag);
#endif
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
  map.insert("table_rows",
             table_rows_spin_ ? table_rows_spin_->value() : 100);

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
  if (table_rows_spin_) {
    const int rows = settings.value("table_rows", table_rows_spin_->value()).toInt();
    table_rows_spin_->setValue(qBound(10, rows, 5000));
  }

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
  if (plot_view_) {
    update_plot_view();
  }
  if (table_view_) {
    update_table_view();
  }
}

void VtkViewer::set_exodus_history(const QStringList& paths) {
  output_combo_->clear();
  for (const auto& p : paths) {
    output_combo_->addItem(QFileInfo(p).fileName(), p);
  }
  if (!paths.isEmpty()) {
    output_combo_->setCurrentIndex(0);
  }
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

QString VtkViewer::plot_snapshot_text() const {
  return cached_plot_text_;
}

QString VtkViewer::plot_stats_snapshot() const {
  return cached_plot_stats_;
}

QString VtkViewer::table_snapshot_text() const {
  return cached_table_text_;
}

QString VtkViewer::table_stats_snapshot() const {
  return cached_table_stats_;
}

void VtkViewer::on_reload() {
  if (!current_file_.isEmpty()) {
    load_file(current_file_);
  }
}

void VtkViewer::on_time_changed(int index) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (mode_ != DataMode::Exodus) {
    return;
  }
  if (current_file_.isEmpty()) {
    return;
  }
  if (time_steps_.empty()) {
    return;
  }
  if (index < 0 || index >= static_cast<int>(time_steps_.size())) {
    return;
  }
  time_label_->setText(QString("t=%1").arg(time_steps_[index]));
  if (array_combo_->count() == 0) {
    update_pipeline();
  } else {
    refresh_time_only();
  }
  update_vector_tab();
  if (plot_view_) {
    update_plot_view();
  }
  if (table_view_) {
    update_table_view();
  }
#else
  Q_UNUSED(index);
#endif
}

void VtkViewer::on_array_changed(int index) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!mapper_) {
    return;
  }
  if (index < 0) {
    return;
  }
  const QString key = array_combo_->currentData().toString();
  if (key.isEmpty()) {
    return;
  }
  const bool is_point = key.startsWith("P:");
  const QString name = key.mid(2);

  vtkDataSet* data = nullptr;
  if (mode_ == DataMode::Exodus && mapper_) {
    data = vtkDataSet::SafeDownCast(mapper_->GetInput());
    if (!data && geom_) {
      geom_->Update();
      data = geom_->GetOutput();
    }
  } else if (mode_ == DataMode::Mesh && mapper_) {
    data = vtkDataSet::SafeDownCast(mapper_->GetInput());
    if (!data && mesh_geom_) {
      mesh_geom_->Update();
      data = mesh_geom_->GetOutput();
    }
  }
  if (!data) {
    return;
  }

  vtkDataArray* array = nullptr;
  if (is_point) {
    mapper_->SetScalarModeToUsePointFieldData();
    array = data->GetPointData()->GetArray(name.toUtf8().constData());
  } else {
    mapper_->SetScalarModeToUseCellFieldData();
    array = data->GetCellData()->GetArray(name.toUtf8().constData());
  }

  if (array) {
    double range[2] = {0.0, 1.0};
    array->GetRange(range);
    mapper_->SelectColorArray(name.toUtf8().constData());
    mapper_->ScalarVisibilityOn();
    if (auto_range_->isChecked()) {
      range_min_->setValue(range[0]);
      range_max_->setValue(range[1]);
    }
    apply_lookup_table();
    on_apply_range();
    if (scalar_bar_) {
      scalar_bar_->SetTitle(name.toUtf8().constData());
      scalar_bar_->SetLookupTable(mapper_->GetLookupTable());
    }
  } else {
    mapper_->ScalarVisibilityOff();
  }
  if (render_window_) {
    render_window_->Render();
  }
  update_vector_tab();
  if (plot_view_) {
    update_plot_view();
  }
  if (table_view_) {
    update_table_view();
  }
#else
  Q_UNUSED(index);
#endif
  update_array_list();
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
      if ((self->pick_enable_ && self->pick_enable_->isChecked()) ||
          (self->probe_enable_ && self->probe_enable_->isChecked())) {
        int pos[2] = {0, 0};
        iren->GetEventPosition(pos);
        self->handle_pick(pos[0], pos[1]);
      }
      if (auto* style =
              vtkInteractorStyle::SafeDownCast(iren->GetInteractorStyle())) {
        style->OnLeftButtonDown();
      }
    });
    interactor->AddObserver(vtkCommand::LeftButtonPressEvent, pick_callback_);
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
    geom_->Update();
    update_deformation_pipeline();
  } else if (mode_ == DataMode::Mesh) {
    update_mesh_pipeline();
  }
  populate_arrays();
  update_vector_list();
  update_vector_tab();
  update_scene_extras();
  if (plot_view_) {
    update_plot_view();
  }
  if (table_view_) {
    update_table_view();
  }
  if (first_render_) {
    renderer_->ResetCamera();
    first_render_ = false;
  }
  render_window_->Render();
#endif
}

void VtkViewer::populate_arrays() {
#ifdef GMP_ENABLE_VTK_VIEWER
  vtkDataSet* data = nullptr;
  if (mode_ == DataMode::Exodus && mapper_) {
    data = vtkDataSet::SafeDownCast(mapper_->GetInput());
    if (!data && geom_) {
      geom_->Update();
      data = geom_->GetOutput();
    }
  } else if (mode_ == DataMode::Mesh && mapper_) {
    data = vtkDataSet::SafeDownCast(mapper_->GetInput());
    if (!data && mesh_geom_) {
      mesh_geom_->Update();
      data = mesh_geom_->GetOutput();
    }
  }
  if (!data) {
    return;
  }

  const QString current = array_combo_->currentData().toString();
  array_combo_->blockSignals(true);
  array_combo_->clear();

  auto* pd = data->GetPointData();
  if (pd) {
    for (int i = 0; i < pd->GetNumberOfArrays(); ++i) {
      const char* name = pd->GetArrayName(i);
      if (name) {
        array_combo_->addItem(QString("Point: %1").arg(name),
                              QString("P:%1").arg(name));
      }
    }
  }

  auto* cd = data->GetCellData();
  if (cd) {
    for (int i = 0; i < cd->GetNumberOfArrays(); ++i) {
      const char* name = cd->GetArrayName(i);
      if (name) {
        array_combo_->addItem(QString("Cell: %1").arg(name),
                              QString("C:%1").arg(name));
      }
    }
  }

  array_combo_->blockSignals(false);

  int idx = array_combo_->findData(current);
  if (idx < 0 && mode_ == DataMode::Mesh) {
    const QString preferred =
        (show_quality_ && show_quality_->isChecked()) ? "C:Quality" : "C:phys_id";
    idx = array_combo_->findData(preferred);
  }
  if (idx < 0 && array_combo_->count() > 0) {
    idx = 0;
    for (int i = 0; i < array_combo_->count(); ++i) {
      const QString label = array_combo_->itemText(i);
      if (!label.contains("ObjectId", Qt::CaseInsensitive)) {
        idx = i;
        break;
      }
    }
  }
  if (idx >= 0) {
    array_combo_->setCurrentIndex(idx);
    on_array_changed(idx);
  } else {
    mapper_->ScalarVisibilityOff();
  }
  update_array_list();
#endif
}

void VtkViewer::update_array_list() {
  if (!array_list_ || !array_combo_) {
    return;
  }
  const QString filter =
      array_filter_ ? array_filter_->currentData().toString() : "all";
  const QString current = array_combo_->currentData().toString();
  array_list_->blockSignals(true);
  array_list_->clear();
  for (int i = 0; i < array_combo_->count(); ++i) {
    const QString key = array_combo_->itemData(i).toString();
    if (filter == "P" && !key.startsWith("P:")) {
      continue;
    }
    if (filter == "C" && !key.startsWith("C:")) {
      continue;
    }
    auto* item = new QListWidgetItem(array_combo_->itemText(i), array_list_);
    item->setData(Qt::UserRole, key);
    if (key == current) {
      array_list_->setCurrentItem(item);
      item->setSelected(true);
    }
  }
  array_list_->blockSignals(false);
}

void VtkViewer::update_vector_tab() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!vector_array_combo_ || !vector_info_) {
    return;
  }
  const bool vector_mode =
      (mode_ == DataMode::Exodus || mode_ == DataMode::Mesh);
  vector_array_combo_->setEnabled(vector_mode);
  vector_apply_to_deform_->setEnabled(vector_mode);
  vector_auto_sync_deform_->setEnabled(vector_mode);

  vtkDataSet* data = nullptr;
  if (mode_ == DataMode::Exodus && mapper_) {
    data = vtkDataSet::SafeDownCast(mapper_->GetInput());
    if (!data && geom_) {
      geom_->Update();
      data = geom_->GetOutput();
    }
  } else if (mode_ == DataMode::Mesh && mapper_) {
    data = vtkDataSet::SafeDownCast(mapper_->GetInput());
    if (!data && mesh_geom_) {
      mesh_geom_->Update();
      data = mesh_geom_->GetOutput();
    }
  }
  if (!data) {
    vector_array_combo_->clear();
    vector_info_->setText("No vector data loaded");
    return;
  }

  const QString prev = vector_array_combo_->currentData().toString();
  int best_index = -1;
  int selected_index = -1;
  vector_array_combo_->blockSignals(true);
  vector_array_combo_->clear();
  if (auto* pd = data->GetPointData()) {
    for (int i = 0; i < pd->GetNumberOfArrays(); ++i) {
      vtkDataArray* arr = pd->GetArray(i);
      if (!arr) {
        continue;
      }
      const char* name_c = arr->GetName();
      if (!name_c || arr->GetNumberOfComponents() < 2) {
        continue;
      }
      const QString name = QString::fromUtf8(name_c);
      const int idx = vector_array_combo_->count();
      vector_array_combo_->addItem(QString("Point: %1").arg(name), QString("P:%1").arg(name));
      if (best_index < 0) {
        const QString lower_name = name.toLower();
        if (lower_name.contains("disp") || lower_name.contains("displacement")) {
          best_index = idx;
        }
      }
      if (selected_index < 0 && name == prev.mid(2)) {
        selected_index = idx;
      }
    }
  }
  if (auto* cd = data->GetCellData()) {
    for (int i = 0; i < cd->GetNumberOfArrays(); ++i) {
      vtkDataArray* arr = cd->GetArray(i);
      if (!arr) {
        continue;
      }
      const char* name_c = arr->GetName();
      if (!name_c || arr->GetNumberOfComponents() < 2) {
        continue;
      }
      const QString name = QString::fromUtf8(name_c);
      const int idx = vector_array_combo_->count();
      vector_array_combo_->addItem(QString("Cell: %1").arg(name), QString("C:%1").arg(name));
      if (selected_index < 0 && name == prev.mid(2)) {
        selected_index = idx;
      }
      if (best_index < 0) {
        const QString lower_name = name.toLower();
        if (lower_name.contains("disp") || lower_name.contains("displacement")) {
          best_index = idx;
        }
      }
    }
  }

  int target_index = -1;
  if (!prev.isEmpty()) {
    target_index = vector_array_combo_->findData(prev);
    if (selected_index >= 0) {
      target_index = selected_index;
    }
  }
  if (target_index < 0 && best_index >= 0) {
    target_index = best_index;
  }
  if (target_index < 0 && vector_array_combo_->count() > 0) {
    target_index = 0;
  }
  if (target_index >= 0) {
    vector_array_combo_->setCurrentIndex(target_index);
  }
  vector_array_combo_->blockSignals(false);

  if (vector_array_combo_->count() == 0) {
    vector_array_combo_->setEnabled(false);
    vector_apply_to_deform_->setEnabled(false);
    vector_auto_sync_deform_->setEnabled(false);
    vector_info_->setText("No vector arrays (>=2 components)");
    return;
  }

  const QString key = vector_array_combo_->currentData().toString();
  const QString name = key.mid(2);
  vtkDataArray* target = nullptr;
  if (key.startsWith("P:")) {
    target = data->GetPointData()
                 ? data->GetPointData()->GetArray(name.toUtf8().constData())
                 : nullptr;
  } else if (key.startsWith("C:")) {
    target = data->GetCellData()
                 ? data->GetCellData()->GetArray(name.toUtf8().constData())
                 : nullptr;
  }
  if (!target) {
    vector_info_->setText("No selectable vector array");
    return;
  }
  const VectorStats stats = AnalyzeVectorArray(target);
  if (!stats.has_data) {
    vector_info_->setText("Array must have 2+ components");
    return;
  }
  const QString sample = ArrayValueSample(target, 0);
  vector_info_->setText(
      QString("%1 | %2 | sample[0]=%3")
          .arg(FormatVectorStatsText(stats))
          .arg(key)
          .arg(sample));

  if (deform_vector_ && vector_auto_sync_deform_ &&
      vector_auto_sync_deform_->isChecked()) {
    deform_vector_->blockSignals(true);
    const int deform_index = deform_vector_->findText(name);
    if (deform_index >= 0) {
      deform_vector_->setCurrentIndex(deform_index);
    }
    deform_vector_->blockSignals(false);
  }
  if (deform_enable_) {
    update_deformation_pipeline();
    update_scene_extras();
    if (render_window_) {
      render_window_->Render();
    }
  }
#else
  Q_UNUSED(this);
#endif
}

void VtkViewer::update_plot_view() {
#ifndef GMP_ENABLE_VTK_VIEWER
  if (plot_view_) {
    plot_view_->setPlainText("vtk disabled");
    if (plot_stats_) {
      plot_stats_->setText("vtk disabled");
    }
  }
  cached_plot_text_ = QString::fromUtf8("vtk disabled");
  cached_plot_stats_ = QString::fromUtf8("vtk disabled");
  return;
#endif
  if (!plot_view_) {
    cached_plot_text_ = QString::fromUtf8("No plot widget");
    cached_plot_stats_ = QString::fromUtf8("No plot widget");
    return;
  }
  vtkDataSet* data = nullptr;
  if (mode_ == DataMode::Exodus && mapper_) {
    data = vtkDataSet::SafeDownCast(mapper_->GetInput());
    if (!data && geom_) {
      geom_->Update();
      data = geom_->GetOutput();
    }
  } else if (mode_ == DataMode::Mesh && mapper_) {
    data = vtkDataSet::SafeDownCast(mapper_->GetInput());
    if (!data && mesh_geom_) {
      mesh_geom_->Update();
      data = mesh_geom_->GetOutput();
    }
  }
  if (!data) {
    plot_view_->setPlainText("No data");
    if (plot_stats_) {
      plot_stats_->setText("No data");
    }
    cached_plot_text_ = QString::fromUtf8("No data");
    cached_plot_stats_ = QString::fromUtf8("No data");
    return;
  }

  QString key;
  if (array_combo_ && array_combo_->count() > 0) {
    key = array_combo_->currentData().toString();
  }
  if (key.isEmpty() && vector_array_combo_ && vector_array_combo_->count() > 0) {
    key = vector_array_combo_->currentData().toString();
  }
  if (key.isEmpty()) {
    plot_view_->setPlainText("No array selected");
    if (plot_stats_) {
      plot_stats_->setText("No array selected");
    }
    cached_plot_text_ = QString::fromUtf8("No array selected");
    cached_plot_stats_ = QString::fromUtf8("No array selected");
    return;
  }

  vtkDataArray* array = nullptr;
  if (key.startsWith("P:")) {
    const QString name = key.mid(2);
    array = data->GetPointData()
                ? data->GetPointData()->GetArray(name.toUtf8().constData())
                : nullptr;
  } else if (key.startsWith("C:")) {
    const QString name = key.mid(2);
    array = data->GetCellData()
                ? data->GetCellData()->GetArray(name.toUtf8().constData())
                : nullptr;
  }
  if (!array) {
    plot_view_->setPlainText("Selected array not found");
    if (plot_stats_) {
      plot_stats_->setText("Invalid array");
    }
    cached_plot_text_ = QString::fromUtf8("Selected array not found");
    cached_plot_stats_ = QString::fromUtf8("Invalid array");
    return;
  }

  const int comps = array->GetNumberOfComponents();
  const vtkIdType tuples = array->GetNumberOfTuples();
  const vtkIdType limit = std::min<vtkIdType>(
      tuples, static_cast<vtkIdType>(500));
  QStringList lines;
  lines << QString("Array: %1").arg(key);
  lines << QString("Components: %1").arg(comps);
  lines << QString("Tuples: %1").arg(tuples);

  double min_v = std::numeric_limits<double>::infinity();
  double max_v = -std::numeric_limits<double>::infinity();
  double mean_acc = 0.0;
  double rms_acc = 0.0;
  for (vtkIdType i = 0; i < tuples; ++i) {
    const double v = (comps > 1) ? ComputeMagnitude(array, i)
                                 : array->GetComponent(i, 0);
    min_v = std::min(min_v, v);
    max_v = std::max(max_v, v);
    mean_acc += v;
    rms_acc += v * v;
  }
  if (tuples > 0) {
    const double mean = mean_acc / static_cast<double>(tuples);
    const double rms = std::sqrt(rms_acc / static_cast<double>(tuples));
    if (comps > 1) {
      lines << QString("Magnitude stats: min=%1 max=%2 mean=%3 rms=%4")
                   .arg(min_v, 0, 'g', 6)
                   .arg(max_v, 0, 'g', 6)
                   .arg(mean, 0, 'g', 6)
                   .arg(rms, 0, 'g', 6);
    } else {
      lines << QString("Scalar stats: min=%1 max=%2 mean=%3 rms=%4")
                   .arg(min_v, 0, 'g', 6)
                   .arg(max_v, 0, 'g', 6)
                   .arg(mean, 0, 'g', 6)
                   .arg(rms, 0, 'g', 6);
    }
  }
  lines << "";
  lines << QString("Preview (first %1 rows)").arg(limit);
  for (vtkIdType i = 0; i < limit; ++i) {
    if (comps <= 1) {
      lines << QString("%1: %2").arg(i).arg(array->GetComponent(i, 0), 0, 'g', 6);
    } else {
      lines << QString("%1: %2").arg(i).arg(ArrayValueSample(array, i));
    }
  }
  if (tuples > limit) {
    lines << QString("... %1 rows omitted ...").arg(tuples - limit);
  }
  plot_view_->setPlainText(lines.join('\n'));
  if (plot_stats_) {
    plot_stats_->setText(QString("mode=%1 tuples=%2").arg(mode_ == DataMode::Mesh ? "mesh"
                                                                              : "exodus")
                                                  .arg(tuples));
  }
  cached_plot_text_ = lines.join('\n');
  cached_plot_stats_ = plot_stats_ ? plot_stats_->text() : QString("No stats");
}

void VtkViewer::update_table_view() {
#ifndef GMP_ENABLE_VTK_VIEWER
  if (table_view_) {
    table_view_->setRowCount(0);
    if (table_stats_) {
      table_stats_->setText("vtk disabled");
    }
  }
  cached_table_text_ = QString::fromUtf8("vtk disabled");
  cached_table_stats_ = QString::fromUtf8("vtk disabled");
  return;
#endif
  if (!table_view_) {
    cached_table_text_ = QString::fromUtf8("No table widget");
    cached_table_stats_ = QString::fromUtf8("No table widget");
    return;
  }
  vtkDataSet* data = nullptr;
  if (mode_ == DataMode::Exodus && mapper_) {
    data = vtkDataSet::SafeDownCast(mapper_->GetInput());
    if (!data && geom_) {
      geom_->Update();
      data = geom_->GetOutput();
    }
  } else if (mode_ == DataMode::Mesh && mapper_) {
    data = vtkDataSet::SafeDownCast(mapper_->GetInput());
    if (!data && mesh_geom_) {
      mesh_geom_->Update();
      data = mesh_geom_->GetOutput();
    }
  }
  if (!data) {
    table_view_->setRowCount(0);
    if (table_stats_) {
      table_stats_->setText("No data");
    }
    cached_table_text_ = QString::fromUtf8("No data");
    cached_table_stats_ = QString::fromUtf8("No data");
    return;
  }

  QString key;
  if (array_combo_ && array_combo_->count() > 0) {
    key = array_combo_->currentData().toString();
  }
  if (key.isEmpty() && vector_array_combo_ && vector_array_combo_->count() > 0) {
    key = vector_array_combo_->currentData().toString();
  }
  if (key.isEmpty()) {
    table_view_->setRowCount(0);
    if (table_stats_) {
      table_stats_->setText("No array selected");
    }
    cached_table_text_ = QString::fromUtf8("No array selected");
    cached_table_stats_ = QString::fromUtf8("No array selected");
    return;
  }

  vtkDataArray* array = nullptr;
  if (key.startsWith("P:")) {
    const QString name = key.mid(2);
    array = data->GetPointData()
                ? data->GetPointData()->GetArray(name.toUtf8().constData())
                : nullptr;
  } else if (key.startsWith("C:")) {
    const QString name = key.mid(2);
    array = data->GetCellData()
                ? data->GetCellData()->GetArray(name.toUtf8().constData())
                : nullptr;
  }
  if (!array) {
    table_view_->setRowCount(0);
    if (table_stats_) {
      table_stats_->setText("Invalid array");
    }
    cached_table_text_ = QString::fromUtf8("Invalid array");
    cached_table_stats_ = QString::fromUtf8("Invalid array");
    return;
  }

  const int comps = array->GetNumberOfComponents();
  const vtkIdType tuples = array->GetNumberOfTuples();
  const int show_rows =
      qBound(1, table_rows_spin_ ? table_rows_spin_->value() : 100, 5000);
  const vtkIdType rows = std::min<vtkIdType>(tuples, show_rows);

  QStringList headers;
  headers << "Index";
  for (int i = 0; i < comps; ++i) {
    headers << QString("C%1").arg(i);
  }
  if (comps > 1) {
    headers << "Magnitude";
  }
  table_view_->setColumnCount(headers.size());
  table_view_->setHorizontalHeaderLabels(headers);
  table_view_->setRowCount(static_cast<int>(rows));

  for (vtkIdType i = 0; i < rows; ++i) {
    const int row = static_cast<int>(i);
    table_view_->setItem(row, 0,
                         new QTableWidgetItem(QString::number(i)));
    double m = 0.0;
    if (comps > 1) {
      m = ComputeMagnitude(array, i);
    }
    for (int c = 0; c < comps; ++c) {
      table_view_->setItem(
          row, c + 1,
          new QTableWidgetItem(QString::number(array->GetComponent(i, c), 'g', 6)));
    }
    if (comps > 1) {
      const int mag_col = static_cast<int>(headers.size()) - 1;
      table_view_->setItem(row, mag_col,
                           new QTableWidgetItem(QString::number(m, 'g', 6)));
    }
  }
  table_view_->resizeColumnsToContents();
  QStringList text_rows;
  text_rows << QString("Array: %1").arg(key);
  text_rows << QString("Tuples: %1").arg(tuples);
  text_rows << QString("Showing rows: %1").arg(rows);
  text_rows << "";
  text_rows << headers.join('\t');
  for (vtkIdType i = 0; i < rows; ++i) {
    QStringList row_text;
    row_text << QString::number(i);
    for (int c = 0; c < table_view_->columnCount() - 1; ++c) {
      auto* item = table_view_->item(static_cast<int>(i), c);
      row_text << (item ? item->text() : QString());
    }
    text_rows << row_text.join('\t');
  }
  if (tuples > rows) {
    text_rows << QString("... omitted %1 rows ...").arg(tuples - rows);
  }
  cached_table_text_ = text_rows.join('\n');
  if (table_stats_) {
    if (comps > 1) {
      VectorStats stats = AnalyzeVectorArray(array);
      if (stats.has_data) {
        table_stats_->setText(
            QString("mode=%1, tuples=%2, show=%3, %4")
                .arg(mode_ == DataMode::Mesh ? "mesh" : "exodus")
                .arg(tuples)
                .arg(rows)
                .arg(FormatVectorStatsText(stats)));
      } else {
        table_stats_->setText(
            QString("tuples=%1, show=%2, components=%3")
                .arg(tuples)
                .arg(rows)
                .arg(comps));
      }
    } else {
      table_stats_->setText(
          QString("tuples=%1, show=%2, components=%3")
              .arg(tuples)
              .arg(rows)
              .arg(comps));
    }
  }
  cached_table_stats_ = table_stats_ ? table_stats_->text() : QString("No stats");
}

void VtkViewer::update_vector_list() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!deform_vector_) {
    return;
  }
  if (mode_ != DataMode::Exodus) {
    deform_vector_->blockSignals(true);
    deform_vector_->clear();
    deform_vector_->blockSignals(false);
    deform_vector_->setEnabled(false);
    return;
  }

  vtkDataSet* data = nullptr;
  if (mapper_) {
    data = vtkDataSet::SafeDownCast(mapper_->GetInput());
  }
  if (!data && geom_) {
    geom_->Update();
    data = geom_->GetOutput();
  }
  if (!data) {
    return;
  }

  auto* pd = data->GetPointData();
  if (!pd) {
    return;
  }

  const QString current = deform_vector_->currentData().toString();
  int best_index = -1;
  deform_vector_->blockSignals(true);
  deform_vector_->clear();
  for (int i = 0; i < pd->GetNumberOfArrays(); ++i) {
    vtkDataArray* arr = pd->GetArray(i);
    if (!arr) {
      continue;
    }
    const char* name = arr->GetName();
    if (!name) {
      continue;
    }
    if (arr->GetNumberOfComponents() < 2) {
      continue;
    }
    const QString label = QString::fromUtf8(name);
    const int idx = deform_vector_->count();
    deform_vector_->addItem(label, label);
    const QString lower = label.toLower();
    if (best_index < 0 &&
        (lower.contains("disp") || lower.contains("displacement"))) {
      best_index = idx;
    }
  }
  int select_idx = -1;
  if (!current.isEmpty()) {
    select_idx = deform_vector_->findData(current);
  }
  if (select_idx < 0) {
    select_idx = best_index;
  }
  if (select_idx < 0 && deform_vector_->count() > 0) {
    select_idx = 0;
  }
  if (select_idx >= 0) {
    deform_vector_->setCurrentIndex(select_idx);
  }
  deform_vector_->blockSignals(false);
  deform_vector_->setEnabled(deform_vector_->count() > 0);
  update_deformation_pipeline();
#endif
}

void VtkViewer::update_deformation_pipeline() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (mode_ != DataMode::Exodus || !mapper_ || !geom_) {
    return;
  }
  const bool enabled = deform_enable_ && deform_enable_->isChecked();
  const QString vector_name =
      deform_vector_ ? deform_vector_->currentData().toString() : "";
  if (!enabled || vector_name.isEmpty()) {
    mapper_->SetInputConnection(geom_->GetOutputPort());
    return;
  }
  if (!warp_filter_) {
    warp_filter_ = vtkSmartPointer<vtkWarpVector>::New();
  }
  warp_filter_->SetInputConnection(geom_->GetOutputPort());
  warp_filter_->SetScaleFactor(
      deform_scale_ ? deform_scale_->value() : 1.0);
  warp_filter_->SetInputArrayToProcess(
      0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS,
      vector_name.toUtf8().constData());
  mapper_->SetInputConnection(warp_filter_->GetOutputPort());
#endif
}

void VtkViewer::on_open_file() {
  const QString path =
      QFileDialog::getOpenFileName(this, "Open Result or Mesh", current_file_,
                                   "Exodus (*.e);;Gmsh Mesh (*.msh)");
  if (!path.isEmpty()) {
    load_file(path);
  }
}

void VtkViewer::on_apply_range() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!mapper_) {
    return;
  }
  const bool auto_range = auto_range_->isChecked();
  range_min_->setEnabled(!auto_range);
  range_max_->setEnabled(!auto_range);
  if (auto_range) {
    // Range already updated in on_array_changed.
  } else {
    mapper_->SetScalarRange(range_min_->value(), range_max_->value());
  }
  if (render_window_) {
    render_window_->Render();
  }
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

void VtkViewer::on_auto_refresh_toggled(bool enabled) {
  set_refresh_enabled(enabled);
}

void VtkViewer::on_auto_refresh_tick() {
  refresh_from_disk();
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

void VtkViewer::set_refresh_enabled(bool enabled) {
  if (enabled) {
    refresh_timer_->start(refresh_ms_->value());
  } else {
    refresh_timer_->stop();
  }
}

void VtkViewer::setup_watcher(const QString& file_path) {
  if (!watcher_) {
    watcher_ = new QFileSystemWatcher(this);
    connect(watcher_, &QFileSystemWatcher::fileChanged, this,
            [this](const QString&) { schedule_reload(); });
    connect(watcher_, &QFileSystemWatcher::directoryChanged, this,
            [this](const QString&) { schedule_reload(); });
  }
  watcher_->removePaths(watcher_->files());
  watcher_->removePaths(watcher_->directories());

  if (file_path.isEmpty()) {
    return;
  }
  QFileInfo fi(file_path);
  const QString dir = fi.absolutePath();
  if (!dir.isEmpty()) {
    watcher_->addPath(dir);
  }
  if (fi.exists()) {
    watcher_->addPath(fi.absoluteFilePath());
    last_file_size_ = fi.size();
    last_file_mtime_ = fi.lastModified();
  }
}

void VtkViewer::schedule_reload() {
  pending_reload_ = true;
  if (debounce_timer_) {
    debounce_timer_->start(300);
  }
}

void VtkViewer::refresh_from_disk() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (current_file_.isEmpty()) {
    return;
  }
  QFileInfo fi(current_file_);
  if (!fi.exists()) {
    return;
  }
  const bool changed =
      (last_file_size_ != fi.size()) || (last_file_mtime_ != fi.lastModified());
  if (changed) {
    last_file_size_ = fi.size();
    last_file_mtime_ = fi.lastModified();
    if (mode_ == DataMode::Exodus) {
      update_time_steps_from_reader(true);
    }
    if (array_combo_->count() == 0) {
      populate_arrays();
    }
    if (mode_ == DataMode::Mesh) {
      set_mesh_file(current_file_);
      return;
    }
  }
  if (mode_ == DataMode::Exodus) {
    refresh_time_only();
  } else {
    update_pipeline();
  }
#endif
}

void VtkViewer::refresh_time_only() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (mode_ != DataMode::Exodus) {
    return;
  }
  if (!reader_ || !geom_ || !render_window_) {
    return;
  }
  if (current_file_.isEmpty() || !pipeline_ready_) {
    return;
  }
  if (!time_steps_.empty()) {
    vtkInformation* info = reader_->GetOutputInformation(0);
    if (info) {
      const int idx = time_slider_->value();
      const double t = time_steps_[idx];
      info->Set(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP(), t);
    }
  }
  reader_->Update();
  geom_->Update();
  render_window_->Render();
#endif
}

void VtkViewer::update_time_steps_from_reader(bool keep_index) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!reader_ || mode_ != DataMode::Exodus) {
    return;
  }
  const int prev_index = time_slider_->value();
  reader_->UpdateInformation();

  time_steps_.clear();
  vtkInformation* info = reader_->GetOutputInformation(0);
  if (info && info->Has(vtkStreamingDemandDrivenPipeline::TIME_STEPS())) {
    const int len = info->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
    const double* steps =
        info->Get(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
    for (int i = 0; i < len; ++i) {
      time_steps_.push_back(steps[i]);
    }
  }

  if (time_steps_.empty()) {
    time_slider_->setRange(0, 0);
    time_slider_->setEnabled(false);
    time_label_->setText("t=0");
    return;
  }

  time_slider_->setEnabled(true);
  time_slider_->setRange(0, static_cast<int>(time_steps_.size()) - 1);
  const int idx = keep_index && prev_index < static_cast<int>(time_steps_.size())
                      ? prev_index
                      : 0;
  time_slider_->blockSignals(true);
  time_slider_->setValue(idx);
  time_slider_->blockSignals(false);
  time_label_->setText(QString("t=%1").arg(time_steps_[idx]));
#endif
}

void VtkViewer::load_file(const QString& path) {
  if (path.endsWith(".msh", Qt::CaseInsensitive)) {
    set_mesh_file(path);
  } else {
    set_exodus_file(path);
  }
}

void VtkViewer::update_mesh_controls() {
#ifdef GMP_ENABLE_VTK_VIEWER
  const bool mesh_mode = mode_ == DataMode::Mesh;
  const bool exodus_mode = mode_ == DataMode::Exodus;
  if (show_nodes_) {
    show_nodes_->setEnabled(mesh_mode);
  }
  if (show_quality_) {
    show_quality_->setEnabled(mesh_mode);
  }
  if (show_faces_) {
    show_faces_->setEnabled(mesh_mode);
  }
  if (show_edges_) {
    show_edges_->setEnabled(mesh_mode);
  }
  if (show_shell_) {
    show_shell_->setEnabled(mesh_mode);
  }
  if (mesh_dim_) {
    mesh_dim_->setEnabled(mesh_mode);
  }
  if (mesh_group_) {
    mesh_group_->setEnabled(mesh_mode);
  }
  if (mesh_entity_) {
    mesh_entity_->setEnabled(mesh_mode);
  }
  if (mesh_type_) {
    mesh_type_->setEnabled(mesh_mode);
  }
  if (mesh_opacity_) {
    mesh_opacity_->setEnabled(mesh_mode);
  }
  if (mesh_shrink_) {
    mesh_shrink_->setEnabled(mesh_mode);
  }
  if (mesh_scalar_bar_) {
    mesh_scalar_bar_->setEnabled(true);
  }
  if (pick_enable_) {
    pick_enable_->setEnabled(mesh_mode);
  }
  if (pick_mode_) {
    pick_mode_->setEnabled(mesh_mode);
  }
  if (pick_clear_) {
    pick_clear_->setEnabled(mesh_mode);
  }
  if (pick_info_) {
    pick_info_->setVisible(mesh_mode);
  }
  if (probe_enable_) {
    probe_enable_->setEnabled(exodus_mode);
  }
  if (probe_mode_) {
    probe_mode_->setEnabled(exodus_mode);
  }
  if (probe_clear_) {
    probe_clear_->setEnabled(exodus_mode);
  }
  if (probe_info_) {
    probe_info_->setVisible(exodus_mode);
  }
  if (deform_enable_) {
    deform_enable_->setEnabled(exodus_mode);
  }
  if (deform_vector_) {
    deform_vector_->setEnabled(exodus_mode);
  }
  if (deform_scale_) {
    deform_scale_->setEnabled(exodus_mode);
  }
  if (view_combo_) {
    view_combo_->setEnabled(mesh_mode || mode_ == DataMode::Exodus);
  }
  if (view_apply_) {
    view_apply_->setEnabled(mesh_mode || mode_ == DataMode::Exodus);
  }
  if (show_axes_) {
    show_axes_->setEnabled(mesh_mode || mode_ == DataMode::Exodus);
  }
  if (show_outline_) {
    show_outline_->setEnabled(mesh_mode || mode_ == DataMode::Exodus);
  }
  if (slice_enable_) {
    slice_enable_->setEnabled(mesh_mode);
  }
  if (slice_axis_) {
    slice_axis_->setEnabled(mesh_mode && slice_enable_->isChecked());
  }
  if (slice_slider_) {
    slice_slider_->setEnabled(mesh_mode && slice_enable_->isChecked());
  }
  if (mesh_legend_) {
    mesh_legend_->setEnabled(mesh_mode);
    mesh_legend_->setVisible(mesh_mode);
  }
  if (repr_combo_) {
    repr_combo_->setEnabled(!mesh_mode);
  }

  if (!mesh_mode) {
    if (mesh_legend_) {
      mesh_legend_->setText("Groups: none");
    }
    return;
  }

  if (mesh_group_) {
    mesh_group_->blockSignals(true);
    mesh_group_->clear();
    mesh_group_->addItem("All", -1);
    for (size_t i = 0; i < mesh_groups_.size(); ++i) {
      const auto& g = mesh_groups_[i];
      const QString label = g.name.isEmpty()
                                ? QString("%1:%2").arg(g.dim).arg(g.id)
                                : QString("%1:%2 %3").arg(g.dim).arg(g.id).arg(g.name);
      mesh_group_->addItem(label, static_cast<int>(i));
    }
    mesh_group_->setCurrentIndex(0);
    mesh_group_->blockSignals(false);
  }

  if (mesh_entity_) {
    // 首次填充时 currentData() 为无效 QVariant，toInt() 会返回 0，
    // 进而 findData(0) 命中首个实体导致误加实体过滤器；必须显式判无效。
    const QVariant current_data = mesh_entity_->currentData();
    const int current = current_data.isValid() ? current_data.toInt() : -1;
    mesh_entity_->blockSignals(true);
    mesh_entity_->clear();
    mesh_entity_->addItem("All", -1);
    for (size_t i = 0; i < mesh_entities_.size(); ++i) {
      const auto& ent = mesh_entities_[i];
      const QString label = QString("%1:%2").arg(ent.dim).arg(ent.tag);
      mesh_entity_->addItem(label, static_cast<int>(i));
    }
    int idx = mesh_entity_->findData(current);
    if (idx < 0) {
      idx = 0;
    }
    mesh_entity_->setCurrentIndex(idx);
    mesh_entity_->blockSignals(false);
  }

  if (mesh_type_) {
    const QVariant current_data = mesh_type_->currentData();
    const int current = current_data.isValid() ? current_data.toInt() : -1;
    mesh_type_->blockSignals(true);
    mesh_type_->clear();
    mesh_type_->addItem("All", -1);
    for (int t : mesh_elem_types_) {
      mesh_type_->addItem(ElementTypeLabel(t), t);
    }
    int idx = mesh_type_->findData(current);
    if (idx < 0) {
      idx = 0;
    }
    mesh_type_->setCurrentIndex(idx);
    mesh_type_->blockSignals(false);
  }

  if (mesh_dim_) {
    mesh_dim_->blockSignals(true);
    mesh_dim_->setCurrentIndex(0);
    mesh_dim_->blockSignals(false);
  }

  if (mesh_grid_) {
    mesh_grid_->GetBounds(mesh_bounds_);
    const double dx = mesh_bounds_[1] - mesh_bounds_[0];
    const double dy = mesh_bounds_[3] - mesh_bounds_[2];
    const double dz = mesh_bounds_[5] - mesh_bounds_[4];
    int axis = 0;
    double max_extent = dx;
    if (dy > max_extent) {
      axis = 1;
      max_extent = dy;
    }
    if (dz > max_extent) {
      axis = 2;
    }
    if (slice_axis_) {
      slice_axis_->setCurrentIndex(axis);
    }
    if (slice_slider_) {
      slice_slider_->setValue(50);
    }
  }

  if (mesh_legend_) {
    if (mesh_groups_.empty()) {
      mesh_legend_->setText("Groups: none");
    } else {
      QStringList labels;
      for (const auto& g : mesh_groups_) {
        const QString label =
            g.name.isEmpty() ? QString("%1:%2").arg(g.dim).arg(g.id)
                             : QString("%1:%2 %3").arg(g.dim).arg(g.id).arg(g.name);
        labels << label;
      }
      mesh_legend_->setText(QString("Groups: %1").arg(labels.join(", ")));
    }
  }
#endif
}

void VtkViewer::update_mesh_pipeline() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (mode_ != DataMode::Mesh) {
    return;
  }
  if (!mesh_grid_ || !mapper_) {
    return;
  }
  if (!mesh_geom_) {
    mesh_geom_ = vtkSmartPointer<vtkDataSetSurfaceFilter>::New();
  }
  if (!mesh_dim_threshold_) {
    mesh_dim_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!mesh_group_threshold_) {
    mesh_group_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!mesh_type_threshold_) {
    mesh_type_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!mesh_entity_dim_threshold_) {
    mesh_entity_dim_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!mesh_entity_tag_threshold_) {
    mesh_entity_tag_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!mesh_slice_plane_) {
    mesh_slice_plane_ = vtkSmartPointer<vtkPlane>::New();
  }
  if (!mesh_slice_cutter_) {
    mesh_slice_cutter_ = vtkSmartPointer<vtkCutter>::New();
  }
  if (!mesh_shrink_filter_) {
    mesh_shrink_filter_ = vtkSmartPointer<vtkShrinkFilter>::New();
  }

  if (!mesh_quality_ready_ && mesh_grid_->GetCellData()) {
    if (mesh_grid_->GetCellData()->HasArray("Quality")) {
      mesh_quality_ready_ = true;
    } else {
      auto quality = vtkSmartPointer<vtkMeshQuality>::New();
      quality->SetInputData(mesh_grid_);
      quality->Update();
      auto* arr = quality->GetOutput()->GetCellData()->GetArray("Quality");
      if (arr) {
        vtkSmartPointer<vtkDataArray> copy;
        copy.TakeReference(arr->NewInstance());
        copy->DeepCopy(arr);
        copy->SetName("Quality");
        mesh_grid_->GetCellData()->AddArray(copy);
        mesh_quality_ready_ = true;
      }
    }
  }

  int dim_filter = mesh_dim_ ? mesh_dim_->currentData().toInt() : -1;
  int group_index = mesh_group_ ? mesh_group_->currentData().toInt() : -1;
  int entity_index = mesh_entity_ ? mesh_entity_->currentData().toInt() : -1;
  const int type_filter = mesh_type_ ? mesh_type_->currentData().toInt() : -1;
  int group_dim = -1;
  int group_id = -1;
  int entity_dim = -1;
  int entity_tag = -1;
  if (group_index >= 0 &&
      group_index < static_cast<int>(mesh_groups_.size())) {
    const auto& g = mesh_groups_[group_index];
    group_dim = g.dim;
    group_id = g.id;
    dim_filter = g.dim;
    if (mesh_dim_) {
      const int dim_idx = mesh_dim_->findData(group_dim);
      if (dim_idx >= 0 && mesh_dim_->currentIndex() != dim_idx) {
        mesh_dim_->blockSignals(true);
        mesh_dim_->setCurrentIndex(dim_idx);
        mesh_dim_->blockSignals(false);
      }
    }
  }
  if (entity_index >= 0 &&
      entity_index < static_cast<int>(mesh_entities_.size())) {
    const auto& ent = mesh_entities_[entity_index];
    entity_dim = ent.dim;
    entity_tag = ent.tag;
    dim_filter = ent.dim;
    if (mesh_dim_) {
      const int dim_idx = mesh_dim_->findData(entity_dim);
      if (dim_idx >= 0 && mesh_dim_->currentIndex() != dim_idx) {
        mesh_dim_->blockSignals(true);
        mesh_dim_->setCurrentIndex(dim_idx);
        mesh_dim_->blockSignals(false);
      }
    }
  }
  if (group_index >= 0 && group_id >= 0) {
    selected_group_dim_ = group_dim;
    selected_group_id_ = group_id;
  } else if (mesh_group_ && mesh_group_->currentData().toInt() < 0) {
    selected_group_dim_ = -1;
    selected_group_id_ = -1;
  }
  if (entity_index >= 0 && entity_tag >= 0 && entity_dim >= 0) {
    selected_entity_dim_ = entity_dim;
    selected_entity_tag_ = entity_tag;
  } else if (mesh_entity_ && mesh_entity_->currentData().toInt() < 0) {
    selected_entity_dim_ = -1;
    selected_entity_tag_ = -1;
  }

  vtkAlgorithmOutput* current_port = nullptr;
  if (dim_filter >= 0) {
    mesh_dim_threshold_->SetInputData(mesh_grid_);
    mesh_dim_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "phys_dim");
    mesh_dim_threshold_->SetLowerThreshold(dim_filter);
    mesh_dim_threshold_->SetUpperThreshold(dim_filter);
    current_port = mesh_dim_threshold_->GetOutputPort();
  }

  if (group_index >= 0 && group_id >= 0) {
    if (current_port) {
      mesh_group_threshold_->SetInputConnection(current_port);
    } else {
      mesh_group_threshold_->SetInputData(mesh_grid_);
    }
    mesh_group_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "phys_id");
    mesh_group_threshold_->SetLowerThreshold(group_id);
    mesh_group_threshold_->SetUpperThreshold(group_id);
    current_port = mesh_group_threshold_->GetOutputPort();
  }

  if (entity_index >= 0 && entity_tag >= 0 && entity_dim >= 0) {
    if (current_port) {
      mesh_entity_dim_threshold_->SetInputConnection(current_port);
    } else {
      mesh_entity_dim_threshold_->SetInputData(mesh_grid_);
    }
    mesh_entity_dim_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "entity_dim");
    mesh_entity_dim_threshold_->SetLowerThreshold(entity_dim);
    mesh_entity_dim_threshold_->SetUpperThreshold(entity_dim);
    mesh_entity_tag_threshold_->SetInputConnection(
        mesh_entity_dim_threshold_->GetOutputPort());
    mesh_entity_tag_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "entity_tag");
    mesh_entity_tag_threshold_->SetLowerThreshold(entity_tag);
    mesh_entity_tag_threshold_->SetUpperThreshold(entity_tag);
    current_port = mesh_entity_tag_threshold_->GetOutputPort();
  }

  if (type_filter >= 0) {
    if (current_port) {
      mesh_type_threshold_->SetInputConnection(current_port);
    } else {
      mesh_type_threshold_->SetInputData(mesh_grid_);
    }
    mesh_type_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "elem_type");
    mesh_type_threshold_->SetLowerThreshold(type_filter);
    mesh_type_threshold_->SetUpperThreshold(type_filter);
    current_port = mesh_type_threshold_->GetOutputPort();
  }

  if (current_port) {
    mesh_geom_->SetInputConnection(current_port);
  } else {
    mesh_geom_->SetInputData(mesh_grid_);
  }

  const bool slice_on = slice_enable_ && slice_enable_->isChecked();
  const bool shell_on = show_shell_ ? show_shell_->isChecked() : true;
  if (slice_axis_) {
    slice_axis_->setEnabled(slice_on);
  }
  if (slice_slider_) {
    slice_slider_->setEnabled(slice_on);
  }
  vtkAlgorithmOutput* final_port = nullptr;
  if (slice_on) {
    mesh_grid_->GetBounds(mesh_bounds_);
    const int axis = slice_axis_ ? slice_axis_->currentIndex() : 0;
    const double minv = mesh_bounds_[2 * axis];
    const double maxv = mesh_bounds_[2 * axis + 1];
    const double range = maxv - minv;
    const bool has_range = range > 1e-12;
    if (slice_slider_) {
      slice_slider_->setEnabled(has_range);
    }
    const double t = slice_slider_ ? slice_slider_->value() / 100.0 : 0.5;
    const double pos = has_range ? minv + t * range : minv;
    double origin[3] = {(mesh_bounds_[0] + mesh_bounds_[1]) * 0.5,
                        (mesh_bounds_[2] + mesh_bounds_[3]) * 0.5,
                        (mesh_bounds_[4] + mesh_bounds_[5]) * 0.5};
    origin[axis] = pos;
    double normal[3] = {0.0, 0.0, 0.0};
    normal[axis] = 1.0;
    mesh_slice_plane_->SetOrigin(origin);
    mesh_slice_plane_->SetNormal(normal);
    mesh_slice_cutter_->SetCutFunction(mesh_slice_plane_);
    if (current_port) {
      mesh_slice_cutter_->SetInputConnection(current_port);
    } else {
      mesh_slice_cutter_->SetInputData(mesh_grid_);
    }
    final_port = mesh_slice_cutter_->GetOutputPort();
    mesh_slice_cutter_->Update();
  } else if (shell_on) {
    if (current_port) {
      mesh_geom_->SetInputConnection(current_port);
    } else {
      mesh_geom_->SetInputData(mesh_grid_);
    }
    final_port = mesh_geom_->GetOutputPort();
    mesh_geom_->Update();
  } else {
    if (current_port) {
      final_port = current_port;
    } else {
      final_port = nullptr;
    }
  }

  const double shrink = mesh_shrink_ ? mesh_shrink_->value() : 1.0;
  if (shrink < 0.999) {
    mesh_shrink_filter_->SetShrinkFactor(shrink);
    if (final_port) {
      mesh_shrink_filter_->SetInputConnection(final_port);
    } else if (mesh_grid_) {
      mesh_shrink_filter_->SetInputData(mesh_grid_);
    }
    mapper_->SetInputConnection(mesh_shrink_filter_->GetOutputPort());
  } else if (final_port) {
    mapper_->SetInputConnection(final_port);
  } else if (mesh_grid_) {
    mapper_->SetInputData(mesh_grid_);
  }

  update_nodes_visibility();
  apply_mesh_visuals();
  update_selection_pipeline();
  update_scene_extras();
  if (render_window_) {
    render_window_->Render();
  }
#endif
}

void VtkViewer::apply_mesh_visuals() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!actor_) {
    return;
  }
  if (mode_ != DataMode::Mesh) {
    return;
  }
  const bool faces = show_faces_ ? show_faces_->isChecked() : true;
  const bool edges = show_edges_ ? show_edges_->isChecked() : false;
  if (!faces && !edges) {
    actor_->SetVisibility(false);
  } else {
    actor_->SetVisibility(true);
    if (faces && edges) {
      actor_->GetProperty()->SetRepresentationToSurface();
      actor_->GetProperty()->SetEdgeVisibility(1);
    } else if (faces) {
      actor_->GetProperty()->SetRepresentationToSurface();
      actor_->GetProperty()->SetEdgeVisibility(0);
    } else {
      actor_->GetProperty()->SetRepresentationToWireframe();
      actor_->GetProperty()->SetEdgeVisibility(0);
    }
  }
  if (mesh_opacity_) {
    actor_->GetProperty()->SetOpacity(mesh_opacity_->value());
  }
  if (scalar_bar_) {
    const bool show_bar =
        !mesh_scalar_bar_ || mesh_scalar_bar_->isChecked();
    scalar_bar_->SetVisibility(show_bar ? 1 : 0);
  }
  if (render_window_) {
    render_window_->Render();
  }
#endif
}

void VtkViewer::update_nodes_visibility() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!renderer_ || !render_window_) {
    return;
  }
  const bool show =
      show_nodes_ && show_nodes_->isChecked() && mode_ == DataMode::Mesh;
  if (!show) {
    if (nodes_actor_) {
      nodes_actor_->SetVisibility(false);
      render_window_->Render();
    }
    return;
  }
  vtkAlgorithmOutput* port = nullptr;
  if (mesh_geom_) {
    port = mesh_geom_->GetOutputPort();
  } else if (mapper_ && mapper_->GetInputConnection(0, 0)) {
    port = mapper_->GetInputConnection(0, 0);
  }
  if (!port) {
    return;
  }
  if (!nodes_filter_) {
    nodes_filter_ = vtkSmartPointer<vtkVertexGlyphFilter>::New();
    nodes_mapper_ = vtkSmartPointer<vtkPolyDataMapper>::New();
    nodes_actor_ = vtkSmartPointer<vtkActor>::New();
    nodes_mapper_->SetInputConnection(nodes_filter_->GetOutputPort());
    nodes_actor_->SetMapper(nodes_mapper_);
    nodes_actor_->GetProperty()->SetPointSize(4);
    nodes_actor_->GetProperty()->SetColor(0.1, 0.1, 0.1);
    renderer_->AddActor(nodes_actor_);
  }
  nodes_filter_->SetInputConnection(port);
  nodes_filter_->Update();
  nodes_actor_->SetVisibility(true);
  render_window_->Render();
#endif
}

void VtkViewer::handle_pick(int x, int y) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!renderer_ || !mapper_) {
    return;
  }
  if (!picker_) {
    picker_ = vtkSmartPointer<vtkCellPicker>::New();
    picker_->SetTolerance(0.0005);
  }

  if (mode_ == DataMode::Mesh) {
    if (!pick_enable_ || !pick_enable_->isChecked()) {
      return;
    }
    if (!picker_->Pick(x, y, 0, renderer_)) {
      if (pick_info_) {
        pick_info_->setText("Pick: none");
      }
      return;
    }
    vtkIdType cell_id = picker_->GetCellId();
    if (cell_id < 0) {
      if (pick_info_) {
        pick_info_->setText("Pick: none");
      }
      return;
    }
    vtkDataSet* data = vtkDataSet::SafeDownCast(mapper_->GetInput());
    if (!data && mesh_geom_) {
      mesh_geom_->Update();
      data = mesh_geom_->GetOutput();
    }
    if (!data) {
      return;
    }
    auto* cd = data->GetCellData();
    int phys_id = -1;
    int phys_dim = -1;
    int elem_type = -1;
    int cell_index = -1;
    int ent_dim = -1;
    int ent_tag = -1;
    double quality = 0.0;
    if (cd) {
      if (auto* arr = vtkIntArray::SafeDownCast(cd->GetArray("phys_id"))) {
        if (cell_id < arr->GetNumberOfTuples()) {
          phys_id = arr->GetValue(cell_id);
        }
      }
      if (auto* arr = vtkIntArray::SafeDownCast(cd->GetArray("phys_dim"))) {
        if (cell_id < arr->GetNumberOfTuples()) {
          phys_dim = arr->GetValue(cell_id);
        }
      }
      if (auto* arr = vtkIntArray::SafeDownCast(cd->GetArray("elem_type"))) {
        if (cell_id < arr->GetNumberOfTuples()) {
          elem_type = arr->GetValue(cell_id);
        }
      }
      if (auto* arr = vtkIntArray::SafeDownCast(cd->GetArray("cell_id"))) {
        if (cell_id < arr->GetNumberOfTuples()) {
          cell_index = arr->GetValue(cell_id);
        }
      }
      if (auto* arr = vtkIntArray::SafeDownCast(cd->GetArray("entity_dim"))) {
        if (cell_id < arr->GetNumberOfTuples()) {
          ent_dim = arr->GetValue(cell_id);
        }
      }
      if (auto* arr = vtkIntArray::SafeDownCast(cd->GetArray("entity_tag"))) {
        if (cell_id < arr->GetNumberOfTuples()) {
          ent_tag = arr->GetValue(cell_id);
        }
      }
      if (auto* arr = cd->GetArray("Quality")) {
        if (cell_id < arr->GetNumberOfTuples()) {
          quality = arr->GetComponent(cell_id, 0);
        }
      }
    }

    QString group_name;
    if (phys_id >= 0 && phys_dim >= 0) {
      for (const auto& g : mesh_groups_) {
        if (g.dim == phys_dim && g.id == phys_id) {
          group_name = g.name;
          break;
        }
      }
    }
    const QString group_label =
        group_name.isEmpty()
            ? QString("%1:%2").arg(phys_dim).arg(phys_id)
            : QString("%1:%2 %3").arg(phys_dim).arg(phys_id).arg(group_name);
    const QString ent_label =
        (ent_dim >= 0 && ent_tag >= 0)
            ? QString("%1:%2").arg(ent_dim).arg(ent_tag)
            : QString("n/a");
    if (pick_info_) {
      pick_info_->setText(
          QString("Pick: cell=%1 group=%2 entity=%3 type=%4 q=%5")
              .arg(cell_index >= 0 ? cell_index : cell_id)
              .arg(group_label)
              .arg(ent_label)
              .arg(elem_type)
              .arg(quality, 0, 'g', 4));
    }
    if (phys_dim >= 0 && phys_id >= 0) {
      emit mesh_group_picked(phys_dim, phys_id);
    }
    const int mode = pick_mode_ ? pick_mode_->currentData().toInt() : 0;
    if (mode == 2) {
      selected_cell_id_ =
          cell_index >= 0 ? cell_index : static_cast<int>(cell_id);
      selected_group_dim_ = phys_dim;
      selected_group_id_ = phys_id;
      selected_entity_dim_ = ent_dim;
      selected_entity_tag_ = ent_tag;
    } else if (mode == 1) {
      selected_entity_dim_ = ent_dim;
      selected_entity_tag_ = ent_tag;
      selected_group_dim_ = phys_dim;
      selected_group_id_ = phys_id;
      selected_cell_id_ = -1;
      if (ent_dim >= 0 && ent_tag >= 0) {
        emit mesh_entity_picked(ent_dim, ent_tag);
      }
    } else {
      selected_group_dim_ = phys_dim;
      selected_group_id_ = phys_id;
      selected_entity_dim_ = ent_dim;
      selected_entity_tag_ = ent_tag;
      selected_cell_id_ = -1;
    }
    update_selection_pipeline();
    return;
  }

  if (mode_ != DataMode::Exodus) {
    return;
  }
  if (!probe_enable_ || !probe_enable_->isChecked()) {
    return;
  }
  if (!picker_->Pick(x, y, 0, renderer_)) {
    if (probe_info_) {
      probe_info_->setText("Probe: none");
    }
    return;
  }

  vtkDataSet* data = vtkDataSet::SafeDownCast(mapper_->GetInput());
  if (!data && geom_) {
    geom_->Update();
    data = geom_->GetOutput();
  }
  if (!data) {
    return;
  }

  const int probe_mode = probe_mode_ ? probe_mode_->currentData().toInt() : 0;
  const bool want_point = (probe_mode == 0);
  vtkIdType cell_id = picker_->GetCellId();
  vtkIdType point_id = picker_->GetPointId();
  double pos[3] = {0.0, 0.0, 0.0};
  picker_->GetPickPosition(pos);

  if (want_point && point_id < 0) {
    point_id = data->FindPoint(pos);
  }
  if (want_point && point_id < 0) {
    if (probe_info_) {
      probe_info_->setText("Probe: none");
    }
    return;
  }
  if (!want_point && cell_id < 0) {
    if (probe_info_) {
      probe_info_->setText("Probe: none");
    }
    return;
  }

  vtkDataArray* array = nullptr;
  QString array_name;
  const QString key =
      array_combo_ ? array_combo_->currentData().toString() : "";

  if (want_point) {
    auto* pd = data->GetPointData();
    if (pd) {
      if (key.startsWith("P:")) {
        array_name = key.mid(2);
        array = pd->GetArray(array_name.toUtf8().constData());
      }
      if (!array && pd->GetNumberOfArrays() > 0) {
        array = pd->GetArray(0);
        if (array && array->GetName()) {
          array_name = QString::fromUtf8(array->GetName());
        }
      }
    }
  } else {
    auto* cd = data->GetCellData();
    if (cd) {
      if (key.startsWith("C:")) {
        array_name = key.mid(2);
        array = cd->GetArray(array_name.toUtf8().constData());
      }
      if (!array && cd->GetNumberOfArrays() > 0) {
        array = cd->GetArray(0);
        if (array && array->GetName()) {
          array_name = QString::fromUtf8(array->GetName());
        }
      }
    }
  }

  auto format_value = [](vtkDataArray* arr, vtkIdType id) -> QString {
    if (!arr || id < 0 || id >= arr->GetNumberOfTuples()) {
      return "n/a";
    }
    const int comps = arr->GetNumberOfComponents();
    if (comps <= 1) {
      return QString::number(arr->GetComponent(id, 0), 'g', 6);
    }
    const int show = std::min(comps, 3);
    QStringList parts;
    for (int i = 0; i < show; ++i) {
      parts << QString::number(arr->GetComponent(id, i), 'g', 6);
    }
    const QString suffix = comps > 3 ? ", ..." : "";
    return QString("(%1%2)").arg(parts.join(", ")).arg(suffix);
  };

  const vtkIdType id = want_point ? point_id : cell_id;
  const QString value = format_value(array, id);
  const QString mode_label = want_point ? "Point" : "Cell";
  const QString label = array_name.isEmpty() ? "value" : array_name;
  if (probe_info_) {
    probe_info_->setText(QString("Probe (%1): id=%2 pos=(%3, %4, %5) %6=%7")
                             .arg(mode_label)
                             .arg(id)
                             .arg(pos[0], 0, 'g', 6)
                             .arg(pos[1], 0, 'g', 6)
                             .arg(pos[2], 0, 'g', 6)
                             .arg(label)
                             .arg(value));
  }
#endif
}


void VtkViewer::update_selection_pipeline() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!renderer_ || !mesh_grid_) {
    return;
  }
  if (!pick_enable_ || !pick_enable_->isChecked()) {
    if (mesh_select_actor_) {
      mesh_select_actor_->SetVisibility(false);
    }
    return;
  }
  if (mode_ != DataMode::Mesh) {
    if (mesh_select_actor_) {
      mesh_select_actor_->SetVisibility(false);
    }
    return;
  }

  const int mode = pick_mode_ ? pick_mode_->currentData().toInt() : 0;
  if (mode == 2 && selected_cell_id_ < 0) {
    if (mesh_select_actor_) {
      mesh_select_actor_->SetVisibility(false);
    }
    return;
  }
  if (mode == 1 && (selected_entity_tag_ < 0 || selected_entity_dim_ < 0)) {
    if (mesh_select_actor_) {
      mesh_select_actor_->SetVisibility(false);
    }
    return;
  }
  if (mode == 0 && (selected_group_id_ < 0 || selected_group_dim_ < 0)) {
    if (mesh_select_actor_) {
      mesh_select_actor_->SetVisibility(false);
    }
    return;
  }

  if (!mesh_select_dim_threshold_) {
    mesh_select_dim_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!mesh_select_group_threshold_) {
    mesh_select_group_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!mesh_select_cell_threshold_) {
    mesh_select_cell_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!mesh_select_entity_dim_threshold_) {
    mesh_select_entity_dim_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!mesh_select_entity_tag_threshold_) {
    mesh_select_entity_tag_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!mesh_select_geom_) {
    mesh_select_geom_ = vtkSmartPointer<vtkDataSetSurfaceFilter>::New();
  }
  if (!mesh_select_mapper_) {
    mesh_select_mapper_ = vtkSmartPointer<vtkDataSetMapper>::New();
  }
  if (!mesh_select_actor_) {
    mesh_select_actor_ = vtkSmartPointer<vtkActor>::New();
    mesh_select_actor_->SetMapper(mesh_select_mapper_);
    mesh_select_actor_->GetProperty()->SetColor(1.0, 0.9, 0.2);
    mesh_select_actor_->GetProperty()->SetLineWidth(2.0);
    mesh_select_actor_->GetProperty()->SetEdgeVisibility(1);
    mesh_select_actor_->GetProperty()->SetRepresentationToSurface();
    renderer_->AddActor(mesh_select_actor_);
  }

  vtkAlgorithmOutput* current = nullptr;
  if (mode == 2) {
    mesh_select_cell_threshold_->SetInputData(mesh_grid_);
    mesh_select_cell_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "cell_id");
    mesh_select_cell_threshold_->SetLowerThreshold(selected_cell_id_);
    mesh_select_cell_threshold_->SetUpperThreshold(selected_cell_id_);
    current = mesh_select_cell_threshold_->GetOutputPort();
  } else if (mode == 1) {
    mesh_select_entity_dim_threshold_->SetInputData(mesh_grid_);
    mesh_select_entity_dim_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "entity_dim");
    mesh_select_entity_dim_threshold_->SetLowerThreshold(selected_entity_dim_);
    mesh_select_entity_dim_threshold_->SetUpperThreshold(selected_entity_dim_);
    mesh_select_entity_tag_threshold_->SetInputConnection(
        mesh_select_entity_dim_threshold_->GetOutputPort());
    mesh_select_entity_tag_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "entity_tag");
    mesh_select_entity_tag_threshold_->SetLowerThreshold(selected_entity_tag_);
    mesh_select_entity_tag_threshold_->SetUpperThreshold(selected_entity_tag_);
    current = mesh_select_entity_tag_threshold_->GetOutputPort();
  } else {
    mesh_select_dim_threshold_->SetInputData(mesh_grid_);
    mesh_select_dim_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "phys_dim");
    mesh_select_dim_threshold_->SetLowerThreshold(selected_group_dim_);
    mesh_select_dim_threshold_->SetUpperThreshold(selected_group_dim_);
    mesh_select_group_threshold_->SetInputConnection(
        mesh_select_dim_threshold_->GetOutputPort());
    mesh_select_group_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "phys_id");
    mesh_select_group_threshold_->SetLowerThreshold(selected_group_id_);
    mesh_select_group_threshold_->SetUpperThreshold(selected_group_id_);
    current = mesh_select_group_threshold_->GetOutputPort();
  }

  mesh_select_geom_->SetInputConnection(current);
  mesh_select_mapper_->SetInputConnection(mesh_select_geom_->GetOutputPort());
  mesh_select_actor_->SetVisibility(true);
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
  const double cx = (bounds[0] + bounds[1]) * 0.5;
  const double cy = (bounds[2] + bounds[3]) * 0.5;
  const double cz = (bounds[4] + bounds[5]) * 0.5;
  const double dx = bounds[1] - bounds[0];
  const double dy = bounds[3] - bounds[2];
  const double dz = bounds[5] - bounds[4];
  const double max_extent = std::max({dx, dy, dz, 1.0});
  const double dist = max_extent * 2.5;
  auto* cam = renderer_->GetActiveCamera();
  if (!cam) {
    return;
  }
  if (preset == 0) {
    renderer_->ResetCamera();
    render_window_->Render();
    return;
  }
  if (preset == 1) {  // Front (+X)
    cam->SetPosition(cx + dist, cy, cz);
    cam->SetViewUp(0, 0, 1);
  } else if (preset == 2) {  // Right (+Y)
    cam->SetPosition(cx, cy + dist, cz);
    cam->SetViewUp(0, 0, 1);
  } else if (preset == 3) {  // Top (+Z)
    cam->SetPosition(cx, cy, cz + dist);
    cam->SetViewUp(0, 1, 0);
  } else {  // Iso
    cam->SetPosition(cx + dist, cy + dist, cz + dist);
    cam->SetViewUp(0, 0, 1);
  }
  cam->SetFocalPoint(cx, cy, cz);
  renderer_->ResetCameraClippingRange();
  render_window_->Render();
#else
  Q_UNUSED(preset);
#endif
}

}  // namespace gmp
