#pragma once

#include <QMainWindow>
#include <QHash>
#include <QList>
#include <QMap>
#include <QPair>
#include <QStringList>
#include <QVariantMap>
#include <QVariantList>

#include <memory>

#include "gmp/ApplicationProfile.h"
#include "gmp/MooseMappingRegistry.h"
#include "gmp/PhysicalGroupManifest.h"
#include "gmp/MooseInputGenerator.h"
#include "gmp/ProjectStore.h"
#include "gmp/TransactionManager.h"

class QPlainTextEdit;
class QAction;
class QPushButton;
class QLineEdit;
class QStackedWidget;
class QTabBar;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QTreeWidget;
class QTreeWidgetItem;
class QString;
class QLabel;
class QMenu;
class QTableWidget;
class QDockWidget;
class QSplitter;
class QCloseEvent;
class QTabWidget;
class QToolBar;
class QEvent;
class QProgressBar;
class QCheckBox;
class QTimer;
class QSlider;
class QSpinBox;

namespace gmp {

class MoosePanel;
class VtkViewer;
class PropertyEditor;
class GmshPanel;
class SketchPanel;
class PartFeaturePanel;
class SketchDocument;
class StageLeftToolbar;
class FloatingPropertyForm;
class ModelTreeAdapter;
class ResultsPlotWidget;
class ResultsTableWidget;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
 explicit MainWindow(QWidget* parent = nullptr);
 ~MainWindow() override;

 // 文档截图巡览：依次切换各模块页与中栏页签，抓取窗口截图保存到 dir 后退出。
 // 由 main.cpp 在设置 GMP_SCREENSHOT_DIR 环境变量时触发。
 void run_screenshot_tour(const QString& dir);

 protected:
  void closeEvent(QCloseEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void build_menu();
  void build_toolbar();
  void apply_language_to_windows();
  // 创建并接线一个工具组（顶部停靠、紧凑尺寸、浮动恢复）；复位时
  // 也用同一入口重建被拖出的工具组。
  QToolBar* make_tool_group(const QString& title, const QString& object_name);
  // 受控浮动/停靠（setParent(Qt::Tool) 路径，与 Qt 原生拖出浮动完全
  // 隔离；Qt 原生浮动在 macOS 上不可靠已禁用）。View → Toolbars →
  // Float Group 菜单触发；复位时浮动态组走重建兜底回顶部。
  void toggle_group_float(const QString& object_name, bool floating);
  // 受控拖拽交互（替代不可靠的 Qt 原生拖出浮动）：工具组空白区拖拽
  // 超阈值浮出为 Tool 顶层窗；浮动窗拖动靠近顶部工具条行 12px 内时
  // 磁吸停靠回行内。磁吸仅在按住左键拖动时触发。
  void float_group_at(QToolBar* toolbar, const QPoint& global_pos);
  // 磁吸判定（Move 事件与轮询共用）。
  void try_snap_group(QToolBar* group_tb);
  void ensure_group_snap_timer();
  void build_model_tree();
  void apply_theme();
  void reset_tool_group_layout(bool show_feedback = true);
  void position_default_display_group();
  void recover_floating_tool_groups();
  void clear_model_tree_children();
  QTreeWidgetItem* find_root_item(const QString& name) const;
  QTreeWidgetItem* find_child_by_param(QTreeWidgetItem* root,
                                       const QString& key,
                                       const QString& value) const;
  bool child_name_exists(QTreeWidgetItem* root, const QString& name,
                         const QTreeWidgetItem* exclude = nullptr) const;
  QString unique_child_name(QTreeWidgetItem* root,
                            const QString& preferred,
                            const QTreeWidgetItem* exclude = nullptr) const;
  bool prompt_unique_child_name(QTreeWidgetItem* root, const QString& title,
                                const QString& initial_name,
                                QString* accepted_name,
                                QTreeWidgetItem* exclude = nullptr);
  QTreeWidgetItem* add_child_item(QTreeWidgetItem* root,
                                  const QString& name,
                                  const QString& kind,
                                  const QVariantMap& params,
                                  int index = -1);
  QTreeWidgetItem* active_part_item() const;
  QVariantList assembly_instance_specs() const;
  bool build_assembly_model(bool show_error = true);
  QTreeWidgetItem* attach_feature_to_part(QTreeWidgetItem* part,
                                          const QString& type,
                                          const QVariantMap& params,
                                          int gmsh_volume_tag,
                                          const QList<int>& gmsh_volume_tags,
                                          const QString& brep_path);
  void upsert_mesh_item(const QString& path);
  void upsert_result_item(const QString& path, const QString& job_name);
  // 导入外部结果文件（.e/.exo/.msh/.csv/.txt/.log）为 Results 节点；
  // Exodus/网格同时载入中央舞台。
  void import_result_file(const QString& path);
  void import_result_package(const QString& path,
                             const QString& replace_root = QString());
  QVariantMap default_params_for_kind(const QString& kind) const;
  QVariantMap normalize_params_for_kind(const QString& kind,
                                        const QVariantMap& params) const;
  void add_item_under_root(QTreeWidgetItem* root);
  void remove_item(QTreeWidgetItem* item);
  void duplicate_item(QTreeWidgetItem* item);
  void rename_item(QTreeWidgetItem* item);
  // 浮动工作窗也是独立顶层窗口；新对话框必须以当前工作窗为瞬态父窗，
  // 否则 macOS 上可能被工作窗覆盖却仍阻塞其输入。
  QWidget* dialog_parent(QWidget* preferred = nullptr) const;
  void open_property_form(QTreeWidgetItem* item,
                          QWidget* transient_parent = nullptr);
  bool load_project(const QString& path);
  bool save_project(const QString& path);
  // 用户主动保存后的非模态确认：主工作区右上角短暂显示，不阻塞连续编辑。
  void show_project_saved_feedback(const QString& path, bool saved_as = false);
  void position_project_saved_feedback();
  void migrate_project_mesh_paths(const QString& project_path);
  void set_project_dirty(bool dirty);
  void update_window_title();
  void update_project_status();
  void add_recent_project(const QString& path);
  void update_recent_menu();
  void export_debug_bundle();
  void refresh_job_table();
  // 作业监控：选中行后按本地/远程分流详情；远程拉取执行状态与制品清单。
  void apply_job_selection(int row);
  void update_remote_job_detail(const QVariantMap& status);
  void update_remote_job_files(const QVariantMap& body);
  void refresh_results_panel();
  // I-04 Results 对比窗口：多实例、可区分标题、按实例编号的独立几何记忆。
  QDockWidget* create_results_compare_window();
  void populate_results_compare_list(QListWidget* list) const;
  void refresh_results_navigation();
  // 结果导航树条目 → 模型树条目（path 优先、名称为辅的匹配）。
  QTreeWidgetItem* model_item_for_navigation(QTreeWidgetItem* nav_item) const;
  // 构建结果导航树右键菜单（根/子节点、Jobs/Results 分流），供巡览断言。
  void build_results_navigation_menu(QMenu* menu, QTreeWidgetItem* item);
  void sync_results_tree_selection(const QListWidgetItem* row);
  void apply_model_tree_filter(const QString& text);
  void apply_results_tree_filter(const QString& text);
  void refresh_tree_statuses();
  void select_model_item_from_results_navigation(QTreeWidgetItem* item);
  void select_model_item_for_mesh_reference(int dim, int tag,
                                            bool physical_group);
  void invalidate_downstream_from(const QString& source_kind);
  void start_submit_workflow();
  // G0 / W-05：收集并展示工作流预检结果，不修改模型树与输入文本。错误阻止
  // 快照与提交，警告仅提示；定位时选中模型树对象并打开对应属性表单。
  // 校验前仅按 mesh_snapshot_ 对齐 PropertyEditor 的派生组缓存。
  QVariantList collect_workflow_issues() const;
  // 工作流校验以项目持久化的 mesh_snapshot_ 为组清单真源：校验前把
  // PropertyEditor 的边界/体组缓存对齐到清单，避免 Gmsh 模型重建空窗期
  // 的空发射让 Materials/Contact 校验误报。无清单时保留实时发射的候选。
  void sync_property_editor_groups_from_snapshot() const;
  bool validate_workflow_for_submit(bool show_report = true);
  void show_workflow_validation_report(const QVariantList& issues);
  void focus_workflow_issue(const QVariantMap& issue,
                            QWidget* transient_parent = nullptr);
  int append_job_row(const QString& name, const QVariantMap& params);
  void update_job_row(int row, const QString& name, const QVariantMap& params);
  void update_job_detail(int row);
  // W-01c：Selections 生产路径。从物理组创建 Selection 子项
  //（type=PhysicalGroup, group_name/group_dim/group_tag），名称默认组名。
  QTreeWidgetItem* create_selection_from_group(const QString& group_name,
                                               int group_dim);
  // W-01c：对话框入口（选维度 2/3 + 组名），供 Selections 根右键菜单调用。
  void prompt_new_selection_from_group();
  // W-03a：收集 Materials 子项 params 中的 *_file 绝对引用，返回
  // basename -> 绝对路径，作为快照 v2 file_sources 的显式来源表。
  QMap<QString, QString> collect_material_file_sources() const;
  // 决策 7：unit_contract 的 display_to_solver_factors（缺省为空，
  // 表单/生成按各自回退因子处理）。
  QMap<QString, double> display_unit_factors() const;
  // W-02b：显式导入 Exodus 网格为输入网格（决策 6 例外路径）：
  // 登记 Mesh 节点（role=input_mesh）→ 载入舞台 → 提取 boundary 名
  // 写入节点 params 并喂给组 chips 通道。返回 false = 未导入。
  bool import_exodus_mesh(const QString& path);
  // 菜单/快捷按钮入口：文件对话框选 .e/.exo 后调用 import_exodus_mesh；
  // 巡览可用 GMP_TOUR_EXODUS_IMPORT 环境变量覆盖路径（绕过对话框）。
  void on_import_exodus_mesh();
  // W-03b：Physics action 候选与档案解析。physics_action_options 给出
  // action 下拉候选（CDPQuasiStatic 仅当档案 extra.physics_action 声明）；
  // resolve_displacements 取档案声明的位移变量名（缺省 disp_x/y/z）。
  QStringList physics_action_options() const;
  bool active_profile_supports_block(const QString& block_name) const;
  QStringList load_type_options() const;
  QStringList interaction_type_options() const;
  QString resolve_displacements() const;
  // TASK-V02-020/030：block 生成已下沉到 gmp::MooseInputGenerator
  //（hc_simulation 过渡层，无 Widget 依赖）。下面两个是兼容 wrapper：
  // 从当前模型树采集条目后委托给生成器，供巡览合同与存量调用点使用。
  // collect_model_entries 是 Tree→纯数据条目的唯一采集口（树序保持）。
  QList<ProjectModelEntry> collect_model_entries() const;
  MooseInputGenerator::Input generator_input() const;
  QString build_materials_block(QTreeWidgetItem* root) const;
  // W-04：生成报告记录 block 到模型树对象/Physical Group/mapping 的来源。
  QString build_generation_report() const;
  bool sync_model_to_input(const QString& project_path_override = QString());
  void load_demo_diffusion(bool run);
  void load_demo_thermo(bool run);
  void load_demo_nonlinear_heat(bool run);
  void refresh_module_node_list(QListWidget* list,
                               const QString& root_name,
                               const QString& empty_text) const;
  void refresh_module_pages();
  void refresh_work_context();
  // W-00a：活动应用档案。注册表加载、上下文条选择器刷新、档案应用与显示。
  void init_app_profile_support();
  void refresh_app_profile_selector();
  void set_active_app_profile(const QString& profile_id, bool mark_dirty);
  void update_app_profile_display();
  // W-00d：按活动档案声明加载 mapping 注册表；失败仅状态栏报错，不崩溃。
  void reload_mapping_registry();
  // W-00c：把活动档案/单位合同/PG 清单/项目路径注入 MoosePanel，
  // 供快照 v2 导出与远程提交使用。上下文任一变化后调用。
  void push_context_to_moose_panel();
  // Module Workspace 复用内容容器，但不同内容使用独立尺寸/位置配置。
  // Sketch Editor 使用紧凑 profile，不继承 Part/Material 等通用大窗尺寸。
  void apply_module_workspace_profile(bool sketch_editor);
  // 工作窗公共越界恢复：按窗口中心定位屏幕，尺寸与位置夹取到可用区域。
  // 所有独立工作窗（Mesh/Job/Visualization/Results/对比窗）复用同一规则。
  void clamp_window_to_screen(QWidget* window);
  // macOS 自愈：浮动窗口拖拽后延迟重排/重绘。
  void force_native_relayout();
  void remember_active_object_for_module(int module_index);
  void restore_active_object_for_module(int module_index);
  void sync_active_ui_context();
  void update_command_availability();
  QString context_root_for_module(int module_index) const;
  QString build_step_sequence_preview() const;
  void refresh_workflow_status();
  int child_count(const QString& root_name) const;

  QTabBar* module_tabs_ = nullptr;
  QComboBox* module_selector_ = nullptr;
  QLabel* context_project_label_ = nullptr;
  QComboBox* context_object_selector_ = nullptr;
  QComboBox* app_profile_selector_ = nullptr;
  QLabel* app_profile_status_label_ = nullptr;
  QSplitter* main_split_ = nullptr;
  QSplitter* vertical_split_ = nullptr;
  QDockWidget* module_work_window_ = nullptr;
  QDockWidget* mesh_work_window_ = nullptr;
  QDockWidget* job_work_window_ = nullptr;
  QDockWidget* visualization_work_window_ = nullptr;
  QDockWidget* results_work_window_ = nullptr;
  QToolBar* display_tool_group_ = nullptr;
  QTabWidget* results_work_tabs_ = nullptr;
  QList<QDockWidget*> results_compare_windows_;
  int results_compare_counter_ = 0;
  StageLeftToolbar* stage_left_toolbar_ = nullptr;
  bool layout_ready_ = false;
  bool module_workspace_sketch_profile_ = false;
  QTabWidget* navigation_tabs_ = nullptr;
  QTreeWidget* model_tree_ = nullptr;
  QTreeWidget* results_navigation_tree_ = nullptr;
  QLineEdit* model_tree_filter_ = nullptr;
  QLineEdit* results_tree_filter_ = nullptr;
  QStackedWidget* property_stack_ = nullptr;
  PropertyEditor* property_editor_ = nullptr;
  // TASK-V02-020：.gmp.yaml 持久化实现（schema v2 读写与网格路径迁移）。
  ProjectStore project_store_;
  // HARD-050：对象写入只通过 ProjectDocument 领域命令。
  gmp::core::TransactionManager transaction_manager_;
  bool execute_document_command(
      const QString& label, std::unique_ptr<gmp::core::Command> command);
  bool commit_object_edit(QTreeWidgetItem* item, const QString& name,
                          const QVariantMap& params);
  bool set_object_status(QTreeWidgetItem* item, const QString& status);
  // HARD-050：Document→Tree 单向投影适配器。
  ModelTreeAdapter* model_tree_adapter_ = nullptr;
  FloatingPropertyForm* floating_property_form_ = nullptr;
  QPlainTextEdit* console_ = nullptr;
  VtkViewer* viewer_ = nullptr;
  QTableWidget* job_table_ = nullptr;
  QPlainTextEdit* job_detail_ = nullptr;
  // 作业监控（参照 LIMS 任务监控/制品库）
  QComboBox* job_state_filter_ = nullptr;
  QCheckBox* job_auto_refresh_ = nullptr;
  QTimer* job_auto_refresh_timer_ = nullptr;
  QStackedWidget* job_detail_stack_ = nullptr;
  QLabel* job_detail_title_ = nullptr;
  QProgressBar* job_progress_bar_ = nullptr;
  QLabel* job_progress_text_ = nullptr;
  QHash<QString, QLabel*> job_detail_fields_;
  QTableWidget* job_files_table_ = nullptr;
  QPushButton* job_cancel_button_ = nullptr;
  QString selected_job_id_;
  bool selected_job_remote_ = false;
  bool selected_job_running_ = false;
  QListWidget* results_list_ = nullptr;
  ResultsPlotWidget* results_plot_widget_ = nullptr;
  ResultsTableWidget* results_table_widget_ = nullptr;
  QSpinBox* results_table_time_step_ = nullptr;
  QPlainTextEdit* results_preview_ = nullptr;
  QComboBox* results_type_filter_ = nullptr;
  QListWidget* module_part_list_ = nullptr;
  QListWidget* module_material_list_ = nullptr;
  QListWidget* module_section_list_ = nullptr;
  QListWidget* module_assembly_list_ = nullptr;
  QListWidget* module_step_list_ = nullptr;
  QListWidget* module_interaction_list_ = nullptr;
  QListWidget* module_load_list_ = nullptr;
  QPlainTextEdit* step_sequence_preview_ = nullptr;
  QString project_path_;
  bool project_dirty_ = false;
  bool suppress_dirty_ = false;
  QLabel* project_status_label_ = nullptr;
  QLabel* dirty_status_label_ = nullptr;
  QLabel* project_saved_toast_ = nullptr;
  QTimer* project_saved_toast_timer_ = nullptr;
  QLabel* active_context_status_label_ = nullptr;
  QLabel* workflow_status_label_ = nullptr;
  QTreeWidgetItem* active_job_item_ = nullptr;
  int active_job_row_ = -1;
  MoosePanel* moose_panel_ = nullptr;
  GmshPanel* gmsh_panel_ = nullptr;
  SketchPanel* sketch_panel_ = nullptr;
  PartFeaturePanel* part_feature_panel_ = nullptr;
  // 草图编辑会话状态: 正在编辑的文档 (拥有) 与对应模型树节点
  SketchDocument* active_sketch_doc_ = nullptr;
  QTreeWidgetItem* active_sketch_item_ = nullptr;
  // 撤销/重做: YAML 快照栈 (粒度 = 一次 sketch_modified)
  QStringList sketch_undo_stack_;
  QStringList sketch_redo_stack_;
  QString active_sketch_yaml_;  // 最近一次已同步的文档快照

  struct ActiveUiContext {
    int module_index = -1;
    QString object_root;
    QString object_name;
    QList<QPair<int, int>> stage_selections;
    bool mesh_running = false;
    bool job_running = false;
    bool synchronizing = false;
  } active_ui_context_;
  QHash<int, QString> module_object_memory_;

  QMenu* recent_menu_ = nullptr;
  QMenu* view_menu_ = nullptr;
  QAction* action_new_ = nullptr;
  QAction* action_open_ = nullptr;
  QAction* action_save_ = nullptr;
  QAction* action_save_as_ = nullptr;
  QAction* action_export_bundle_ = nullptr;
  QAction* action_edit_properties_ = nullptr;
  QAction* action_sync_ = nullptr;
  QAction* action_undo_ = nullptr;
  QAction* action_redo_ = nullptr;
  QAction* action_screenshot_ = nullptr;
  QAction* action_mesh_ = nullptr;
  QAction* action_preview_mesh_ = nullptr;
  QAction* action_import_exodus_ = nullptr;
  QAction* action_run_ = nullptr;
  QAction* action_check_ = nullptr;
  QAction* action_stop_ = nullptr;
  QAction* action_display_mode_ = nullptr;
  QAction* action_stage_pick_ = nullptr;
  QAction* action_stage_clear_ = nullptr;
  QAction* action_stage_slice_ = nullptr;
  QAction* action_playback_play_ = nullptr;   // 时间步动画 播放
  QAction* action_playback_pause_ = nullptr;  // 时间步动画 暂停
  QAction* action_playback_stop_ = nullptr;   // 终止播放并回到第 0 帧
  QAction* action_playback_prev_ = nullptr;   // 单步后退
  QAction* action_playback_next_ = nullptr;   // 单步前进
  QSlider* playback_slider_ = nullptr;        // 播放进度（可拖动定位）
  QTimer* playback_timer_ = nullptr;
  QAction* action_reset_tool_layout_ = nullptr;
  QPushButton* job_run_button_ = nullptr;
  QPushButton* job_stop_button_ = nullptr;
  QPushButton* job_retry_button_ = nullptr;
  bool tool_drag_guard_active_ = false;
  bool tool_drag_restore_picking_ = false;
  // 受控拖拽状态：按下的工具组与按下点（全局坐标）。
  QToolBar* tool_group_press_target_ = nullptr;
  QPoint tool_group_press_global_;
  // 浮动组被按住拖动的跟踪标志（真实/合成事件均可靠，不依赖
  // QGuiApplication::mouseButtons 的物理按键状态）。
  bool tool_group_float_dragging_ = false;
  // 磁吸轮询定时器：原生标题栏拖拽不向控件投递 Move/鼠标事件，
  // 事件监听不可靠，改以轮询判定磁吸区（任一浮动组存在时运行）。
  QTimer* group_snap_timer_ = nullptr;

  // Phase 0 数据合同字段
  int schema_version_ = 2;
  QVariantMap application_profile_;
  QVariantMap unit_contract_;
  PhysicalGroupManifest mesh_snapshot_;
  QStringList input_snapshots_;
  // W-00a/W-00d：应用档案注册表与当前 mapping 注册表句柄（供 W-03/W-04 消费）。
  ApplicationProfileRegistry app_profile_registry_;
  MooseMappingRegistry mapping_registry_;
};

}  // namespace gmp
