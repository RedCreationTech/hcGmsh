#include "gmp/ResultData.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QRegularExpression>
#include <cmath>
#include <limits>

namespace gmp {
namespace {

QStringList split_delimited_line(const QString& line, QChar delimiter) {
  QStringList fields;
  QString field;
  bool quoted = false;
  for (int i = 0; i < line.size(); ++i) {
    const QChar ch = line.at(i);
    if (ch == '"') {
      if (quoted && i + 1 < line.size() && line.at(i + 1) == '"') {
        field += '"';
        ++i;
      } else {
        quoted = !quoted;
      }
    } else if (ch == delimiter && !quoted) {
      fields << field.trimmed();
      field.clear();
    } else {
      field += ch;
    }
  }
  fields << field.trimmed();
  return fields;
}

QChar detect_delimiter(const QString& header) {
  const QList<QChar> candidates = {',', ';', '\t'};
  int best_count = -1;
  QChar best = ',';
  for (const QChar candidate : candidates) {
    const int count = split_delimited_line(header, candidate).size();
    if (count > best_count) {
      best_count = count;
      best = candidate;
    }
  }
  return best;
}

bool parse_number(const QString& text, double* value) {
  const QString token = text.trimmed();
  if (token.compare("nan", Qt::CaseInsensitive) == 0) {
    *value = std::numeric_limits<double>::quiet_NaN();
    return true;
  }
  if (token.compare("inf", Qt::CaseInsensitive) == 0 ||
      token.compare("+inf", Qt::CaseInsensitive) == 0 ||
      token.compare("infinity", Qt::CaseInsensitive) == 0) {
    *value = std::numeric_limits<double>::infinity();
    return true;
  }
  if (token.compare("-inf", Qt::CaseInsensitive) == 0 ||
      token.compare("-infinity", Qt::CaseInsensitive) == 0) {
    *value = -std::numeric_limits<double>::infinity();
    return true;
  }
  bool ok = false;
  *value = token.toDouble(&ok);
  return ok;
}

QString unit_from_header(const QString& header) {
  static const QRegularExpression unit_re(
      R"((?:\[([^\]]+)\]|\(([^\)]+)\))\s*$)");
  const auto match = unit_re.match(header.trimmed());
  return match.hasMatch()
             ? (match.captured(1).isEmpty() ? match.captured(2)
                                             : match.captured(1))
             : QString();
}

QString header_name_without_unit(const QString& header) {
  static const QRegularExpression unit_re(
      R"(\s*(?:\[[^\]]+\]|\([^\)]+\))\s*$)");
  QString name = header.trimmed();
  name.remove(unit_re);
  return name.trimmed();
}

QString sha256_file(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return QString();
  }
  QCryptographicHash hash(QCryptographicHash::Sha256);
  while (!file.atEnd()) {
    hash.addData(file.read(1024 * 1024));
  }
  return QString::fromLatin1(hash.result().toHex());
}

QString package_root_for(const QString& selected_path) {
  QFileInfo info(selected_path);
  QDir dir(info.isDir() ? info.absoluteFilePath() : info.absolutePath());
  if (dir.dirName().compare("results", Qt::CaseInsensitive) == 0 &&
      QFileInfo::exists(dir.absoluteFilePath("../snapshot-manifest.json"))) {
    dir.cdUp();
  }
  return dir.absolutePath();
}

bool is_exodus(const QString& path) {
  const QString suffix = QFileInfo(path).suffix().toLower();
  return suffix == "e" || suffix == "exo" || suffix == "exodus";
}

bool is_aux_times_csv(const QString& path) {
  static const QRegularExpression pattern(
      R"(_field_output_times_\d+\.csv$)",
      QRegularExpression::CaseInsensitiveOption);
  return pattern.match(QFileInfo(path).fileName()).hasMatch();
}

}  // namespace

QStringList CsvData::numeric_headers() const {
  QStringList result;
  for (int i = 0; i < headers.size() && i < numeric_columns.size(); ++i) {
    if (numeric_columns.at(i)) {
      result << headers.at(i);
    }
  }
  return result;
}

QVariantMap CsvData::table_snapshot() const {
  QVariantMap map;
  map.insert("source", path);
  map.insert("field", QFileInfo(path).fileName());
  map.insert("association", "CSV");
  map.insert("time", QString());
  map.insert("columns", QStringList{"Row"} + headers);
  QVariantList values;
  for (int index = 0; index < rows.size(); ++index) {
    QVariantList row{index};
    row.append(rows.at(index));
    values << QVariant(row);
  }
  map.insert("rows", values);
  map.insert("total_rows", rows.size());
  map.insert("errors", errors);
  map.insert("ignored_bad_rows", ignored_bad_rows);
  return map;
}

CsvData read_csv_data(const QString& path, bool ignore_bad_rows,
                      QChar forced_delimiter) {
  CsvData data;
  data.path = QFileInfo(path).absoluteFilePath();
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    data.errors << QString("Cannot open CSV: %1").arg(path);
    return data;
  }
  QByteArray bytes = file.readAll();
  if (bytes.startsWith("\xEF\xBB\xBF")) {
    bytes.remove(0, 3);
    data.encoding = "UTF-8 BOM";
  }
  const QString text = QString::fromUtf8(bytes);
  if (text.contains(QChar::ReplacementCharacter)) {
    data.errors << "CSV is not valid UTF-8.";
    return data;
  }
  QStringList lines = text.split(QRegularExpression("\\r?\\n"));
  while (!lines.isEmpty() && lines.last().trimmed().isEmpty()) {
    lines.removeLast();
  }
  if (lines.isEmpty()) {
    data.errors << "CSV is empty.";
    return data;
  }
  const QChar delimiter = forced_delimiter.isNull()
                              ? detect_delimiter(lines.first())
                              : forced_delimiter;
  data.delimiter = delimiter == '\t' ? "Tab" : QString(delimiter);
  data.headers = split_delimited_line(lines.takeFirst(), delimiter);
  for (QString& header : data.headers) {
    header = header.trimmed();
    data.units << unit_from_header(header);
  }
  data.numeric_columns.fill(false, data.headers.size());
  QList<QStringList> parsed_lines;
  QList<int> source_lines;
  for (int line_index = 0; line_index < lines.size(); ++line_index) {
    if (lines.at(line_index).trimmed().isEmpty()) {
      continue;
    }
    const QStringList fields = split_delimited_line(lines.at(line_index), delimiter);
    if (fields.size() != data.headers.size()) {
      const QString error = QString("Line %1 has %2 columns; expected %3.")
                                .arg(line_index + 2)
                                .arg(fields.size())
                                .arg(data.headers.size());
      if (!ignore_bad_rows) {
        data.errors << error;
      } else {
        ++data.ignored_bad_rows;
        data.ignored_row_errors << error;
      }
      continue;
    }
    parsed_lines << fields;
    source_lines << line_index + 2;
  }
  for (int column = 0; column < data.headers.size(); ++column) {
    for (const QStringList& fields : parsed_lines) {
      if (fields.at(column).trimmed().isEmpty()) continue;
      double ignored = 0.0;
      data.numeric_columns[column] = parse_number(fields.at(column), &ignored);
      break;
    }
  }
  for (int row_index = 0; row_index < parsed_lines.size(); ++row_index) {
    const QStringList& fields = parsed_lines.at(row_index);
    QVariantList row;
    bool bad = false;
    for (int column = 0; column < fields.size(); ++column) {
      double number = 0.0;
      if (data.numeric_columns.at(column)) {
        if (fields.at(column).trimmed().isEmpty()) {
          number = std::numeric_limits<double>::quiet_NaN();
        } else if (!parse_number(fields.at(column), &number)) {
          const QString error =
              QString("Line %1, column %2 is not numeric: %3")
                  .arg(source_lines.at(row_index))
                  .arg(column + 1)
                  .arg(fields.at(column));
          if (!ignore_bad_rows) {
            data.errors << error;
          } else {
            ++data.ignored_bad_rows;
            data.ignored_row_errors << error;
          }
          bad = true;
          break;
        }
        row << number;
      } else {
        row << fields.at(column);
      }
    }
    if (!bad) {
      data.rows << row;
    }
  }
  if (!data.errors.isEmpty() && !ignore_bad_rows) {
    data.rows.clear();
  }
  return data;
}

QVariantMap ResultPackage::to_params(const QString& import_mode) const {
  QVariantMap params;
  params.insert("role", "result_package");
  params.insert("package_root", root_path);
  params.insert("path", main_exodus);
  params.insert("job", job_id);
  params.insert("package_job_id", job_id);
  params.insert("case_name", case_name);
  params.insert("managed", managed);
  params.insert("import_mode", import_mode);
  params.insert("main_exodus", main_exodus);
  params.insert("main_csv", main_csv);
  params.insert("exodus_candidates", exodus_candidates);
  params.insert("csv_candidates", csv_candidates);
  params.insert("auxiliary_times_csv", auxiliary_times_csv);
  params.insert("input_files", input_files);
  params.insert("log_report_files", log_report_files);
  params.insert("warnings", warnings);
  params.insert("fingerprint", fingerprint);
  params.insert("source_state", source_state);
  return params;
}

ResultPackage inspect_result_package(const QString& selected_path) {
  ResultPackage package;
  package.root_path = package_root_for(selected_path);
  const QString manifest_path =
      QDir(package.root_path).absoluteFilePath("snapshot-manifest.json");
  QFile manifest_file(manifest_path);
  QStringList relative_files;
  QHash<QString, QVariantMap> manifest_entries;
  if (manifest_file.open(QIODevice::ReadOnly)) {
    const QByteArray bytes = manifest_file.readAll();
    QJsonParseError parse_error;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parse_error);
    if (parse_error.error == QJsonParseError::NoError && doc.isObject()) {
      package.managed = true;
      const QJsonObject object = doc.object();
      package.job_id = object.value("job_id").toString();
      for (const QJsonValue& value : object.value("files").toArray()) {
        const QVariantMap entry = value.toObject().toVariantMap();
        const QString relative = entry.value("path").toString();
        if (!relative.isEmpty()) {
          relative_files << relative;
          manifest_entries.insert(QDir::cleanPath(relative), entry);
        }
      }
      package.fingerprint = QString::fromLatin1(
          QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    } else {
      package.warnings << "snapshot-manifest.json is not valid JSON.";
    }
  }
  if (relative_files.isEmpty()) {
    QDirIterator iterator(package.root_path, QDir::Files,
                          QDirIterator::Subdirectories);
    const QDir root(package.root_path);
    while (iterator.hasNext()) {
      relative_files << root.relativeFilePath(iterator.next());
    }
    relative_files.sort();
    QByteArray metadata;
    for (const QString& relative : relative_files) {
      const QFileInfo info(root.absoluteFilePath(relative));
      metadata += relative.toUtf8() + '\0' + QByteArray::number(info.size()) +
                  '\0' +
                  QByteArray::number(info.lastModified().toMSecsSinceEpoch()) +
                  '\n';
    }
    package.fingerprint =
        "unmanaged:" +
        QString::fromLatin1(
            QCryptographicHash::hash(metadata, QCryptographicHash::Sha256)
                .toHex());
  }

  const QDir root(package.root_path);
  QByteArray source_metadata;
  QStringList ordinary_csv;
  for (const QString& relative_raw : relative_files) {
    const QString relative = QDir::cleanPath(relative_raw);
    const QString absolute = root.absoluteFilePath(relative);
    const QFileInfo info(absolute);
    source_metadata += relative.toUtf8() + '\0' +
                       QByteArray::number(info.exists() ? info.size() : -1) +
                       '\0' +
                       QByteArray::number(
                           info.exists()
                               ? info.lastModified().toMSecsSinceEpoch()
                               : -1) +
                       '\n';
    if (!info.exists()) {
      package.warnings << "Missing: " + relative;
      if (relative.startsWith("results/", Qt::CaseInsensitive) &&
          is_exodus(relative))
        package.exodus_candidates << absolute;
      continue;
    }
    const QVariantMap manifest_entry = manifest_entries.value(relative);
    if (!manifest_entry.isEmpty() &&
        manifest_entry.value("size").toLongLong() != info.size()) {
      package.warnings << "Size changed: " + relative;
    }
    if (relative.startsWith("results/", Qt::CaseInsensitive)) {
      if (is_exodus(relative)) {
        package.exodus_candidates << absolute;
      } else if (info.suffix().compare("csv", Qt::CaseInsensitive) == 0) {
        if (is_aux_times_csv(relative)) {
          package.auxiliary_times_csv << absolute;
        } else {
          ordinary_csv << absolute;
        }
      }
    } else if (relative.startsWith("input/", Qt::CaseInsensitive)) {
      package.input_files << absolute;
    } else if (relative.startsWith("logs/", Qt::CaseInsensitive) ||
               info.fileName() == "task.md" ||
               info.fileName() == "snapshot-manifest.json") {
      package.log_report_files << absolute;
    }
  }
  for (const QString& csv : ordinary_csv) {
    const CsvData parsed = read_csv_data(csv);
    int time_column = -1;
    for (int column = 0; column < parsed.headers.size(); ++column) {
      if (header_name_without_unit(parsed.headers.at(column))
              .compare("time", Qt::CaseInsensitive) == 0) {
        time_column = column;
        break;
      }
    }
    if (parsed.valid() && time_column >= 0 &&
        time_column < parsed.numeric_columns.size() &&
        parsed.numeric_columns.at(time_column) &&
        parsed.numeric_headers().size() >= 2) {
      package.csv_candidates << csv;
    }
  }
  if (package.exodus_candidates.size() == 1) {
    package.main_exodus = package.exodus_candidates.first();
    const QString base = QFileInfo(package.main_exodus).completeBaseName();
    for (const QString& csv : package.csv_candidates) {
      if (QFileInfo(csv).completeBaseName() == base) {
        package.main_csv = csv;
        break;
      }
    }
    package.case_name = base;
  }
  if (package.main_csv.isEmpty() && package.csv_candidates.size() == 1) {
    package.main_csv = package.csv_candidates.first();
  }
  if (package.case_name.isEmpty() && !package.main_csv.isEmpty()) {
    package.case_name = QFileInfo(package.main_csv).completeBaseName();
  }
  package.exodus_candidates.sort();
  package.csv_candidates.sort();
  package.auxiliary_times_csv.sort();
  package.input_files.sort();
  package.log_report_files.sort();
  package.source_state = QString::fromLatin1(
      QCryptographicHash::hash(source_metadata, QCryptographicHash::Sha256)
          .toHex());
  return package;
}

QStringList verify_result_package(const ResultPackage& package) {
  QStringList issues;
  const QString manifest_path =
      QDir(package.root_path).absoluteFilePath("snapshot-manifest.json");
  QFile file(manifest_path);
  if (!file.open(QIODevice::ReadOnly)) {
    issues << "No readable snapshot-manifest.json; integrity is unverified.";
    return issues;
  }
  QJsonParseError error;
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject()) {
    issues << "snapshot-manifest.json is invalid.";
    return issues;
  }
  const QDir root(package.root_path);
  for (const QJsonValue& value : doc.object().value("files").toArray()) {
    const QJsonObject entry = value.toObject();
    const QString relative = entry.value("path").toString();
    const QString absolute = root.absoluteFilePath(relative);
    if (!QFileInfo::exists(absolute)) {
      issues << "Missing: " + relative;
      continue;
    }
    const QString expected = entry.value("sha256").toString().toLower();
    if (!expected.isEmpty() && sha256_file(absolute) != expected) {
      issues << "SHA-256 mismatch: " + relative;
    }
  }
  return issues;
}

bool result_file_is_usable(const ResultPackage& package, const QString& path) {
  if (path.isEmpty() || !QFileInfo::exists(path)) return false;
  const QString relative =
      QDir::cleanPath(QDir(package.root_path).relativeFilePath(path));
  return !package.warnings.contains("Missing: " + relative) &&
         !package.warnings.contains("Size changed: " + relative);
}

QString result_file_sha256(const QString& path) { return sha256_file(path); }

}  // namespace gmp
