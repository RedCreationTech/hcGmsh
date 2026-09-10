#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class QFile;
class QNetworkReply;

namespace gmp {

// LIMS 仿真 Facade (CONTRACT-API) 客户端：作业提交/状态轮询/制品列表/制品下载。
// 只经 LIMS Facade（默认 http://127.0.0.1:8200），浏览器/客户端不直连 C06，
// 不携带、不保存任何凭据。
class SimClient : public QObject {
  Q_OBJECT
 public:
  explicit SimClient(QObject* parent = nullptr);

  void set_base_url(const QString& base_url);
  QString base_url() const { return base_url_; }

  // 由 C01 任务快照 manifest（export_job_snapshot 产出的
  // contract/input_snapshot 结构）构造 C06 提交侧 manifest
  // （project_id/case_name/input_file/input_sha256/mesh_files/extra_files/
  // command/solver_program，v2 快照另透传 profile_id/profile_version/
  // mapping_version——向后兼容，服务端可忽略未知字段）。
  // command 采用白名单形态 "<solver_program> -i <input_file>"。
  // 失败返回空对象并填写 error。
  static QJsonObject build_submission_manifest(
      const QJsonObject& snapshot_manifest, const QString& project_id,
      const QString& solver_program, QString* error);

  // ---- TASK-E2E-11：提交与轮询 ----
  // snapshot_dir 为导出任务快照目录（含 manifest.json 与全部输入文件）。
  // solver 程序名读快照 manifest 的 application_profile.solver_program；
  // v1 旧快照缺该字段时回落到缺省 "DamSafetyApp-opt" 并打日志说明。
  void submit_snapshot(const QString& snapshot_dir, const QString& project_id);
  void fetch_job(const QString& job_id);
  // 拉取任务摘要列表（GET /api/sim/jobs，可带 project_id/limit），
  // 用于把历史会话提交的作业同步进作业列表。
  void fetch_jobs(const QString& project_id, int limit = 50);
  // ---- 作业监控（参照 LIMS 任务监控/制品库）----
  // 实时执行状态：资源/进度/计时/收敛/health。
  void fetch_execution_status(const QString& job_id);
  // 作业工作区文件清单（任意状态可查；运行中为实时快照）。
  void fetch_job_files(const QString& job_id);
  // 求解日志尾（text/plain）。
  void fetch_job_log(const QString& job_id, int tail = 200);
  // 取消任务（202/404/409 透传）。
  void cancel_job(const QString& job_id);
  // 下载作业工作区单文件（运行中为实时快照，不做 sha256 校验）。
  void download_job_file(const QString& job_id, const QString& file_path,
                         const QString& dest_path);

  // ---- TASK-E2E-12：制品包 ----
  void fetch_bundles();
  // file_path 为包内相对路径（如 results/xxx_out.e），dest_path 为本地落点；
  // 下载完成后按 expected_sha256 校验，不一致则删除并报告错误。
  void download_bundle_file(const QString& job_id, const QString& file_path,
                            const QString& expected_sha256,
                            const QString& dest_path);

 signals:
  void submit_finished(bool ok, const QJsonObject& body, const QString& error);
  void job_fetched(bool ok, const QJsonObject& body, const QString& error);
  void jobs_fetched(bool ok, const QJsonArray& jobs, const QString& error);
  void execution_status_fetched(bool ok, const QJsonObject& body,
                                const QString& error);
  void job_files_fetched(bool ok, const QJsonObject& body,
                         const QString& error);
  void job_log_fetched(bool ok, const QString& text, const QString& error);
  void job_cancel_finished(bool ok, const QJsonObject& body,
                           const QString& error);
  void job_file_downloaded(bool ok, const QString& dest_path,
                           const QString& file_path, const QString& error);
  void bundles_fetched(bool ok, const QJsonArray& bundles,
                       const QString& error);
  void download_progress(qint64 received, qint64 total);
  void download_finished(bool ok, const QString& dest_path,
                         const QString& error);

 private:
  struct DownloadCtx {
    QString dest_path;
    QString expected_sha256;
    QFile* file = nullptr;
  };

  // 从响应体提取可诊断错误：422 failure.code+message / 400 code+message /
  // Facade 502 error / 其余 HTTP 状态码。
  static QString describe_http_error(int status, const QJsonObject& body);

  QNetworkAccessManager nam_;
  QString base_url_ = QStringLiteral("http://127.0.0.1:8200");
  QHash<QNetworkReply*, DownloadCtx*> downloads_;
};

}  // namespace gmp
