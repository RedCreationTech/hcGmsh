#pragma once

#include <memory>

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QMap>

#include "gmp/Runner.h"
#include "gmp/RunnerFactory.h"
#include "gmp/MooseTemplates.h"
#include "gmp/PhysicalGroupManifest.h"

#include <QJsonObject>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

namespace gmp {

class SimClient;
struct ApplicationProfile;

class MoosePanel : public QWidget {
  Q_OBJECT
 public:
  explicit MoosePanel(QWidget* parent = nullptr);
  ~MoosePanel() override = default;

 signals:
  void exodus_ready(const QString& path);
  void exodus_history(const QStringList& paths);
  void job_started(const QVariantMap& info);
  void job_finished(const QVariantMap& info);
  // 远程（LIMS Facade）作业事件：event=submitted（提交成功）或
  // event=status（状态刷新）；携带 job_id/state/server/project/snapshot/
  // submit_time/progress 等字段，供主窗口登记到 Jobs 树与作业列表。
  void remote_job_event(const QVariantMap& info);
  // 作业监控转发信号（数据为 QVariantMap，便于主窗口与巡览复用）。
  void remote_execution_status(const QVariantMap& status);
  void remote_files(const QVariantMap& files);
  void remote_log(const QString& job_id, const QString& text);
  void remote_cancel_done(const QVariantMap& result);
  void remote_file_downloaded(const QString& job_id, const QString& file_path,
                              const QString& local_path);
  void mesh_path_changed(const QString& path);

 public slots:
  void set_mesh_path(const QString& path);
  void set_mesh_paths(const QStringList& paths);
  void set_boundary_groups(const QStringList& names);
  void apply_model_blocks(const QString& functions,
                          const QString& variables,
                          const QString& materials,
                          const QString& bcs,
                          const QString& kernels,
                          const QString& outputs,
                          const QString& executioner);
  QVariantMap moose_settings() const;
  void apply_moose_settings(const QVariantMap& settings);
  void set_template_by_key(const QString& key, bool apply_now = true);
  void run_job();
  void check_input();
  void stop_job();
  void set_external_busy(bool busy);
  QString log_text() const;
  QString log_tail(int max_lines) const;
  // 刷新远程作业：本会话作业详情 + LIMS 任务摘要列表全量同步。
  // 网络失败只记录日志，不弹窗；可在任何时机安全调用。
  void on_refresh_job();
  // 作业监控：按 job_id 拉取实时执行状态/文件清单/日志尾，请求取消，
  // 下载作业工作区单文件。内部自动套用当前服务器字段。
  void refresh_job_execution(const QString& job_id);
  void refresh_job_files(const QString& job_id);
  void request_job_log(const QString& job_id);
  void request_cancel_job(const QString& job_id);
  void download_remote_file(const QString& job_id, const QString& file_path,
                            const QString& dest_path);

  // ---- W-00c：快照合同 v2 上下文注入（由 MainWindow 接线）----
  // 活动应用档案（项目 YAML application_profile 的 QVariantMap 形态；
  // 兼容原始 profile JSON 的嵌套 mapping_registry 结构）。
  void set_application_profile(const QVariantMap& profile);
  // 单位合同（项目 YAML unit_contract；读取其中 display_to_solver_factors）。
  void set_unit_contract(const QVariantMap& unit_contract);
  // Physical Groups 清单（W-00b 生产者填充 mesh_snapshot_ 后注入）。
  void set_physical_group_manifest(const PhysicalGroupManifest& manifest);
  // 项目文件路径（traceability.project_path/project_version 来源）。
  void set_project_context(const QString& project_path);
  // 快照 input_mode：structured | expert | manual；非法值拒绝并保留原值。
  void set_input_mode(const QString& input_mode);
  // W-03a：材料 CSV 等显式文件来源表（basename -> 绝对路径，MainWindow 收集
  // Materials 节点 params 中的 *_file 引用后注入）。快照导出时并入
  // SnapshotExportConfig.file_sources，让相对引用按显式来源表解析打包。
  void set_extra_file_sources(const QMap<QString, QString>& sources);
  // 当前输入编辑器文本（巡览/装配断言用）。
  QString input_text() const;

 private slots:
  void on_pick_exec();
  void on_pick_input();
  void on_pick_workdir();
  void on_write_input();
  void on_run();
  void on_stop();
  void on_check_input();
  void on_apply_template();
  void on_export_snapshot();
  void on_submit_job();
  void on_open_artifacts();
  void on_sim_submit_finished(bool ok, const QJsonObject& body,
                              const QString& error);
  void on_sim_job_fetched(bool ok, const QJsonObject& body,
                          const QString& error);
  void on_insert_mesh_block();
  void on_insert_bcs_block();

 private:
  void append_log(const QString& text);
  void apply_library_template(const QString& key);
  void update_template_status_label();
  void handle_output(const QString& text);
  void flush_output();
  void set_running(bool running);
  QString template_generated_mesh() const;
  QString template_file_mesh(const QString& mesh_path) const;
  QString template_tm_generated_mesh() const;
  QString template_tm_file_mesh(const QString& mesh_path) const;
  QString template_heat_generated_mesh() const;
  QString inject_mesh_block(const QString& input, const QString& mesh_path) const;
  QStringList read_boundary_groups_from_mesh(const QString& mesh_path) const;
  QStringList parse_msh_physical_groups(const QString& mesh_path) const;
  QString inject_bcs_block(const QString& input, const QStringList& names,
                           bool force) const;
  QStringList sanitize_names(const QStringList& names) const;
  QString find_latest_exodus(const QString& dir_path) const;
  QStringList list_exodus_files(const QString& dir_path) const;
  QString pick_latest_exodus(const QStringList& files) const;
  QStringList collect_exodus_files(const QStringList& dirs) const;
  void run_task(bool check_only);
  QString upsert_block(const QString& input,
                       const QString& block_name,
                       const QString& block_text) const;
  QString resolve_exodus_path(const QString& token) const;
  void maybe_emit_exodus(const QString& path);
  // W-00c：由注入的 application_profile_map_ 组装档案；单位合同缺省时回落
  // unit_contract_map_ 的标量键。
  ApplicationProfile snapshot_profile() const;
  // W-00c：把 .i 中的绝对文件引用归一化为快照相对名（返回空串=成功；
  // file_sources 记录 相对名->来源绝对路径，file_roles 记录显式 .e 角色）。
  QString normalize_snapshot_refs(QString* input_text,
                                  QMap<QString, QString>* file_sources,
                                  QMap<QString, QString>* file_roles) const;
  void load_settings();
  void save_settings() const;
  void update_exec_history(const QString& path);
  QString auto_detect_exec() const;
  QString find_exec_in_parents(const QString& relative, int max_levels) const;

  QComboBox* exec_path_ = nullptr;
  QLineEdit* input_path_ = nullptr;
  QLineEdit* workdir_path_ = nullptr;
  QComboBox* mesh_path_ = nullptr;
  QLineEdit* extra_args_ = nullptr;
  QCheckBox* use_mpi_ = nullptr;
  QSpinBox* mpi_ranks_ = nullptr;
  QComboBox* runner_kind_ = nullptr;
  QComboBox* template_kind_ = nullptr;

  QPlainTextEdit* input_editor_ = nullptr;
  QPlainTextEdit* log_ = nullptr;
  QPlainTextEdit* boundary_list_ = nullptr;
  QLabel* template_status_label_ = nullptr;
  QLineEdit* sim_server_ = nullptr;
  QLineEdit* sim_project_ = nullptr;
  QLabel* sim_status_label_ = nullptr;
  QPushButton* run_btn_ = nullptr;
  QPushButton* check_btn_ = nullptr;
  QPushButton* stop_btn_ = nullptr;

  std::unique_ptr<Runner> runner_;
  QStringList boundary_names_;
  QString output_buffer_;
  QString last_exodus_;
  MooseTemplateInfo current_template_;
  SimClient* sim_client_ = nullptr;
  QString last_snapshot_dir_;
  QString last_job_id_;
  // W-00c：快照 v2 导出上下文（缺省为空 => 导出被拒绝并给出可读原因）。
  QVariantMap application_profile_map_;
  QVariantMap unit_contract_map_;
  PhysicalGroupManifest physical_group_manifest_;
  QString project_path_;
  QString input_mode_ = QStringLiteral("structured");
  // W-03a：显式文件来源表（材料 CSV 等），见 set_extra_file_sources。
  QMap<QString, QString> extra_file_sources_;
  // 监控请求归属：日志/下载请求对应的 job_id（响应异步返回时回填）。
  QString log_job_id_;
  QString download_job_id_;
  bool running_ = false;
  bool external_busy_ = false;
};

}  // namespace gmp
