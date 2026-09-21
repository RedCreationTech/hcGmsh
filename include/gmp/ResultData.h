#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace gmp {

struct CsvData {
  QString path;
  QString encoding = "UTF-8";
  QString delimiter;
  QStringList headers;
  QStringList units;
  QList<bool> numeric_columns;
  QList<QVariantList> rows;
  QStringList errors;
  QStringList ignored_row_errors;
  int ignored_bad_rows = 0;

  bool valid() const { return errors.isEmpty() && !headers.isEmpty(); }
  QStringList numeric_headers() const;
  QVariantMap table_snapshot() const;
};

CsvData read_csv_data(const QString& path, bool ignore_bad_rows = false,
                      QChar forced_delimiter = QChar());

struct ResultPackage {
  QString root_path;
  QString job_id;
  QString case_name;
  QString main_exodus;
  QString main_csv;
  QStringList exodus_candidates;
  QStringList csv_candidates;
  QStringList auxiliary_times_csv;
  QStringList input_files;
  QStringList log_report_files;
  QStringList warnings;
  QString fingerprint;
  QString source_state;
  bool managed = false;

  bool has_unique_main_results() const {
    return exodus_candidates.size() == 1 && csv_candidates.size() == 1;
  }
  QVariantMap to_params(const QString& import_mode = "reference") const;
};

ResultPackage inspect_result_package(const QString& selected_path);
QStringList verify_result_package(const ResultPackage& package);
bool result_file_is_usable(const ResultPackage& package, const QString& path);
QString result_file_sha256(const QString& path);

}  // namespace gmp
