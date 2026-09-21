#include "gmp/MeshViewport.h"

#include "gmp/VtkViewer.h"
#include "ViewportInternal.h"
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
#include <gmsh.h>

namespace gmp {

bool MeshViewport::stage_data_visible() const {
#ifdef GMP_ENABLE_VTK_VIEWER
  return (host_->actor_ && host_->actor_->GetVisibility() != 0) ||
         (host_->nodes_actor_ && host_->nodes_actor_->GetVisibility() != 0) ||
         (host_->outline_actor_ && host_->outline_actor_->GetVisibility() != 0) ||
         (host_->mesh_select_actor_ && host_->mesh_select_actor_->GetVisibility() != 0) ||
         (host_->scalar_bar_ && host_->scalar_bar_->GetVisibility() != 0);
#else
  return false;
#endif
}


int MeshViewport::visible_mesh_entity_count(int dim) const {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (host_->mode_ != VtkViewer::DataMode::Mesh || !host_->mesh_geom_) {
    return 0;
  }
  host_->mesh_geom_->Update();
  auto* visible = host_->mesh_geom_->GetOutput();
  auto* cells = visible ? visible->GetCellData() : nullptr;
  auto* dims = cells
                   ? vtkIntArray::SafeDownCast(cells->GetArray("entity_dim"))
                   : nullptr;
  auto* tags = cells
                   ? vtkIntArray::SafeDownCast(cells->GetArray("entity_tag"))
                   : nullptr;
  if (!dims || !tags) {
    return 0;
  }
  std::vector<int> unique_tags;
  const vtkIdType count = std::min(dims->GetNumberOfTuples(),
                                   tags->GetNumberOfTuples());
  for (vtkIdType cell = 0; cell < count; ++cell) {
    if (dims->GetValue(cell) == dim) {
      unique_tags.push_back(tags->GetValue(cell));
    }
  }
  std::sort(unique_tags.begin(), unique_tags.end());
  unique_tags.erase(std::unique(unique_tags.begin(), unique_tags.end()),
                    unique_tags.end());
  return static_cast<int>(unique_tags.size());
#else
  Q_UNUSED(dim);
  return 0;
#endif
}


int MeshViewport::current_mesh_dimension() const {
  return host_->mesh_dim_ ? host_->mesh_dim_->currentData().toInt() : -1;
}


void MeshViewport::set_mesh_file(const QString& path) {
  host_->set_mesh_file_impl(path, false);
}


void MeshViewport::set_mesh_file_from_current_model(const QString& path) {
  host_->set_mesh_file_impl(path, true);
}


void MeshViewport::set_mesh_file_impl(const QString& path,
                                   bool use_current_gmsh_model) {
  host_->current_file_ = path;
  if (!host_->file_label_) {
    return;
  }
  host_->file_label_->setText(path.isEmpty() ? "No file loaded" : path);
#ifdef GMP_ENABLE_VTK_VIEWER
  if (path.isEmpty()) {
    return;
  }
  if (!host_->renderer_) {
    return;
  }
  if (!host_->mapper_) {
    host_->mapper_ = vtkSmartPointer<vtkDataSetMapper>::New();
    host_->actor_ = vtkSmartPointer<vtkActor>::New();
    host_->lut_ = vtkSmartPointer<vtkLookupTable>::New();
    host_->lut_->SetNumberOfTableValues(256);
    host_->lut_->Build();
    host_->mapper_->SetLookupTable(host_->lut_);
    host_->scalar_bar_ = vtkSmartPointer<vtkScalarBarActor>::New();
    style_scalar_bar(host_->scalar_bar_);
    position_scalar_bar(host_->scalar_bar_, host_->scalar_bar_pos_);
  }
  if (!host_->mesh_geom_) {
    host_->mesh_geom_ = vtkSmartPointer<vtkDataSetSurfaceFilter>::New();
  }

#ifdef GMP_ENABLE_GMSH_GUI
  host_->mesh_quality_ready_ = false;
  host_->mesh_groups_.clear();
  host_->mesh_elem_types_.clear();
  host_->mesh_entities_.clear();
  try {
    std::unique_ptr<ScopedGmshFileModel> file_model;
    if (!use_current_gmsh_model) {
      file_model = std::make_unique<ScopedGmshFileModel>(path);
    }
    host_->mesh_grid_ = BuildGridFromCurrentGmshModel();
    if (!host_->mesh_grid_) {
      host_->file_label_->setText("Failed to load mesh");
      return;
    }
    host_->mesh_grid_->GetBounds(host_->mesh_bounds_);

    std::vector<std::pair<int, int>> groups;
    gmsh::model::getPhysicalGroups(groups);
    for (const auto& pg : groups) {
      std::string name;
      gmsh::model::getPhysicalName(pg.first, pg.second, name);
      host_->mesh_groups_.push_back(
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
      host_->mesh_elem_types_.push_back(t);
    }

    std::vector<std::pair<int, int>> entities;
    gmsh::model::getEntities(entities);
    std::sort(entities.begin(), entities.end());
    for (const auto& ent : entities) {
      host_->mesh_entities_.push_back({ent.first, ent.second});
    }
  } catch (...) {
    host_->file_label_->setText("Failed to load mesh");
    return;
  }
  host_->mesh_geom_->SetInputData(host_->mesh_grid_);
#else
  Q_UNUSED(use_current_gmsh_model);
  host_->file_label_->setText("Mesh preview requires libgmsh");
  return;
#endif

  host_->mapper_->SetInputConnection(host_->mesh_geom_->GetOutputPort());
  host_->actor_->SetMapper(host_->mapper_);
  if (host_->renderer_ && !host_->actor_added_) {
    host_->renderer_->AddActor(host_->actor_);
    host_->scalar_bar_->SetLookupTable(host_->mapper_->GetLookupTable());
    host_->renderer_->AddViewProp(host_->scalar_bar_);
    host_->actor_added_ = true;
  }
  host_->pipeline_ready_ = true;
  host_->first_render_ = true;
  host_->mode_ = VtkViewer::DataMode::Mesh;
  // A newly loaded mesh must not inherit entity/type filters from the
  // previously displayed Part.  The indices refer to different Gmsh models
  // and can otherwise hide an otherwise valid assembly preview.
  host_->selection_.group_dim_ = -1;
  host_->selection_.group_id_ = -1;
  host_->selection_.entity_dim_ = -1;
  host_->selection_.entity_tag_ = -1;
  host_->selection_.preview_dim_ = -1;
  host_->selection_.preview_tag_ = -1;
  host_->preview_visual_active_ = false;
  if (host_->mesh_entity_) {
    host_->mesh_entity_->blockSignals(true);
    host_->mesh_entity_->setCurrentIndex(0);
    host_->mesh_entity_->blockSignals(false);
  }
  if (host_->mesh_type_) {
    host_->mesh_type_->blockSignals(true);
    host_->mesh_type_->setCurrentIndex(0);
    host_->mesh_type_->blockSignals(false);
  }
  host_->time_steps_.clear();
  host_->time_slider_->setEnabled(false);
  host_->time_slider_->setRange(0, 0);
  host_->time_label_->setText("t=0");
  if (host_->repr_combo_ && host_->repr_combo_->currentIndex() == 0) {
    host_->repr_combo_->setCurrentIndex(2);
  }
  host_->update_mesh_controls();

  host_->setup_watcher(path);
  host_->update_pipeline();
  host_->update_nodes_visibility();
#else
  Q_UNUSED(path);
  Q_UNUSED(use_current_gmsh_model);
#endif
}


void MeshViewport::set_mesh_group_filter(int dim, int tag) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!host_->mesh_group_ || host_->mesh_group_->count() == 0) {
    return;
  }
  const bool clear_filter = dim < 0 || tag < 0;
  host_->selection_.set_group_filter(dim, tag);
  int target_index = 0;
  if (!clear_filter) {
    for (size_t i = 0; i < host_->mesh_groups_.size(); ++i) {
      const auto& g = host_->mesh_groups_[i];
      if (g.dim == dim && g.id == tag) {
        const int idx = host_->mesh_group_->findData(static_cast<int>(i));
        if (idx >= 0) {
          target_index = idx;
        }
        break;
      }
    }
  }
  host_->mesh_group_->blockSignals(true);
  host_->mesh_group_->setCurrentIndex(target_index);
  host_->mesh_group_->blockSignals(false);

  if (clear_filter) {
    // 物理组筛选会把维度下拉框同步到该组的维度。仅把 Group 切回 All
    // 会因此停留在 2D，并把 contact/fixed/load 等边界组按 phys_id
    // 覆盖显示为红绿拼色。清除筛选时同时回到网格最高维，并清掉独立的
    // entity/cell 选择，恢复进入筛选前的完整实体视图。
    host_->selection_.group_dim_ = -1;
    host_->selection_.group_id_ = -1;
    host_->selection_.entity_dim_ = -1;
    host_->selection_.entity_tag_ = -1;
    host_->selection_.cell_id_ = -1;
    if (host_->mesh_entity_) {
      const int all_entities = host_->mesh_entity_->findData(-1);
      if (all_entities >= 0) {
        host_->mesh_entity_->blockSignals(true);
        host_->mesh_entity_->setCurrentIndex(all_entities);
        host_->mesh_entity_->blockSignals(false);
      }
    }
    int highest_dim = -1;
    // 以当前网格实际包含的单元为准。装配轻量预览虽保留 3D 几何实体，
    // 但只生成 2D 单元；若按几何实体维度恢复会把舞台过滤成空白。
    if (host_->mesh_grid_ && host_->mesh_grid_->GetCellData()) {
      if (auto* dims = vtkIntArray::SafeDownCast(
              host_->mesh_grid_->GetCellData()->GetArray("phys_dim"))) {
        for (vtkIdType cell = 0; cell < dims->GetNumberOfTuples(); ++cell) {
          highest_dim = std::max(highest_dim, dims->GetValue(cell));
        }
      }
    }
    if (highest_dim < 0) {
      for (const auto& entity : host_->mesh_entities_) {
        highest_dim = std::max(highest_dim, entity.dim);
      }
    }
    if (host_->mesh_dim_) {
      const int highest_index = host_->mesh_dim_->findData(highest_dim);
      if (highest_index >= 0) {
        host_->mesh_dim_->blockSignals(true);
        host_->mesh_dim_->setCurrentIndex(highest_index);
        host_->mesh_dim_->blockSignals(false);
      }
    }
  }
  host_->update_mesh_pipeline();
  host_->update_selection_pipeline();
#else
  Q_UNUSED(dim);
  Q_UNUSED(tag);
#endif
}


void MeshViewport::set_mesh_entity_filter(int dim, int tag) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!host_->mesh_entity_ || host_->mesh_entity_->count() == 0) {
    return;
  }
  host_->selection_.set_entity_filter(dim, tag);
  int target_index = 0;
  if (dim >= 0 && tag >= 0) {
    for (size_t i = 0; i < host_->mesh_entities_.size(); ++i) {
      const auto& ent = host_->mesh_entities_[i];
      if (ent.dim == dim && ent.tag == tag) {
        const int idx = host_->mesh_entity_->findData(static_cast<int>(i));
        if (idx >= 0) {
          target_index = idx;
        }
        break;
      }
    }
  }
  host_->mesh_entity_->blockSignals(true);
  host_->mesh_entity_->setCurrentIndex(target_index);
  host_->mesh_entity_->blockSignals(false);
  host_->update_mesh_pipeline();
  host_->update_selection_pipeline();
#else
  Q_UNUSED(dim);
  Q_UNUSED(tag);
#endif
}


void MeshViewport::preview_mesh_entity(int dim, int tag, double view_x,
                                    double view_y, double view_z) {
#ifdef GMP_ENABLE_VTK_VIEWER
  const bool activate = dim >= 0 && tag >= 0;
  if (activate && !host_->preview_visual_active_) {
    host_->preview_saved_scalar_visibility_ =
        host_->mapper_ ? host_->mapper_->GetScalarVisibility() : 1;
    host_->preview_saved_scalar_bar_visibility_ =
        host_->scalar_bar_ && host_->scalar_bar_->GetVisibility() != 0;
    if (host_->actor_) {
      host_->actor_->GetProperty()->GetColor(host_->preview_saved_actor_color_);
    }
    host_->preview_visual_active_ = true;
  }
  host_->selection_.set_preview(activate ? dim : -1, activate ? tag : -1);
  if (!activate) {
    host_->mesh_select_occ_geometry_ = nullptr;
  }

  if (activate) {
    // 预览时把完整装配降为中性线框，候选面保持不透明黄色；这样内部
    // 接触面不会继续被红/蓝实体着色淹没。
    if (host_->mapper_) {
      host_->mapper_->ScalarVisibilityOff();
    }
    if (host_->actor_) {
      host_->actor_->SetVisibility(true);
      host_->actor_->GetProperty()->SetRepresentationToWireframe();
      host_->actor_->GetProperty()->SetEdgeVisibility(0);
      host_->actor_->GetProperty()->SetColor(0.42, 0.46, 0.52);
      host_->actor_->GetProperty()->SetOpacity(0.28);
    }
    if (host_->scalar_bar_) {
      host_->scalar_bar_->SetVisibility(false);
    }
  } else if (host_->preview_visual_active_) {
    if (host_->mapper_) {
      host_->mapper_->SetScalarVisibility(host_->preview_saved_scalar_visibility_);
    }
    if (host_->actor_) {
      host_->actor_->GetProperty()->SetColor(host_->preview_saved_actor_color_);
    }
    host_->preview_visual_active_ = false;
    host_->apply_mesh_visuals();
    if (host_->scalar_bar_) {
      host_->scalar_bar_->SetVisibility(host_->preview_saved_scalar_bar_visibility_);
    }
  }

  host_->update_selection_pipeline();
  if (activate && host_->renderer_ && host_->mesh_select_geom_) {
    host_->mesh_select_geom_->Update();
    vtkDataSet* selected =
        dim <= 1 && host_->mesh_select_occ_geometry_
            ? static_cast<vtkDataSet*>(host_->mesh_select_occ_geometry_.GetPointer())
            : static_cast<vtkDataSet*>(host_->mesh_select_geom_->GetOutput());
    double bounds[6] = {0, 0, 0, 0, 0, 0};
    if (selected) {
      selected->GetBounds(bounds);
    }
    const gmp::PreviewCameraResult focus = gmp::preview_focus_camera(
        bounds, host_->mesh_bounds_, view_x, view_y, view_z);
    if (focus.valid) {
      vtkCamera* camera = host_->renderer_->GetActiveCamera();
      camera->SetFocalPoint(focus.focal);
      camera->SetPosition(focus.position);
      camera->SetViewUp(focus.view_up);
      host_->renderer_->ResetCameraClippingRange();
    }
  }
  // 清除预览时 host_->update_selection_pipeline() 可能走隐藏分支；这里补一次
  // 渲染，确保黄色叠加立即消失并恢复原有舞台选择状态。
  if (host_->render_window_) {
    host_->render_window_->Render();
  }
#else
  Q_UNUSED(dim);
  Q_UNUSED(tag);
  Q_UNUSED(view_x);
  Q_UNUSED(view_y);
  Q_UNUSED(view_z);
#endif
}


bool MeshViewport::is_mesh_entity_previewed(int dim, int tag) const {
#ifdef GMP_ENABLE_VTK_VIEWER
  return host_->selection_.is_previewed(dim, tag);
#else
  Q_UNUSED(dim);
  Q_UNUSED(tag);
  return false;
#endif
}


bool MeshViewport::is_mesh_entity_preview_visible(int dim, int tag) const {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!host_->is_mesh_entity_previewed(dim, tag) || !host_->mesh_select_actor_ ||
      host_->mesh_select_actor_->GetVisibility() == 0 || !host_->mesh_select_geom_) {
    return false;
  }
  host_->mesh_select_geom_->Update();
  vtkDataSet* selected =
      dim <= 1 && host_->mesh_select_occ_geometry_
          ? static_cast<vtkDataSet*>(host_->mesh_select_occ_geometry_.GetPointer())
          : static_cast<vtkDataSet*>(host_->mesh_select_geom_->GetOutput());
  if (!selected || selected->GetNumberOfCells() <= 0) {
    return false;
  }
  auto* property = host_->mesh_select_actor_->GetProperty();
  if (!property) {
    return false;
  }
  if (dim == 0) {
    return host_->mesh_select_point_source_ &&
           host_->mesh_select_point_source_->GetRadius() > 0.0 &&
           property->GetRepresentation() == VTK_SURFACE;
  }
  if (dim == 1) {
    return host_->mesh_select_curve_tube_ && host_->mesh_select_curve_tube_->GetRadius() > 0.0 &&
           property->GetRepresentation() == VTK_SURFACE;
  }
  return property->GetRepresentation() == VTK_SURFACE;
#else
  Q_UNUSED(dim);
  Q_UNUSED(tag);
  return false;
#endif
}


void MeshViewport::update_mesh_controls() {
#ifdef GMP_ENABLE_VTK_VIEWER
  const bool mesh_mode = host_->mode_ == VtkViewer::DataMode::Mesh;
  const bool exodus_mode = host_->mode_ == VtkViewer::DataMode::Exodus;
  if (host_->show_nodes_) {
    host_->show_nodes_->setEnabled(mesh_mode);
  }
  if (host_->show_quality_) {
    host_->show_quality_->setEnabled(mesh_mode);
  }
  if (host_->show_faces_) {
    host_->show_faces_->setEnabled(mesh_mode);
  }
  if (host_->show_edges_) {
    host_->show_edges_->setEnabled(mesh_mode);
  }
  if (host_->show_shell_) {
    host_->show_shell_->setEnabled(mesh_mode);
  }
  if (host_->mesh_dim_) {
    host_->mesh_dim_->setEnabled(mesh_mode);
  }
  if (host_->mesh_group_) {
    host_->mesh_group_->setEnabled(mesh_mode);
  }
  if (host_->mesh_entity_) {
    host_->mesh_entity_->setEnabled(mesh_mode);
  }
  if (host_->mesh_type_) {
    host_->mesh_type_->setEnabled(mesh_mode);
  }
  if (host_->mesh_opacity_) {
    host_->mesh_opacity_->setEnabled(mesh_mode);
  }
  if (host_->mesh_shrink_) {
    host_->mesh_shrink_->setEnabled(mesh_mode);
  }
  if (host_->mesh_scalar_bar_) {
    host_->mesh_scalar_bar_->setEnabled(true);
  }
  if (host_->pick_enable_) {
    host_->pick_enable_->setEnabled(mesh_mode);
  }
  if (host_->pick_mode_) {
    host_->pick_mode_->setEnabled(mesh_mode);
  }
  if (host_->pick_clear_) {
    host_->pick_clear_->setEnabled(mesh_mode);
  }
  if (host_->pick_info_) {
    host_->pick_info_->setVisible(mesh_mode);
  }
  if (host_->probe_enable_) {
    host_->probe_enable_->setEnabled(exodus_mode);
  }
  if (host_->probe_mode_) {
    host_->probe_mode_->setEnabled(exodus_mode);
  }
  if (host_->probe_clear_) {
    host_->probe_clear_->setEnabled(exodus_mode);
  }
  if (host_->probe_info_) {
    host_->probe_info_->setVisible(exodus_mode);
  }
  if (host_->deform_enable_) {
    host_->deform_enable_->setEnabled(exodus_mode);
  }
  if (host_->deform_vector_) {
    host_->deform_vector_->setEnabled(exodus_mode);
  }
  if (host_->deform_scale_) {
    host_->deform_scale_->setEnabled(exodus_mode);
  }
  if (host_->view_combo_) {
    host_->view_combo_->setEnabled(mesh_mode || host_->mode_ == VtkViewer::DataMode::Exodus);
  }
  if (host_->view_apply_) {
    host_->view_apply_->setEnabled(mesh_mode || host_->mode_ == VtkViewer::DataMode::Exodus);
  }
  if (host_->show_axes_) {
    host_->show_axes_->setEnabled(mesh_mode || host_->mode_ == VtkViewer::DataMode::Exodus);
  }
  if (host_->show_outline_) {
    host_->show_outline_->setEnabled(mesh_mode || host_->mode_ == VtkViewer::DataMode::Exodus);
  }
  if (host_->slice_enable_) {
    host_->slice_enable_->setEnabled(mesh_mode);
  }
  if (host_->slice_axis_) {
    host_->slice_axis_->setEnabled(mesh_mode && host_->slice_enable_->isChecked());
  }
  if (host_->slice_slider_) {
    host_->slice_slider_->setEnabled(mesh_mode && host_->slice_enable_->isChecked());
  }
  if (host_->mesh_legend_) {
    host_->mesh_legend_->setEnabled(mesh_mode);
    host_->mesh_legend_->setVisible(mesh_mode);
  }
  if (host_->repr_combo_) {
    host_->repr_combo_->setEnabled(!mesh_mode);
  }

  if (!mesh_mode) {
    if (host_->mesh_legend_) {
      host_->mesh_legend_->setText("Groups: none");
    }
    return;
  }

  if (host_->mesh_group_) {
    host_->mesh_group_->blockSignals(true);
    host_->mesh_group_->clear();
    host_->mesh_group_->addItem("All", -1);
    for (size_t i = 0; i < host_->mesh_groups_.size(); ++i) {
      const auto& g = host_->mesh_groups_[i];
      const QString label = g.name.isEmpty()
                                ? QString("%1:%2").arg(g.dim).arg(g.id)
                                : QString("%1:%2 %3").arg(g.dim).arg(g.id).arg(g.name);
      host_->mesh_group_->addItem(label, static_cast<int>(i));
    }
    host_->mesh_group_->setCurrentIndex(0);
    host_->mesh_group_->blockSignals(false);
  }

  if (host_->mesh_entity_) {
    // 首次填充时 currentData() 为无效 QVariant，toInt() 会返回 0，
    // 进而 findData(0) 命中首个实体导致误加实体过滤器；必须显式判无效。
    const QVariant current_data = host_->mesh_entity_->currentData();
    const int current = current_data.isValid() ? current_data.toInt() : -1;
    host_->mesh_entity_->blockSignals(true);
    host_->mesh_entity_->clear();
    host_->mesh_entity_->addItem("All", -1);
    for (size_t i = 0; i < host_->mesh_entities_.size(); ++i) {
      const auto& ent = host_->mesh_entities_[i];
      const QString label = QString("%1:%2").arg(ent.dim).arg(ent.tag);
      host_->mesh_entity_->addItem(label, static_cast<int>(i));
    }
    int idx = host_->mesh_entity_->findData(current);
    if (idx < 0) {
      idx = 0;
    }
    host_->mesh_entity_->setCurrentIndex(idx);
    host_->mesh_entity_->blockSignals(false);
  }

  if (host_->mesh_type_) {
    const QVariant current_data = host_->mesh_type_->currentData();
    const int current = current_data.isValid() ? current_data.toInt() : -1;
    host_->mesh_type_->blockSignals(true);
    host_->mesh_type_->clear();
    host_->mesh_type_->addItem("All", -1);
    for (int t : host_->mesh_elem_types_) {
      host_->mesh_type_->addItem(ElementTypeLabel(t), t);
    }
    int idx = host_->mesh_type_->findData(current);
    if (idx < 0) {
      idx = 0;
    }
    host_->mesh_type_->setCurrentIndex(idx);
    host_->mesh_type_->blockSignals(false);
  }

  if (host_->mesh_dim_) {
    int highest_dim = -1;
    // OCC may still contain 3-D volume entities when only a lightweight 2-D
    // surface preview has been meshed.  Derive the default dimension from
    // cells that actually exist, instead of from the geometric entity list.
    if (host_->mesh_grid_ && host_->mesh_grid_->GetCellData()) {
      if (auto* dims = vtkIntArray::SafeDownCast(
              host_->mesh_grid_->GetCellData()->GetArray("phys_dim"))) {
        for (vtkIdType cell = 0; cell < dims->GetNumberOfTuples(); ++cell) {
          highest_dim = std::max(highest_dim, dims->GetValue(cell));
        }
      }
    }
    if (highest_dim < 0) {
      for (const auto& ent : host_->mesh_entities_) {
        highest_dim = std::max(highest_dim, ent.dim);
      }
    }
    host_->mesh_dim_->blockSignals(true);
    const int highest_index = host_->mesh_dim_->findData(highest_dim);
    host_->mesh_dim_->setCurrentIndex(highest_index >= 0 ? highest_index : 0);
    host_->mesh_dim_->blockSignals(false);
  }

  if (host_->mesh_grid_) {
    host_->mesh_grid_->GetBounds(host_->mesh_bounds_);
    const double dx = host_->mesh_bounds_[1] - host_->mesh_bounds_[0];
    const double dy = host_->mesh_bounds_[3] - host_->mesh_bounds_[2];
    const double dz = host_->mesh_bounds_[5] - host_->mesh_bounds_[4];
    int axis = 0;
    double max_extent = dx;
    if (dy > max_extent) {
      axis = 1;
      max_extent = dy;
    }
    if (dz > max_extent) {
      axis = 2;
    }
    if (host_->slice_axis_) {
      host_->slice_axis_->setCurrentIndex(axis);
    }
    if (host_->slice_slider_) {
      host_->slice_slider_->setValue(50);
    }
  }

  if (host_->mesh_legend_) {
    if (host_->mesh_groups_.empty()) {
      host_->mesh_legend_->setText("Groups: none");
    } else {
      QStringList labels;
      for (const auto& g : host_->mesh_groups_) {
        const QString label =
            g.name.isEmpty() ? QString("%1:%2").arg(g.dim).arg(g.id)
                             : QString("%1:%2 %3").arg(g.dim).arg(g.id).arg(g.name);
        labels << label;
      }
      host_->mesh_legend_->setText(QString("Groups: %1").arg(labels.join(", ")));
    }
  }
#endif
}


void MeshViewport::update_mesh_pipeline() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (host_->mode_ != VtkViewer::DataMode::Mesh) {
    return;
  }
  if (!host_->mesh_grid_ || !host_->mapper_) {
    return;
  }
  if (!host_->mesh_geom_) {
    host_->mesh_geom_ = vtkSmartPointer<vtkDataSetSurfaceFilter>::New();
  }
  if (!host_->mesh_dim_threshold_) {
    host_->mesh_dim_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!host_->mesh_group_threshold_) {
    host_->mesh_group_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!host_->mesh_type_threshold_) {
    host_->mesh_type_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!host_->mesh_entity_dim_threshold_) {
    host_->mesh_entity_dim_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!host_->mesh_entity_tag_threshold_) {
    host_->mesh_entity_tag_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!host_->mesh_slice_plane_) {
    host_->mesh_slice_plane_ = vtkSmartPointer<vtkPlane>::New();
  }
  if (!host_->mesh_slice_cutter_) {
    host_->mesh_slice_cutter_ = vtkSmartPointer<vtkCutter>::New();
  }
  if (!host_->mesh_shrink_filter_) {
    host_->mesh_shrink_filter_ = vtkSmartPointer<vtkShrinkFilter>::New();
  }

  if (!host_->mesh_quality_ready_ && host_->mesh_grid_->GetCellData()) {
    if (host_->mesh_grid_->GetCellData()->HasArray("Quality")) {
      host_->mesh_quality_ready_ = true;
    } else {
      auto quality = vtkSmartPointer<vtkMeshQuality>::New();
      quality->SetInputData(host_->mesh_grid_);
      quality->Update();
      auto* arr = quality->GetOutput()->GetCellData()->GetArray("Quality");
      if (arr) {
        vtkSmartPointer<vtkDataArray> copy;
        copy.TakeReference(arr->NewInstance());
        copy->DeepCopy(arr);
        copy->SetName("Quality");
        host_->mesh_grid_->GetCellData()->AddArray(copy);
        host_->mesh_quality_ready_ = true;
      }
    }
  }

  int dim_filter = host_->mesh_dim_ ? host_->mesh_dim_->currentData().toInt() : -1;
  int group_index = host_->mesh_group_ ? host_->mesh_group_->currentData().toInt() : -1;
  int entity_index = host_->mesh_entity_ ? host_->mesh_entity_->currentData().toInt() : -1;
  const int type_filter = host_->mesh_type_ ? host_->mesh_type_->currentData().toInt() : -1;
  int group_dim = -1;
  int group_id = -1;
  int entity_dim = -1;
  int entity_tag = -1;
  if (group_index >= 0 &&
      group_index < static_cast<int>(host_->mesh_groups_.size())) {
    const auto& g = host_->mesh_groups_[group_index];
    group_dim = g.dim;
    group_id = g.id;
    dim_filter = g.dim;
    if (host_->mesh_dim_) {
      const int dim_idx = host_->mesh_dim_->findData(group_dim);
      if (dim_idx >= 0 && host_->mesh_dim_->currentIndex() != dim_idx) {
        host_->mesh_dim_->blockSignals(true);
        host_->mesh_dim_->setCurrentIndex(dim_idx);
        host_->mesh_dim_->blockSignals(false);
      }
    }
  }
  if (entity_index >= 0 &&
      entity_index < static_cast<int>(host_->mesh_entities_.size())) {
    const auto& ent = host_->mesh_entities_[entity_index];
    entity_dim = ent.dim;
    entity_tag = ent.tag;
    dim_filter = ent.dim;
    if (host_->mesh_dim_) {
      const int dim_idx = host_->mesh_dim_->findData(entity_dim);
      if (dim_idx >= 0 && host_->mesh_dim_->currentIndex() != dim_idx) {
        host_->mesh_dim_->blockSignals(true);
        host_->mesh_dim_->setCurrentIndex(dim_idx);
        host_->mesh_dim_->blockSignals(false);
      }
    }
  }
  if (group_index >= 0 && group_id >= 0) {
    host_->selection_.group_dim_ = group_dim;
    host_->selection_.group_id_ = group_id;
  } else if (host_->mesh_group_ && host_->mesh_group_->currentData().toInt() < 0) {
    host_->selection_.group_dim_ = -1;
    host_->selection_.group_id_ = -1;
  }
  if (entity_index >= 0 && entity_tag >= 0 && entity_dim >= 0) {
    host_->selection_.entity_dim_ = entity_dim;
    host_->selection_.entity_tag_ = entity_tag;
  } else if (host_->mesh_entity_ && host_->mesh_entity_->currentData().toInt() < 0) {
    host_->selection_.entity_dim_ = -1;
    host_->selection_.entity_tag_ = -1;
  }

  vtkAlgorithmOutput* current_port = nullptr;
  if (dim_filter >= 0) {
    host_->mesh_dim_threshold_->SetInputData(host_->mesh_grid_);
    host_->mesh_dim_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "phys_dim");
    host_->mesh_dim_threshold_->SetLowerThreshold(dim_filter);
    host_->mesh_dim_threshold_->SetUpperThreshold(dim_filter);
    current_port = host_->mesh_dim_threshold_->GetOutputPort();
  }

  if (group_index >= 0 && group_id >= 0) {
    if (current_port) {
      host_->mesh_group_threshold_->SetInputConnection(current_port);
    } else {
      host_->mesh_group_threshold_->SetInputData(host_->mesh_grid_);
    }
    host_->mesh_group_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "phys_id");
    host_->mesh_group_threshold_->SetLowerThreshold(group_id);
    host_->mesh_group_threshold_->SetUpperThreshold(group_id);
    current_port = host_->mesh_group_threshold_->GetOutputPort();
  }

  if (entity_index >= 0 && entity_tag >= 0 && entity_dim >= 0) {
    if (current_port) {
      host_->mesh_entity_dim_threshold_->SetInputConnection(current_port);
    } else {
      host_->mesh_entity_dim_threshold_->SetInputData(host_->mesh_grid_);
    }
    host_->mesh_entity_dim_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "entity_dim");
    host_->mesh_entity_dim_threshold_->SetLowerThreshold(entity_dim);
    host_->mesh_entity_dim_threshold_->SetUpperThreshold(entity_dim);
    host_->mesh_entity_tag_threshold_->SetInputConnection(
        host_->mesh_entity_dim_threshold_->GetOutputPort());
    host_->mesh_entity_tag_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "entity_tag");
    host_->mesh_entity_tag_threshold_->SetLowerThreshold(entity_tag);
    host_->mesh_entity_tag_threshold_->SetUpperThreshold(entity_tag);
    current_port = host_->mesh_entity_tag_threshold_->GetOutputPort();
  }

  if (type_filter >= 0) {
    if (current_port) {
      host_->mesh_type_threshold_->SetInputConnection(current_port);
    } else {
      host_->mesh_type_threshold_->SetInputData(host_->mesh_grid_);
    }
    host_->mesh_type_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "elem_type");
    host_->mesh_type_threshold_->SetLowerThreshold(type_filter);
    host_->mesh_type_threshold_->SetUpperThreshold(type_filter);
    current_port = host_->mesh_type_threshold_->GetOutputPort();
  }

  if (current_port) {
    host_->mesh_geom_->SetInputConnection(current_port);
  } else {
    host_->mesh_geom_->SetInputData(host_->mesh_grid_);
  }

  const bool slice_on = host_->slice_enable_ && host_->slice_enable_->isChecked();
  const bool shell_on = host_->show_shell_ ? host_->show_shell_->isChecked() : true;
  if (host_->slice_axis_) {
    host_->slice_axis_->setEnabled(slice_on);
  }
  if (host_->slice_slider_) {
    host_->slice_slider_->setEnabled(slice_on);
  }
  vtkAlgorithmOutput* final_port = nullptr;
  if (slice_on) {
    host_->mesh_grid_->GetBounds(host_->mesh_bounds_);
    const int axis = host_->slice_axis_ ? host_->slice_axis_->currentIndex() : 0;
    const double minv = host_->mesh_bounds_[2 * axis];
    const double maxv = host_->mesh_bounds_[2 * axis + 1];
    const double range = maxv - minv;
    const bool has_range = range > 1e-12;
    if (host_->slice_slider_) {
      host_->slice_slider_->setEnabled(has_range);
    }
    const double t = host_->slice_slider_ ? host_->slice_slider_->value() / 100.0 : 0.5;
    const double pos = has_range ? minv + t * range : minv;
    double origin[3] = {(host_->mesh_bounds_[0] + host_->mesh_bounds_[1]) * 0.5,
                        (host_->mesh_bounds_[2] + host_->mesh_bounds_[3]) * 0.5,
                        (host_->mesh_bounds_[4] + host_->mesh_bounds_[5]) * 0.5};
    origin[axis] = pos;
    double normal[3] = {0.0, 0.0, 0.0};
    normal[axis] = 1.0;
    host_->mesh_slice_plane_->SetOrigin(origin);
    host_->mesh_slice_plane_->SetNormal(normal);
    host_->mesh_slice_cutter_->SetCutFunction(host_->mesh_slice_plane_);
    if (current_port) {
      host_->mesh_slice_cutter_->SetInputConnection(current_port);
    } else {
      host_->mesh_slice_cutter_->SetInputData(host_->mesh_grid_);
    }
    final_port = host_->mesh_slice_cutter_->GetOutputPort();
    host_->mesh_slice_cutter_->Update();
  } else if (shell_on) {
    if (current_port) {
      host_->mesh_geom_->SetInputConnection(current_port);
    } else {
      host_->mesh_geom_->SetInputData(host_->mesh_grid_);
    }
    final_port = host_->mesh_geom_->GetOutputPort();
    host_->mesh_geom_->Update();
  } else {
    if (current_port) {
      final_port = current_port;
    } else {
      final_port = nullptr;
    }
  }

  const double shrink = host_->mesh_shrink_ ? host_->mesh_shrink_->value() : 1.0;
  if (shrink < 0.999) {
    host_->mesh_shrink_filter_->SetShrinkFactor(shrink);
    if (final_port) {
      host_->mesh_shrink_filter_->SetInputConnection(final_port);
    } else if (host_->mesh_grid_) {
      host_->mesh_shrink_filter_->SetInputData(host_->mesh_grid_);
    }
    host_->mapper_->SetInputConnection(host_->mesh_shrink_filter_->GetOutputPort());
  } else if (final_port) {
    host_->mapper_->SetInputConnection(final_port);
  } else if (host_->mesh_grid_) {
    host_->mapper_->SetInputData(host_->mesh_grid_);
  }

  host_->update_nodes_visibility();
  host_->apply_mesh_visuals();
  host_->update_selection_pipeline();
  host_->update_scene_extras();
  if (host_->render_window_) {
    host_->render_window_->Render();
  }
#endif
}


void MeshViewport::apply_mesh_visuals() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!host_->actor_) {
    return;
  }
  if (host_->mode_ != VtkViewer::DataMode::Mesh) {
    return;
  }
  const bool faces = host_->show_faces_ ? host_->show_faces_->isChecked() : true;
  const bool edges = host_->show_edges_ ? host_->show_edges_->isChecked() : false;
  if (!faces && !edges) {
    host_->actor_->SetVisibility(false);
  } else {
    host_->actor_->SetVisibility(true);
    if (faces && edges) {
      host_->actor_->GetProperty()->SetRepresentationToSurface();
      host_->actor_->GetProperty()->SetEdgeVisibility(1);
    } else if (faces) {
      host_->actor_->GetProperty()->SetRepresentationToSurface();
      host_->actor_->GetProperty()->SetEdgeVisibility(0);
    } else {
      host_->actor_->GetProperty()->SetRepresentationToWireframe();
      host_->actor_->GetProperty()->SetEdgeVisibility(0);
    }
  }
  if (host_->mesh_opacity_) {
    host_->actor_->GetProperty()->SetOpacity(host_->mesh_opacity_->value());
  }
  if (host_->scalar_bar_) {
    const bool show_bar =
        !host_->mesh_scalar_bar_ || host_->mesh_scalar_bar_->isChecked();
    host_->scalar_bar_->SetVisibility(show_bar ? 1 : 0);
  }
  if (host_->render_window_) {
    host_->render_window_->Render();
  }
#endif
}


void MeshViewport::update_nodes_visibility() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!host_->renderer_ || !host_->render_window_) {
    return;
  }
  const bool show =
      host_->show_nodes_ && host_->show_nodes_->isChecked() && host_->mode_ == VtkViewer::DataMode::Mesh;
  if (!show) {
    if (host_->nodes_actor_) {
      host_->nodes_actor_->SetVisibility(false);
      host_->render_window_->Render();
    }
    return;
  }
  vtkAlgorithmOutput* port = nullptr;
  if (host_->mesh_geom_) {
    port = host_->mesh_geom_->GetOutputPort();
  } else if (host_->mapper_ && host_->mapper_->GetInputConnection(0, 0)) {
    port = host_->mapper_->GetInputConnection(0, 0);
  }
  if (!port) {
    return;
  }
  if (!host_->nodes_filter_) {
    host_->nodes_filter_ = vtkSmartPointer<vtkVertexGlyphFilter>::New();
    host_->nodes_mapper_ = vtkSmartPointer<vtkPolyDataMapper>::New();
    host_->nodes_actor_ = vtkSmartPointer<vtkActor>::New();
    host_->nodes_mapper_->SetInputConnection(host_->nodes_filter_->GetOutputPort());
    host_->nodes_actor_->SetMapper(host_->nodes_mapper_);
    host_->nodes_actor_->GetProperty()->SetPointSize(4);
    host_->nodes_actor_->GetProperty()->SetColor(0.1, 0.1, 0.1);
    host_->renderer_->AddActor(host_->nodes_actor_);
  }
  host_->nodes_filter_->SetInputConnection(port);
  host_->nodes_filter_->Update();
  host_->nodes_actor_->SetVisibility(true);
  host_->render_window_->Render();
#endif
}


void MeshViewport::handle_pick(int x, int y) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!host_->renderer_ || !host_->mapper_) {
    return;
  }
  if (!host_->picker_) {
    host_->picker_ = vtkSmartPointer<vtkCellPicker>::New();
    host_->picker_->SetTolerance(0.0005);
  }

  if (host_->mode_ == VtkViewer::DataMode::Mesh) {
    if (!host_->pick_enable_ || !host_->pick_enable_->isChecked()) {
      return;
    }
    if (!host_->picker_->Pick(x, y, 0, host_->renderer_)) {
      if (host_->pick_info_) {
        host_->pick_info_->setText("Pick: none");
      }
      return;
    }
    vtkIdType cell_id = host_->picker_->GetCellId();
    if (cell_id < 0) {
      if (host_->pick_info_) {
        host_->pick_info_->setText("Pick: none");
      }
      return;
    }
    vtkDataSet* data = vtkDataSet::SafeDownCast(host_->mapper_->GetInput());
    if (!data && host_->mesh_geom_) {
      host_->mesh_geom_->Update();
      data = host_->mesh_geom_->GetOutput();
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
      for (const auto& g : host_->mesh_groups_) {
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
    if (host_->pick_info_) {
      host_->pick_info_->setText(
          QString("Pick: cell=%1 group=%2 entity=%3 type=%4 q=%5")
              .arg(cell_index >= 0 ? cell_index : cell_id)
              .arg(group_label)
              .arg(ent_label)
              .arg(elem_type)
              .arg(quality, 0, 'g', 4));
    }
    if (phys_dim >= 0 && phys_id >= 0) {
      emit host_->mesh_group_picked(phys_dim, phys_id);
    }
    const int mode = host_->pick_mode_ ? host_->pick_mode_->currentData().toInt() : 0;
    if (mode == 2) {
      host_->selection_.cell_id_ =
          cell_index >= 0 ? cell_index : static_cast<int>(cell_id);
      host_->selection_.group_dim_ = phys_dim;
      host_->selection_.group_id_ = phys_id;
      host_->selection_.entity_dim_ = ent_dim;
      host_->selection_.entity_tag_ = ent_tag;
    } else if (mode == 1) {
      host_->selection_.entity_dim_ = ent_dim;
      host_->selection_.entity_tag_ = ent_tag;
      host_->selection_.group_dim_ = phys_dim;
      host_->selection_.group_id_ = phys_id;
      host_->selection_.cell_id_ = -1;
      if (ent_dim >= 0 && ent_tag >= 0) {
        emit host_->mesh_entity_picked(ent_dim, ent_tag);
      }
    } else {
      host_->selection_.group_dim_ = phys_dim;
      host_->selection_.group_id_ = phys_id;
      host_->selection_.entity_dim_ = ent_dim;
      host_->selection_.entity_tag_ = ent_tag;
      host_->selection_.cell_id_ = -1;
    }
    host_->update_selection_pipeline();
    return;
  }

  if (host_->mode_ != VtkViewer::DataMode::Exodus) {
    return;
  }
  if (!host_->probe_enable_ || !host_->probe_enable_->isChecked()) {
    return;
  }
  if (!host_->picker_->Pick(x, y, 0, host_->renderer_)) {
    if (host_->probe_info_) {
      host_->probe_info_->setText("Probe: none");
    }
    return;
  }

  vtkDataSet* data = vtkDataSet::SafeDownCast(host_->mapper_->GetInput());
  if (!data && host_->geom_) {
    host_->geom_->Update();
    data = host_->geom_->GetOutput();
  }
  if (!data) {
    return;
  }

  const int probe_mode = host_->probe_mode_ ? host_->probe_mode_->currentData().toInt() : 0;
  const bool path_mode = (probe_mode == 2);
  const bool want_point = (probe_mode == 0 || path_mode);
  vtkIdType cell_id = host_->picker_->GetCellId();
  vtkIdType point_id = host_->picker_->GetPointId();
  double pos[3] = {0.0, 0.0, 0.0};
  host_->picker_->GetPickPosition(pos);

  if (want_point && point_id < 0) {
    point_id = data->FindPoint(pos);
  }
  if (want_point && point_id < 0) {
    if (host_->probe_info_) {
      host_->probe_info_->setText("Probe: none");
    }
    return;
  }
  if (!want_point && cell_id < 0) {
    if (host_->probe_info_) {
      host_->probe_info_->setText("Probe: none");
    }
    return;
  }

  vtkDataArray* array = nullptr;
  QString array_name;
  const QString key =
      host_->array_combo_ ? host_->array_combo_->currentData().toString() : "";

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
  host_->result_probe_id_ = static_cast<qlonglong>(id);
  host_->result_probe_point_ = want_point;
  if (path_mode &&
      (host_->result_path_point_ids_.isEmpty() ||
       host_->result_path_point_ids_.last() != id)) {
    host_->result_path_point_ids_ << static_cast<qlonglong>(id);
  } else if (!path_mode) {
    host_->result_path_point_ids_.clear();
  }
  const QString value = format_value(array, id);
  const QString mode_label = path_mode ? "Path" : (want_point ? "Point" : "Cell");
  const QString label = array_name.isEmpty() ? "value" : array_name;
  if (host_->probe_info_) {
    host_->probe_info_->setText(QString("Probe (%1): id=%2 pos=(%3, %4, %5) %6=%7%8")
                             .arg(mode_label)
                             .arg(id)
                             .arg(pos[0], 0, 'g', 6)
                             .arg(pos[1], 0, 'g', 6)
                             .arg(pos[2], 0, 'g', 6)
                             .arg(label)
                             .arg(value)
                             .arg(path_mode
                                      ? QString(" · %1 path points")
                                            .arg(host_->result_path_point_ids_.size())
                                      : QString()));
  }
  host_->update_plot_view();
  host_->update_table_view();
#endif
}



void MeshViewport::update_selection_pipeline() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!host_->renderer_ || !host_->mesh_grid_) {
    return;
  }
  const bool entity_preview =
      host_->selection_.preview_dim_ >= 0 && host_->selection_.preview_tag_ >= 0;
  if (!entity_preview && (!host_->pick_enable_ || !host_->pick_enable_->isChecked())) {
    if (host_->mesh_select_actor_) {
      host_->mesh_select_actor_->SetVisibility(false);
    }
    return;
  }
  if (host_->mode_ != VtkViewer::DataMode::Mesh) {
    if (host_->mesh_select_actor_) {
      host_->mesh_select_actor_->SetVisibility(false);
    }
    return;
  }

  const int mode =
      entity_preview ? 1 : (host_->pick_mode_ ? host_->pick_mode_->currentData().toInt() : 0);
  const int entity_dim =
      entity_preview ? host_->selection_.preview_dim_ : host_->selection_.entity_dim_;
  const int entity_tag =
      entity_preview ? host_->selection_.preview_tag_ : host_->selection_.entity_tag_;
  if (mode == 2 && host_->selection_.cell_id_ < 0) {
    if (host_->mesh_select_actor_) {
      host_->mesh_select_actor_->SetVisibility(false);
    }
    return;
  }
  if (mode == 1 && (entity_tag < 0 || entity_dim < 0)) {
    if (host_->mesh_select_actor_) {
      host_->mesh_select_actor_->SetVisibility(false);
    }
    return;
  }
  if (mode == 0 && (host_->selection_.group_id_ < 0 || host_->selection_.group_dim_ < 0)) {
    if (host_->mesh_select_actor_) {
      host_->mesh_select_actor_->SetVisibility(false);
    }
    return;
  }

  if (!host_->mesh_select_dim_threshold_) {
    host_->mesh_select_dim_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!host_->mesh_select_group_threshold_) {
    host_->mesh_select_group_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!host_->mesh_select_cell_threshold_) {
    host_->mesh_select_cell_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!host_->mesh_select_entity_dim_threshold_) {
    host_->mesh_select_entity_dim_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!host_->mesh_select_entity_tag_threshold_) {
    host_->mesh_select_entity_tag_threshold_ = vtkSmartPointer<vtkThreshold>::New();
  }
  if (!host_->mesh_select_geom_) {
    host_->mesh_select_geom_ = vtkSmartPointer<vtkDataSetSurfaceFilter>::New();
  }
  if (!host_->mesh_select_mapper_) {
    host_->mesh_select_mapper_ = vtkSmartPointer<vtkDataSetMapper>::New();
    host_->mesh_select_mapper_->ScalarVisibilityOff();
  }
  if (!host_->mesh_select_actor_) {
    host_->mesh_select_actor_ = vtkSmartPointer<vtkActor>::New();
    host_->mesh_select_actor_->SetMapper(host_->mesh_select_mapper_);
    host_->mesh_select_actor_->GetProperty()->SetColor(1.0, 0.78, 0.05);
    host_->mesh_select_actor_->GetProperty()->SetEdgeColor(0.20, 0.14, 0.0);
    host_->mesh_select_actor_->GetProperty()->SetAmbient(1.0);
    host_->mesh_select_actor_->GetProperty()->SetDiffuse(0.0);
    host_->mesh_select_actor_->GetProperty()->SetLineWidth(2.0);
    host_->mesh_select_actor_->GetProperty()->SetEdgeVisibility(1);
    host_->mesh_select_actor_->GetProperty()->SetRepresentationToSurface();
    host_->renderer_->AddActor(host_->mesh_select_actor_);
  }

  // 点、边、面/体共用同一选择 actor，但必须使用不同的可视强调方式。
  // 仅增加 OpenGL point/line width 仍会与灰色装配线框共面并被深度缓冲
  // 淹没，因此点生成真实球体，边生成真实管体；面和体保持原有黄色表面。
  // 标记半径相对完整模型对角线计算，缩放后仍有稳定的空间可读性。
  auto* selection_property = host_->mesh_select_actor_->GetProperty();
  selection_property->SetColor(1.0, 0.78, 0.05);
  selection_property->SetEdgeColor(0.20, 0.14, 0.0);
  selection_property->SetOpacity(1.0);
  selection_property->SetAmbient(1.0);
  selection_property->SetDiffuse(0.0);
  selection_property->SetPointSize(5.0);
  selection_property->SetLineWidth(2.0);
  selection_property->SetRenderPointsAsSpheres(false);
  selection_property->SetRenderLinesAsTubes(false);
  selection_property->SetRepresentationToSurface();
  selection_property->SetEdgeVisibility(entity_preview && entity_dim <= 1 ? 0
                                                                          : 1);

  vtkAlgorithmOutput* current = nullptr;
  if (mode == 2) {
    host_->mesh_select_cell_threshold_->SetInputData(host_->mesh_grid_);
    host_->mesh_select_cell_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "cell_id");
    host_->mesh_select_cell_threshold_->SetLowerThreshold(host_->selection_.cell_id_);
    host_->mesh_select_cell_threshold_->SetUpperThreshold(host_->selection_.cell_id_);
    current = host_->mesh_select_cell_threshold_->GetOutputPort();
  } else if (mode == 1) {
    host_->mesh_select_entity_dim_threshold_->SetInputData(host_->mesh_grid_);
    host_->mesh_select_entity_dim_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "entity_dim");
    host_->mesh_select_entity_dim_threshold_->SetLowerThreshold(entity_dim);
    host_->mesh_select_entity_dim_threshold_->SetUpperThreshold(entity_dim);
    host_->mesh_select_entity_tag_threshold_->SetInputConnection(
        host_->mesh_select_entity_dim_threshold_->GetOutputPort());
    host_->mesh_select_entity_tag_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "entity_tag");
    host_->mesh_select_entity_tag_threshold_->SetLowerThreshold(entity_tag);
    host_->mesh_select_entity_tag_threshold_->SetUpperThreshold(entity_tag);
    current = host_->mesh_select_entity_tag_threshold_->GetOutputPort();
  } else {
    host_->mesh_select_dim_threshold_->SetInputData(host_->mesh_grid_);
    host_->mesh_select_dim_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "phys_dim");
    host_->mesh_select_dim_threshold_->SetLowerThreshold(host_->selection_.group_dim_);
    host_->mesh_select_dim_threshold_->SetUpperThreshold(host_->selection_.group_dim_);
    host_->mesh_select_group_threshold_->SetInputConnection(
        host_->mesh_select_dim_threshold_->GetOutputPort());
    host_->mesh_select_group_threshold_->SetInputArrayToProcess(
        0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "phys_id");
    host_->mesh_select_group_threshold_->SetLowerThreshold(host_->selection_.group_id_);
    host_->mesh_select_group_threshold_->SetUpperThreshold(host_->selection_.group_id_);
    current = host_->mesh_select_group_threshold_->GetOutputPort();
  }

  host_->mesh_select_geom_->SetInputConnection(current);
  host_->mesh_select_geom_->Update();
  if (entity_preview && entity_dim <= 1) {
#ifdef GMP_ENABLE_GMSH_GUI
    host_->mesh_select_occ_geometry_ =
        BuildGmshEntityPreviewGeometry(entity_dim, entity_tag);
#else
    host_->mesh_select_occ_geometry_ = nullptr;
#endif
  } else {
    host_->mesh_select_occ_geometry_ = nullptr;
  }
  vtkDataSet* selected_geometry =
      host_->mesh_select_occ_geometry_
          ? static_cast<vtkDataSet*>(host_->mesh_select_occ_geometry_.GetPointer())
          : static_cast<vtkDataSet*>(host_->mesh_select_geom_->GetOutput());
  if (!selected_geometry || selected_geometry->GetNumberOfCells() <= 0) {
    host_->mesh_select_actor_->SetVisibility(false);
    if (host_->render_window_) {
      host_->render_window_->Render();
    }
    return;
  }

  const double dx = host_->mesh_bounds_[1] - host_->mesh_bounds_[0];
  const double dy = host_->mesh_bounds_[3] - host_->mesh_bounds_[2];
  const double dz = host_->mesh_bounds_[5] - host_->mesh_bounds_[4];
  const double model_diagonal =
      std::max(1e-6, std::sqrt(dx * dx + dy * dy + dz * dz));
  if (entity_preview && entity_dim == 0) {
    if (!host_->mesh_select_point_source_) {
      host_->mesh_select_point_source_ = vtkSmartPointer<vtkSphereSource>::New();
      host_->mesh_select_point_source_->SetThetaResolution(24);
      host_->mesh_select_point_source_->SetPhiResolution(16);
    }
    double bounds[6] = {0, 0, 0, 0, 0, 0};
    selected_geometry->GetBounds(bounds);
    host_->mesh_select_point_source_->SetCenter(0.5 * (bounds[0] + bounds[1]),
                                         0.5 * (bounds[2] + bounds[3]),
                                         0.5 * (bounds[4] + bounds[5]));
    host_->mesh_select_point_source_->SetRadius(0.003 * model_diagonal);
    host_->mesh_select_mapper_->SetInputConnection(
        host_->mesh_select_point_source_->GetOutputPort());
  } else if (entity_preview && entity_dim == 1) {
    if (!host_->mesh_select_curve_tube_) {
      host_->mesh_select_curve_tube_ = vtkSmartPointer<vtkTubeFilter>::New();
      host_->mesh_select_curve_tube_->SetNumberOfSides(12);
      host_->mesh_select_curve_tube_->CappingOn();
    }
    if (host_->mesh_select_occ_geometry_) {
      host_->mesh_select_curve_tube_->SetInputData(host_->mesh_select_occ_geometry_);
    } else {
      host_->mesh_select_curve_tube_->SetInputConnection(
          host_->mesh_select_geom_->GetOutputPort());
    }
    host_->mesh_select_curve_tube_->SetRadius(0.000875 * model_diagonal);
    host_->mesh_select_mapper_->SetInputConnection(
        host_->mesh_select_curve_tube_->GetOutputPort());
  } else {
    host_->mesh_select_mapper_->SetInputConnection(host_->mesh_select_geom_->GetOutputPort());
  }
  host_->mesh_select_actor_->SetVisibility(true);
  if (host_->render_window_) {
    host_->render_window_->Render();
  }
#endif
}


}  // namespace gmp
