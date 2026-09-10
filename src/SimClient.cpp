#include "gmp/SimClient.h"

#include <QDebug>
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
#include <QUrlQuery>

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
  // 服务端对文件条目同样严格校验：v2 快照条目带 role 等溯源字段，
  // 提交时裁剪为服务端合同允许的 {name, sha256}（role 留在快照
  // manifest.json 中，不进提交报文）。
  const auto sanitize_files = [](const QJsonValue& files) {
    QJsonArray out;
    for (const auto& value : files.toArray()) {
      const QJsonObject entry = value.toObject();
      QJsonObject slim;
      slim.insert("name", entry.value("name").toString());
      slim.insert("sha256", entry.value("sha256").toString());
      out.append(slim);
    }
    return out;
  };
  sub.insert("mesh_files", sanitize_files(snap.value("mesh_files")));
  sub.insert("extra_files", sanitize_files(snap.value("extra_files")));
  // solver 程序名经 command 字段传达（W-00c 起从活动档案读取）。
  // 服务端按严格 schema 校验提交清单（additionalProperties=false），
  // 不得附加 solver_program/profile_* 等额外键——档案溯源信息保留在
  // 快照自带的 manifest.json 中，提交清单只放服务端合同允许的 7 个键。
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
  // W-00c：solver 程序名优先读快照 manifest 的 application_profile
  // （v2 合同）；v1 旧快照缺该字段时回落到缺省 profile 并打日志说明。
  QString solver_program = doc.object()
                               .value(QStringLiteral("application_profile"))
                               .toObject()
                               .value(QStringLiteral("solver_program"))
                               .toString()
                               .trimmed();
  if (solver_program.isEmpty()) {
    solver_program = QStringLiteral("DamSafetyApp-opt");
    qWarning() << "SimClient: snapshot manifest has no"
                  " application_profile.solver_program; falling back to"
                  " default solver" << solver_program;
  }
  QString error;
  const QJsonObject sub = build_submission_manifest(doc.object(), project_id,
                                                    solver_program, &error);
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

void SimClient::fetch_jobs(const QString& project_id, int limit) {
  QUrlQuery query;
  if (!project_id.trimmed().isEmpty()) {
    query.addQueryItem("project_id", project_id.trimmed());
  }
  if (limit > 0) {
    query.addQueryItem("limit", QString::number(limit));
  }
  QUrl url = make_url(base_url_, QStringLiteral("/api/sim/jobs"));
  url.setQuery(query);
  QNetworkRequest request(url);
  QNetworkReply* reply = nam_.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const QByteArray raw = reply->readAll();
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonObject body = QJsonDocument::fromJson(raw).object();
    if (reply->error() != QNetworkReply::NoError && status == 0) {
      emit jobs_fetched(false, QJsonArray(),
                        "网络错误（LIMS Facade 不可达？）: " +
                            reply->errorString());
    } else if (status >= 200 && status < 300) {
      emit jobs_fetched(true, body.value("jobs").toArray(), QString());
    } else {
      emit jobs_fetched(false, QJsonArray(),
                        describe_http_error(status, body));
    }
    reply->deleteLater();
  });
}

void SimClient::fetch_execution_status(const QString& job_id) {
  QNetworkRequest request(make_url(
      base_url_, QStringLiteral("/api/sim/jobs/") + job_id +
                     QStringLiteral("/execution-status")));
  QNetworkReply* reply = nam_.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const QByteArray raw = reply->readAll();
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonObject body = QJsonDocument::fromJson(raw).object();
    if (reply->error() != QNetworkReply::NoError && status == 0) {
      emit execution_status_fetched(false, body,
                                    "网络错误（LIMS Facade 不可达？）: " +
                                        reply->errorString());
    } else if (status >= 200 && status < 300) {
      emit execution_status_fetched(true, body, QString());
    } else {
      emit execution_status_fetched(false, body,
                                    describe_http_error(status, body));
    }
    reply->deleteLater();
  });
}

void SimClient::fetch_job_files(const QString& job_id) {
  QNetworkRequest request(make_url(base_url_, QStringLiteral("/api/sim/jobs/") +
                                                  job_id +
                                                  QStringLiteral("/files")));
  QNetworkReply* reply = nam_.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const QByteArray raw = reply->readAll();
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonObject body = QJsonDocument::fromJson(raw).object();
    if (reply->error() != QNetworkReply::NoError && status == 0) {
      emit job_files_fetched(false, body,
                             "网络错误（LIMS Facade 不可达？）: " +
                                 reply->errorString());
    } else if (status >= 200 && status < 300) {
      emit job_files_fetched(true, body, QString());
    } else {
      emit job_files_fetched(false, body, describe_http_error(status, body));
    }
    reply->deleteLater();
  });
}

void SimClient::fetch_job_log(const QString& job_id, int tail) {
  QUrl url = make_url(base_url_,
                      QStringLiteral("/api/sim/jobs/") + job_id +
                          QStringLiteral("/log"));
  if (tail > 0) {
    QUrlQuery query;
    query.addQueryItem("tail", QString::number(tail));
    url.setQuery(query);
  }
  QNetworkRequest request(url);
  QNetworkReply* reply = nam_.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const QByteArray raw = reply->readAll();
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError && status == 0) {
      emit job_log_fetched(false, QString(),
                           "网络错误（LIMS Facade 不可达？）: " +
                               reply->errorString());
    } else if (status >= 200 && status < 300) {
      emit job_log_fetched(true, QString::fromUtf8(raw), QString());
    } else {
      const QJsonObject body = QJsonDocument::fromJson(raw).object();
      emit job_log_fetched(false, QString(),
                           describe_http_error(status, body));
    }
    reply->deleteLater();
  });
}

void SimClient::cancel_job(const QString& job_id) {
  QNetworkRequest request(make_url(base_url_, QStringLiteral("/api/sim/jobs/") +
                                                  job_id +
                                                  QStringLiteral("/cancel")));
  QNetworkReply* reply = nam_.post(request, QByteArray());
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const QByteArray raw = reply->readAll();
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonObject body = QJsonDocument::fromJson(raw).object();
    if (reply->error() != QNetworkReply::NoError && status == 0) {
      emit job_cancel_finished(false, body,
                               "网络错误（LIMS Facade 不可达？）: " +
                                   reply->errorString());
    } else if (status >= 200 && status < 300) {
      emit job_cancel_finished(true, body, QString());
    } else {
      emit job_cancel_finished(false, body,
                               describe_http_error(status, body));
    }
    reply->deleteLater();
  });
}

void SimClient::download_job_file(const QString& job_id,
                                  const QString& file_path,
                                  const QString& dest_path) {
  QDir().mkpath(QFileInfo(dest_path).absolutePath());
  auto* out = new QFile(dest_path);
  if (!out->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    delete out;
    emit job_file_downloaded(false, dest_path, file_path,
                             "无法写入本地缓存: " + dest_path);
    return;
  }
  QNetworkRequest request(make_url(base_url_, QStringLiteral("/api/sim/jobs/") +
                                                  job_id +
                                                  QStringLiteral("/files/") +
                                                  file_path));
  QNetworkReply* reply = nam_.get(request);
  connect(reply, &QNetworkReply::readyRead, this,
          [reply, out]() { out->write(reply->readAll()); });
  connect(reply, &QNetworkReply::finished, this,
          [this, reply, out, dest_path, file_path]() {
            out->flush();
            out->close();
            delete out;
            const int status =
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                    .toInt();
            if (reply->error() != QNetworkReply::NoError || status < 200 ||
                status >= 300) {
              QFile::remove(dest_path);
              emit job_file_downloaded(
                  false, dest_path, file_path,
                  status == 0
                      ? "网络错误（LIMS Facade 不可达？）: " +
                            reply->errorString()
                      : QString("下载失败 (HTTP %1)").arg(status));
            } else {
              emit job_file_downloaded(true, dest_path, file_path, QString());
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
