#pragma once

// v0.2 Stage 5 hc_ui 过渡层（Q5 批复：逻辑边界先行）。
// SketchViewport：草图交互编辑：工具/捕捉/拖拽/约束与尺寸插入/求解联动。方法体自 VtkViewer 逐字迁入，经 host_ 委托访问共享
// 场景状态；不持有 VTK 对象所有权，生命周期由 VtkViewer（facade）托管。
// 无 Q_OBJECT：信号经 host_ 发射（VtkViewer 声明其为 friend）。

#include <QList>

#include "gmp/SketchDocument.h"
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace gmp {

class VtkViewer;
class SketchDocument;

class SketchViewport {
 public:
  explicit SketchViewport(VtkViewer* host) : host_(host) {}
  void set_2d_mode(bool on);
  void set_sketch_document(SketchDocument* doc);
  void set_sketch_preview(const SketchDocument* doc);
  void set_sketch_tool(int tool);
  void refresh_sketch();
  bool sketch_display_to_world(int x, int y, SketchPoint2d* out) const;
  double sketch_pick_tol() const;
  SketchPoint2d sketch_snap(const SketchPoint2d& pt) const;
  void set_sketch_selection(const QList<int>& ids);
  void rebuild_sketch_actors();
  void update_sketch_preview();
  void sketch_press(const SketchPoint2d& pt, bool shift,
                             bool subentity);
  void sketch_move(const SketchPoint2d& pt);
  void sketch_release();
  void sketch_delete_selected();
  void solve_sketch_and_refresh();
  void add_constraint_for_selection(int type);
  void add_dimension_for_selection(int type, double value);

 private:
  VtkViewer* host_;
};

}  // namespace gmp
