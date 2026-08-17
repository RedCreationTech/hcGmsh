#pragma once

#include <QWidget>
#include <QString>
#include <QDateTime>
#include <QList>
#include <QVariantMap>

#include "gmp/SketchDocument.h"

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
class vtkMultiBlockDataSetAlgorithm;
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
class vtkInteractorStyle;
class vtkInteractorStyleImage;

#include <vtkSmartPointer.h>
#include <vector>
#endif

class QStackedWidget;

namespace gmp {

// 草图绘制工具 (VtkViewer::set_sketch_tool 参数,
// 与 SketchPanel::tool_selected 信号的 int 值一一对应)
enum SketchTool {
  SketchToolSelect = 0,     // 选择 (点击单选, Shift+点击切换多选)
  SketchToolDrawLine = 1,   // 两点直线
  SketchToolDrawCircle = 2, // 圆心 + 半径点
  SketchToolDrawArc = 3,    // 三点式: 圆心 + 起点(定半径/起始角) + 终点角
  SketchToolDelete = 4,     // 点击删除图元
  SketchToolDrawRectangle = 5  // 两对角点轴对齐矩形 (生成 4 线 + 角点重合约束)
};

class VtkViewer : public QWidget {
  Q_OBJECT
 public:
  explicit VtkViewer(QWidget* parent = nullptr);
  ~VtkViewer() override = default;

  // 视口控制区(标量/网格/视图/...), 供 MainWindow 迁移到右侧边栏
  QWidget* control_tabs() const { return control_tabs_; }
  // 顶部文件/输出操作区, 同样迁移到右侧边栏
  QWidget* top_bar() const { return top_bar_; }

 public slots:
  // 2D 草图模式: 相机切到 XY 正交俯视(平行投影, +Z 看向原点, view-up +Y),
  // 交互样式换成 vtkInteractorStyleImage(禁旋转); 关闭后恢复透视投影与原样式。
  // 2D 模式下 apply_view_preset 不生效。
  void set_2d_mode(bool on);
  bool is_2d_mode() const { return mode_2d_; }

  // ---- 草图编辑会话 (WS1) ----
  // 进入草图编辑: doc 为非拥有指针(由 MainWindow 持有), viewer 直接改它;
  // 进入时自动开 2D 模式并渲染草图; 传 nullptr 退出编辑并恢复 3D 视图。
  void set_sketch_document(SketchDocument* doc);
  SketchDocument* sketch_document() const { return sketch_doc_; }
  // 切换当前绘制工具, 取值为 SketchTool 枚举; 切换会取消进行中的绘制
  void set_sketch_tool(int tool);
  int sketch_tool() const { return sketch_tool_; }
  // 当前选中图元 id 列表
  QList<int> sketch_selection() const { return sketch_selection_; }
  // 为当前选中图元添加几何约束 (type 为 SketchConstraintType 的 int 值:
  // Horizontal/Vertical/Parallel/Perpendicular/Coincident 等),
  // 选中图元数量/类型不满足时静默忽略; 成功后调用 SketchSolver::solve
  // 尝试求解(容忍失败)并发出 sketch_modified()
  void add_constraint_for_selection(int type);
  // 为选中图元添加 driving 尺寸约束 (type 为 Distance/Radius),
  // value 为目标值(必须 > 0); 同样尝试求解并发出 sketch_modified()
  void add_dimension_for_selection(int type, double value);
  // 外部(非交互)改动 doc 后请求重绘草图
  void refresh_sketch();
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
  // 交互改动了草图 doc (增删图元/增删约束/求解回写), MainWindow 据此序列化
  void sketch_modified();
  // 草图选中集变化
  void sketch_selection_changed();
  // 草图编辑中鼠标的世界坐标 (XY 平面, 毫米), 供面板显示
  void sketch_cursor_moved(double x, double y);

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
  void apply_scalar_bar_pos();
  void apply_view_preset(int preset);
  void update_array_list();
  void update_vector_list();
  void update_deformation_pipeline();
  void update_vector_tab();
  void update_plot_view();
  void update_table_view();

  // ---- 草图编辑内部实现 (非 VTK 构建下为空实现) ----
  void rebuild_sketch_actors();    // 全量重建草图/选中高亮 actor
  void update_sketch_preview();    // 仅重建橡皮筋预览 actor
  void sketch_press(const SketchPoint2d& pt, bool shift);  // 左键按下分发
  void sketch_move(const SketchPoint2d& pt);               // 鼠标移动(预览/坐标)
  void sketch_delete_selected();
  bool sketch_display_to_world(int x, int y, SketchPoint2d* out) const;
  double sketch_pick_tol() const;  // 拾取/吸附容差 (世界单位, 约 10 像素)
  SketchPoint2d sketch_snap(const SketchPoint2d& pt) const;  // 端点吸附
  void set_sketch_selection(const QList<int>& ids);
  void solve_sketch_and_refresh();  // 调 SketchSolver(容忍失败) + 重绘 + 发信号

  QString current_file_;
  bool mode_2d_ = false;  // 2D 草图模式标志(非 VTK 构建下也保留, 便于状态查询)

  // ---- 草图编辑会话状态 (非 VTK 构建下同样保留, 保证 stub 可编译) ----
  SketchDocument* sketch_doc_ = nullptr;  // 非拥有, MainWindow 持有
  int sketch_tool_ = SketchToolSelect;
  QList<int> sketch_selection_;           // 选中图元 id
  SketchPoint2d sketch_cursor_{};         // 最近一次鼠标世界坐标
  int sketch_stage_ = 0;                  // 绘制进度: 0=待首点 1/2=已定锚点
  SketchPoint2d sketch_anchor1_{};        // 第一个锚点 (线起点/圆心)
  SketchPoint2d sketch_anchor2_{};        // 第二个锚点 (弧起点, 定半径/起始角)
  QLabel* file_label_ = nullptr;
  QPushButton* open_btn_ = nullptr;
  QComboBox* array_combo_ = nullptr;
  QComboBox* preset_combo_ = nullptr;
  QComboBox* repr_combo_ = nullptr;
  QComboBox* scalar_bar_pos_combo_ = nullptr;
  QWidget* control_tabs_ = nullptr;
  QWidget* top_bar_ = nullptr;
  QComboBox* control_nav_ = nullptr;
  QStackedWidget* control_stack_ = nullptr;
  int scalar_bar_pos_ = 0;  // 0=右下 1=右上 2=左下 3=左上
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
  // Exodus 多块输出在扁平化前补齐"部分块变量"（0 填充缺失块），见实现注释
  vtkSmartPointer<vtkMultiBlockDataSetAlgorithm> block_pad_;
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
  vtkSmartPointer<vtkInteractorStyle> style_3d_;
  vtkSmartPointer<vtkInteractorStyleImage> style_2d_;

  // ---- 草图渲染/交互 (WS1) ----
  vtkSmartPointer<vtkPolyDataMapper> sketch_mapper_;         // 全部图元
  vtkSmartPointer<vtkActor> sketch_actor_;
  vtkSmartPointer<vtkPolyDataMapper> sketch_sel_mapper_;     // 选中高亮
  vtkSmartPointer<vtkActor> sketch_sel_actor_;
  vtkSmartPointer<vtkPolyDataMapper> sketch_preview_mapper_; // 橡皮筋预览
  vtkSmartPointer<vtkActor> sketch_preview_actor_;
  vtkSmartPointer<vtkCallbackCommand> sketch_move_callback_; // 鼠标移动观察器
  vtkSmartPointer<vtkCallbackCommand> sketch_key_callback_;  // Delete 键观察器
  // 草图会话期间暂存的 3D 场景 actor 可见性 (退出会话时恢复),
  // 避免 2D 草图与 3D 网格/结果叠显
  bool pre_sketch_vis_main_ = false;
  bool pre_sketch_vis_nodes_ = false;
  bool pre_sketch_vis_outline_ = false;
  bool pre_sketch_vis_scalar_bar_ = false;
  bool pre_sketch_vis_select_ = false;
  bool first_render_ = true;
  bool pipeline_ready_ = false;
  bool actor_added_ = false;
  bool mesh_quality_ready_ = false;
  double mesh_bounds_[6] = {0, 0, 0, 0, 0, 0};
#endif
};

}  // namespace gmp
