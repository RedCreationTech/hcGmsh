#include "gmp/OperationLog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>
#include <QtGlobal>

#include <utility>

namespace {

QMutex g_log_mutex;
QString g_log_path;
std::function<void(const QString&)> g_console_hook;
QtMessageHandler g_previous_handler = nullptr;
bool g_initialized = false;

constexpr int kKeepLogFiles = 10;

QString format_line(const QString& level, const QString& category,
                    const QString& message) {
  return QString("%1 [%2] %3 | %4")
      .arg(QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz"),
           level, category, message);
}

void write_line(const QString& line) {
  QMutexLocker locker(&g_log_mutex);
  if (!g_log_path.isEmpty()) {
    QFile file(g_log_path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
      QTextStream out(&file);
      out << line << "\n";
    }
  }
  if (g_console_hook) {
    g_console_hook(line);
  }
}

void message_handler(QtMsgType type, const QMessageLogContext& context,
                     const QString& message) {
  // qWarning/qCritical/qFatal 进入操作日志，避免 GUI 启动方式下丢失错误
  // 现场；qInfo/qDebug 仍只走默认 stderr，避免噪音。注意 QtInfoMsg 数值
  // 大于 QtWarningMsg，不能简单用 >= 比较。
  if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
    write_line(format_line("qt", "runtime", message));
  }
  if (g_previous_handler) {
    g_previous_handler(type, context, message);
  }
}

}  // namespace

namespace gmp {

void init_operation_log() {
  QMutexLocker locker(&g_log_mutex);
  if (g_initialized) {
    return;
  }
  g_initialized = true;

  const QString base = QStandardPaths::writableLocation(
      QStandardPaths::AppDataLocation);
  const QString log_dir_path = base + "/logs";
  QDir log_dir(log_dir_path);
  if (!log_dir.mkpath(".")) {
    return;
  }
  g_log_path = log_dir.filePath(
      "operations-" + QDateTime::currentDateTime().toString("yyyy-MM-dd") +
      ".log");

  // 只保留最近 kKeepLogFiles 个日志文件。
  const QFileInfoList files = log_dir.entryInfoList(
      {"operations-*.log"}, QDir::Files, QDir::Name);
  for (int i = 0; i + kKeepLogFiles < files.size(); ++i) {
    QFile::remove(files.at(i).absoluteFilePath());
  }

  g_previous_handler = qInstallMessageHandler(message_handler);

  locker.unlock();
  log_operation("app", "Operation log started: " + g_log_path);
}

QString operation_log_path() {
  QMutexLocker locker(&g_log_mutex);
  return g_log_path;
}

void log_operation(const QString& category, const QString& message) {
  write_line(format_line("op", category, message));
}

void set_operation_log_console_hook(
    std::function<void(const QString&)> hook) {
  QMutexLocker locker(&g_log_mutex);
  g_console_hook = std::move(hook);
}

}  // namespace gmp
