#pragma once

#include <QWidget>
#include <QString>
#include <QDateTime>
#include <QVariantMap>

class QLabel;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QPlainTextEdit;
class QSlider;
class QPushButton;
class QTimer;
class QSpinBox;
class QFileSystemWatcher;
class QListWidget;
class QTableWidget;

#ifdef GMP_ENABLE_VTK_VIEWER
class QVTKOpenGLNativeWidget;
class vtkGenericOpenGLRenderWindow;
class vtkRenderer;
class vtkExodusIIReader;
class vtkCompositeDataGeometryFilter;
class vtkDataSetSurfaceFilter;
class vtkUnstructuredGrid;
class vtkVertexGlyphFilter;
class vtkShrinkFilter;
class vtkOutlineFilter;
class vtkAxesActor;
class vtkOrientationMarkerWidget;
class vtkThreshold;
class vtkPlane;
class vtkCutter;
class vtkDataSetMapper;
class vtkPolyDataMapper;
class vtkActor;
class vtkScalarBarActor;
class vtkLookupTable;
class vtkCellPicker;
class vtkCallbackCommand;
class vtkWarpVector;

#include <vtkSmartPointer.h>
#include <vector>
#endif

namespace gmp {

class VtkViewer : public QWidget {
  Q_OBJECT
 public:
  explicit VtkViewer(QWidget* parent = nullptr);
  ~VtkViewer() override = default;

 public slots:
  void set_exodus_file(const QString& path);
  void set_exodus_history(const QStringList& paths);
  bool save_screenshot(const QString& path);
  void set_mesh_file(const QString& path);
  void set_mesh_group_filter(int dim, int tag);
  void set_mesh_entity_filter(int dim, int tag);
  QVariantMap viewer_settings() const;
  void apply_viewer_settings(const QVariantMap& settings);
  QString plot_snapshot_text() const;
  QString plot_stats_snapshot() const;
  QString table_snapshot_text() const;
  QString table_stats_snapshot() const;

signals:
  void mesh_group_picked(int dim, int tag);
  void mesh_entity_picked(int dim, int tag);

 private slots:
  void on_time_changed(int index);
  void on_array_changed(int index);
  void on_reload();
  void on_open_file();
  void on_apply_range();
  void on_preset_changed(int index);
  void on_repr_changed(int index);
  void on_auto_refresh_toggled(bool enabled);
  void on_auto_refresh_tick();

 private:
  void init_vtk();
  void update_pipeline();
  void refresh_time_only();
  void refresh_from_disk();
  void update_time_steps_from_reader(bool keep_index);
  void populate_arrays();
  void apply_representation();
  void apply_lookup_table();
  void set_refresh_enabled(bool enabled);
  void setup_watcher(const QString& file_path);
  void schedule_reload();
  void load_file(const QString& path);
  void update_nodes_visibility();
  void update_mesh_pipeline();
  void update_mesh_controls();
  void apply_mesh_visuals();
  void handle_pick(int x, int y);
  void update_selection_pipeline();
  void update_scene_extras();
  void apply_view_preset(int preset);
  void update_array_list();
  void update_vector_list();
  void update_deformation_pipeline();
  void update_vector_tab();
  void update_plot_view();
  void update_table_view();

  QString current_file_;
  QLabel* file_label_ = nullptr;
  QPushButton* open_btn_ = nullptr;
  QComboBox* array_combo_ = nullptr;
  QComboBox* preset_combo_ = nullptr;
  QComboBox* repr_combo_ = nullptr;
  QCheckBox* auto_range_ = nullptr;
  QCheckBox* auto_refresh_ = nullptr;
  QCheckBox* show_nodes_ = nullptr;
  QCheckBox* show_quality_ = nullptr;
  QCheckBox* show_faces_ = nullptr;
  QCheckBox* show_edges_ = nullptr;
  QCheckBox* show_shell_ = nullptr;
  QLabel* mesh_legend_ = nullptr;
  QComboBox* mesh_group_ = nullptr;
  QComboBox* mesh_dim_ = nullptr;
  QComboBox* mesh_entity_ = nullptr;
  QComboBox* mesh_type_ = nullptr;
  QDoubleSpinBox* mesh_opacity_ = nullptr;
  QDoubleSpinBox* mesh_shrink_ = nullptr;
  QCheckBox* mesh_scalar_bar_ = nullptr;
  QCheckBox* pick_enable_ = nullptr;
  QComboBox* pick_mode_ = nullptr;
  QPushButton* pick_clear_ = nullptr;
  QLabel* pick_info_ = nullptr;
  QCheckBox* show_axes_ = nullptr;
  QCheckBox* show_outline_ = nullptr;
  QComboBox* view_combo_ = nullptr;
  QPushButton* view_apply_ = nullptr;
  QCheckBox* slice_enable_ = nullptr;
  QComboBox* slice_axis_ = nullptr;
  QSlider* slice_slider_ = nullptr;
  QSpinBox* refresh_ms_ = nullptr;
  QTimer* refresh_timer_ = nullptr;
  QTimer* debounce_timer_ = nullptr;
  QFileSystemWatcher* watcher_ = nullptr;
  qint64 last_file_size_ = -1;
  QDateTime last_file_mtime_;
  bool pending_reload_ = false;
  QDoubleSpinBox* range_min_ = nullptr;
  QDoubleSpinBox* range_max_ = nullptr;
  QSlider* time_slider_ = nullptr;
  QLabel* time_label_ = nullptr;
  QPushButton* reload_btn_ = nullptr;
  QLabel* output_label_ = nullptr;
  QComboBox* output_combo_ = nullptr;
  QPushButton* output_pick_ = nullptr;
  QComboBox* array_filter_ = nullptr;
  QListWidget* array_list_ = nullptr;
  QCheckBox* probe_enable_ = nullptr;
  QComboBox* probe_mode_ = nullptr;
  QPushButton* probe_clear_ = nullptr;
  QLabel* probe_info_ = nullptr;
  QCheckBox* deform_enable_ = nullptr;
  QComboBox* deform_vector_ = nullptr;
  QDoubleSpinBox* deform_scale_ = nullptr;
  QComboBox* vector_array_combo_ = nullptr;
  QCheckBox* vector_auto_sync_deform_ = nullptr;
  QPushButton* vector_apply_to_deform_ = nullptr;
  QLabel* vector_info_ = nullptr;
 QPlainTextEdit* plot_view_ = nullptr;
  QString cached_plot_text_;
  QString cached_plot_stats_;
  QPushButton* plot_refresh_btn_ = nullptr;
  QLabel* plot_stats_ = nullptr;
  QTableWidget* table_view_ = nullptr;
  QString cached_table_text_;
  QString cached_table_stats_;
  QSpinBox* table_rows_spin_ = nullptr;
  QPushButton* table_refresh_btn_ = nullptr;
  QLabel* table_stats_ = nullptr;

#ifdef GMP_ENABLE_VTK_VIEWER
  QVTKOpenGLNativeWidget* vtk_widget_ = nullptr;
  std::vector<double> time_steps_;

  enum class DataMode { None, Exodus, Mesh };
  DataMode mode_ = DataMode::None;

  struct MeshGroup {
    int dim = 0;
    int id = 0;
    QString name;
  };
  struct MeshEntity {
    int dim = 0;
    int tag = 0;
  };
  std::vector<MeshGroup> mesh_groups_;
  std::vector<int> mesh_elem_types_;
  std::vector<MeshEntity> mesh_entities_;
  int selected_group_dim_ = -1;
  int selected_group_id_ = -1;
  int selected_cell_id_ = -1;
  int selected_entity_dim_ = -1;
  int selected_entity_tag_ = -1;

  vtkSmartPointer<vtkGenericOpenGLRenderWindow> render_window_;
  vtkSmartPointer<vtkRenderer> renderer_;
  vtkSmartPointer<vtkExodusIIReader> reader_;
  vtkSmartPointer<vtkCompositeDataGeometryFilter> geom_;
  vtkSmartPointer<vtkUnstructuredGrid> mesh_grid_;
  vtkSmartPointer<vtkDataSetSurfaceFilter> mesh_geom_;
  vtkSmartPointer<vtkShrinkFilter> mesh_shrink_filter_;
  vtkSmartPointer<vtkThreshold> mesh_dim_threshold_;
  vtkSmartPointer<vtkThreshold> mesh_group_threshold_;
  vtkSmartPointer<vtkThreshold> mesh_type_threshold_;
  vtkSmartPointer<vtkThreshold> mesh_entity_dim_threshold_;
  vtkSmartPointer<vtkThreshold> mesh_entity_tag_threshold_;
  vtkSmartPointer<vtkThreshold> mesh_select_dim_threshold_;
  vtkSmartPointer<vtkThreshold> mesh_select_group_threshold_;
  vtkSmartPointer<vtkThreshold> mesh_select_cell_threshold_;
  vtkSmartPointer<vtkThreshold> mesh_select_entity_dim_threshold_;
  vtkSmartPointer<vtkThreshold> mesh_select_entity_tag_threshold_;
  vtkSmartPointer<vtkDataSetSurfaceFilter> mesh_select_geom_;
  vtkSmartPointer<vtkDataSetMapper> mesh_select_mapper_;
  vtkSmartPointer<vtkActor> mesh_select_actor_;
  vtkSmartPointer<vtkPlane> mesh_slice_plane_;
  vtkSmartPointer<vtkCutter> mesh_slice_cutter_;
  vtkSmartPointer<vtkDataSetMapper> mapper_;
  vtkSmartPointer<vtkActor> actor_;
  vtkSmartPointer<vtkVertexGlyphFilter> nodes_filter_;
  vtkSmartPointer<vtkPolyDataMapper> nodes_mapper_;
  vtkSmartPointer<vtkActor> nodes_actor_;
  vtkSmartPointer<vtkScalarBarActor> scalar_bar_;
  vtkSmartPointer<vtkLookupTable> lut_;
  vtkSmartPointer<vtkOutlineFilter> outline_filter_;
  vtkSmartPointer<vtkPolyDataMapper> outline_mapper_;
  vtkSmartPointer<vtkActor> outline_actor_;
  vtkSmartPointer<vtkAxesActor> axes_actor_;
  vtkSmartPointer<vtkOrientationMarkerWidget> axes_marker_;
  vtkSmartPointer<vtkCellPicker> picker_;
  vtkSmartPointer<vtkCallbackCommand> pick_callback_;
  vtkSmartPointer<vtkWarpVector> warp_filter_;
  bool first_render_ = true;
  bool pipeline_ready_ = false;
  bool actor_added_ = false;
  bool mesh_quality_ready_ = false;
  double mesh_bounds_[6] = {0, 0, 0, 0, 0, 0};
#endif
};

}  // namespace gmp
