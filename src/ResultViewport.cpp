#include "gmp/ResultViewport.h"

#include "gmp/MooseSnapshot.h"
#include "gmp/PhysicalGroupManifest.h"
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
#include <QSet>
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

void ResultViewport::set_exodus_file(const QString& path) {
  host_->current_file_ = path;
  if (!host_->file_label_) {
    return;
  }
  host_->file_label_->setText(path.isEmpty() ? "No file loaded" : path);
#ifdef GMP_ENABLE_VTK_VIEWER
  if (path.isEmpty()) {
    return;
  }

  if (!host_->reader_) {
    host_->reader_ = vtkSmartPointer<vtkExodusIIReader>::New();
  }
  if (!host_->geom_) {
    host_->geom_ = vtkSmartPointer<vtkCompositeDataGeometryFilter>::New();
  }
  if (!host_->block_pad_) {
    host_->block_pad_ = vtkSmartPointer<vtkPadPartialBlockArrays>::New();
  }
  if (!host_->mapper_) {
    host_->mapper_ = vtkSmartPointer<vtkDataSetMapper>::New();
  }
  if (!host_->actor_) {
    host_->actor_ = vtkSmartPointer<vtkActor>::New();
  }
  if (!host_->lut_) {
    host_->lut_ = vtkSmartPointer<vtkLookupTable>::New();
    host_->lut_->SetNumberOfTableValues(256);
    host_->lut_->Build();
  }
  if (!host_->scalar_bar_) {
    host_->scalar_bar_ = vtkSmartPointer<vtkScalarBarActor>::New();
    style_scalar_bar(host_->scalar_bar_);
    position_scalar_bar(host_->scalar_bar_, host_->scalar_bar_pos_);
  }
  host_->mapper_->SetLookupTable(host_->lut_);
  host_->block_pad_->SetInputConnection(host_->reader_->GetOutputPort());
  host_->geom_->SetInputConnection(host_->block_pad_->GetOutputPort());
  host_->mapper_->SetInputConnection(host_->geom_->GetOutputPort());
  host_->actor_->SetMapper(host_->mapper_);
  if (host_->renderer_ && !host_->actor_added_) {
    host_->renderer_->AddActor(host_->actor_);
    host_->scalar_bar_->SetLookupTable(host_->mapper_->GetLookupTable());
    host_->renderer_->AddViewProp(host_->scalar_bar_);
    host_->actor_added_ = true;
  }
  host_->pipeline_ready_ = true;

  host_->first_render_ = true;
  host_->mode_ = VtkViewer::DataMode::Exodus;
  if (host_->actor_) {
    host_->actor_->SetVisibility(1);
  }
  host_->reader_->SetFileName(path.toUtf8().constData());
  host_->reader_->UpdateInformation();
  host_->reader_->SetAllArrayStatus(vtkExodusIIReader::NODAL, 1);
  host_->reader_->SetAllArrayStatus(vtkExodusIIReader::ELEM_BLOCK, 1);
  host_->reader_->SetAllArrayStatus(vtkExodusIIReader::GLOBAL, 1);
  const int n_points = host_->reader_->GetNumberOfPointResultArrays();
  for (int i = 0; i < n_points; ++i) {
    host_->reader_->SetPointResultArrayStatus(
        host_->reader_->GetPointResultArrayName(i), 1);
  }
  const int n_elems = host_->reader_->GetNumberOfElementResultArrays();
  for (int i = 0; i < n_elems; ++i) {
    host_->reader_->SetElementResultArrayStatus(
        host_->reader_->GetElementResultArrayName(i), 1);
  }
  host_->update_time_steps_from_reader(false);
  host_->update_mesh_controls();
  host_->setup_watcher(path);
  host_->update_pipeline();
#else
  Q_UNUSED(path);
#endif
}


void ResultViewport::set_exodus_history(const QStringList& paths) {
  host_->output_combo_->clear();
  for (const auto& p : paths) {
    host_->output_combo_->addItem(QFileInfo(p).fileName(), p);
  }
  if (!paths.isEmpty()) {
    host_->output_combo_->setCurrentIndex(0);
  }
}


QStringList ResultViewport::read_exodus_side_set_names(const QString& path) const {
  QStringList names;
  const PhysicalGroupManifest manifest = read_exodus_mesh_manifest(path);
  const int boundary_dim = manifest.mesh_dim - 1;
  for (const auto& group : manifest.groups) {
    if (group.dim == boundary_dim) {
      names << group.name;
    }
  }
  names.removeDuplicates();
  return names;
}


PhysicalGroupManifest ResultViewport::read_exodus_mesh_manifest(
    const QString& path) const {
  PhysicalGroupManifest manifest;
  if (path.isEmpty() || !QFileInfo::exists(path)) {
    return manifest;
  }
  manifest.mesh_path = QFileInfo(path).absoluteFilePath();
  bool hash_ok = false;
  manifest.mesh_sha256 = sha256_file_hex(manifest.mesh_path, &hash_ok);
  if (!hash_ok) {
    manifest.mesh_sha256.clear();
  }
#ifdef GMP_ENABLE_VTK_VIEWER
  auto reader = vtkSmartPointer<vtkExodusIIReader>::New();
  reader->SetFileName(manifest.mesh_path.toUtf8().constData());
  reader->UpdateInformation();
  manifest.mesh_dim = reader->GetDimensionality();
  manifest.node_count = reader->GetNumberOfNodesInFile();
  manifest.element_count = reader->GetNumberOfElementsInFile();

  QSet<QString> seen;
  auto append_objects = [&](int object_type, int dim) {
    const int count = reader->GetNumberOfObjects(object_type);
    for (int i = 0; i < count; ++i) {
      const char* raw_name = reader->GetObjectName(object_type, i);
      const QString name = raw_name ? QString::fromUtf8(raw_name).trimmed()
                                    : QString();
      if (name.isEmpty() || seen.contains(name)) {
        continue;
      }
      PhysicalGroupEntry entry;
      entry.name = name;
      entry.dim = dim;
      entry.tags = {reader->GetObjectId(object_type, i)};
      entry.entity_count = 1;
      entry.element_count =
          reader->GetNumberOfEntriesInObject(object_type, i);
      manifest.groups.append(entry);
      seen.insert(name);
    }
  };
  append_objects(vtkExodusIIReader::ELEM_BLOCK, manifest.mesh_dim);
  append_objects(vtkExodusIIReader::SIDE_SET, manifest.mesh_dim - 1);
  append_objects(vtkExodusIIReader::NODE_SET, manifest.mesh_dim - 1);

  reader->SetAllArrayStatus(vtkExodusIIReader::ELEM_BLOCK, 1);
  reader->Update();
  auto* out = vtkMultiBlockDataSet::SafeDownCast(reader->GetOutput());
  if (out && out->GetNumberOfBlocks() > 0) {
    auto* blocks = vtkMultiBlockDataSet::SafeDownCast(out->GetBlock(0));
    if (blocks) {
      for (int i = 0; i < blocks->GetNumberOfBlocks(); ++i) {
        auto* data = vtkDataSet::SafeDownCast(blocks->GetBlock(i));
        if (!data || data->GetNumberOfCells() == 0) {
          continue;
        }
        const int cell_type = data->GetCellType(0);
        manifest.element_type =
            cell_type == VTK_HEXAHEDRON
                ? QStringLiteral("HEX8")
                : QString("VTK_%1").arg(cell_type);
        break;
      }
    }
  }
#else
  Q_UNUSED(path);
#endif
  return manifest;
}


QString ResultViewport::plot_snapshot_text() const {
  return host_->cached_plot_text_;
}


QString ResultViewport::plot_stats_snapshot() const {
  return host_->cached_plot_stats_;
}


QString ResultViewport::table_snapshot_text() const {
  return host_->cached_table_text_;
}


QString ResultViewport::table_stats_snapshot() const {
  return host_->cached_table_stats_;
}


void ResultViewport::on_reload() {
  if (!host_->current_file_.isEmpty()) {
    host_->load_file(host_->current_file_);
  }
}


int ResultViewport::current_time_step_index() const {
  return host_->time_slider_ ? host_->time_slider_->value() : 0;
}


void ResultViewport::set_time_step_index(int index) {
  if (!host_->time_slider_ || host_->time_steps_.empty()) {
    return;
  }
  const int clamped = qBound(0, index, static_cast<int>(host_->time_steps_.size()) - 1);
  // 经滑块赋值，让 on_time_changed 统一刷新管线/曲线/表格。
  host_->time_slider_->setValue(clamped);
}


void ResultViewport::on_time_changed(int index) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (host_->mode_ != VtkViewer::DataMode::Exodus) {
    return;
  }
  if (host_->current_file_.isEmpty()) {
    return;
  }
  if (host_->time_steps_.empty()) {
    return;
  }
  if (index < 0 || index >= static_cast<int>(host_->time_steps_.size())) {
    return;
  }
  host_->time_label_->setText(QString("t=%1").arg(host_->time_steps_[index]));
  if (host_->array_combo_->count() == 0) {
    host_->update_pipeline();
  } else {
    host_->refresh_time_only();
  }
  host_->update_vector_tab();
  host_->update_plot_view();
  host_->update_table_view();
#else
  Q_UNUSED(index);
#endif
}


void ResultViewport::on_array_changed(int index) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!host_->mapper_) {
    return;
  }
  if (index < 0) {
    return;
  }
  const QString key = host_->array_combo_->currentData().toString();
  if (key.isEmpty()) {
    return;
  }
  const bool is_point = key.startsWith("P:");
  const QString name = key.mid(2);

  vtkDataSet* data = nullptr;
  if (host_->mode_ == VtkViewer::DataMode::Exodus && host_->mapper_) {
    data = vtkDataSet::SafeDownCast(host_->mapper_->GetInput());
    if (!data && host_->geom_) {
      host_->geom_->Update();
      data = host_->geom_->GetOutput();
    }
  } else if (host_->mode_ == VtkViewer::DataMode::Mesh && host_->mapper_) {
    data = vtkDataSet::SafeDownCast(host_->mapper_->GetInput());
    if (!data && host_->mesh_geom_) {
      host_->mesh_geom_->Update();
      data = host_->mesh_geom_->GetOutput();
    }
  }
  if (!data) {
    return;
  }

  vtkDataArray* array = nullptr;
  if (is_point) {
    host_->mapper_->SetScalarModeToUsePointFieldData();
    array = data->GetPointData()->GetArray(name.toUtf8().constData());
  } else {
    host_->mapper_->SetScalarModeToUseCellFieldData();
    array = data->GetCellData()->GetArray(name.toUtf8().constData());
  }

  if (array) {
    double range[2] = {0.0, 1.0};
    array->GetRange(range);
    host_->mapper_->SelectColorArray(name.toUtf8().constData());
    host_->mapper_->ScalarVisibilityOn();
    if (host_->auto_range_->isChecked()) {
      // 两个端点必须作为一个范围原子更新。逐个 setValue() 会分别触发
      // host_->on_apply_range()；当新最小值大于旧最大值时，VTK 会短暂收到
      // 反向区间（例如 [2, 0]），造成报错和一帧错误的单色显示。
      const QSignalBlocker min_blocker(host_->range_min_);
      const QSignalBlocker max_blocker(host_->range_max_);
      host_->range_min_->setValue(range[0]);
      host_->range_max_->setValue(range[1]);
    }
    // 先应用范围（mapper + LUT），再建表渲染；
    // 反过来的话 apply_lookup_table 内部的 Render 会用旧范围画一帧。
    host_->on_apply_range();
    host_->apply_lookup_table();
    if (host_->scalar_bar_) {
      host_->scalar_bar_->SetTitle(name.toUtf8().constData());
      host_->scalar_bar_->SetLookupTable(host_->mapper_->GetLookupTable());
    }
  } else {
    host_->mapper_->ScalarVisibilityOff();
  }
  if (host_->render_window_) {
    host_->render_window_->Render();
  }
  host_->update_vector_tab();
  host_->update_plot_view();
  host_->update_table_view();
#else
  Q_UNUSED(index);
#endif
  host_->update_array_list();
}


void ResultViewport::populate_arrays() {
#ifdef GMP_ENABLE_VTK_VIEWER
  vtkDataSet* data = nullptr;
  if (host_->mode_ == VtkViewer::DataMode::Exodus && host_->mapper_) {
    data = vtkDataSet::SafeDownCast(host_->mapper_->GetInput());
    if (!data && host_->geom_) {
      host_->geom_->Update();
      data = host_->geom_->GetOutput();
    }
  } else if (host_->mode_ == VtkViewer::DataMode::Mesh && host_->mapper_) {
    data = vtkDataSet::SafeDownCast(host_->mapper_->GetInput());
    if (!data && host_->mesh_geom_) {
      host_->mesh_geom_->Update();
      data = host_->mesh_geom_->GetOutput();
    }
  }
  if (!data) {
    return;
  }

  const QString current = host_->array_combo_->currentData().toString();
  host_->array_combo_->blockSignals(true);
  host_->array_combo_->clear();

  auto* pd = data->GetPointData();
  if (pd) {
    for (int i = 0; i < pd->GetNumberOfArrays(); ++i) {
      const char* name = pd->GetArrayName(i);
      if (name) {
        host_->array_combo_->addItem(QString("Point: %1").arg(name),
                              QString("P:%1").arg(name));
      }
    }
  }

  auto* cd = data->GetCellData();
  if (cd) {
    for (int i = 0; i < cd->GetNumberOfArrays(); ++i) {
      const char* name = cd->GetArrayName(i);
      if (name) {
        host_->array_combo_->addItem(QString("Cell: %1").arg(name),
                              QString("C:%1").arg(name));
      }
    }
  }

  host_->array_combo_->blockSignals(false);

  int idx = host_->array_combo_->findData(current);
  if (idx < 0 && host_->mode_ == VtkViewer::DataMode::Mesh) {
    const QString preferred =
        (host_->show_quality_ && host_->show_quality_->isChecked()) ? "C:Quality" : "C:phys_id";
    idx = host_->array_combo_->findData(preferred);
  }
  if (idx < 0 && host_->array_combo_->count() > 0) {
    idx = 0;
    for (int i = 0; i < host_->array_combo_->count(); ++i) {
      const QString label = host_->array_combo_->itemText(i);
      if (!label.contains("ObjectId", Qt::CaseInsensitive)) {
        idx = i;
        break;
      }
    }
  }
  if (idx >= 0) {
    host_->array_combo_->setCurrentIndex(idx);
    host_->on_array_changed(idx);
  } else {
    host_->mapper_->ScalarVisibilityOff();
  }
  host_->update_array_list();
#endif
}


void ResultViewport::update_array_list() {
  if (!host_->array_list_ || !host_->array_combo_) {
    return;
  }
  const QString filter =
      host_->array_filter_ ? host_->array_filter_->currentData().toString() : "all";
  const QString current = host_->array_combo_->currentData().toString();
  host_->array_list_->blockSignals(true);
  host_->array_list_->clear();
  for (int i = 0; i < host_->array_combo_->count(); ++i) {
    const QString key = host_->array_combo_->itemData(i).toString();
    if (filter == "P" && !key.startsWith("P:")) {
      continue;
    }
    if (filter == "C" && !key.startsWith("C:")) {
      continue;
    }
    auto* item = new QListWidgetItem(host_->array_combo_->itemText(i), host_->array_list_);
    item->setData(Qt::UserRole, key);
    if (key == current) {
      host_->array_list_->setCurrentItem(item);
      item->setSelected(true);
    }
  }
  host_->array_list_->blockSignals(false);
}


void ResultViewport::update_vector_tab() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!host_->vector_array_combo_ || !host_->vector_info_) {
    return;
  }
  const bool vector_mode =
      (host_->mode_ == VtkViewer::DataMode::Exodus || host_->mode_ == VtkViewer::DataMode::Mesh);
  host_->vector_array_combo_->setEnabled(vector_mode);
  host_->vector_apply_to_deform_->setEnabled(vector_mode);
  host_->vector_auto_sync_deform_->setEnabled(vector_mode);

  vtkDataSet* data = nullptr;
  if (host_->mode_ == VtkViewer::DataMode::Exodus && host_->mapper_) {
    data = vtkDataSet::SafeDownCast(host_->mapper_->GetInput());
    if (!data && host_->geom_) {
      host_->geom_->Update();
      data = host_->geom_->GetOutput();
    }
  } else if (host_->mode_ == VtkViewer::DataMode::Mesh && host_->mapper_) {
    data = vtkDataSet::SafeDownCast(host_->mapper_->GetInput());
    if (!data && host_->mesh_geom_) {
      host_->mesh_geom_->Update();
      data = host_->mesh_geom_->GetOutput();
    }
  }
  if (!data) {
    host_->vector_array_combo_->clear();
    host_->vector_info_->setText("No vector data loaded");
    return;
  }

  const QString prev = host_->vector_array_combo_->currentData().toString();
  int best_index = -1;
  int selected_index = -1;
  host_->vector_array_combo_->blockSignals(true);
  host_->vector_array_combo_->clear();
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
      const int idx = host_->vector_array_combo_->count();
      host_->vector_array_combo_->addItem(QString("Point: %1").arg(name), QString("P:%1").arg(name));
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
      const int idx = host_->vector_array_combo_->count();
      host_->vector_array_combo_->addItem(QString("Cell: %1").arg(name), QString("C:%1").arg(name));
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
    target_index = host_->vector_array_combo_->findData(prev);
    if (selected_index >= 0) {
      target_index = selected_index;
    }
  }
  if (target_index < 0 && best_index >= 0) {
    target_index = best_index;
  }
  if (target_index < 0 && host_->vector_array_combo_->count() > 0) {
    target_index = 0;
  }
  if (target_index >= 0) {
    host_->vector_array_combo_->setCurrentIndex(target_index);
  }
  host_->vector_array_combo_->blockSignals(false);

  if (host_->vector_array_combo_->count() == 0) {
    host_->vector_array_combo_->setEnabled(false);
    host_->vector_apply_to_deform_->setEnabled(false);
    host_->vector_auto_sync_deform_->setEnabled(false);
    host_->vector_info_->setText("No vector arrays (>=2 components)");
    return;
  }

  const QString key = host_->vector_array_combo_->currentData().toString();
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
    host_->vector_info_->setText("No selectable vector array");
    return;
  }
  const VectorStats stats = AnalyzeVectorArray(target);
  if (!stats.has_data) {
    host_->vector_info_->setText("Array must have 2+ components");
    return;
  }
  const QString sample = ArrayValueSample(target, 0);
  host_->vector_info_->setText(
      QString("%1 | %2 | sample[0]=%3")
          .arg(FormatVectorStatsText(stats))
          .arg(key)
          .arg(sample));

  if (host_->deform_vector_ && host_->vector_auto_sync_deform_ &&
      host_->vector_auto_sync_deform_->isChecked()) {
    host_->deform_vector_->blockSignals(true);
    const int deform_index = host_->deform_vector_->findText(name);
    if (deform_index >= 0) {
      host_->deform_vector_->setCurrentIndex(deform_index);
    }
    host_->deform_vector_->blockSignals(false);
  }
  if (host_->deform_enable_) {
    host_->update_deformation_pipeline();
    host_->update_scene_extras();
    if (host_->render_window_) {
      host_->render_window_->Render();
    }
  }
#else
  Q_UNUSED(host_);
#endif
}


void ResultViewport::update_plot_view() {
#ifndef GMP_ENABLE_VTK_VIEWER
  host_->cached_plot_text_ = QString::fromUtf8("vtk disabled");
  host_->cached_plot_stats_ = QString::fromUtf8("vtk disabled");
  return;
#endif
  vtkDataSet* data = nullptr;
  if (host_->mode_ == VtkViewer::DataMode::Exodus && host_->mapper_) {
    data = vtkDataSet::SafeDownCast(host_->mapper_->GetInput());
    if (!data && host_->geom_) {
      host_->geom_->Update();
      data = host_->geom_->GetOutput();
    }
  } else if (host_->mode_ == VtkViewer::DataMode::Mesh && host_->mapper_) {
    data = vtkDataSet::SafeDownCast(host_->mapper_->GetInput());
    if (!data && host_->mesh_geom_) {
      host_->mesh_geom_->Update();
      data = host_->mesh_geom_->GetOutput();
    }
  }
  if (!data) {
    host_->cached_plot_text_ = QString::fromUtf8("No data");
    host_->cached_plot_stats_ = QString::fromUtf8("No data");
    return;
  }

  QString key;
  if (host_->array_combo_ && host_->array_combo_->count() > 0) {
    key = host_->array_combo_->currentData().toString();
  }
  if (key.isEmpty() && host_->vector_array_combo_ && host_->vector_array_combo_->count() > 0) {
    key = host_->vector_array_combo_->currentData().toString();
  }
  if (key.isEmpty()) {
    host_->cached_plot_text_ = QString::fromUtf8("No array selected");
    host_->cached_plot_stats_ = QString::fromUtf8("No array selected");
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
    host_->cached_plot_text_ = QString::fromUtf8("Selected array not found");
    host_->cached_plot_stats_ = QString::fromUtf8("Invalid array");
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
  host_->cached_plot_text_ = lines.join('\n');
  host_->cached_plot_stats_ = QString("mode=%1 tuples=%2")
                           .arg(host_->mode_ == VtkViewer::DataMode::Mesh ? "mesh" : "exodus")
                           .arg(tuples);
}


void ResultViewport::update_table_view() {
#ifndef GMP_ENABLE_VTK_VIEWER
  host_->cached_table_text_ = QString::fromUtf8("vtk disabled");
  host_->cached_table_stats_ = QString::fromUtf8("vtk disabled");
  return;
#endif
  vtkDataSet* data = nullptr;
  if (host_->mode_ == VtkViewer::DataMode::Exodus && host_->mapper_) {
    data = vtkDataSet::SafeDownCast(host_->mapper_->GetInput());
    if (!data && host_->geom_) {
      host_->geom_->Update();
      data = host_->geom_->GetOutput();
    }
  } else if (host_->mode_ == VtkViewer::DataMode::Mesh && host_->mapper_) {
    data = vtkDataSet::SafeDownCast(host_->mapper_->GetInput());
    if (!data && host_->mesh_geom_) {
      host_->mesh_geom_->Update();
      data = host_->mesh_geom_->GetOutput();
    }
  }
  if (!data) {
    host_->cached_table_text_ = QString::fromUtf8("No data");
    host_->cached_table_stats_ = QString::fromUtf8("No data");
    return;
  }

  QString key;
  if (host_->array_combo_ && host_->array_combo_->count() > 0) {
    key = host_->array_combo_->currentData().toString();
  }
  if (key.isEmpty() && host_->vector_array_combo_ && host_->vector_array_combo_->count() > 0) {
    key = host_->vector_array_combo_->currentData().toString();
  }
  if (key.isEmpty()) {
    host_->cached_table_text_ = QString::fromUtf8("No array selected");
    host_->cached_table_stats_ = QString::fromUtf8("No array selected");
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
    host_->cached_table_text_ = QString::fromUtf8("Invalid array");
    host_->cached_table_stats_ = QString::fromUtf8("Invalid array");
    return;
  }

  const int comps = array->GetNumberOfComponents();
  const vtkIdType tuples = array->GetNumberOfTuples();
  const int show_rows = qBound(1, host_->table_rows_, 5000);
  const vtkIdType rows = std::min<vtkIdType>(tuples, show_rows);

  QStringList headers;
  headers << "Index";
  for (int i = 0; i < comps; ++i) {
    headers << QString("C%1").arg(i);
  }
  if (comps > 1) {
    headers << "Magnitude";
  }
  QStringList text_rows;
  text_rows << QString("Array: %1").arg(key);
  text_rows << QString("Tuples: %1").arg(tuples);
  text_rows << QString("Showing rows: %1").arg(rows);
  text_rows << "";
  text_rows << headers.join('\t');
  for (vtkIdType i = 0; i < rows; ++i) {
    QStringList row_text;
    row_text << QString::number(i);
    for (int c = 0; c < comps; ++c) {
      row_text << QString::number(array->GetComponent(i, c), 'g', 6);
    }
    if (comps > 1) {
      row_text << QString::number(ComputeMagnitude(array, i), 'g', 6);
    }
    text_rows << row_text.join('\t');
  }
  if (tuples > rows) {
    text_rows << QString("... omitted %1 rows ...").arg(tuples - rows);
  }
  host_->cached_table_text_ = text_rows.join('\n');
  if (comps > 1) {
    VectorStats stats = AnalyzeVectorArray(array);
    if (stats.has_data) {
      host_->cached_table_stats_ =
          QString("mode=%1, tuples=%2, show=%3, %4")
              .arg(host_->mode_ == VtkViewer::DataMode::Mesh ? "mesh" : "exodus")
              .arg(tuples)
              .arg(rows)
              .arg(FormatVectorStatsText(stats));
    } else {
      host_->cached_table_stats_ =
          QString("tuples=%1, show=%2, components=%3")
              .arg(tuples)
              .arg(rows)
              .arg(comps);
    }
  } else {
    host_->cached_table_stats_ =
        QString("tuples=%1, show=%2, components=%3")
            .arg(tuples)
            .arg(rows)
            .arg(comps);
  }
}


void ResultViewport::update_vector_list() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!host_->deform_vector_) {
    return;
  }
  if (host_->mode_ != VtkViewer::DataMode::Exodus) {
    host_->deform_vector_->blockSignals(true);
    host_->deform_vector_->clear();
    host_->deform_vector_->blockSignals(false);
    host_->deform_vector_->setEnabled(false);
    return;
  }

  vtkDataSet* data = nullptr;
  if (host_->mapper_) {
    data = vtkDataSet::SafeDownCast(host_->mapper_->GetInput());
  }
  if (!data && host_->geom_) {
    host_->geom_->Update();
    data = host_->geom_->GetOutput();
  }
  if (!data) {
    return;
  }

  auto* pd = data->GetPointData();
  if (!pd) {
    return;
  }

  const QString current = host_->deform_vector_->currentData().toString();
  int best_index = -1;
  host_->deform_vector_->blockSignals(true);
  host_->deform_vector_->clear();
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
    const int idx = host_->deform_vector_->count();
    host_->deform_vector_->addItem(label, label);
    const QString lower = label.toLower();
    if (best_index < 0 &&
        (lower.contains("disp") || lower.contains("displacement"))) {
      best_index = idx;
    }
  }
  int select_idx = -1;
  if (!current.isEmpty()) {
    select_idx = host_->deform_vector_->findData(current);
  }
  if (select_idx < 0) {
    select_idx = best_index;
  }
  if (select_idx < 0 && host_->deform_vector_->count() > 0) {
    select_idx = 0;
  }
  if (select_idx >= 0) {
    host_->deform_vector_->setCurrentIndex(select_idx);
  }
  host_->deform_vector_->blockSignals(false);
  host_->deform_vector_->setEnabled(host_->deform_vector_->count() > 0);
  host_->update_deformation_pipeline();
#endif
}


void ResultViewport::update_deformation_pipeline() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (host_->mode_ != VtkViewer::DataMode::Exodus || !host_->mapper_ || !host_->geom_) {
    return;
  }
  const bool enabled = host_->deform_enable_ && host_->deform_enable_->isChecked();
  const QString vector_name =
      host_->deform_vector_ ? host_->deform_vector_->currentData().toString() : "";
  if (!enabled || vector_name.isEmpty()) {
    host_->mapper_->SetInputConnection(host_->geom_->GetOutputPort());
    return;
  }
  if (!host_->warp_filter_) {
    host_->warp_filter_ = vtkSmartPointer<vtkWarpVector>::New();
  }
  host_->warp_filter_->SetInputConnection(host_->geom_->GetOutputPort());
  host_->warp_filter_->SetScaleFactor(
      host_->deform_scale_ ? host_->deform_scale_->value() : 1.0);
  host_->warp_filter_->SetInputArrayToProcess(
      0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS,
      vector_name.toUtf8().constData());
  host_->mapper_->SetInputConnection(host_->warp_filter_->GetOutputPort());
#endif
}


void ResultViewport::on_open_file() {
  const QString path =
      QFileDialog::getOpenFileName(host_, "Open Result or Mesh", host_->current_file_,
                                   "Exodus (*.e);;Gmsh Mesh (*.msh)");
  if (!path.isEmpty()) {
    host_->load_file(path);
  }
}


void ResultViewport::on_apply_range() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!host_->mapper_) {
    return;
  }
  const bool auto_range = host_->auto_range_->isChecked();
  host_->range_min_->setEnabled(!auto_range);
  host_->range_max_->setEnabled(!auto_range);
  // 自动范围时 on_array_changed 已把当前数组范围写入 host_->range_min_/host_->range_max_。
  // 这里必须同步到 mapper，否则 LUT 停留在默认 0-1，云图恒为均匀色。
  host_->mapper_->SetScalarRange(host_->range_min_->value(), host_->range_max_->value());
  if (host_->lut_) {
    // 色标刻度取自 LUT 自身的 Range；渲染时 painter 回写 Range 的时机
    // 晚于色标的刻度布局，会导致切换变量后刻度滞后一帧，这里显式同步。
    host_->lut_->SetRange(host_->range_min_->value(), host_->range_max_->value());
  }
  if (host_->render_window_) {
    host_->render_window_->Render();
  }
#endif
}


void ResultViewport::on_auto_refresh_toggled(bool enabled) {
  host_->set_refresh_enabled(enabled);
}


void ResultViewport::on_auto_refresh_tick() {
  host_->refresh_from_disk();
}


void ResultViewport::set_refresh_enabled(bool enabled) {
  if (enabled) {
    host_->refresh_timer_->start(host_->refresh_ms_->value());
  } else {
    host_->refresh_timer_->stop();
  }
}


void ResultViewport::setup_watcher(const QString& file_path) {
  if (!host_->watcher_) {
    host_->watcher_ = new QFileSystemWatcher(host_);
    QObject::connect(host_->watcher_, &QFileSystemWatcher::fileChanged, host_,
            [this](const QString&) { host_->schedule_reload(); });
    QObject::connect(host_->watcher_, &QFileSystemWatcher::directoryChanged, host_,
            [this](const QString&) { host_->schedule_reload(); });
  }
  const QStringList watched_files = host_->watcher_->files();
  if (!watched_files.isEmpty()) {
    host_->watcher_->removePaths(watched_files);
  }
  const QStringList watched_directories = host_->watcher_->directories();
  if (!watched_directories.isEmpty()) {
    host_->watcher_->removePaths(watched_directories);
  }

  if (file_path.isEmpty()) {
    return;
  }
  QFileInfo fi(file_path);
  const QString dir = fi.absolutePath();
  if (!dir.isEmpty()) {
    host_->watcher_->addPath(dir);
  }
  if (fi.exists()) {
    host_->watcher_->addPath(fi.absoluteFilePath());
    host_->last_file_size_ = fi.size();
    host_->last_file_mtime_ = fi.lastModified();
  }
}


void ResultViewport::schedule_reload() {
  host_->pending_reload_ = true;
  if (host_->debounce_timer_) {
    host_->debounce_timer_->start(300);
  }
}


void ResultViewport::refresh_from_disk() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (host_->current_file_.isEmpty()) {
    return;
  }
  QFileInfo fi(host_->current_file_);
  if (!fi.exists()) {
    return;
  }
  const bool changed =
      (host_->last_file_size_ != fi.size()) || (host_->last_file_mtime_ != fi.lastModified());
  if (changed) {
    host_->last_file_size_ = fi.size();
    host_->last_file_mtime_ = fi.lastModified();
    if (host_->mode_ == VtkViewer::DataMode::Exodus) {
      host_->update_time_steps_from_reader(true);
    }
    if (host_->array_combo_->count() == 0) {
      host_->populate_arrays();
    }
    if (host_->mode_ == VtkViewer::DataMode::Mesh) {
      host_->set_mesh_file(host_->current_file_);
      return;
    }
  }
  if (host_->mode_ == VtkViewer::DataMode::Exodus) {
    host_->refresh_time_only();
  } else {
    host_->update_pipeline();
  }
#endif
}


void ResultViewport::refresh_time_only() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (host_->mode_ != VtkViewer::DataMode::Exodus) {
    return;
  }
  if (!host_->reader_ || !host_->geom_ || !host_->render_window_) {
    return;
  }
  if (host_->current_file_.isEmpty() || !host_->pipeline_ready_) {
    return;
  }
  if (!host_->time_steps_.empty()) {
    vtkInformation* info = host_->reader_->GetOutputInformation(0);
    if (info) {
      const int idx = host_->time_slider_->value();
      const double t = host_->time_steps_[idx];
      info->Set(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP(), t);
    }
  }
  host_->reader_->Update();
  // 同 update_pipeline：强制下游随新时间步重新执行（reader 输出原地复用，
  // 仅 reader->Update() 不会触发 geom 重算）。
  if (host_->block_pad_) {
    host_->block_pad_->Modified();
  }
  host_->geom_->Update();
  // 时间步变化后场数据已更新，必须重算当前数组范围并刷新映射，
  // 否则色标仍停留在旧时间步（加载时 t=0 全为 0，云图恒为均匀色）。
  if (host_->array_combo_ && host_->array_combo_->count() > 0) {
    host_->on_array_changed(host_->array_combo_->currentIndex());
  }
  host_->render_window_->Render();
#endif
}


void ResultViewport::update_time_steps_from_reader(bool keep_index) {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!host_->reader_ || host_->mode_ != VtkViewer::DataMode::Exodus) {
    return;
  }
  const int prev_index = host_->time_slider_->value();
  // 暂存当前时间步：文件被重写过程中 UpdateInformation 可能瞬时读不到
  // TIME_STEPS，若此时清空滑块状态，setRange(0,0) 会把当前值钳到 0 且
  // 无法恢复（后续 keep_index 只能保住已经被钳掉的 0）。
  std::vector<double> prev_steps;
  prev_steps.swap(host_->time_steps_);
  host_->reader_->UpdateInformation();

  host_->time_steps_.clear();
  vtkInformation* info = host_->reader_->GetOutputInformation(0);
  if (info && info->Has(vtkStreamingDemandDrivenPipeline::TIME_STEPS())) {
    const int len = info->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
    const double* steps =
        info->Get(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
    for (int i = 0; i < len; ++i) {
      host_->time_steps_.push_back(steps[i]);
    }
  }

  if (host_->time_steps_.empty()) {
    if (!prev_steps.empty()) {
      // 瞬时读取失败：恢复原时间步状态，等下一次 watcher/refresh 事件再试。
      host_->time_steps_.swap(prev_steps);
      return;
    }
    host_->time_slider_->blockSignals(true);
    host_->time_slider_->setRange(0, 0);
    host_->time_slider_->setValue(0);
    host_->time_slider_->blockSignals(false);
    host_->time_slider_->setEnabled(false);
    host_->time_label_->setText("t=0");
    return;
  }

  host_->time_slider_->setEnabled(true);
  const int idx = keep_index && prev_index < static_cast<int>(host_->time_steps_.size())
                      ? prev_index
                      : 0;
  // setRange 可能在钳位时触发 valueChanged，必须一并屏蔽，
  // 否则会伪发一次 on_time_changed 把时间步打回 0。
  host_->time_slider_->blockSignals(true);
  host_->time_slider_->setRange(0, static_cast<int>(host_->time_steps_.size()) - 1);
  host_->time_slider_->setValue(idx);
  host_->time_slider_->blockSignals(false);
  host_->time_label_->setText(QString("t=%1").arg(host_->time_steps_[idx]));
#endif
  emit host_->time_steps_changed();
}

void ResultViewport::load_file(const QString& path) {
  if (path.endsWith(".msh", Qt::CaseInsensitive)) {
    host_->set_mesh_file(path);
  } else {
    host_->set_exodus_file(path);
  }
}


}  // namespace gmp
