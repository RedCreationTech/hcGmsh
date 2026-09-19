#pragma once

// v0.2 Stage 5 hc_ui 过渡层：VtkViewer 与三个视口共享的文件级内部件
//（交互样式、RAII 模型读取、数组/格式化辅助）。仅供 src/ 内实现文件
// 包含；匿名命名空间项在各 TU 内私有，行为与合并前逐字一致。

#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include "gmp/ComboPopupFix.h"

#ifdef GMP_ENABLE_VTK_VIEWER
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCompositeDataGeometryFilter.h>
#include <vtkDataArray.h>
#include <vtkDataSet.h>
#include <vtkFieldData.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkIntArray.h>
#include <vtkInteractorStyleImage.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkMultiBlockDataSetAlgorithm.h>
#include <vtkObject.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkScalarBarActor.h>
#include <vtkSmartPointer.h>
#include <vtkTextProperty.h>
#include <vtkUnstructuredGrid.h>
#endif
#ifdef GMP_ENABLE_GMSH_GUI
#include <gmsh.h>
#endif

namespace gmp {

namespace {

class StageInteractorStyle : public vtkInteractorStyleTrackballCamera {
 public:
  static StageInteractorStyle* New();
  vtkTypeMacro(StageInteractorStyle, vtkInteractorStyleTrackballCamera);

  void SetStageMode(int mode) { mode_ = std::clamp(mode, 0, 2); }

  void OnLeftButtonDown() override {
    if (mode_ == 1) {
      Superclass::OnMiddleButtonDown();
    } else if (mode_ == 2) {
      Superclass::OnRightButtonDown();
    } else {
      Superclass::OnLeftButtonDown();
    }
  }

  void OnLeftButtonUp() override {
    if (mode_ == 1) {
      Superclass::OnMiddleButtonUp();
    } else if (mode_ == 2) {
      Superclass::OnRightButtonUp();
    } else {
      Superclass::OnLeftButtonUp();
    }
  }

 private:
  int mode_ = 0;
};

vtkStandardNewMacro(StageInteractorStyle);

// vtkInteractorStyleImage 默认把左键拖动解释为窗宽/窗位，不能直接作为
// CAD 草图导航。这里复用其成熟的中键平移、右键缩放实现，并按舞台按钮把
// 左键事件映射过去；选择/绘制模式仍由 VtkViewer 的草图回调接管。
class SketchInteractorStyle : public vtkInteractorStyleImage {
 public:
  static SketchInteractorStyle* New();
  vtkTypeMacro(SketchInteractorStyle, vtkInteractorStyleImage);

  void SetStageMode(int mode) { mode_ = std::clamp(mode, 0, 2); }

  void OnLeftButtonDown() override {
    if (mode_ == 1) {
      Superclass::OnMiddleButtonDown();
    } else if (mode_ == 2) {
      Superclass::OnRightButtonDown();
    } else {
      Superclass::OnLeftButtonDown();
    }
  }

  void OnLeftButtonUp() override {
    if (mode_ == 1) {
      Superclass::OnMiddleButtonUp();
    } else if (mode_ == 2) {
      Superclass::OnRightButtonUp();
    } else {
      Superclass::OnLeftButtonUp();
    }
  }

 private:
  int mode_ = 0;
};

vtkStandardNewMacro(SketchInteractorStyle);

// 统一约束色标条样式：尺寸按视口比例自适应（随黑色显示区缩放），
// 必须先关闭 VTK 默认的约束字体模式，否则它会自动放大字体填满色标条。
void style_scalar_bar(vtkScalarBarActor* bar) {
  if (!bar) {
    return;
  }
  bar->SetUnconstrainedFontSize(1);
  // 宽度 0.08 时指数形式刻度（如 -2.00e+07）会被裁掉指数位；
  // 改用紧凑格式（-2e+07）并略加宽。
  bar->SetLabelFormat("%.3g");
  bar->SetWidth(0.10);
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

// Exodus 按 element block 分块输出，MOOSE 只把部分变量（如 ASR_*）写在
// 定义了对应材料的块上；vtkCompositeDataGeometryFilter 扁平化多块数据时只保留
// 所有块共有的数组，导致这些"部分块变量"整体丢失。该过滤器在扁平化前把每个块
// 缺失的点/单元数组用 0 填充补齐，保证变量列表与云图数据完整。
class vtkPadPartialBlockArrays : public vtkMultiBlockDataSetAlgorithm {
 public:
  static vtkPadPartialBlockArrays* New();
  vtkTypeMacro(vtkPadPartialBlockArrays, vtkMultiBlockDataSetAlgorithm);

 protected:
  vtkPadPartialBlockArrays() = default;

  int RequestData(vtkInformation*, vtkInformationVector** input_vector,
                  vtkInformationVector* output_vector) override {
    auto* input = vtkMultiBlockDataSet::GetData(input_vector[0], 0);
    auto* output = vtkMultiBlockDataSet::GetData(output_vector, 0);
    if (!input || !output) {
      return 0;
    }
    output->CopyStructure(input);

    struct ArrayProto {
      std::string name;
      int data_type = VTK_DOUBLE;
      int components = 1;
    };
    auto collect = [](vtkFieldData* fd, std::vector<ArrayProto>& protos) {
      for (int i = 0; i < fd->GetNumberOfArrays(); ++i) {
        vtkDataArray* arr = fd->GetArray(i);
        if (!arr || !arr->GetName()) {
          continue;
        }
        const std::string name = arr->GetName();
        bool known = false;
        for (const auto& p : protos) {
          known = known || p.name == name;
        }
        if (!known) {
          protos.push_back({name, arr->GetDataType(),
                            arr->GetNumberOfComponents()});
        }
      }
    };

    // 第一遍：收集所有叶子块点/单元数组的并集原型。
    std::vector<ArrayProto> point_protos;
    std::vector<ArrayProto> cell_protos;
    auto* scan = input->NewIterator();
    for (scan->InitTraversal(); !scan->IsDoneWithTraversal();
         scan->GoToNextItem()) {
      auto* ds = vtkDataSet::SafeDownCast(scan->GetCurrentDataObject());
      if (!ds) {
        continue;
      }
      collect(ds->GetPointData(), point_protos);
      collect(ds->GetCellData(), cell_protos);
    }
    scan->Delete();

    // 第二遍：浅拷贝叶子块（拥有自己的字段列表），缺失数组补 0。
    auto pad = [](vtkFieldData* fd, vtkIdType tuples,
                  const std::vector<ArrayProto>& protos) {
      for (const auto& p : protos) {
        if (fd->GetArray(p.name.c_str())) {
          continue;
        }
        auto* arr = vtkDataArray::CreateDataArray(p.data_type);
        arr->SetNumberOfComponents(p.components);
        arr->SetNumberOfTuples(tuples);
        arr->Fill(0.0);
        arr->SetName(p.name.c_str());
        fd->AddArray(arr);
        arr->Delete();
      }
    };
    auto* in_it = input->NewIterator();
    auto* out_it = output->NewIterator();
    // 关闭空节点跳过：CopyStructure 后输出叶子暂为空，且两个迭代器必须
    // 按相同顺序对齐遍历（输入的空叶子保持为空即可）。
    in_it->SkipEmptyNodesOff();
    out_it->SkipEmptyNodesOff();
    for (in_it->InitTraversal(), out_it->InitTraversal();
         !in_it->IsDoneWithTraversal() && !out_it->IsDoneWithTraversal();
         in_it->GoToNextItem(), out_it->GoToNextItem()) {
      auto* ds = vtkDataSet::SafeDownCast(in_it->GetCurrentDataObject());
      if (!ds) {
        continue;
      }
      auto* copy = static_cast<vtkDataSet*>(ds->NewInstance());
      copy->ShallowCopy(ds);
      pad(copy->GetPointData(), copy->GetNumberOfPoints(), point_protos);
      pad(copy->GetCellData(), copy->GetNumberOfCells(), cell_protos);
      output->SetDataSet(out_it, copy);
      copy->Delete();
    }
    in_it->Delete();
    out_it->Delete();
    return 1;
  }
};
vtkStandardNewMacro(vtkPadPartialBlockArrays);

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
// VtkViewer 与建模/网格面板共享同一个进程内 Gmsh 会话。读取预览文件时
// 不能 gmsh::clear()：它会把当前 OCC 几何、物理组和网格场一并销毁。
// 打开文件前先快照 current model；读取完成后优先切回仍存在的原 model，
// 若该 Gmsh 版本的 open() 替换了原 model，则从快照恢复。
class ScopedGmshFileModel {
 public:
  explicit ScopedGmshFileModel(const QString& path) {
    if (!gmsh::isInitialized()) {
      gmsh::initialize(0, nullptr, false, false);
    }
    gmsh::option::setNumber("General.Terminal", 0);

    std::vector<std::string> names;
    gmsh::model::list(names);
    gmsh::model::getCurrent(previous_model_);
    has_previous_model_ =
        std::find(names.begin(), names.end(), previous_model_) != names.end();

    if (has_previous_model_) {
      restore_dir_ = std::make_unique<QTemporaryDir>(
          QDir::tempPath() + "/gmp_vtk_model_restore_XXXXXX");
      if (restore_dir_->isValid()) {
        restore_path_ = restore_dir_->filePath("current_model.geo_unrolled");
        try {
          gmsh::write(restore_path_.toStdString());
        } catch (...) {
          restore_path_.clear();
        }
      }
    }

    try {
      gmsh::open(path.toStdString());
      file_model_opened_ = true;
    } catch (...) {
      restore();
      throw;
    }
  }

  ScopedGmshFileModel(const ScopedGmshFileModel&) = delete;
  ScopedGmshFileModel& operator=(const ScopedGmshFileModel&) = delete;

  ~ScopedGmshFileModel() { restore(); }

 private:
  void restore() noexcept {
    if (file_model_opened_) {
      try {
        gmsh::model::remove();
      } catch (...) {
      }
      file_model_opened_ = false;
    }
    if (has_previous_model_) {
      try {
        std::vector<std::string> names;
        gmsh::model::list(names);
        if (std::find(names.begin(), names.end(), previous_model_) !=
            names.end()) {
          gmsh::model::setCurrent(previous_model_);
          return;
        }
      } catch (...) {
      }
    }
    if (!restore_path_.isEmpty()) {
      try {
        gmsh::open(restore_path_.toStdString());
      } catch (...) {
      }
    }
  }

  std::string previous_model_;
  QString restore_path_;
  std::unique_ptr<QTemporaryDir> restore_dir_;
  bool has_previous_model_ = false;
  bool file_model_opened_ = false;
};

vtkSmartPointer<vtkUnstructuredGrid> BuildGridFromCurrentGmshModel() {
  try {
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

// 点和边可能存在于当前 OCC 模型，却因为 .msh 仅保存 Physical Groups
// 而没有进入 VTK 网格。实体拾取预览因此不能只依赖 mesh_grid_ 的
// entity_dim/entity_tag 阈值；这里直接从当前 Gmsh 几何构造独立 PolyData。
vtkSmartPointer<vtkPolyData> BuildGmshEntityPreviewGeometry(int dim, int tag) {
  if (dim < 0 || dim > 1 || tag < 0) {
    return nullptr;
  }
  try {
    auto poly = vtkSmartPointer<vtkPolyData>::New();
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto cells = vtkSmartPointer<vtkCellArray>::New();
    if (dim == 0) {
      double xmin = 0.0, ymin = 0.0, zmin = 0.0;
      double xmax = 0.0, ymax = 0.0, zmax = 0.0;
      gmsh::model::getBoundingBox(dim, tag, xmin, ymin, zmin, xmax, ymax,
                                  zmax);
      points->InsertNextPoint(0.5 * (xmin + xmax), 0.5 * (ymin + ymax),
                              0.5 * (zmin + zmax));
      cells->InsertNextCell(1);
      cells->InsertCellPoint(0);
      poly->SetPoints(points);
      poly->SetVerts(cells);
      return poly;
    }

    std::vector<double> param_min;
    std::vector<double> param_max;
    gmsh::model::getParametrizationBounds(dim, tag, param_min, param_max);
    if (param_min.empty() || param_max.empty() ||
        !std::isfinite(param_min[0]) || !std::isfinite(param_max[0])) {
      return nullptr;
    }
    constexpr int kCurveSamples = 96;
    std::vector<double> parameters;
    parameters.reserve(kCurveSamples);
    for (int i = 0; i < kCurveSamples; ++i) {
      const double ratio = static_cast<double>(i) / (kCurveSamples - 1);
      parameters.push_back(param_min[0] + ratio * (param_max[0] - param_min[0]));
    }
    std::vector<double> coordinates;
    gmsh::model::getValue(dim, tag, parameters, coordinates);
    if (coordinates.size() < static_cast<size_t>(kCurveSamples * 3)) {
      return nullptr;
    }
    for (int i = 0; i < kCurveSamples; ++i) {
      points->InsertNextPoint(coordinates[3 * i], coordinates[3 * i + 1],
                              coordinates[3 * i + 2]);
    }
    cells->InsertNextCell(kCurveSamples);
    for (int i = 0; i < kCurveSamples; ++i) {
      cells->InsertCellPoint(i);
    }
    poly->SetPoints(points);
    poly->SetLines(cells);
    return poly;
  } catch (...) {
    return nullptr;
  }
}
#endif

void AttachComboPopupFix(QComboBox* combo) {
  gmp::install_combo_popup_fix(combo);
}


}  // namespace

}  // namespace gmp
