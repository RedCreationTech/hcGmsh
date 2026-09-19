#pragma once

// v0.2 Stage 5 hc_ui 过渡层（Q5 批复：逻辑边界先行）。
// ResultViewport：Exodus 结果、云图、变形、时间步、切片与历史回放。方法体自 VtkViewer 逐字迁入，经 host_ 委托访问共享
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

class ResultViewport {
 public:
  explicit ResultViewport(VtkViewer* host) : host_(host) {}
  void set_exodus_file(const QString& path);
  void set_exodus_history(const QStringList& paths);
  QStringList read_exodus_side_set_names(const QString& path) const;
  QString plot_snapshot_text() const;
  QString plot_stats_snapshot() const;
  QString table_snapshot_text() const;
  QString table_stats_snapshot() const;
  void on_reload();
  int current_time_step_index() const;
  void set_time_step_index(int index);
  void on_time_changed(int index);
  void on_array_changed(int index);
  void populate_arrays();
  void update_array_list();
  void update_vector_tab();
  void update_plot_view();
  void update_table_view();
  void update_vector_list();
  void update_deformation_pipeline();
  void on_open_file();
  void on_apply_range();
  void on_auto_refresh_toggled(bool enabled);
  void on_auto_refresh_tick();
  void set_refresh_enabled(bool enabled);
  void setup_watcher(const QString& file_path);
  void schedule_reload();
  void refresh_from_disk();
  void refresh_time_only();
  void update_time_steps_from_reader(bool keep_index);
  void load_file(const QString& path);

 private:
  VtkViewer* host_;
};

}  // namespace gmp
