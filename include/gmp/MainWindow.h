#pragma once

#include <QMainWindow>
#include <QStringList>
#include <QVariantMap>

#include "gmp/PhysicalGroupManifest.h"

class QPlainTextEdit;
class QAction;
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

namespace gmp {

class MoosePanel;
class VtkViewer;
class PropertyEditor;
class GmshPanel;
class SketchPanel;
class PartFeaturePanel;
class SketchDocument;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
 explicit MainWindow(QWidget* parent = nullptr);

 // 文档截图巡览：依次切换各模块页与中栏页签，抓取窗口截图保存到 dir 后退出。
 // 由 main.cpp 在设置 GMP_SCREENSHOT_DIR 环境变量时触发。
 void run_screenshot_tour(const QString& dir);

 private:
  void build_menu();
  void build_toolbar();
  void build_model_tree();
  void apply_theme();
  void clear_model_tree_children();
  QTreeWidgetItem* find_root_item(const QString& name) const;
  QTreeWidgetItem* find_child_by_param(QTreeWidgetItem* root,
                                       const QString& key,
                                       const QString& value) const;
  QTreeWidgetItem* add_child_item(QTreeWidgetItem* root,
                                  const QString& name,
                                  const QString& kind,
                                  const QVariantMap& params);
  void upsert_mesh_item(const QString& path);
  void upsert_result_item(const QString& path, const QString& job_name);
  QVariantMap default_params_for_kind(const QString& kind) const;
  QVariantMap normalize_params_for_kind(const QString& kind,
                                        const QVariantMap& params) const;
  void add_item_under_root(QTreeWidgetItem* root);
  void remove_item(QTreeWidgetItem* item);
  void duplicate_item(QTreeWidgetItem* item);
  void load_project(const QString& path);
  bool save_project(const QString& path);
  void set_project_dirty(bool dirty);
  void update_window_title();
  void update_project_status();
  void add_recent_project(const QString& path);
  void update_recent_menu();
  void export_debug_bundle();
  void refresh_job_table();
  void refresh_results_panel();
  void sync_results_tree_selection(const QListWidgetItem* row);
  void start_submit_workflow();
  void ensure_basic_workflow_nodes();
  int append_job_row(const QString& name, const QVariantMap& params);
  void update_job_row(int row, const QString& name, const QVariantMap& params);
  void update_job_detail(int row);
  QString build_block_from_root(QTreeWidgetItem* root,
                                const QString& block_name,
                                const QString& default_type,
                                const QStringList& skip_keys) const;
  QString build_variables_block(QTreeWidgetItem* root) const;
  QString build_executioner_block(QTreeWidgetItem* root) const;
  void sync_model_to_input();
  void load_demo_diffusion(bool run);
  void load_demo_thermo(bool run);
  void load_demo_nonlinear_heat(bool run);
  void refresh_module_node_list(QListWidget* list,
                               const QString& root_name,
                               const QString& empty_text) const;
  void refresh_module_pages();
  QString build_step_sequence_preview() const;
  void refresh_workflow_status();
  int child_count(const QString& root_name) const;

  QTabBar* module_tabs_ = nullptr;
  QTreeWidget* model_tree_ = nullptr;
  QStackedWidget* property_stack_ = nullptr;
  PropertyEditor* property_editor_ = nullptr;
  QPlainTextEdit* console_ = nullptr;
  VtkViewer* viewer_ = nullptr;
  QTableWidget* job_table_ = nullptr;
  QPlainTextEdit* job_detail_ = nullptr;
  QListWidget* results_list_ = nullptr;
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

  QMenu* recent_menu_ = nullptr;
  QAction* action_new_ = nullptr;
  QAction* action_open_ = nullptr;
  QAction* action_save_ = nullptr;
  QAction* action_save_as_ = nullptr;
  QAction* action_export_bundle_ = nullptr;
  QAction* action_sync_ = nullptr;
  QAction* action_screenshot_ = nullptr;
  QAction* action_mesh_ = nullptr;
  QAction* action_preview_mesh_ = nullptr;
  QAction* action_run_ = nullptr;
  QAction* action_check_ = nullptr;
  QAction* action_stop_ = nullptr;

  // Phase 0 数据合同字段
  int schema_version_ = 2;
  QVariantMap application_profile_;
  QVariantMap unit_contract_;
  PhysicalGroupManifest mesh_snapshot_;
  QStringList input_snapshots_;
};

}  // namespace gmp
