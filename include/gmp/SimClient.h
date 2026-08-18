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
  // （project_id/case_name/input_file/input_sha256/mesh_files/extra_files/command）。
  // command 采用白名单形态 "<solver_program> -i <input_file>"。
  // 失败返回空对象并填写 error。
  static QJsonObject build_submission_manifest(
      const QJsonObject& snapshot_manifest, const QString& project_id,
      const QString& solver_program, QString* error);

  // ---- TASK-E2E-11：提交与轮询 ----
  // snapshot_dir 为导出任务快照目录（含 manifest.json 与全部输入文件）。
  void submit_snapshot(const QString& snapshot_dir, const QString& project_id);
  void fetch_job(const QString& job_id);

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
