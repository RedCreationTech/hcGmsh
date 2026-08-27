#include "gmp/SimClient.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

#include "gmp/MooseSnapshot.h"

namespace gmp {

namespace {

QUrl make_url(const QString& base, const QString& path) {
  QUrl url(base);
  url.setPath(path);
  return url;
}

// 逐段百分号编码包内相对路径（保留 '/' 分隔符），交给 QUrl::setPath 解码侧语义
QString bundle_file_url_path(const QString& job_id, const QString& file_path) {
  return QStringLiteral("/api/sim/bundles/") + job_id + "/" + file_path;
}

}  // namespace

SimClient::SimClient(QObject* parent) : QObject(parent) {
  const QString configured =
      QString::fromLocal8Bit(qgetenv("GMP_LIMS_BASE_URL")).trimmed();
  if (!configured.isEmpty()) {
    set_base_url(configured);
  }
  // 大文件下载时只要数据持续到达即不超时
  nam_.setTransferTimeout(60000);
}

void SimClient::set_base_url(const QString& base_url) {
  base_url_ = base_url.trimmed();
  while (base_url_.endsWith('/')) {
    base_url_.chop(1);
  }
}

QJsonObject SimClient::build_submission_manifest(
    const QJsonObject& snapshot_manifest, const QString& project_id,
    const QString& solver_program, QString* error) {
  const auto fail = [error](const QString& msg) {
    if (error) {
      *error = msg;
    }
    return QJsonObject();
  };
  if (project_id.trimmed().isEmpty()) {
    return fail("project_id 为空");
  }
  const QJsonObject snap =
      snapshot_manifest.value("input_snapshot").toObject();
  if (snap.isEmpty()) {
    return fail("快照 manifest 缺少 input_snapshot 对象");
  }
  const QString input_file = snap.value("input_file").toString();
  static const QRegularExpression input_re(QStringLiteral("\\.i$"));
  if (!input_re.match(input_file).hasMatch()) {
    return fail("input_file 非法（需 .i 后缀）: " + input_file);
  }
  const QString input_sha = snap.value("input_sha256").toString();
  static const QRegularExpression sha_re(QStringLiteral("^[0-9a-f]{64}$"));
  if (!sha_re.match(input_sha).hasMatch()) {
    return fail("input_sha256 非法: " + input_sha);
  }
  const QString case_name =
      snapshot_manifest.value("case_name").toString(input_file);
  if (solver_program.contains('/') || solver_program.contains('\\')) {
    return fail("solver 程序名不允许路径分量: " + solver_program);
  }

  QJsonObject sub;
  sub.insert("project_id", project_id.trimmed());
  sub.insert("case_name", case_name);
  sub.insert("input_file", input_file);
  sub.insert("input_sha256", input_sha);
  sub.insert("mesh_files", snap.value("mesh_files").toArray());
  sub.insert("extra_files", snap.value("extra_files").toArray());
  sub.insert("command", solver_program + " -i " + input_file);
  return sub;
}

QString SimClient::describe_http_error(int status, const QJsonObject& body) {
  const QJsonObject failure = body.value("failure").toObject();
  if (!failure.isEmpty()) {
    return QString("failure %1: %2")
        .arg(failure.value("code").toString("?"),
             failure.value("message").toString("?"));
  }
  const QString message = body.value("message").toString();
  if (!message.isEmpty()) {
    const QString code = body.value("code").toString();
    return code.isEmpty() ? message : (code + ": " + message);
  }
  const QString error = body.value("error").toString();
  if (!error.isEmpty()) {
    return error;
  }
  return QString("HTTP %1").arg(status);
}

void SimClient::submit_snapshot(const QString& snapshot_dir,
                                const QString& project_id) {
  const QString manifest_path = snapshot_dir + "/manifest.json";
  QFile manifest_file(manifest_path);
  if (!manifest_file.open(QIODevice::ReadOnly)) {
    emit submit_finished(false, QJsonObject(),
                         "无法读取快照 manifest: " + manifest_path);
    return;
  }
  const QJsonDocument doc =
      QJsonDocument::fromJson(manifest_file.readAll());
  if (!doc.isObject()) {
    emit submit_finished(false, QJsonObject(),
                         "快照 manifest 不是合法 JSON: " + manifest_path);
    return;
  }
  QString error;
  const QJsonObject sub = build_submission_manifest(
      doc.object(), project_id, QStringLiteral("DamSafetyApp-opt"), &error);
  if (sub.isEmpty()) {
    emit submit_finished(false, doc.object(), error);
    return;
  }

  // 收集上传文件：主输入 + mesh_files + extra_files（不含 manifest.json）
  QStringList names;
  names << sub.value("input_file").toString();
  const auto append_names = [&names](const QJsonValue& v) {
    const QJsonArray arr = v.toArray();
    for (const auto& item : arr) {
      names << item.toObject().value("name").toString();
    }
  };
  append_names(sub.value("mesh_files"));
  append_names(sub.value("extra_files"));

  auto* multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
  QHttpPart manifest_part;
  manifest_part.setHeader(
      QNetworkRequest::ContentDispositionHeader,
      QVariant(QStringLiteral("form-data; name=\"manifest\"")));
  manifest_part.setBody(QJsonDocument(sub).toJson(QJsonDocument::Compact));
  multi->append(manifest_part);

  for (const auto& name : names) {
    const QString path = snapshot_dir + "/" + name;
    auto* file = new QFile(path);
    if (!file->open(QIODevice::ReadOnly)) {
      delete file;
      multi->deleteLater();
      emit submit_finished(false, doc.object(),
                           "无法读取快照文件: " + path);
      return;
    }
    QHttpPart part;
    part.setHeader(QNetworkRequest::ContentTypeHeader,
                   QVariant(QStringLiteral("application/octet-stream")));
    part.setHeader(QNetworkRequest::ContentDispositionHeader,
                   QVariant(QString("form-data; name=\"files\"; "
                                    "filename=\"%1\"")
                                .arg(QString::fromUtf8(
                                    QUrl::toPercentEncoding(name)))));
    part.setBodyDevice(file);
    file->setParent(multi);
    multi->append(part);
  }

  QNetworkRequest request(make_url(base_url_, QStringLiteral("/api/sim/jobs")));
  QNetworkReply* reply = nam_.post(request, multi);
  multi->setParent(reply);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const QByteArray raw = reply->readAll();
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonDocument body_doc = QJsonDocument::fromJson(raw);
    const QJsonObject body = body_doc.object();
    if (reply->error() != QNetworkReply::NoError && status == 0) {
      emit submit_finished(false, body,
                           "网络错误（LIMS Facade 不可达？）: " +
                               reply->errorString());
    } else if (status == 201) {
      emit submit_finished(true, body, QString());
    } else {
      emit submit_finished(false, body, describe_http_error(status, body));
    }
    reply->deleteLater();
  });
}

void SimClient::fetch_job(const QString& job_id) {
  QNetworkRequest request(
      make_url(base_url_, QStringLiteral("/api/sim/jobs/") + job_id));
  QNetworkReply* reply = nam_.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const QByteArray raw = reply->readAll();
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonObject body = QJsonDocument::fromJson(raw).object();
    if (reply->error() != QNetworkReply::NoError && status == 0) {
      emit job_fetched(false, body,
                       "网络错误（LIMS Facade 不可达？）: " +
                           reply->errorString());
    } else if (status >= 200 && status < 300) {
      emit job_fetched(true, body, QString());
    } else {
      emit job_fetched(false, body, describe_http_error(status, body));
    }
    reply->deleteLater();
  });
}

void SimClient::fetch_bundles() {
  QNetworkRequest request(
      make_url(base_url_, QStringLiteral("/api/sim/bundles")));
  QNetworkReply* reply = nam_.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const QByteArray raw = reply->readAll();
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonObject body = QJsonDocument::fromJson(raw).object();
    if (reply->error() != QNetworkReply::NoError && status == 0) {
      emit bundles_fetched(false, QJsonArray(),
                           "网络错误（LIMS Facade 不可达？）: " +
                               reply->errorString());
    } else if (status >= 200 && status < 300) {
      emit bundles_fetched(true, body.value("bundles").toArray(), QString());
    } else {
      emit bundles_fetched(false, QJsonArray(),
                           describe_http_error(status, body));
    }
    reply->deleteLater();
  });
}

void SimClient::download_bundle_file(const QString& job_id,
                                     const QString& file_path,
                                     const QString& expected_sha256,
                                     const QString& dest_path) {
  QDir().mkpath(QFileInfo(dest_path).absolutePath());
  auto* out = new QFile(dest_path);
  if (!out->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    delete out;
    emit download_finished(false, dest_path,
                           "无法写入本地缓存: " + dest_path);
    return;
  }

  QNetworkRequest request(
      make_url(base_url_, bundle_file_url_path(job_id, file_path)));
  QNetworkReply* reply = nam_.get(request);
  auto* ctx = new DownloadCtx{dest_path, expected_sha256, out};
  downloads_.insert(reply, ctx);

  connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
    DownloadCtx* c = downloads_.value(reply);
    if (c && c->file) {
      c->file->write(reply->readAll());
    }
  });
  connect(reply, &QNetworkReply::downloadProgress, this,
          [this](qint64 received, qint64 total) {
            emit download_progress(received, total);
          });
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    DownloadCtx* c = downloads_.take(reply);
    if (!c) {
      reply->deleteLater();
      return;
    }
    const QString dest = c->dest_path;
    const QString expected = c->expected_sha256;
    QFile* file = c->file;
    delete c;

    QString error;
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError) {
      error = status == 0
                  ? ("网络错误: " + reply->errorString())
                  : describe_http_error(
                        status,
                        QJsonDocument::fromJson(reply->readAll()).object());
    }
    reply->deleteLater();

    file->flush();
    file->close();
    delete file;

    if (error.isEmpty() && !expected.isEmpty()) {
      bool ok = false;
      const QString actual = sha256_file_hex(dest, &ok);
      if (!ok || actual != expected) {
        error = "SHA-256 校验不一致: " + dest;
      }
    }
    if (!error.isEmpty()) {
      QFile::remove(dest);
      emit download_finished(false, dest, error);
      return;
    }
    emit download_finished(true, dest, QString());
  });
}

}  // namespace gmp
