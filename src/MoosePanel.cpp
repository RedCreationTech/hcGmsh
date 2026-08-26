#include "gmp/MoosePanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QRegularExpression>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSet>
#include <QSpinBox>
#include <QString>
#include <QVBoxLayout>
#include <QStringList>
#include <QProcess>
#include <QStandardPaths>

#include "gmp/ComboPopupFix.h"

#include "gmp/ArtifactDialog.h"
#include "gmp/MooseSnapshot.h"
#include "gmp/RunSpec.h"
#include "gmp/SimClient.h"

#ifdef GMP_ENABLE_GMSH_GUI
#include <gmsh.h>
#endif

namespace gmp {

namespace {

bool is_executable_file(const QString& path) {
  if (path.isEmpty()) {
    return false;
  }
  const QFileInfo info(path);
  return info.exists() && info.isFile() && info.isExecutable();
}

RunnerKind RunnerKindFromIndex(int idx) {
  switch (idx) {
    case 1:
      return RunnerKind::kWsl;
    case 2:
      return RunnerKind::kRemote;
    default:
      return RunnerKind::kLocal;
  }
}

}  // namespace

MoosePanel::MoosePanel(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);

  auto* title = new QLabel("MOOSE Panel");
  layout->addWidget(title);

  auto* paths_box = new QGroupBox("Paths");
  auto* paths_form = new QFormLayout(paths_box);

  exec_path_ = new QComboBox();
  install_combo_popup_fix(exec_path_);
  exec_path_->setEditable(true);
  exec_path_->setInsertPolicy(QComboBox::NoInsert);
  exec_path_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  exec_path_->setToolTip("Path to moose executable");
  if (exec_path_->lineEdit()) {
    exec_path_->lineEdit()->setPlaceholderText(
        "Auto-detect MOOSE executable");
  }
  auto* pick_exec = new QPushButton("Pick");
  connect(pick_exec, &QPushButton::clicked, this, &MoosePanel::on_pick_exec);
  auto* exec_row = new QHBoxLayout();
  exec_row->addWidget(exec_path_);
  exec_row->addWidget(pick_exec);
  auto* exec_container = new QWidget();
  exec_container->setLayout(exec_row);
  paths_form->addRow("Executable", exec_container);

  input_path_ = new QLineEdit();
  input_path_->setPlaceholderText("Input file path (*.i)");
  input_path_->setText(QDir::currentPath() + "/out/sample.i");
  auto* pick_input = new QPushButton("Pick");
  connect(pick_input, &QPushButton::clicked, this, &MoosePanel::on_pick_input);
  auto* input_row = new QHBoxLayout();
  input_row->addWidget(input_path_);
  input_row->addWidget(pick_input);
  auto* input_container = new QWidget();
  input_container->setLayout(input_row);
  paths_form->addRow("Input File", input_container);

  workdir_path_ = new QLineEdit();
  workdir_path_->setPlaceholderText("Working directory (optional)");
  workdir_path_->setText(QDir::currentPath());
  auto* pick_workdir = new QPushButton("Pick");
  connect(pick_workdir, &QPushButton::clicked, this, &MoosePanel::on_pick_workdir);
  auto* workdir_row = new QHBoxLayout();
  workdir_row->addWidget(workdir_path_);
  workdir_row->addWidget(pick_workdir);
  auto* workdir_container = new QWidget();
  workdir_container->setLayout(workdir_row);
  paths_form->addRow("Work Dir", workdir_container);

  layout->addWidget(paths_box);

  auto* mesh_box = new QGroupBox("Mesh");
  auto* mesh_form = new QFormLayout(mesh_box);
  mesh_path_ = new QLineEdit();
  mesh_path_->setPlaceholderText("Path to mesh file (.msh)");
  auto* insert_mesh_btn = new QPushButton("Insert Mesh Block");
  connect(insert_mesh_btn, &QPushButton::clicked, this,
          &MoosePanel::on_insert_mesh_block);
  auto* mesh_row = new QHBoxLayout();
  mesh_row->addWidget(mesh_path_);
  mesh_row->addWidget(insert_mesh_btn);
  auto* mesh_container = new QWidget();
  mesh_container->setLayout(mesh_row);
  mesh_form->addRow("Mesh File", mesh_container);
  layout->addWidget(mesh_box);

  auto* groups_box = new QGroupBox("Physical Groups");
  auto* groups_layout = new QVBoxLayout(groups_box);
  boundary_list_ = new QPlainTextEdit();
  boundary_list_->setReadOnly(true);
  boundary_list_->setPlaceholderText("No boundary groups detected yet.");
  groups_layout->addWidget(boundary_list_);
  auto* bc_btn = new QPushButton("Insert BCs From Groups");
  connect(bc_btn, &QPushButton::clicked, this, &MoosePanel::on_insert_bcs_block);
  groups_layout->addWidget(bc_btn);
  layout->addWidget(groups_box);

  auto* run_box = new QGroupBox("Run");
  auto* run_form = new QFormLayout(run_box);

  use_mpi_ = new QCheckBox("Use mpiexec");
  use_mpi_->setChecked(false);
  run_form->addRow("", use_mpi_);

  mpi_ranks_ = new QSpinBox();
  mpi_ranks_->setRange(1, 4096);
  mpi_ranks_->setValue(4);
  run_form->addRow("MPI Ranks", mpi_ranks_);

  runner_kind_ = new QComboBox();
  install_combo_popup_fix(runner_kind_);
  runner_kind_->addItem("Local", static_cast<int>(RunnerKind::kLocal));
  runner_kind_->addItem("WSL", static_cast<int>(RunnerKind::kWsl));
  runner_kind_->addItem("Remote", static_cast<int>(RunnerKind::kRemote));
  run_form->addRow("Runner", runner_kind_);

  extra_args_ = new QLineEdit();
  extra_args_->setPlaceholderText("Extra args (e.g. --n-threads=4)");
  run_form->addRow("Extra Args", extra_args_);

  layout->addWidget(run_box);

  // TASK-E2E-11: 作业提交入口（经 LIMS Facade, 不直连 C06, 不保存凭据）
  auto* remote_box = new QGroupBox("Remote Job (via LIMS Facade)");
  auto* remote_form = new QFormLayout(remote_box);
  sim_server_ = new QLineEdit();
  sim_server_->setPlaceholderText("http://127.0.0.1:8200");
  sim_server_->setToolTip("LIMS Facade base URL");
  remote_form->addRow("Server", sim_server_);
  sim_project_ = new QLineEdit();
  sim_project_->setPlaceholderText("gmp-ise");
  remote_form->addRow("Project ID", sim_project_);
  auto* sim_btn_row = new QHBoxLayout();
  auto* submit_btn = new QPushButton("Submit Job");
  submit_btn->setToolTip("Submit latest exported snapshot via LIMS Facade");
  auto* refresh_btn = new QPushButton("Refresh Status");
  auto* artifacts_btn = new QPushButton("Open Remote Artifact");
  connect(submit_btn, &QPushButton::clicked, this, &MoosePanel::on_submit_job);
  connect(refresh_btn, &QPushButton::clicked, this,
          &MoosePanel::on_refresh_job);
  connect(artifacts_btn, &QPushButton::clicked, this,
          &MoosePanel::on_open_artifacts);
  sim_btn_row->addWidget(submit_btn);
  sim_btn_row->addWidget(refresh_btn);
  sim_btn_row->addWidget(artifacts_btn);
  sim_btn_row->addStretch(1);
  auto* sim_btn_container = new QWidget();
  sim_btn_container->setLayout(sim_btn_row);
  remote_form->addRow("", sim_btn_container);
  sim_status_label_ = new QLabel("(no job submitted)");
  remote_form->addRow("Job", sim_status_label_);
  layout->addWidget(remote_box);

  sim_client_ = new SimClient(this);
  connect(sim_client_, &SimClient::submit_finished, this,
          &MoosePanel::on_sim_submit_finished);
  connect(sim_client_, &SimClient::job_fetched, this,
          &MoosePanel::on_sim_job_fetched);

  auto* io_box = new QGroupBox("Input Editor");
  auto* io_layout = new QVBoxLayout(io_box);
  auto* template_row = new QHBoxLayout();
  template_kind_ = new QComboBox();
  install_combo_popup_fix(template_kind_);
  template_kind_->addItem("GeneratedMesh (Transient Diffusion)", "generated");
  template_kind_->addItem("FileMesh (Transient Diffusion)", "filemesh");
  template_kind_->addItem("GeneratedMesh (Nonlinear Heat)", "heat_generated");
  template_kind_->addItem("GeneratedMesh (Thermo-Mechanics)", "tm_generated");
  template_kind_->addItem("FileMesh (Thermo-Mechanics)", "tm_filemesh");
  // TASK-E2E-10 模板库（templates/moose/<key>/，见 template.json 来源记录）
  template_kind_->addItem("ASR 约束膨胀（小算例）", "tpl-asr-strip");
  template_kind_->addItem("ASR 约束膨胀（完整算例）", "tpl-asr-full");
  template_kind_->addItem("2D 坝静力预载 [prototype]", "tpl-dam-2d-static");
  template_kind_->addItem("2D 坝静力—动力两阶段 [prototype]",
                          "tpl-dam-2d-dyn");
  template_kind_->addItem("2D 坝 CDP 动力损伤（prototype）", "tpl-dam-2d-dyn-cdp");
  template_kind_->addItem("北京混凝土 CDP 参考等效损伤静力对标（prototype）",
                          "tpl-bj-concrete-cdp-static");
  auto* apply_template = new QPushButton("Apply Template");
  connect(apply_template, &QPushButton::clicked, this,
          &MoosePanel::on_apply_template);
  template_row->addWidget(new QLabel("Template"));
  template_row->addWidget(template_kind_);
  template_row->addWidget(apply_template);
  template_status_label_ = new QLabel();
  template_row->addWidget(template_status_label_);
  template_row->addStretch(1);
  io_layout->addLayout(template_row);

  input_editor_ = new QPlainTextEdit();
  input_editor_->setPlainText(template_generated_mesh());
  io_layout->addWidget(input_editor_);

  auto* io_actions = new QHBoxLayout();
  auto* write_btn = new QPushButton("Write Input");
  connect(write_btn, &QPushButton::clicked, this, &MoosePanel::on_write_input);
  io_actions->addWidget(write_btn);
  auto* export_btn = new QPushButton("Export Job Snapshot");
  export_btn->setToolTip(
      "Export .i + referenced mesh/extra files + manifest.json "
      "(CONTRACT-JOB input_snapshot)");
  connect(export_btn, &QPushButton::clicked, this,
          &MoosePanel::on_export_snapshot);
  io_actions->addWidget(export_btn);
  io_actions->addStretch(1);
  io_layout->addLayout(io_actions);

  layout->addWidget(io_box, 2);

  auto* action_row = new QHBoxLayout();
  run_btn_ = new QPushButton("Run");
  check_btn_ = new QPushButton("Check Input");
  stop_btn_ = new QPushButton("Stop");
  stop_btn_->setEnabled(false);
  connect(run_btn_, &QPushButton::clicked, this, &MoosePanel::on_run);
  connect(check_btn_, &QPushButton::clicked, this, &MoosePanel::on_check_input);
  connect(stop_btn_, &QPushButton::clicked, this, &MoosePanel::on_stop);
  action_row->addWidget(run_btn_);
  action_row->addWidget(check_btn_);
  action_row->addWidget(stop_btn_);
  action_row->addStretch(1);
  layout->addLayout(action_row);

  log_ = new QPlainTextEdit();
  log_->setReadOnly(true);
  layout->addWidget(log_, 1);

  // 右侧边栏宽度有限: 表单标签换行到控件上方(垂直布局), 减少横向溢出
  for (auto* f : findChildren<QFormLayout*>()) {
    f->setRowWrapPolicy(QFormLayout::WrapAllRows);
  }

  append_log("MOOSE panel ready.");
  load_settings();
}

void MoosePanel::on_pick_exec() {
  const QString path = QFileDialog::getOpenFileName(this, "Select MOOSE executable",
                                                    exec_path_->currentText());
  if (!path.isEmpty()) {
    exec_path_->setCurrentText(path);
    update_exec_history(path);
  }
}

void MoosePanel::on_pick_input() {
  const QString path =
      QFileDialog::getSaveFileName(this, "Select input file", input_path_->text(),
                                   "MOOSE Input (*.i)");
  if (!path.isEmpty()) {
    input_path_->setText(path);
    save_settings();
  }
}

void MoosePanel::on_pick_workdir() {
  const QString path =
      QFileDialog::getExistingDirectory(this, "Select working directory",
                                        workdir_path_->text());
  if (!path.isEmpty()) {
    workdir_path_->setText(path);
    save_settings();
  }
}

void MoosePanel::on_write_input() {
  const QString out_path = input_path_->text();
  if (out_path.isEmpty()) {
    append_log("Input path is empty.");
    return;
  }

  QDir().mkpath(QFileInfo(out_path).absolutePath());
  QFile file(out_path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    append_log("Failed to write input file.");
    return;
  }
  file.write(input_editor_->toPlainText().toUtf8());
  file.close();
  append_log("Input file written: " + out_path);
  save_settings();
}

void MoosePanel::on_run() {
  run_task(false);
}

void MoosePanel::run_job() {
  on_run();
}

void MoosePanel::on_stop() {
  if (!runner_) {
    return;
  }
  runner_->stop();
}

void MoosePanel::stop_job() {
  on_stop();
}

QString MoosePanel::log_text() const {
  return log_ ? log_->toPlainText() : QString();
}

QString MoosePanel::log_tail(int max_lines) const {
  if (!log_) {
    return QString();
  }
  QStringList lines = log_->toPlainText().split('\n');
  if (max_lines > 0 && lines.size() > max_lines) {
    lines = lines.mid(lines.size() - max_lines);
  }
  return lines.join("\n");
}

void MoosePanel::on_check_input() {
  run_task(true);
}

void MoosePanel::check_input() {
  on_check_input();
}

void MoosePanel::append_log(const QString& text) {
  if (log_) {
    log_->appendPlainText(text);
  }
}

void MoosePanel::handle_output(const QString& text) {
  output_buffer_ += text;
  int idx = -1;
  while ((idx = output_buffer_.indexOf('\n')) != -1) {
    const QString line = output_buffer_.left(idx).trimmed();
    output_buffer_.remove(0, idx + 1);
    if (!line.isEmpty()) {
      append_log(line);
      const QRegularExpression re(R"m((['"]?)([A-Za-z0-9_./\\-]+\.e)\1)m");
      auto it = re.globalMatch(line);
      while (it.hasNext()) {
        const auto m = it.next();
        const QString token = m.captured(2);
        const QString resolved = resolve_exodus_path(token);
        if (!resolved.isEmpty()) {
          maybe_emit_exodus(resolved);
        }
      }
    }
  }
}

void MoosePanel::flush_output() {
  if (output_buffer_.isEmpty()) {
    return;
  }
  handle_output("\n");
}

void MoosePanel::set_running(bool running) {
  if (run_btn_) {
    run_btn_->setEnabled(!running);
  }
  if (check_btn_) {
    check_btn_->setEnabled(!running);
  }
  if (stop_btn_) {
    stop_btn_->setEnabled(running);
  }
}

void MoosePanel::set_mesh_path(const QString& path) {
  mesh_path_->setText(path);
  const QString updated = inject_mesh_block(input_editor_->toPlainText(), path);
  if (!updated.isEmpty()) {
    input_editor_->setPlainText(updated);
    append_log("Mesh path injected into [Mesh] block.");
  }
  if (!path.isEmpty()) {
    set_boundary_groups(read_boundary_groups_from_mesh(path));
  }
  save_settings();
}

void MoosePanel::set_boundary_groups(const QStringList& names) {
  boundary_names_ = names;
  boundary_list_->setPlainText(boundary_names_.join("\n"));

  // Auto-update BCs only if current BCs look like the default template.
  const QString updated =
      inject_bcs_block(input_editor_->toPlainText(), boundary_names_, false);
  if (!updated.isEmpty()) {
    input_editor_->setPlainText(updated);
    append_log("BCs block updated from physical groups.");
  }
  save_settings();
}

QVariantMap MoosePanel::moose_settings() const {
  QVariantMap map;
  map.insert("exec_path", exec_path_ ? exec_path_->currentText() : "");
  map.insert("input_path", input_path_ ? input_path_->text() : "");
  map.insert("workdir", workdir_path_ ? workdir_path_->text() : "");
  map.insert("mesh_path", mesh_path_ ? mesh_path_->text() : "");
  map.insert("use_mpi", use_mpi_ && use_mpi_->isChecked());
  map.insert("mpi_ranks", mpi_ranks_ ? mpi_ranks_->value() : 1);
  map.insert("runner_kind",
             runner_kind_ ? runner_kind_->currentIndex() : 0);
  map.insert("template_key",
             template_kind_ ? template_kind_->currentData().toString() : "");
  map.insert("extra_args", extra_args_ ? extra_args_->text() : "");
  map.insert("input_text", input_editor_ ? input_editor_->toPlainText() : "");
  return map;
}

void MoosePanel::apply_moose_settings(const QVariantMap& settings) {
  if (exec_path_) {
    const QString exec_path =
        settings.value("exec_path", exec_path_->currentText()).toString();
    if (!exec_path.isEmpty()) {
      update_exec_history(exec_path);
    }
    exec_path_->setCurrentText(exec_path);
  }
  if (input_path_) {
    input_path_->setText(
        settings.value("input_path", input_path_->text()).toString());
  }
  if (workdir_path_) {
    workdir_path_->setText(
        settings.value("workdir", workdir_path_->text()).toString());
  }
  if (mesh_path_) {
    mesh_path_->setText(
        settings.value("mesh_path", mesh_path_->text()).toString());
  }
  if (use_mpi_) {
    use_mpi_->setChecked(
        settings.value("use_mpi", use_mpi_->isChecked()).toBool());
  }
  if (mpi_ranks_) {
    mpi_ranks_->setValue(
        settings.value("mpi_ranks", mpi_ranks_->value()).toInt());
  }
  if (runner_kind_) {
    runner_kind_->setCurrentIndex(
        settings.value("runner_kind", runner_kind_->currentIndex()).toInt());
  }
  const QString template_key = settings.value("template_key").toString();
  const QString input_text = settings.value("input_text").toString();
  if (!template_key.isEmpty()) {
    const bool apply_now = input_text.isEmpty();
    set_template_by_key(template_key, apply_now);
  }
  if (extra_args_) {
    extra_args_->setText(
        settings.value("extra_args", extra_args_->text()).toString());
  }
  if (input_editor_ && !input_text.isEmpty()) {
    input_editor_->setPlainText(input_text);
  }
  save_settings();
}

void MoosePanel::apply_model_blocks(const QString& functions,
                                    const QString& variables,
                                    const QString& materials,
                                    const QString& bcs,
                                    const QString& kernels,
                                    const QString& outputs,
                                    const QString& executioner) {
  QString input = input_editor_->toPlainText();
  input = upsert_block(input, "Functions", functions);
  input = upsert_block(input, "Variables", variables);
  input = upsert_block(input, "Materials", materials);
  input = upsert_block(input, "BCs", bcs);
  input = upsert_block(input, "Kernels", kernels);
  input = upsert_block(input, "Outputs", outputs);
  input = upsert_block(input, "Executioner", executioner);
  if (input != input_editor_->toPlainText()) {
    input_editor_->setPlainText(input);
    append_log("Input updated from Model Tree.");
  }
  save_settings();
}

void MoosePanel::set_template_by_key(const QString& key, bool apply_now) {
  if (!template_kind_) {
    return;
  }
  int idx = -1;
  for (int i = 0; i < template_kind_->count(); ++i) {
    if (template_kind_->itemData(i).toString() == key) {
      idx = i;
      break;
    }
  }
  if (idx < 0) {
    append_log("Unknown template key: " + key);
    return;
  }
  template_kind_->setCurrentIndex(idx);
  if (apply_now) {
    on_apply_template();
  }
}

void MoosePanel::on_apply_template() {
  const QString key = template_kind_->currentData().toString();
  if (key.startsWith("tpl-")) {
    apply_library_template(key);
    save_settings();
    return;
  }
  current_template_ = MooseTemplateInfo();
  update_template_status_label();
  if (key == "filemesh") {
    const QString path =
        mesh_path_->text().isEmpty() ? "path/to/mesh.msh" : mesh_path_->text();
    input_editor_->setPlainText(template_file_mesh(path));
  } else if (key == "heat_generated") {
    input_editor_->setPlainText(template_heat_generated_mesh());
  } else if (key == "tm_filemesh") {
    const QString path =
        mesh_path_->text().isEmpty() ? "path/to/mesh.msh" : mesh_path_->text();
    input_editor_->setPlainText(template_tm_file_mesh(path));
  } else if (key == "tm_generated") {
    input_editor_->setPlainText(template_tm_generated_mesh());
  } else {
    input_editor_->setPlainText(template_generated_mesh());
  }

  if (key.startsWith("tm_")) {
    const QString combined =
        find_exec_in_parents("external/moose/modules/combined/combined-opt", 6);
    if (!combined.isEmpty()) {
      update_exec_history(combined);
      append_log("Thermo-mechanics template selected. Using: " + combined);
    } else {
      append_log("Thermo-mechanics template selected, but combined-opt was not found.");
    }
  }
  save_settings();
}

void MoosePanel::apply_library_template(const QString& key) {
  const QString root = moose_templates_root();
  if (root.isEmpty()) {
    append_log("Template library not found (set GMP_MOOSE_TEMPLATES).");
    return;
  }
  const MooseTemplateInfo info = load_moose_template(root, key);
  if (!info.valid) {
    append_log("Template not found or invalid: " + key + " under " + root);
    return;
  }
  QFile file(info.input_path);
  if (!file.open(QIODevice::ReadOnly)) {
    append_log("Failed to read template input: " + info.input_path);
    return;
  }
  input_editor_->setPlainText(QString::fromUtf8(file.readAll()));
  current_template_ = info;
  update_template_status_label();
  append_log("Template loaded: " + key + " [" + info.status + "]");
  if (!info.source_desc.isEmpty()) {
    append_log("  source: " + info.source_desc);
  }
  if (info.status == "prototype") {
    append_log(
        "  WARNING: prototype template - 不作为工程结论 (not for "
        "engineering conclusions).");
  }
}

void MoosePanel::update_template_status_label() {
  if (!template_status_label_) {
    return;
  }
  if (!current_template_.valid) {
    template_status_label_->clear();
    template_status_label_->setStyleSheet(QString());
    return;
  }
  template_status_label_->setText(
      QString("[%1] %2").arg(current_template_.status,
                             current_template_.display_name));
  if (current_template_.status == "prototype") {
    template_status_label_->setStyleSheet(
        "color: #b26a00; font-weight: bold;");
  } else {
    template_status_label_->setStyleSheet("color: #2e7d32;");
  }
}

void MoosePanel::on_export_snapshot() {
  const QString start_dir = workdir_path_ && !workdir_path_->text().isEmpty()
                                ? workdir_path_->text()
                                : QDir::currentPath();
  const QString dest = QFileDialog::getExistingDirectory(
      this, "Select snapshot export directory", start_dir);
  if (dest.isEmpty()) {
    return;
  }

  QString input_name;
  if (current_template_.valid) {
    input_name = current_template_.key + ".i";
  } else {
    input_name = QFileInfo(input_path_->text()).fileName();
    if (!input_name.endsWith(".i")) {
      input_name = "custom.i";
    }
  }

  const SnapshotExportResult result = export_job_snapshot(
      dest, input_editor_->toPlainText(), input_name, current_template_);
  if (!result.ok) {
    append_log("Snapshot export failed: " + result.error);
    return;
  }
  last_snapshot_dir_ = result.dir;
  append_log("Job snapshot exported to: " + result.dir);
  append_log("  input: " + result.input_file +
             " sha256=" + result.input_sha256);
  for (const auto& name : result.mesh_files) {
    append_log("  mesh: " + name);
  }
  for (const auto& name : result.extra_files) {
    append_log("  extra: " + name);
  }
  for (const auto& ref : result.missing_files) {
    append_log("  WARNING: referenced file not found, not exported: " + ref);
  }
  append_log("  manifest: " + result.manifest_path);
  if (current_template_.valid && current_template_.status == "prototype") {
    append_log("  NOTE: prototype template - snapshot marked accordingly.");
  }
}

void MoosePanel::on_submit_job() {
  QString dir = last_snapshot_dir_;
  if (dir.isEmpty() || !QFileInfo::exists(dir + "/manifest.json")) {
    dir = QFileDialog::getExistingDirectory(
        this, "Select exported snapshot directory",
        workdir_path_ && !workdir_path_->text().isEmpty()
            ? workdir_path_->text()
            : QDir::currentPath());
    if (dir.isEmpty()) {
      return;
    }
  }
  const QString server = sim_server_->text().trimmed().isEmpty()
                             ? sim_server_->placeholderText()
                             : sim_server_->text().trimmed();
  const QString project = sim_project_->text().trimmed().isEmpty()
                              ? sim_project_->placeholderText()
                              : sim_project_->text().trimmed();
  sim_client_->set_base_url(server);
  sim_status_label_->setText("submitting...");
  append_log("Submitting snapshot via LIMS Facade: " + server);
  append_log("  snapshot: " + dir + " project: " + project);
  sim_client_->submit_snapshot(dir, project);
  save_settings();
}

void MoosePanel::on_refresh_job() {
  if (last_job_id_.isEmpty()) {
    append_log("No submitted job in this session.");
    return;
  }
  sim_client_->fetch_job(last_job_id_);
}

void MoosePanel::on_sim_submit_finished(bool ok, const QJsonObject& body,
                                        const QString& error) {
  if (!ok) {
    sim_status_label_->setText("submit failed");
    append_log("Job submit failed: " + error);
    return;
  }
  last_job_id_ = body.value("job_id").toString();
  const QString state = body.value("state").toString();
  sim_status_label_->setText(last_job_id_ + " : " + state);
  append_log("Job submitted: " + last_job_id_ + " state=" + state);
}

void MoosePanel::on_sim_job_fetched(bool ok, const QJsonObject& body,
                                    const QString& error) {
  if (!ok) {
    append_log("Job status refresh failed: " + error);
    return;
  }
  const QString state = body.value("state").toString();
  QString text = last_job_id_ + " : " + state;
  const QJsonObject progress = body.value("progress").toObject();
  if (!progress.isEmpty()) {
    QStringList parts;
    if (progress.value("percent").isDouble()) {
      parts << QString("percent=%1")
                   .arg(progress.value("percent").toDouble(), 0, 'f', 1);
    }
    const int step_cur = progress.value("step_current").toInt(-1);
    if (step_cur >= 0) {
      QString step = QString("step=%1").arg(step_cur);
      if (progress.value("step_total").isDouble()) {
        step += QString("/%1").arg(progress.value("step_total").toInt());
      }
      parts << step;
    }
    if (progress.value("time_current").isDouble()) {
      QString time =
          QString("time=%1").arg(progress.value("time_current").toDouble());
      if (progress.value("time_total").isDouble()) {
        time += QString("/%1").arg(progress.value("time_total").toDouble());
      }
      parts << time;
    }
    if (!parts.isEmpty()) {
      text += "  (" + parts.join(", ") + ")";
    }
  }
  sim_status_label_->setText(text);
  append_log("Job status: " + text);
  if (state == "failed") {
    const QJsonObject failure = body.value("failure").toObject();
    append_log("  failure: " + failure.value("code").toString() + " - " +
               failure.value("message").toString());
  }
}

void MoosePanel::on_open_artifacts() {
  const QString server = sim_server_->text().trimmed().isEmpty()
                             ? sim_server_->placeholderText()
                             : sim_server_->text().trimmed();
  ArtifactDialog dialog(server, this);
  connect(&dialog, &ArtifactDialog::exodus_artifact_ready, this,
          [this](const QString& path) {
            append_log("Remote artifact ready: " + path);
            maybe_emit_exodus(path);
          });
  dialog.exec();
}

void MoosePanel::on_insert_mesh_block() {
  const QString path =
      mesh_path_->text().isEmpty() ? "path/to/mesh.msh" : mesh_path_->text();
  const QString updated = inject_mesh_block(input_editor_->toPlainText(), path);
  if (!updated.isEmpty()) {
    input_editor_->setPlainText(updated);
    append_log("Mesh block inserted/updated.");
  }
  save_settings();
}

void MoosePanel::on_insert_bcs_block() {
  if (boundary_names_.isEmpty()) {
    append_log("No boundary groups available.");
    return;
  }
  const QString updated =
      inject_bcs_block(input_editor_->toPlainText(), boundary_names_, true);
  if (!updated.isEmpty()) {
    input_editor_->setPlainText(updated);
    append_log("BCs block inserted/updated.");
  }
}

void MoosePanel::run_task(bool check_only) {
  if (runner_) {
    append_log("A run is already active.");
    return;
  }

  if (exec_path_->currentText().isEmpty()) {
    append_log("Executable is empty.");
    return;
  }

  const QString input_path = input_path_->text();
  if (input_path.isEmpty()) {
    append_log("Input file path is empty.");
    return;
  }

  if (!QFileInfo::exists(input_path)) {
    on_write_input();
  }

  RunSpec spec;
  const QString exec_path = exec_path_->currentText();
  const QStringList extra = QProcess::splitCommand(extra_args_->text());
  if (use_mpi_->isChecked()) {
    spec.program = "mpiexec";
    spec.args << "-n" << QString::number(mpi_ranks_->value())
              << exec_path << "-i" << input_path;
  } else {
    spec.program = exec_path;
    spec.args << "-i" << input_path;
  }
  spec.args.append(extra);
  if (check_only) {
    spec.args << "--check-input";
  }
  spec.working_dir = workdir_path_->text();

  const RunnerKind kind = RunnerKindFromIndex(runner_kind_->currentIndex());
  runner_ = CreateRunner(kind);
  if (!runner_) {
    append_log("Failed to create runner.");
    return;
  }

  QVariantMap start_info;
  start_info.insert("exec", exec_path);
  start_info.insert("input", input_path);
  start_info.insert("workdir", spec.working_dir);
  start_info.insert("mesh", mesh_path_ ? mesh_path_->text() : QString());
  start_info.insert("use_mpi", use_mpi_->isChecked());
  start_info.insert("mpi_ranks", mpi_ranks_->value());
  start_info.insert("check_only", check_only);
  start_info.insert("launcher", spec.program);
  start_info.insert("args", spec.args.join(" "));
  emit job_started(start_info);

  connect(runner_.get(), &Runner::std_out, this, &MoosePanel::handle_output);
  connect(runner_.get(), &Runner::std_err, this, &MoosePanel::handle_output);
  connect(runner_.get(), &Runner::started, this, [this, check_only]() {
    append_log(check_only ? "Input check started." : "Run started.");
    set_running(true);
  });
  connect(runner_.get(), &Runner::finished, this,
          [this](int code, QProcess::ExitStatus status) {
            flush_output();
            append_log(QString("Run finished. exit=%1 status=%2")
                           .arg(code)
                           .arg(status == QProcess::NormalExit ? "Normal" : "Crash"));
            runner_.reset();
            set_running(false);

            const QString workdir = workdir_path_->text().isEmpty()
                                        ? QDir::currentPath()
                                        : workdir_path_->text();
            const QString input_dir = QFileInfo(input_path_->text()).absolutePath();
            QStringList dirs;
            if (!workdir.isEmpty()) {
              dirs << workdir;
            }
            if (!input_dir.isEmpty() && input_dir != workdir) {
              dirs << input_dir;
            }
            const QStringList history = collect_exodus_files(dirs);
            const QString exodus = pick_latest_exodus(history);
            if (!history.isEmpty()) {
              emit exodus_history(history);
            }
            if (!exodus.isEmpty()) {
              maybe_emit_exodus(exodus);
            }

            QVariantMap finish_info;
            finish_info.insert("exit_code", code);
            finish_info.insert("status",
                               status == QProcess::NormalExit ? "Normal"
                                                              : "Crash");
            finish_info.insert("exodus", exodus);
            finish_info.insert("history", history);
            emit job_finished(finish_info);
          });

  append_log("Launching: " + spec.program + " " + spec.args.join(" "));
  runner_->start(spec);
  update_exec_history(exec_path);
  save_settings();
}

QString MoosePanel::upsert_block(const QString& input,
                                 const QString& block_name,
                                 const QString& block_text) const {
  const QString trimmed = block_text.trimmed();
  if (trimmed.isEmpty()) {
    return input;
  }
  const QString pattern =
      QString(R"((?s)\[%1\].*?(?=\n\[|\z))")
          .arg(QRegularExpression::escape(block_name));
  QRegularExpression re(pattern);
  QString out = input;
  if (re.match(out).hasMatch()) {
    out.replace(re, trimmed);
  } else {
    out = out.trimmed();
    if (!out.isEmpty()) {
      out += "\n\n";
    }
    out += trimmed;
    out += "\n";
  }
  return out;
}

QString MoosePanel::resolve_exodus_path(const QString& token) const {
  QFileInfo fi(token);
  if (fi.isAbsolute() && fi.exists()) {
    return fi.absoluteFilePath();
  }
  const QString workdir = workdir_path_->text().isEmpty()
                              ? QDir::currentPath()
                              : workdir_path_->text();
  QFileInfo rel(QDir(workdir), token);
  if (rel.exists()) {
    return rel.absoluteFilePath();
  }
  return QString();
}

void MoosePanel::maybe_emit_exodus(const QString& path) {
  if (path.isEmpty()) {
    return;
  }
  if (path == last_exodus_) {
    return;
  }
  last_exodus_ = path;
  emit exodus_ready(path);
}

QString MoosePanel::template_generated_mesh() const {
  return R"([Mesh]
  type = GeneratedMesh
  dim = 2
  nx = 20
  ny = 20
[]

[Variables]
  [./u]
    family = LAGRANGE
    order = FIRST
  [../]
  [./v]
    family = LAGRANGE
    order = FIRST
  []
  [../]
[]

[Functions]
  [./ic_u]
    type = ParsedFunction
    expression = 'sin(3.14159*x)*sin(3.14159*y)'
  [../]
  [./ic_v]
    type = ParsedFunction
    expression = '0.2*cos(3.14159*x)*cos(3.14159*y)'
  [../]
  [./source_u]
    type = ParsedFunction
    expression = 'exp(-t)*sin(3.14159*x)*sin(3.14159*y)'
  [../]
  [./source_v]
    type = ParsedFunction
    expression = '0.1*exp(-0.5*t)*cos(3.14159*x)'
  [../]
  [./bc_left]
    type = ParsedFunction
    expression = '1.0+0.1*sin(6.28318*t)'
  [../]
  [./bc_right]
    type = ParsedFunction
    expression = '0.0'
  [../]
[]

[ICs]
  [./u_ic]
    type = FunctionIC
    variable = u
    function = ic_u
  [../]
  [./v_ic]
    type = FunctionIC
    variable = v
    function = ic_v
  [../]
[]

[Kernels]
  [./u_dt]
    type = TimeDerivative
    variable = u
  [../]
  [./u_diff]
    type = MatDiffusion
    variable = u
    diffusivity = diff_u
  [../]
  [./u_src]
    type = BodyForce
    variable = u
    function = source_u
  [../]
  [./v_dt]
    type = TimeDerivative
    variable = v
  [../]
  [./v_diff]
    type = MatDiffusion
    variable = v
    diffusivity = diff_v
  [../]
  [./v_src]
    type = BodyForce
    variable = v
    function = source_v
  [../]
[]

[Materials]
  [./diffusion]
    type = GenericConstantMaterial
    prop_names = 'diff_u diff_v'
    prop_values = '1.0 0.25'
  [../]
[]

[BCs]
  [./u_left]
    type = FunctionDirichletBC
    variable = u
    boundary = left
    function = bc_left
  [../]
  [./u_right]
    type = FunctionDirichletBC
    variable = u
    boundary = right
    function = bc_right
  [../]
  [./v_left]
    type = DirichletBC
    variable = v
    boundary = left
    value = 0
  [../]
  [./v_right]
    type = DirichletBC
    variable = v
    boundary = right
    value = 0
  [../]
[]

[Postprocessors]
  [./u_avg]
    type = ElementAverageValue
    variable = u
  [../]
  [./v_avg]
    type = ElementAverageValue
    variable = v
  [../]
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  scheme = 'bdf2'
  dt = 0.01
  end_time = 0.2
[]

[Outputs]
  exodus = true
  csv = true
[]
)";
}

QString MoosePanel::template_heat_generated_mesh() const {
  return R"([Mesh]
  type = GeneratedMesh
  dim = 2
  nx = 30
  ny = 30
  xmin = 0
  xmax = 1
  ymin = 0
  ymax = 1
[]

[Variables]
  [./T]
    initial_condition = 300
  [../]
[]

[Kernels]
  [./T_dt]
    type = TimeDerivative
    variable = T
  [../]
  [./T_cond]
    type = HeatConduction
    variable = T
  [../]
[]

[Materials]
  [./k_T]
    type = ParsedMaterial
    property_name = thermal_conductivity
    coupled_variables = T
    expression = '1 + 0.01*T'
  [../]
[]

[BCs]
  [./temp_left]
    type = DirichletBC
    variable = T
    boundary = left
    value = 500
  [../]
  [./temp_right]
    type = DirichletBC
    variable = T
    boundary = right
    value = 300
  [../]
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  scheme = 'bdf2'
  dt = 0.02
  end_time = 0.5
[]

[Outputs]
  exodus = true
  csv = true
[]
)";
}

QString MoosePanel::template_file_mesh(const QString& mesh_path) const {
  return QString(R"([Mesh]
  type = FileMesh
  file = %1
[]

[Variables]
  [./u]
    family = LAGRANGE
    order = FIRST
  [../]
  [./v]
    family = LAGRANGE
    order = FIRST
  []
  [../]
[]

[Functions]
  [./ic_u]
    type = ParsedFunction
    expression = 'sin(3.14159*x)*sin(3.14159*y)'
  [../]
  [./ic_v]
    type = ParsedFunction
    expression = '0.2*cos(3.14159*x)*cos(3.14159*y)'
  [../]
  [./source_u]
    type = ParsedFunction
    expression = 'exp(-t)*sin(3.14159*x)*sin(3.14159*y)'
  [../]
  [./source_v]
    type = ParsedFunction
    expression = '0.1*exp(-0.5*t)*cos(3.14159*x)'
  [../]
  [./bc_left]
    type = ParsedFunction
    expression = '1.0+0.1*sin(6.28318*t)'
  [../]
  [./bc_right]
    type = ParsedFunction
    expression = '0.0'
  [../]
[]

[ICs]
  [./u_ic]
    type = FunctionIC
    variable = u
    function = ic_u
  [../]
  [./v_ic]
    type = FunctionIC
    variable = v
    function = ic_v
  [../]
[]

[Kernels]
  [./u_dt]
    type = TimeDerivative
    variable = u
  [../]
  [./u_diff]
    type = MatDiffusion
    variable = u
    diffusivity = diff_u
  [../]
  [./u_src]
    type = BodyForce
    variable = u
    function = source_u
  [../]
  [./v_dt]
    type = TimeDerivative
    variable = v
  [../]
  [./v_diff]
    type = MatDiffusion
    variable = v
    diffusivity = diff_v
  [../]
  [./v_src]
    type = BodyForce
    variable = v
    function = source_v
  [../]
[]

[Materials]
  [./diffusion]
    type = GenericConstantMaterial
    prop_names = 'diff_u diff_v'
    prop_values = '1.0 0.25'
  [../]
[]

[BCs]
  [./u_left]
    type = FunctionDirichletBC
    variable = u
    boundary = left
    function = bc_left
  [../]
  [./u_right]
    type = FunctionDirichletBC
    variable = u
    boundary = right
    function = bc_right
  [../]
  [./v_left]
    type = DirichletBC
    variable = v
    boundary = left
    value = 0
  [../]
  [./v_right]
    type = DirichletBC
    variable = v
    boundary = right
    value = 0
  [../]
[]

[Postprocessors]
  [./u_avg]
    type = ElementAverageValue
    variable = u
  [../]
  [./v_avg]
    type = ElementAverageValue
    variable = v
  [../]
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  scheme = 'bdf2'
  dt = 0.01
  end_time = 0.2
[]

[Outputs]
  exodus = true
  csv = true
[]
)").arg(mesh_path);
}

QString MoosePanel::template_tm_generated_mesh() const {
  return R"([Mesh]
  type = GeneratedMesh
  dim = 2
  nx = 30
  ny = 30
  xmin = 0
  xmax = 1
  ymin = 0
  ymax = 1
[]

[Variables]
  [./T]
    initial_condition = 300
  [../]
  [./disp_x]
  [../]
  [./disp_y]
  [../]
[]

[AuxVariables]
  [./sigma_xx]
    order = CONSTANT
    family = MONOMIAL
  [../]
  [./sigma_yy]
    order = CONSTANT
    family = MONOMIAL
  [../]
  [./sigma_xy]
    order = CONSTANT
    family = MONOMIAL
  [../]
[]

[Kernels]
  active = 'TensorMechanics htcond Q_function'
  [./htcond]
    type = HeatConduction
    variable = T
  [../]
  [./TensorMechanics]
    displacements = 'disp_x disp_y'
  [../]
  [./Q_function]
    type = BodyForce
    variable = T
    value = 1
    function = 50.0*exp(-t)*sin(3.14159*x)*sin(3.14159*y)
  [../]
[]

[AuxKernels]
  [./sigma_xx]
    type = RankTwoAux
    variable = sigma_xx
    rank_two_tensor = stress
    index_i = 0
    index_j = 0
  [../]
  [./sigma_yy]
    type = RankTwoAux
    variable = sigma_yy
    rank_two_tensor = stress
    index_i = 1
    index_j = 1
  [../]
  [./sigma_xy]
    type = RankTwoAux
    variable = sigma_xy
    rank_two_tensor = stress
    index_i = 0
    index_j = 1
  [../]
[]

[BCs]
  [./temp_left]
    type = DirichletBC
    variable = T
    boundary = left
    value = 400
  [../]
  [./temp_right]
    type = DirichletBC
    variable = T
    boundary = right
    value = 300
  [../]
  [./fix_x]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0
  [../]
  [./fix_y]
    type = DirichletBC
    variable = disp_y
    boundary = bottom
    value = 0
  [../]
[]

[Materials]
  [./thcond]
    type = GenericConstantMaterial
    prop_names = 'thermal_conductivity'
    prop_values = '1.0'
  [../]
  [./elastic]
    type = ComputeElasticityTensor
    fill_method = symmetric_isotropic
    C_ijkl = '2.1e5 0.8e5'
  [../]
  [./strain]
    type = ComputeSmallStrain
    displacements = 'disp_x disp_y'
    eigenstrain_names = eigenstrain
  [../]
  [./stress]
    type = ComputeLinearElasticStress
  [../]
  [./thermal_strain]
    type = ComputeThermalExpansionEigenstrain
    thermal_expansion_coeff = 1e-5
    temperature = T
    stress_free_temperature = 300
    eigenstrain_name = eigenstrain
  [../]
[]

[Executioner]
  type = Transient
  scheme = bdf2
  dt = 0.05
  end_time = 0.5
  solve_type = PJFNK
  nl_max_its = 10
  l_max_its = 30
  nl_abs_tol = 1e-8
  l_tol = 1e-4
[]

[Outputs]
  exodus = true
  csv = true
[]
)";
}

QString MoosePanel::template_tm_file_mesh(const QString& mesh_path) const {
  return QString(R"([Mesh]
  type = FileMesh
  file = %1
[]

[Variables]
  [./T]
    initial_condition = 300
  [../]
  [./disp_x]
  [../]
  [./disp_y]
  [../]
[]

[AuxVariables]
  [./sigma_xx]
    order = CONSTANT
    family = MONOMIAL
  [../]
  [./sigma_yy]
    order = CONSTANT
    family = MONOMIAL
  [../]
  [./sigma_xy]
    order = CONSTANT
    family = MONOMIAL
  [../]
[]

[Kernels]
  active = 'TensorMechanics htcond Q_function'
  [./htcond]
    type = HeatConduction
    variable = T
  [../]
  [./TensorMechanics]
    displacements = 'disp_x disp_y'
  [../]
  [./Q_function]
    type = BodyForce
    variable = T
    value = 1
    function = 50.0*exp(-t)*sin(3.14159*x)*sin(3.14159*y)
  [../]
[]

[AuxKernels]
  [./sigma_xx]
    type = RankTwoAux
    variable = sigma_xx
    rank_two_tensor = stress
    index_i = 0
    index_j = 0
  [../]
  [./sigma_yy]
    type = RankTwoAux
    variable = sigma_yy
    rank_two_tensor = stress
    index_i = 1
    index_j = 1
  [../]
  [./sigma_xy]
    type = RankTwoAux
    variable = sigma_xy
    rank_two_tensor = stress
    index_i = 0
    index_j = 1
  [../]
[]

[BCs]
  [./temp_left]
    type = DirichletBC
    variable = T
    boundary = left
    value = 400
  [../]
  [./temp_right]
    type = DirichletBC
    variable = T
    boundary = right
    value = 300
  [../]
  [./fix_x]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0
  [../]
  [./fix_y]
    type = DirichletBC
    variable = disp_y
    boundary = bottom
    value = 0
  [../]
[]

[Materials]
  [./thcond]
    type = GenericConstantMaterial
    prop_names = 'thermal_conductivity'
    prop_values = '1.0'
  [../]
  [./elastic]
    type = ComputeElasticityTensor
    fill_method = symmetric_isotropic
    C_ijkl = '2.1e5 0.8e5'
  [../]
  [./strain]
    type = ComputeSmallStrain
    displacements = 'disp_x disp_y'
    eigenstrain_names = eigenstrain
  [../]
  [./stress]
    type = ComputeLinearElasticStress
  [../]
  [./thermal_strain]
    type = ComputeThermalExpansionEigenstrain
    thermal_expansion_coeff = 1e-5
    temperature = T
    stress_free_temperature = 300
    eigenstrain_name = eigenstrain
  [../]
[]

[Executioner]
  type = Transient
  scheme = bdf2
  dt = 0.05
  end_time = 0.5
  solve_type = PJFNK
  nl_max_its = 10
  l_max_its = 30
  nl_abs_tol = 1e-8
  l_tol = 1e-4
[]

[Outputs]
  exodus = true
  csv = true
[]
)").arg(mesh_path);
}

QString MoosePanel::inject_mesh_block(const QString& input,
                                      const QString& mesh_path) const {
  QStringList lines = input.split('\n');
  int start = -1;
  int end = -1;
  for (int i = 0; i < lines.size(); ++i) {
    if (lines[i].trimmed() == "[Mesh]") {
      start = i;
      for (int j = i + 1; j < lines.size(); ++j) {
        if (lines[j].trimmed() == "[]") {
          end = j;
          break;
        }
      }
      break;
    }
  }
  const QString block = QStringList({
      "[Mesh]",
      "  type = FileMesh",
      QString("  file = %1").arg(mesh_path),
      "[]",
  }).join('\n');

  if (start >= 0 && end >= start) {
    lines.erase(lines.begin() + start, lines.begin() + end + 1);
    lines.insert(start, block);
    return lines.join('\n');
  }

  // Prepend if not found.
  return block + "\n\n" + input;
}

QString MoosePanel::inject_bcs_block(const QString& input,
                                     const QStringList& names,
                                     bool force) const {
  if (names.isEmpty()) {
    return QString();
  }

  const QStringList sanitized = sanitize_names(names);
  QStringList bcs_lines;
  bcs_lines << "[BCs]";
  for (int i = 0; i < names.size(); ++i) {
    const QString name = names.at(i);
    const QString safe = sanitized.at(i);
    bcs_lines << QString("  [./%1]").arg(safe);
    bcs_lines << "    type = DirichletBC";
    bcs_lines << "    variable = u";
    bcs_lines << QString("    boundary = %1").arg(name);
    bcs_lines << "    value = 0";
    bcs_lines << "  [../]";
  }
  bcs_lines << "[]";
  const QString block = bcs_lines.join('\n');

  QStringList lines = input.split('\n');
  int start = -1;
  int end = -1;
  for (int i = 0; i < lines.size(); ++i) {
    if (lines[i].trimmed() == "[BCs]") {
      start = i;
      for (int j = i + 1; j < lines.size(); ++j) {
        if (lines[j].trimmed() == "[]") {
          end = j;
          break;
        }
      }
      break;
    }
  }

  if (start >= 0 && end >= start) {
    if (!force) {
      const QString existing =
          lines.mid(start, end - start + 1).join("\n");
      if (!existing.contains("boundary = left") &&
          !existing.contains("boundary = right") &&
          !existing.contains("boundary = boundary")) {
        return QString();
      }
    }
    lines.erase(lines.begin() + start, lines.begin() + end + 1);
    lines.insert(start, block);
    return lines.join('\n');
  }

  return input + "\n\n" + block;
}

QStringList MoosePanel::sanitize_names(const QStringList& names) const {
  QStringList out;
  for (const auto& n : names) {
    QString s = n;
    for (int i = 0; i < s.size(); ++i) {
      const QChar c = s.at(i);
      if (!c.isLetterOrNumber() && c != '_') {
        s[i] = '_';
      }
    }
    if (s.isEmpty()) {
      s = "bc";
    }
    out << s;
  }
  return out;
}

void MoosePanel::load_settings() {
  QSettings settings("gmp-ise", "gmp_ise");
  const QStringList history =
      settings.value("moose/exec_history").toStringList();
  const QString last = settings.value("moose/exec_last").toString();
  exec_path_->clear();
  exec_path_->addItems(history);
  if (!last.isEmpty()) {
    exec_path_->setCurrentText(last);
  } else if (!history.isEmpty()) {
    exec_path_->setCurrentText(history.front());
  }
  input_path_->setText(
      settings.value("moose/input_path", input_path_->text()).toString());
  workdir_path_->setText(
      settings.value("moose/workdir", workdir_path_->text()).toString());
  mesh_path_->setText(
      settings.value("moose/mesh_path", mesh_path_->text()).toString());
  use_mpi_->setChecked(
      settings.value("moose/use_mpi", use_mpi_->isChecked()).toBool());
  mpi_ranks_->setValue(
      settings.value("moose/mpi_ranks", mpi_ranks_->value()).toInt());
  runner_kind_->setCurrentIndex(
      settings.value("moose/runner_kind", runner_kind_->currentIndex()).toInt());
  template_kind_->setCurrentIndex(
      settings.value("moose/template_kind", template_kind_->currentIndex())
          .toInt());
  extra_args_->setText(
      settings.value("moose/extra_args", extra_args_->text()).toString());
  sim_server_->setText(
      settings.value("moose/sim_server", sim_server_->text()).toString());
  sim_project_->setText(
      settings.value("moose/sim_project", sim_project_->text()).toString());

  if (exec_path_->currentText().trimmed().isEmpty()) {
    const QString detected = auto_detect_exec();
    if (!detected.isEmpty()) {
      update_exec_history(detected);
    }
  }
}

void MoosePanel::save_settings() const {
  QSettings settings("gmp-ise", "gmp_ise");
  settings.setValue("moose/exec_last", exec_path_->currentText());
  settings.setValue("moose/input_path", input_path_->text());
  settings.setValue("moose/workdir", workdir_path_->text());
  settings.setValue("moose/mesh_path", mesh_path_->text());
  settings.setValue("moose/use_mpi", use_mpi_->isChecked());
  settings.setValue("moose/mpi_ranks", mpi_ranks_->value());
  settings.setValue("moose/runner_kind", runner_kind_->currentIndex());
  settings.setValue("moose/template_kind", template_kind_->currentIndex());
  settings.setValue("moose/extra_args", extra_args_->text());
  settings.setValue("moose/sim_server", sim_server_->text());
  settings.setValue("moose/sim_project", sim_project_->text());
  QStringList history;
  for (int i = 0; i < exec_path_->count(); ++i) {
    history << exec_path_->itemText(i);
  }
  settings.setValue("moose/exec_history", history);
}

void MoosePanel::update_exec_history(const QString& path) {
  if (path.isEmpty()) {
    return;
  }
  const int existing = exec_path_->findText(path);
  if (existing >= 0) {
    exec_path_->removeItem(existing);
  }
  exec_path_->insertItem(0, path);
  exec_path_->setCurrentText(path);
  save_settings();
}

QString MoosePanel::auto_detect_exec() const {
  const QByteArray env = qgetenv("GMP_MOOSE_EXEC");
  if (!env.isEmpty()) {
    const QString path = QString::fromLocal8Bit(env);
    if (is_executable_file(path)) {
      return QDir(path).absolutePath();
    }
  }

  QString found = QStandardPaths::findExecutable("moose_test-opt");
  if (is_executable_file(found)) {
    return found;
  }
  found = QStandardPaths::findExecutable("moose_test-dbg");
  if (is_executable_file(found)) {
    return found;
  }

  found = find_exec_in_parents("external/moose/test/moose_test-opt", 6);
  if (is_executable_file(found)) {
    return found;
  }
  found = find_exec_in_parents("external/moose/test/moose_test-dbg", 6);
  if (is_executable_file(found)) {
    return found;
  }

  return QString();
}

QString MoosePanel::find_exec_in_parents(const QString& relative,
                                         int max_levels) const {
  QDir dir(QDir::currentPath());
  for (int i = 0; i <= max_levels; ++i) {
    const QString candidate = dir.filePath(relative);
    if (is_executable_file(candidate)) {
      return candidate;
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return QString();
}

QStringList MoosePanel::read_boundary_groups_from_mesh(const QString& mesh_path) const {
  QStringList names;
#ifndef GMP_ENABLE_GMSH_GUI
  return parse_msh_physical_groups(mesh_path);
#else
  try {
    if (!gmsh::isInitialized()) {
      gmsh::initialize();
    }
    gmsh::clear();
    gmsh::open(mesh_path.toStdString());
    std::vector<std::pair<int, int>> phys_groups;
    gmsh::model::getPhysicalGroups(phys_groups);
    int max_dim = 0;
    {
      std::vector<std::pair<int, int>> ents;
      gmsh::model::getEntities(ents);
      for (const auto& e : ents) {
        if (e.first > max_dim) {
          max_dim = e.first;
        }
      }
    }
    const int boundary_dim = std::max(0, max_dim - 1);
    for (const auto& p : phys_groups) {
      if (p.first != boundary_dim) {
        continue;
      }
      std::string name;
      gmsh::model::getPhysicalName(p.first, p.second, name);
      if (name.empty()) {
        name = "boundary_" + std::to_string(p.second);
      }
      names << QString::fromStdString(name);
    }
  } catch (const std::exception& ex) {
    Q_UNUSED(ex);
    return parse_msh_physical_groups(mesh_path);
  }
  return names;
#endif
}

QStringList MoosePanel::parse_msh_physical_groups(const QString& mesh_path) const {
  QStringList names;
  QFile file(mesh_path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return names;
  }

  QMap<int, QStringList> names_by_dim;
  int max_dim = 0;
  bool in_section = false;
  while (!file.atEnd()) {
    const QByteArray raw = file.readLine();
    const QString line = QString::fromUtf8(raw).trimmed();
    if (line == "$PhysicalNames") {
      in_section = true;
      // Next line is count; ignore.
      file.readLine();
      continue;
    }
    if (in_section) {
      if (line == "$EndPhysicalNames") {
        break;
      }
      // Format: <dim> <tag> "<name>"
      const QRegularExpression re(
          R"m(^\s*(\d+)\s+(\d+)\s+"(.*)"\s*$)m");
      const QRegularExpressionMatch m = re.match(line);
      if (m.hasMatch()) {
        const int dim = m.captured(1).toInt();
        const QString name = m.captured(3);
        if (dim > max_dim) {
          max_dim = dim;
        }
        if (!name.isEmpty()) {
          names_by_dim[dim] << name;
        }
      }
    }
  }

  const int boundary_dim = std::max(0, max_dim - 1);
  names = names_by_dim.value(boundary_dim);
  return names;
}

QString MoosePanel::find_latest_exodus(const QString& dir_path) const {
  QDir dir(dir_path);
  if (!dir.exists()) {
    return QString();
  }
  const QFileInfoList files =
      dir.entryInfoList(QStringList() << "*.e", QDir::Files, QDir::Time);
  if (files.isEmpty()) {
    return QString();
  }
  return files.first().absoluteFilePath();
}

QStringList MoosePanel::list_exodus_files(const QString& dir_path) const {
  QStringList list;
  QDir dir(dir_path);
  if (!dir.exists()) {
    return list;
  }
  const QFileInfoList files =
      dir.entryInfoList(QStringList() << "*.e" << "*.e-s*",
                        QDir::Files, QDir::Time);
  for (const auto& f : files) {
    list << f.absoluteFilePath();
  }
  return list;
}

QString MoosePanel::pick_latest_exodus(const QStringList& files) const {
  QFileInfo newest;
  for (const auto& path : files) {
    const QFileInfo fi(path);
    if (!fi.exists()) {
      continue;
    }
    if (!newest.exists() || fi.lastModified() > newest.lastModified()) {
      newest = fi;
    }
  }
  return newest.exists() ? newest.absoluteFilePath() : QString();
}

QStringList MoosePanel::collect_exodus_files(const QStringList& dirs) const {
  QSet<QString> seen;
  QStringList files;
  for (const auto& dir : dirs) {
    const QStringList in_dir = list_exodus_files(dir);
    for (const auto& path : in_dir) {
      if (seen.contains(path)) {
        continue;
      }
      seen.insert(path);
      files << path;
    }
  }
  std::sort(files.begin(), files.end(),
            [](const QString& a, const QString& b) {
              return QFileInfo(a).lastModified() > QFileInfo(b).lastModified();
            });
  return files;
}

}  // namespace gmp
