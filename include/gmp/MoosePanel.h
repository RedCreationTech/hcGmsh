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

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

namespace gmp {

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

 public slots:
  void set_mesh_path(const QString& path);
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
  QString log_text() const;
  QString log_tail(int max_lines) const;

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
  void load_settings();
  void save_settings() const;
  void update_exec_history(const QString& path);
  QString auto_detect_exec() const;
  QString find_exec_in_parents(const QString& relative, int max_levels) const;

  QComboBox* exec_path_ = nullptr;
  QLineEdit* input_path_ = nullptr;
  QLineEdit* workdir_path_ = nullptr;
  QLineEdit* mesh_path_ = nullptr;
  QLineEdit* extra_args_ = nullptr;
  QCheckBox* use_mpi_ = nullptr;
  QSpinBox* mpi_ranks_ = nullptr;
  QComboBox* runner_kind_ = nullptr;
  QComboBox* template_kind_ = nullptr;

  QPlainTextEdit* input_editor_ = nullptr;
  QPlainTextEdit* log_ = nullptr;
  QPlainTextEdit* boundary_list_ = nullptr;
  QLabel* template_status_label_ = nullptr;
  QPushButton* run_btn_ = nullptr;
  QPushButton* check_btn_ = nullptr;
  QPushButton* stop_btn_ = nullptr;

  std::unique_ptr<Runner> runner_;
  QStringList boundary_names_;
  QString output_buffer_;
  QString last_exodus_;
  MooseTemplateInfo current_template_;
};

}  // namespace gmp
