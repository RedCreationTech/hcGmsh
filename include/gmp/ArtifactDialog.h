#pragma once

#include <QDialog>
#include <QJsonArray>
#include <QString>

class QLabel;
class QProgressBar;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace gmp {

class SimClient;

// TASK-E2E-12：远程制品对话框。列出 LIMS Facade /api/sim/bundles 的制品包，
// 选择 kind=exodus 的文件下载到本地缓存后交给既有 Exodus 可视化管线打开。
class ArtifactDialog : public QDialog {
  Q_OBJECT
 public:
  explicit ArtifactDialog(const QString& base_url,
                          QWidget* parent = nullptr);

 signals:
  // 下载/缓存就绪的本地 Exodus 路径（由调用方接入 exodus_ready 管线）
  void exodus_artifact_ready(const QString& local_path);

 private slots:
  void on_refresh();
  void on_open_selected();
  void on_bundles(bool ok, const QJsonArray& bundles, const QString& error);
  void on_download_progress(qint64 received, qint64 total);
  void on_download_finished(bool ok, const QString& dest_path,
                            const QString& error);

 private:
  QString cache_root() const;
  void set_busy(bool busy, const QString& message);

  SimClient* client_ = nullptr;
  QTreeWidget* tree_ = nullptr;
  QProgressBar* progress_ = nullptr;
  QLabel* status_ = nullptr;
  QPushButton* refresh_btn_ = nullptr;
  QPushButton* open_btn_ = nullptr;
  QString pending_dest_;
};

}  // namespace gmp
