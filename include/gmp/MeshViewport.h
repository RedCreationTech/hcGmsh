#pragma once

// v0.2 Stage 5 hc_ui 过渡层（Q5 批复：逻辑边界先行）。
// MeshViewport：网格显示、物理组着色、实体拾取与预览。方法体自 VtkViewer 逐字迁入，经 host_ 委托访问共享
// 场景状态；不持有 VTK 对象所有权，生命周期由 VtkViewer（facade）托管。
// 无 Q_OBJECT：信号经 host_ 发射（VtkViewer 声明其为 friend）。

#include <QList>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace gmp {

class VtkViewer;
class SketchDocument;

class MeshViewport {
 public:
  explicit MeshViewport(VtkViewer* host) : host_(host) {}
  bool stage_data_visible() const;
  int visible_mesh_entity_count(int dim) const;
  int current_mesh_dimension() const;
  void set_mesh_file(const QString& path);
  void set_mesh_file_from_current_model(const QString& path);
  void set_mesh_file_impl(const QString& path,
                                   bool use_current_gmsh_model);
  void set_mesh_group_filter(int dim, int tag);
  void set_mesh_entity_filter(int dim, int tag);
  void preview_mesh_entity(int dim, int tag, double view_x,
                                    double view_y, double view_z);
  bool is_mesh_entity_previewed(int dim, int tag) const;
  bool is_mesh_entity_preview_visible(int dim, int tag) const;
  void update_mesh_controls();
  void update_mesh_pipeline();
  void apply_mesh_visuals();
  void update_nodes_visibility();
  void handle_pick(int x, int y);
  void update_selection_pipeline();

 private:
  VtkViewer* host_;
};

}  // namespace gmp
