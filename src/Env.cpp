#include "gmp/Env.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace gmp {

namespace {

bool load_file(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }

  static const QRegularExpression key_re(
      QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
  const QString content = QString::fromUtf8(file.readAll());
  for (QString line : content.split('\n')) {
    if (!line.isEmpty() && line.front() == QChar::ByteOrderMark) {
      line.remove(0, 1);
    }
    line = line.trimmed();
    if (line.isEmpty() || line.startsWith('#')) {
      continue;
    }
    if (line.startsWith(QStringLiteral("export "))) {
      line = line.mid(7).trimmed();
    }

    const int equals = line.indexOf('=');
    if (equals <= 0) {
      continue;
    }
    const QString key = line.left(equals).trimmed();
    QString value = line.mid(equals + 1).trimmed();
    if (!key_re.match(key).hasMatch()) {
      continue;
    }

    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
      value = value.mid(1, value.size() - 2);
    } else {
      const int comment = value.indexOf(
          QRegularExpression(QStringLiteral("\\s+#")));
      if (comment >= 0) {
        value = value.left(comment).trimmed();
      }
    }

    const QByteArray key_bytes = key.toUtf8();
    if (!qEnvironmentVariableIsSet(key_bytes.constData())) {
      qputenv(key_bytes.constData(), value.toUtf8());
    }
  }
  return true;
}

}  // namespace

QString load_dotenv() {
  QStringList candidates;
  const QString explicit_path =
      QString::fromLocal8Bit(qgetenv("GMP_ENV_FILE")).trimmed();
  if (!explicit_path.isEmpty()) {
    candidates << explicit_path;
  } else {
    candidates << QDir::current().filePath(QStringLiteral(".env"));
    const QDir app_dir(QCoreApplication::applicationDirPath());
    candidates << app_dir.filePath(QStringLiteral(".env"));
    candidates << app_dir.filePath(QStringLiteral("../.env"));
  }

  QSet<QString> visited;
  for (const QString& candidate : candidates) {
    const QString path = QFileInfo(candidate).absoluteFilePath();
    if (visited.contains(path)) {
      continue;
    }
    visited.insert(path);
    if (load_file(path)) {
      return path;
    }
  }
  return QString();
}

}  // namespace gmp
