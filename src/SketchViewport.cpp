#include "gmp/SketchViewport.h"

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

void SketchViewport::set_2d_mode(bool on) {
  host_->mode_2d_ = on;
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!host_->renderer_ || !host_->render_window_) {
    return;
  }
  auto* iren = host_->render_window_->GetInteractor();
  auto* cam = host_->renderer_->GetActiveCamera();
  if (on) {
    if (iren) {
      if (!host_->style_3d_) {
        // GetInteractorStyle 返回 vtkInteractorObserver*, 需向下转换保存
        host_->style_3d_ =
            vtkInteractorStyle::SafeDownCast(iren->GetInteractorStyle());
      }
      if (!host_->style_2d_) {
        host_->style_2d_ = vtkSmartPointer<SketchInteractorStyle>::New();
      }
      if (auto* style = SketchInteractorStyle::SafeDownCast(host_->style_2d_)) {
        style->SetStageMode(host_->sketch_navigation_mode_ < 0
                                ? 0
                                : host_->sketch_navigation_mode_);
      }
      iren->SetInteractorStyle(host_->style_2d_);
    }
    if (cam) {
      cam->SetParallelProjection(true);
      cam->SetPosition(0.0, 0.0, 1.0);
      cam->SetFocalPoint(0.0, 0.0, 0.0);
      cam->SetViewUp(0.0, 1.0, 0.0);
    }
    // ResetCamera 保持视线方向(-Z)并重新取景, 无数据时维持默认俯视
    host_->renderer_->ResetCamera();
    host_->renderer_->ResetCameraClippingRange();
  } else {
    if (iren && host_->style_3d_) {
      iren->SetInteractorStyle(host_->style_3d_);
    }
    if (cam) {
      cam->SetParallelProjection(false);
    }
    host_->renderer_->ResetCamera();
    host_->renderer_->ResetCameraClippingRange();
  }
  host_->render_window_->Render();
#endif
}

// ==================== 草图编辑会话 (WS1) ====================

#ifdef GMP_ENABLE_VTK_VIEWER
namespace {

// 把单个图元离散成折线并追加到 polydata; 同时把特征点(端点/圆心)追加为顶点
void append_sketch_entity(const SketchEntity& e, vtkPoints* pts,
                          vtkCellArray* lines, vtkCellArray* verts) {
  if (!pts || !lines || !verts) {
    return;
  }
  auto add_point = [pts](const SketchPoint2d& p) {
    const vtkIdType id = pts->InsertNextPoint(p.x, p.y, 0.0);
    return id;
  };
  auto add_vertex = [pts, verts](const SketchPoint2d& p) {
    const vtkIdType id = pts->InsertNextPoint(p.x, p.y, 0.0);
    verts->InsertNextCell(1, &id);
  };
  if (e.type == SketchEntityType::Line) {
    const vtkIdType a = add_point(e.p1);
    const vtkIdType b = add_point(e.p2);
    lines->InsertNextCell(2);
    lines->InsertCellPoint(a);
    lines->InsertCellPoint(b);
    add_vertex(e.p1);
    add_vertex(e.p2);
    return;
  }
  // Circle/Arc 离散为折线段
  constexpr double kTwoPi = 6.28318530717958647692;
  double start = 0.0;
  double sweep = kTwoPi;
  if (e.type == SketchEntityType::Arc) {
    start = e.start_angle;
    sweep = e.end_angle - e.start_angle;
    while (sweep <= 0.0) {
      sweep += kTwoPi;  // 约定逆时针为正
    }
  }
  int n = static_cast<int>(sweep / kTwoPi * 96.0);
  n = std::max(8, std::min(96, n));
  std::vector<vtkIdType> ids;
  ids.reserve(static_cast<size_t>(n) + 1);
  for (int i = 0; i <= n; ++i) {
    const double ang = start + sweep * static_cast<double>(i) / n;
    SketchPoint2d p{e.center.x + e.radius * std::cos(ang),
                    e.center.y + e.radius * std::sin(ang)};
    ids.push_back(add_point(p));
  }
  lines->InsertNextCell(static_cast<vtkIdType>(ids.size()));
  for (const vtkIdType id : ids) {
    lines->InsertCellPoint(id);
  }
  add_vertex(e.center);
  if (e.type == SketchEntityType::Arc) {
    SketchPoint2d p1, p2;
    if (SketchDocument::point_at_role(e, SketchPointRole::Start, &p1)) {
      add_vertex(p1);
    }
    if (SketchDocument::point_at_role(e, SketchPointRole::End, &p2)) {
      add_vertex(p2);
    }
  }
}

// 图元可用于 Coincident 约束的角色点集合
std::vector<SketchPointRole> snap_roles_for(const SketchEntity& e) {
  switch (e.type) {
    case SketchEntityType::Line:
      return {SketchPointRole::Start, SketchPointRole::End};
    case SketchEntityType::Arc:
      return {SketchPointRole::Start, SketchPointRole::End,
              SketchPointRole::Center};
    case SketchEntityType::Circle:
      return {SketchPointRole::Center};
  }
  return {};
}

double dist2d(const SketchPoint2d& a, const SketchPoint2d& b) {
  return std::hypot(a.x - b.x, a.y - b.y);
}

}  // namespace
#endif


void SketchViewport::set_sketch_document(SketchDocument* doc) {
  host_->sketch_preview_only_ = false;
  host_->sketch_navigation_mode_ = -1;
  host_->sketch_tool_ = SketchToolSelect;
  host_->sketch_dragging_ = false;
  host_->sketch_drag_changed_ = false;
  host_->sketch_drag_entities_.clear();
  if (host_->sketch_doc_ == doc) {
    host_->refresh_sketch();
    emit host_->sketch_tool_changed(host_->sketch_tool_);
    emit host_->stage_picking_changed(doc != nullptr &&
                               host_->sketch_tool_ == SketchToolSelect);
    return;
  }
  const bool had_doc = (host_->sketch_doc_ != nullptr);
  host_->sketch_doc_ = doc;
  host_->sketch_selection_.clear();
  host_->sketch_stage_ = 0;
#ifdef GMP_ENABLE_VTK_VIEWER
  if (doc) {
    // 进入草图会话: 隐藏 3D 场景内容并暂存其可见性 (退出时恢复),
    // 避免 2D 草图与 3D 网格/结果叠显。仅在新会话 (之前无 doc) 时暂存,
    // 直接切换草图 (doc->doc) 保留首次暂存值。
    if (!had_doc) {
      if (host_->actor_) {
        host_->pre_sketch_vis_main_ = host_->actor_->GetVisibility() != 0;
        host_->actor_->SetVisibility(0);
      }
      if (host_->nodes_actor_) {
        host_->pre_sketch_vis_nodes_ = host_->nodes_actor_->GetVisibility() != 0;
        host_->nodes_actor_->SetVisibility(0);
      }
      if (host_->outline_actor_) {
        host_->pre_sketch_vis_outline_ = host_->outline_actor_->GetVisibility() != 0;
        host_->outline_actor_->SetVisibility(0);
      }
      if (host_->scalar_bar_) {
        host_->pre_sketch_vis_scalar_bar_ = host_->scalar_bar_->GetVisibility() != 0;
        host_->scalar_bar_->SetVisibility(0);
      }
      if (host_->mesh_select_actor_) {
        host_->pre_sketch_vis_select_ = host_->mesh_select_actor_->GetVisibility() != 0;
        host_->mesh_select_actor_->SetVisibility(0);
      }
    }
    // 一次性创建草图 actor: 底层图元 / 选中高亮 / 橡皮筋预览
    if (!host_->sketch_actor_ && host_->renderer_) {
      host_->sketch_mapper_ = vtkSmartPointer<vtkPolyDataMapper>::New();
      host_->sketch_actor_ = vtkSmartPointer<vtkActor>::New();
      host_->sketch_actor_->SetMapper(host_->sketch_mapper_);
      host_->sketch_actor_->GetProperty()->SetColor(0.75, 0.85, 0.95);
      host_->sketch_actor_->GetProperty()->SetLineWidth(2.0);
      host_->sketch_actor_->GetProperty()->SetPointSize(7.0);
      host_->sketch_actor_->SetPickable(0);

      host_->sketch_sel_mapper_ = vtkSmartPointer<vtkPolyDataMapper>::New();
      host_->sketch_sel_actor_ = vtkSmartPointer<vtkActor>::New();
      host_->sketch_sel_actor_->SetMapper(host_->sketch_sel_mapper_);
      host_->sketch_sel_actor_->GetProperty()->SetColor(1.0, 0.55, 0.10);
      host_->sketch_sel_actor_->GetProperty()->SetLineWidth(3.0);
      host_->sketch_sel_actor_->GetProperty()->SetPointSize(10.0);
      host_->sketch_sel_actor_->SetPickable(0);

      host_->sketch_preview_mapper_ = vtkSmartPointer<vtkPolyDataMapper>::New();
      host_->sketch_preview_actor_ = vtkSmartPointer<vtkActor>::New();
      host_->sketch_preview_actor_->SetMapper(host_->sketch_preview_mapper_);
      host_->sketch_preview_actor_->GetProperty()->SetColor(0.95, 0.90, 0.20);
      host_->sketch_preview_actor_->GetProperty()->SetLineWidth(2.0);
      host_->sketch_preview_actor_->SetPickable(0);

      host_->renderer_->AddActor(host_->sketch_actor_);
      host_->renderer_->AddActor(host_->sketch_sel_actor_);
      host_->renderer_->AddActor(host_->sketch_preview_actor_);
    }
    host_->rebuild_sketch_actors();
    host_->set_2d_mode(true);
    // 取景到草图范围 (空草图给默认视野, 单位毫米)
    if (host_->renderer_) {
      double b[6] = {-50.0, 50.0, -50.0, 50.0, -1.0, 1.0};
      bool has = false;
      for (const auto& e : doc->entities()) {
        double x0, x1, y0, y1;
        if (e.type == SketchEntityType::Line) {
          x0 = std::min(e.p1.x, e.p2.x);
          x1 = std::max(e.p1.x, e.p2.x);
          y0 = std::min(e.p1.y, e.p2.y);
          y1 = std::max(e.p1.y, e.p2.y);
        } else {
          x0 = e.center.x - e.radius;
          x1 = e.center.x + e.radius;
          y0 = e.center.y - e.radius;
          y1 = e.center.y + e.radius;
        }
        if (!has) {
          b[0] = x0; b[1] = x1; b[2] = y0; b[3] = y1;
          has = true;
        } else {
          b[0] = std::min(b[0], x0); b[1] = std::max(b[1], x1);
          b[2] = std::min(b[2], y0); b[3] = std::max(b[3], y1);
        }
      }
      if (has) {
        const double px = std::max(10.0, (b[1] - b[0]) * 0.15);
        const double py = std::max(10.0, (b[3] - b[2]) * 0.15);
        b[0] -= px; b[1] += px; b[2] -= py; b[3] += py;
      }
      // 空草图也用默认视野取景, 避免空场景 ResetCamera 留下过小视野
      host_->renderer_->ResetCamera(b);
      host_->renderer_->ResetCameraClippingRange();
      if (host_->render_window_) {
        host_->render_window_->Render();
      }
    }
  } else {
    // 退出草图编辑: 隐藏草图 actor, 恢复 3D 场景可见性与 3D 视图
    if (host_->sketch_actor_) {
      host_->sketch_actor_->SetVisibility(0);
      host_->sketch_sel_actor_->SetVisibility(0);
      host_->sketch_preview_actor_->SetVisibility(0);
    }
    if (host_->actor_) {
      host_->actor_->SetVisibility(host_->pre_sketch_vis_main_ ? 1 : 0);
    }
    if (host_->nodes_actor_) {
      host_->nodes_actor_->SetVisibility(host_->pre_sketch_vis_nodes_ ? 1 : 0);
    }
    if (host_->outline_actor_) {
      host_->outline_actor_->SetVisibility(host_->pre_sketch_vis_outline_ ? 1 : 0);
    }
    if (host_->scalar_bar_) {
      host_->scalar_bar_->SetVisibility(host_->pre_sketch_vis_scalar_bar_ ? 1 : 0);
    }
    if (host_->mesh_select_actor_) {
      host_->mesh_select_actor_->SetVisibility(host_->pre_sketch_vis_select_ ? 1 : 0);
    }
    host_->set_2d_mode(false);
  }
#endif
  emit host_->sketch_tool_changed(host_->sketch_tool_);
  emit host_->stage_picking_changed(doc != nullptr);
  emit host_->sketch_selection_changed();
}


void SketchViewport::set_sketch_preview(const SketchDocument* doc) {
  if (!doc) {
    host_->set_sketch_document(nullptr);
    return;
  }
  // 必须先复制再切换指针：调用方通常会在本函数返回后释放编辑文档。
  host_->sketch_preview_doc_ = *doc;
  host_->set_sketch_document(&host_->sketch_preview_doc_);
  host_->sketch_preview_only_ = true;
  host_->sketch_tool_ = SketchToolSelect;
  host_->sketch_navigation_mode_ = -1;
  host_->sketch_selection_.clear();
  host_->sketch_stage_ = 0;
  host_->rebuild_sketch_actors();
}


void SketchViewport::set_sketch_tool(int tool) {
  if (host_->sketch_preview_only_ && tool != SketchToolSelect) {
    emit host_->stage_command_feedback(
        "草图预览为只读；可选择图元，整体平移请使用鼠标中键拖动。");
    emit host_->sketch_tool_changed(SketchToolSelect);
    emit host_->stage_picking_changed(true);
    return;
  }
  host_->sketch_tool_ = qBound(static_cast<int>(SketchToolSelect), tool,
                        static_cast<int>(SketchToolMove));
  host_->sketch_navigation_mode_ = -1;
  host_->sketch_dragging_ = false;
  host_->sketch_drag_changed_ = false;
  host_->sketch_drag_entities_.clear();
#ifdef GMP_ENABLE_VTK_VIEWER
  if (auto* style = SketchInteractorStyle::SafeDownCast(host_->style_2d_)) {
    style->SetStageMode(0);
  }
  if (host_->vtk_widget_) {
    host_->vtk_widget_->setCursor(host_->sketch_tool_ == SketchToolMove
                               ? Qt::SizeAllCursor
                               : Qt::ArrowCursor);
  }
#endif
  emit host_->sketch_tool_changed(host_->sketch_tool_);
  emit host_->stage_picking_changed(host_->sketch_tool_ == SketchToolSelect);
  if (host_->sketch_tool_ == SketchToolMove) {
    emit host_->stage_command_feedback(
        "移动图形：左键拖动完整图形；Option/Alt 选择子图元，Shift 可多选。");
  } else if (host_->sketch_tool_ == SketchToolSelect) {
    emit host_->stage_command_feedback(
        "草图选择：单击完整图形；Option/Alt 选择子图元，Shift 可多选。");
  }
  // 切换工具时取消进行中的绘制
  if (host_->sketch_stage_ != 0) {
    host_->sketch_stage_ = 0;
    host_->update_sketch_preview();
  }
}


void SketchViewport::refresh_sketch() {
  host_->rebuild_sketch_actors();
}


bool SketchViewport::sketch_display_to_world(int x, int y, SketchPoint2d* out) const {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!host_->renderer_ || !out) {
    return false;
  }
  // 取世界原点对应的显示深度, 保证换算结果落在 Z=0 平面上
  host_->renderer_->SetWorldPoint(0.0, 0.0, 0.0, 1.0);
  host_->renderer_->WorldToDisplay();
  const double z0 = host_->renderer_->GetDisplayPoint()[2];
  host_->renderer_->SetDisplayPoint(static_cast<double>(x), static_cast<double>(y), z0);
  host_->renderer_->DisplayToWorld();
  const double* w = host_->renderer_->GetWorldPoint();
  if (w[3] == 0.0) {
    return false;
  }
  out->x = w[0] / w[3];
  out->y = w[1] / w[3];
  return true;
#else
  Q_UNUSED(x);
  Q_UNUSED(y);
  Q_UNUSED(out);
  return false;
#endif
}


double SketchViewport::sketch_pick_tol() const {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (host_->renderer_) {
    // 10 像素对应的世界长度作为容差 (平行投影下与位置无关)
    SketchPoint2d a, b;
    if (host_->sketch_display_to_world(0, 0, &a) && host_->sketch_display_to_world(10, 0, &b)) {
      const double d = dist2d(a, b);
      if (d > 0.0) {
        return d;
      }
    }
  }
#endif
  return 1e-3;
}


SketchPoint2d SketchViewport::sketch_snap(const SketchPoint2d& pt) const {
  if (!host_->sketch_doc_) {
    return pt;
  }
  const double tol = host_->sketch_pick_tol();
  SketchPoint2d best = pt;
  double best_d = tol;
  for (const auto& e : host_->sketch_doc_->entities()) {
    for (const SketchPointRole role : snap_roles_for(e)) {
      SketchPoint2d p;
      if (!SketchDocument::point_at_role(e, role, &p)) {
        continue;
      }
      const double d = dist2d(pt, p);
      if (d <= best_d) {
        best_d = d;
        best = p;
      }
    }
  }
  return best;
}


void SketchViewport::set_sketch_selection(const QList<int>& ids) {
  // 剔除已不存在的图元 id
  QList<int> pruned;
  if (host_->sketch_doc_) {
    for (const int id : ids) {
      if (host_->sketch_doc_->entity(id) && !pruned.contains(id)) {
        pruned.append(id);
      }
    }
  }
  if (pruned == host_->sketch_selection_) {
    return;
  }
  host_->sketch_selection_ = pruned;
  emit host_->sketch_selection_changed();
  host_->rebuild_sketch_actors();
}


void SketchViewport::rebuild_sketch_actors() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!host_->sketch_actor_ || !host_->renderer_) {
    return;
  }
  auto pts = vtkSmartPointer<vtkPoints>::New();
  auto lines = vtkSmartPointer<vtkCellArray>::New();
  auto verts = vtkSmartPointer<vtkCellArray>::New();
  auto spts = vtkSmartPointer<vtkPoints>::New();
  auto slines = vtkSmartPointer<vtkCellArray>::New();
  auto sverts = vtkSmartPointer<vtkCellArray>::New();
  if (host_->sketch_doc_) {
    for (const auto& e : host_->sketch_doc_->entities()) {
      if (host_->sketch_selection_.contains(e.id)) {
        append_sketch_entity(e, spts, slines, sverts);
      } else {
        append_sketch_entity(e, pts, lines, verts);
      }
    }
  }
  auto poly = vtkSmartPointer<vtkPolyData>::New();
  poly->SetPoints(pts);
  poly->SetLines(lines);
  poly->SetVerts(verts);
  host_->sketch_mapper_->SetInputData(poly);
  auto spoly = vtkSmartPointer<vtkPolyData>::New();
  spoly->SetPoints(spts);
  spoly->SetLines(slines);
  spoly->SetVerts(sverts);
  host_->sketch_sel_mapper_->SetInputData(spoly);
  host_->sketch_actor_->SetVisibility(1);
  host_->sketch_sel_actor_->SetVisibility(1);
  host_->sketch_preview_actor_->SetVisibility(1);
  host_->update_sketch_preview();
  if (host_->render_window_) {
    host_->render_window_->Render();
  }
#endif
}


void SketchViewport::update_sketch_preview() {
#ifdef GMP_ENABLE_VTK_VIEWER
  if (!host_->sketch_preview_actor_) {
    return;
  }
  auto pts = vtkSmartPointer<vtkPoints>::New();
  auto lines = vtkSmartPointer<vtkCellArray>::New();
  auto verts = vtkSmartPointer<vtkCellArray>::New();
  if (host_->sketch_doc_ && host_->sketch_stage_ > 0) {
    const SketchPoint2d cur = host_->sketch_snap(host_->sketch_cursor_);
    if (host_->sketch_tool_ == SketchToolDrawLine && host_->sketch_stage_ == 1) {
      SketchEntity e;
      e.type = SketchEntityType::Line;
      e.p1 = host_->sketch_anchor1_;
      e.p2 = cur;
      append_sketch_entity(e, pts, lines, verts);
    } else if (host_->sketch_tool_ == SketchToolDrawCircle && host_->sketch_stage_ == 1) {
      const double r = dist2d(host_->sketch_anchor1_, cur);
      if (r > 0.0) {
        SketchEntity e;
        e.type = SketchEntityType::Circle;
        e.center = host_->sketch_anchor1_;
        e.radius = r;
        append_sketch_entity(e, pts, lines, verts);
      }
    } else if (host_->sketch_tool_ == SketchToolDrawRectangle && host_->sketch_stage_ == 1) {
      // 轴对齐矩形橡皮筋: anchor1 与当前点为对角
      const double x1 = host_->sketch_anchor1_.x, y1 = host_->sketch_anchor1_.y;
      const double x2 = cur.x, y2 = cur.y;
      const SketchPoint2d corners[4] = {{x1, y1}, {x2, y1}, {x2, y2}, {x1, y2}};
      for (int i = 0; i < 4; ++i) {
        SketchEntity e;
        e.type = SketchEntityType::Line;
        e.p1 = corners[i];
        e.p2 = corners[(i + 1) % 4];
        append_sketch_entity(e, pts, lines, verts);
      }
    } else if (host_->sketch_tool_ == SketchToolDrawArc) {
      if (host_->sketch_stage_ == 1) {
        // 圆心到鼠标的半径引导线
        SketchEntity e;
        e.type = SketchEntityType::Line;
        e.p1 = host_->sketch_anchor1_;
        e.p2 = cur;
        append_sketch_entity(e, pts, lines, verts);
      } else if (host_->sketch_stage_ == 2) {
        const double r = dist2d(host_->sketch_anchor1_, host_->sketch_anchor2_);
        if (r > 0.0) {
          SketchEntity e;
          e.type = SketchEntityType::Arc;
          e.center = host_->sketch_anchor1_;
          e.radius = r;
          e.start_angle =
              std::atan2(host_->sketch_anchor2_.y - host_->sketch_anchor1_.y,
                         host_->sketch_anchor2_.x - host_->sketch_anchor1_.x);
          e.end_angle = std::atan2(cur.y - host_->sketch_anchor1_.y,
                                   cur.x - host_->sketch_anchor1_.x);
          append_sketch_entity(e, pts, lines, verts);
        }
      }
    }
  }
  auto poly = vtkSmartPointer<vtkPolyData>::New();
  poly->SetPoints(pts);
  poly->SetLines(lines);
  poly->SetVerts(verts);
  host_->sketch_preview_mapper_->SetInputData(poly);
  if (host_->render_window_) {
    host_->render_window_->Render();
  }
#endif
}


void SketchViewport::sketch_press(const SketchPoint2d& pt, bool shift,
                             bool subentity) {
  if (!host_->sketch_doc_ ||
      (host_->sketch_preview_only_ && host_->sketch_tool_ != SketchToolSelect)) {
    return;
  }
  const double tol = host_->sketch_pick_tol();
  const auto hit_selection = [this, subentity](int hit) {
    QList<int> ids;
    if (hit < 0) {
      return ids;
    }
    if (subentity) {
      ids.append(hit);
      return ids;
    }
    for (const int id : host_->sketch_doc_->shape_entity_ids(hit)) {
      ids.append(id);
    }
    return ids;
  };
  const auto update_selection = [this, shift](const QList<int>& target,
                                               bool preserve_if_selected) {
    QList<int> sel = host_->sketch_selection_;
    bool all_selected = !target.isEmpty();
    for (const int id : target) {
      all_selected = all_selected && sel.contains(id);
    }
    if (shift) {
      if (all_selected) {
        for (const int id : target) {
          sel.removeAll(id);
        }
      } else {
        for (const int id : target) {
          if (!sel.contains(id)) {
            sel.append(id);
          }
        }
      }
    } else if (!preserve_if_selected || !all_selected) {
      sel = target;
    }
    host_->set_sketch_selection(sel);
    return sel;
  };
  switch (host_->sketch_tool_) {
    case SketchToolSelect: {
      const int hit = host_->sketch_doc_->hit_test(pt, tol);
      if (hit >= 0) {
        update_selection(hit_selection(hit), false);
      } else if (!shift) {
        host_->set_sketch_selection({});
      }
      return;
    }
    case SketchToolMove: {
      const int hit = host_->sketch_doc_->hit_test(pt, tol);
      if (hit < 0) {
        if (!shift) {
          host_->set_sketch_selection({});
        }
        host_->sketch_dragging_ = false;
        host_->sketch_drag_entities_.clear();
        return;
      }
      const QList<int> target = hit_selection(hit);
      const QList<int> sel = update_selection(target, true);
      if (!sel.contains(hit)) {
        return;
      }
      host_->sketch_drag_entities_.clear();
      for (const int id : sel) {
        if (const auto* entity = host_->sketch_doc_->entity(id)) {
          host_->sketch_drag_entities_.push_back(*entity);
        }
      }
      host_->sketch_drag_anchor_ = pt;
      host_->sketch_dragging_ = !host_->sketch_drag_entities_.empty();
      host_->sketch_drag_changed_ = false;
#ifdef GMP_ENABLE_VTK_VIEWER
      if (host_->sketch_dragging_ && host_->vtk_widget_) {
        host_->vtk_widget_->setCursor(Qt::ClosedHandCursor);
      }
#endif
      return;
    }
    case SketchToolDelete: {
      const int hit = host_->sketch_doc_->hit_test(pt, tol);
      if (hit >= 0) {
        bool removed = false;
        for (const int id : hit_selection(hit)) {
          removed = host_->sketch_doc_->remove_entity(id) || removed;
        }
        host_->set_sketch_selection(host_->sketch_selection_);
        if (removed) {
          host_->rebuild_sketch_actors();
          emit host_->sketch_modified();
        }
      }
      return;
    }
    case SketchToolDrawLine: {
      const SketchPoint2d p = host_->sketch_snap(pt);
      if (host_->sketch_stage_ == 0) {
        host_->sketch_anchor1_ = p;
        host_->sketch_stage_ = 1;
        host_->update_sketch_preview();
      } else {
        SketchEntity e;
        e.type = SketchEntityType::Line;
        e.p1 = host_->sketch_anchor1_;
        e.p2 = p;
        host_->sketch_stage_ = 0;
        if (dist2d(e.p1, e.p2) > tol) {  // 忽略零长度线
          host_->sketch_doc_->add_entity(e);
          host_->rebuild_sketch_actors();
          emit host_->sketch_modified();
        } else {
          host_->update_sketch_preview();
        }
      }
      return;
    }
    case SketchToolDrawCircle: {
      if (host_->sketch_stage_ == 0) {
        host_->sketch_anchor1_ = host_->sketch_snap(pt);
        host_->sketch_stage_ = 1;
        host_->update_sketch_preview();
      } else {
        const double r = dist2d(host_->sketch_anchor1_, pt);
        host_->sketch_stage_ = 0;
        if (r > tol) {
          SketchEntity e;
          e.type = SketchEntityType::Circle;
          e.center = host_->sketch_anchor1_;
          e.radius = r;
          host_->sketch_doc_->add_entity(e);
          host_->rebuild_sketch_actors();
          emit host_->sketch_modified();
        } else {
          host_->update_sketch_preview();
        }
      }
      return;
    }
    case SketchToolDrawArc: {
      if (host_->sketch_stage_ == 0) {
        host_->sketch_anchor1_ = host_->sketch_snap(pt);  // 圆心
        host_->sketch_stage_ = 1;
        host_->update_sketch_preview();
      } else if (host_->sketch_stage_ == 1) {
        if (dist2d(host_->sketch_anchor1_, pt) <= tol) {
          return;  // 半径太小, 等待下一个点
        }
        host_->sketch_anchor2_ = pt;  // 起点: 定半径与起始角
        host_->sketch_stage_ = 2;
        host_->update_sketch_preview();
      } else {
        const double r = dist2d(host_->sketch_anchor1_, host_->sketch_anchor2_);
        host_->sketch_stage_ = 0;
        if (r > tol && dist2d(host_->sketch_anchor1_, pt) > tol) {
          constexpr double kTwoPi = 6.28318530717958647692;
          auto norm = [kTwoPi](double a) {
            while (a < 0.0) {
              a += kTwoPi;
            }
            return a;
          };
          SketchEntity e;
          e.type = SketchEntityType::Arc;
          e.center = host_->sketch_anchor1_;
          e.radius = r;
          e.start_angle = norm(std::atan2(host_->sketch_anchor2_.y - host_->sketch_anchor1_.y,
                                          host_->sketch_anchor2_.x - host_->sketch_anchor1_.x));
          e.end_angle = norm(std::atan2(pt.y - host_->sketch_anchor1_.y,
                                        pt.x - host_->sketch_anchor1_.x));
          host_->sketch_doc_->add_entity(e);
          host_->rebuild_sketch_actors();
          emit host_->sketch_modified();
        } else {
          host_->update_sketch_preview();
        }
      }
      return;
    }
    case SketchToolDrawRectangle: {
      const SketchPoint2d p = host_->sketch_snap(pt);
      if (host_->sketch_stage_ == 0) {
        host_->sketch_anchor1_ = p;
        host_->sketch_stage_ = 1;
        host_->update_sketch_preview();
      } else {
        // 第二对角点: 生成 4 条线 + 4 个角点重合约束 (一次修改, 一步撤销)
        const double x1 = host_->sketch_anchor1_.x, y1 = host_->sketch_anchor1_.y;
        const double x2 = p.x, y2 = p.y;
        host_->sketch_stage_ = 0;
        if (std::fabs(x2 - x1) > tol && std::fabs(y2 - y1) > tol) {
          const SketchPoint2d corners[4] = {{x1, y1}, {x2, y1}, {x2, y2}, {x1, y2}};
          const int rectangle_shape_id = host_->sketch_doc_->create_shape_id();
          int line_ids[4] = {-1, -1, -1, -1};
          for (int i = 0; i < 4; ++i) {
            SketchEntity e;
            e.type = SketchEntityType::Line;
            e.shape_id = rectangle_shape_id;
            e.p1 = corners[i];
            e.p2 = corners[(i + 1) % 4];
            line_ids[i] = host_->sketch_doc_->add_entity(e);
          }
          for (int i = 0; i < 4; ++i) {
            SketchConstraint c;
            c.type = SketchConstraintType::Coincident;
            c.entity1 = line_ids[i];
            c.role1 = SketchPointRole::End;
            c.entity2 = line_ids[(i + 1) % 4];
            c.role2 = SketchPointRole::Start;
            host_->sketch_doc_->add_constraint(c);
          }
          host_->rebuild_sketch_actors();
          emit host_->sketch_modified();
        } else {
          host_->update_sketch_preview();
        }
      }
      return;
    }
    default:
      return;
  }
}


void SketchViewport::sketch_move(const SketchPoint2d& pt) {
  host_->sketch_cursor_ = pt;
  emit host_->sketch_cursor_moved(pt.x, pt.y);
  if (host_->sketch_tool_ == SketchToolMove && host_->sketch_dragging_ && host_->sketch_doc_) {
    const double dx = pt.x - host_->sketch_drag_anchor_.x;
    const double dy = pt.y - host_->sketch_drag_anchor_.y;
    if (std::hypot(dx, dy) > 1e-12) {
      std::vector<int> dragged_ids;
      for (const auto& original : host_->sketch_drag_entities_) {
        auto* current = host_->sketch_doc_->entity(original.id);
        if (!current) {
          continue;
        }
        *current = original;
        dragged_ids.push_back(original.id);
      }
      host_->sketch_drag_changed_ =
          host_->sketch_doc_->translate_entities(dragged_ids, dx, dy) ||
          host_->sketch_drag_changed_;
      host_->rebuild_sketch_actors();
    }
    return;
  }
  if (host_->sketch_stage_ > 0) {
    host_->update_sketch_preview();
  }
}


void SketchViewport::sketch_release() {
  if (!host_->sketch_dragging_) {
    return;
  }
  host_->sketch_dragging_ = false;
  host_->sketch_drag_entities_.clear();
#ifdef GMP_ENABLE_VTK_VIEWER
  if (host_->vtk_widget_) {
    host_->vtk_widget_->setCursor(Qt::SizeAllCursor);
  }
#endif
  if (!host_->sketch_drag_changed_) {
    return;
  }
  host_->sketch_drag_changed_ = false;
  // 无约束时保留鼠标给出的精确平移；有约束时再让求解器传播/恢复约束。
  if (host_->sketch_doc_ && !host_->sketch_doc_->constraints().empty()) {
    host_->solve_sketch_and_refresh();
  } else {
    host_->rebuild_sketch_actors();
    emit host_->sketch_modified();
  }
}


void SketchViewport::sketch_delete_selected() {
  if (!host_->sketch_doc_ || host_->sketch_preview_only_ || host_->sketch_selection_.isEmpty()) {
    return;
  }
  bool removed = false;
  for (const int id : host_->sketch_selection_) {
    removed = host_->sketch_doc_->remove_entity(id) || removed;
  }
  host_->sketch_selection_.clear();
  emit host_->sketch_selection_changed();
  if (removed) {
    host_->rebuild_sketch_actors();
    emit host_->sketch_modified();
  }
}


void SketchViewport::solve_sketch_and_refresh() {
  if (host_->sketch_preview_only_) {
    return;
  }
  if (host_->sketch_doc_) {
    // 求解失败(含矛盾约束)容忍, 仅作尝试; 异常双保险(SketchSolver 内部已
    // 兜底, 这里再防一层, 绝不让异常逃逸进事件循环)
    try {
      SketchSolver solver;
      QString err;
      solver.solve(*host_->sketch_doc_, &err);
    } catch (...) {
    }
  }
  host_->rebuild_sketch_actors();
  emit host_->sketch_modified();
}


void SketchViewport::add_constraint_for_selection(int type) {
  if (!host_->sketch_doc_ || host_->sketch_preview_only_) {
    return;
  }
  host_->set_sketch_selection(host_->sketch_selection_);  // 剔除失效 id
  const QList<int> sel = host_->sketch_selection_;
  SketchConstraint c;
  c.type = static_cast<SketchConstraintType>(type);
  auto first_of = [&](bool (*pred)(SketchEntityType)) -> const SketchEntity* {
    for (const int id : sel) {
      if (const SketchEntity* e = host_->sketch_doc_->entity(id); e && pred(e->type)) {
        return e;
      }
    }
    return nullptr;
  };
  auto is_line = [](SketchEntityType t) { return t == SketchEntityType::Line; };
  auto is_round = [](SketchEntityType t) {
    return t == SketchEntityType::Circle || t == SketchEntityType::Arc;
  };
  switch (c.type) {
    case SketchConstraintType::Horizontal:
    case SketchConstraintType::Vertical: {
      const SketchEntity* e = first_of(is_line);
      if (!e) {
        return;
      }
      c.entity1 = e->id;
      break;
    }
    case SketchConstraintType::Parallel:
    case SketchConstraintType::Perpendicular:
    case SketchConstraintType::EqualLength: {
      const SketchEntity* a = first_of(is_line);
      if (!a) {
        return;
      }
      const SketchEntity* b = nullptr;
      for (const int id : sel) {
        const SketchEntity* e = host_->sketch_doc_->entity(id);
        if (e && e->type == SketchEntityType::Line && e->id != a->id) {
          b = e;
          break;
        }
      }
      if (!b) {
        return;
      }
      c.entity1 = a->id;
      c.entity2 = b->id;
      break;
    }
    case SketchConstraintType::EqualRadius: {
      const SketchEntity* a = first_of(is_round);
      if (!a) {
        return;
      }
      const SketchEntity* b = nullptr;
      for (const int id : sel) {
        const SketchEntity* e = host_->sketch_doc_->entity(id);
        if (e && is_round(e->type) && e->id != a->id) {
          b = e;
          break;
        }
      }
      if (!b) {
        return;
      }
      c.entity1 = a->id;
      c.entity2 = b->id;
      break;
    }
    case SketchConstraintType::Coincident: {
      if (sel.size() < 2) {
        return;
      }
      const SketchEntity* a = host_->sketch_doc_->entity(sel[0]);
      const SketchEntity* b = host_->sketch_doc_->entity(sel[1]);
      if (!a || !b) {
        return;
      }
      // 找两图元间距离最近的角色点对
      double best = std::numeric_limits<double>::max();
      SketchPointRole ra = SketchPointRole::None, rb = SketchPointRole::None;
      for (const SketchPointRole r1 : snap_roles_for(*a)) {
        SketchPoint2d p1;
        if (!SketchDocument::point_at_role(*a, r1, &p1)) {
          continue;
        }
        for (const SketchPointRole r2 : snap_roles_for(*b)) {
          SketchPoint2d p2;
          if (!SketchDocument::point_at_role(*b, r2, &p2)) {
            continue;
          }
          const double d = dist2d(p1, p2);
          if (d < best) {
            best = d;
            ra = r1;
            rb = r2;
          }
        }
      }
      if (ra == SketchPointRole::None) {
        return;
      }
      c.entity1 = a->id;
      c.entity2 = b->id;
      c.role1 = ra;
      c.role2 = rb;
      break;
    }
    default:
      return;  // 尺寸类请走 add_dimension_for_selection
  }
  host_->sketch_doc_->add_constraint(c);
  host_->solve_sketch_and_refresh();
}


void SketchViewport::add_dimension_for_selection(int type, double value) {
  if (!host_->sketch_doc_ || host_->sketch_preview_only_ || value <= 0.0) {
    return;
  }
  host_->set_sketch_selection(host_->sketch_selection_);
  SketchConstraint c;
  c.type = static_cast<SketchConstraintType>(type);
  c.value = value;
  c.driving = true;  // driving 尺寸: 改值改图
  if (c.type == SketchConstraintType::Distance) {
    for (const int id : host_->sketch_selection_) {
      if (const SketchEntity* e = host_->sketch_doc_->entity(id);
          e && e->type == SketchEntityType::Line) {
        c.entity1 = e->id;  // 线长 (entity1, 无 role)
        break;
      }
    }
  } else if (c.type == SketchConstraintType::Radius) {
    for (const int id : host_->sketch_selection_) {
      if (const SketchEntity* e = host_->sketch_doc_->entity(id);
          e && (e->type == SketchEntityType::Circle ||
                e->type == SketchEntityType::Arc)) {
        c.entity1 = e->id;
        break;
      }
    }
  } else {
    return;
  }
  if (c.entity1 < 0) {
    return;
  }
  host_->sketch_doc_->add_constraint(c);
  host_->solve_sketch_and_refresh();
}


}  // namespace gmp
