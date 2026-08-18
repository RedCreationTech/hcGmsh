#include "gmp/ArtifactDialog.h"

#include <QBrush>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "gmp/MooseSnapshot.h"
#include "gmp/SimClient.h"

namespace gmp {

namespace {

constexpr int kRoleJobId = Qt::UserRole;
constexpr int kRoleFilePath = Qt::UserRole + 1;
constexpr int kRoleSha256 = Qt::UserRole + 2;

QString human_size(qint64 bytes) {
  if (bytes < 0) {
    return QStringLiteral("-");
  }
  if (bytes >= (1 << 20)) {
    return QStringLiteral("%1 MB").arg(bytes / 1048576.0, 0, 'f', 1);
  }
  if (bytes >= (1 << 10)) {
    return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
  }
  return QStringLiteral("%1 B").arg(bytes);
}

}  // namespace

ArtifactDialog::ArtifactDialog(const QString& base_url, QWidget* parent)
    : QDialog(parent) {
  setWindowTitle("Remote Artifacts");
  resize(640, 420);

  client_ = new SimClient(this);
  client_->set_base_url(base_url);

  auto* layout = new QVBoxLayout(this);
  status_ = new QLabel("Loading bundle list...");
  layout->addWidget(status_);

  tree_ = new QTreeWidget();
  tree_->setColumnCount(2);
  tree_->setHeaderLabels({"Name", "Size"});
  tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  tree_->setSelectionMode(QAbstractItemView::SingleSelection);
  layout->addWidget(tree_, 1);

  progress_ = new QProgressBar();
  progress_->setRange(0, 100);
  progress_->setValue(0);
  progress_->setVisible(false);
  layout->addWidget(progress_);

  auto* buttons = new QHBoxLayout();
  refresh_btn_ = new QPushButton("Refresh");
  open_btn_ = new QPushButton("Download && Open");
  open_btn_->setEnabled(false);
  auto* close_btn = new QPushButton("Close");
  buttons->addWidget(refresh_btn_);
  buttons->addStretch(1);
  buttons->addWidget(open_btn_);
  buttons->addWidget(close_btn);
  layout->addLayout(buttons);

  connect(refresh_btn_, &QPushButton::clicked, this,
          &ArtifactDialog::on_refresh);
  connect(open_btn_, &QPushButton::clicked, this,
          &ArtifactDialog::on_open_selected);
  connect(close_btn, &QPushButton::clicked, this, &QDialog::reject);
  connect(tree_, &QTreeWidget::itemSelectionChanged, this, [this]() {
    bool selectable = false;
    const auto items = tree_->selectedItems();
    if (!items.isEmpty()) {
      selectable = !items.first()->data(0, kRoleFilePath).toString().isEmpty();
    }
    open_btn_->setEnabled(selectable);
  });
  connect(tree_, &QTreeWidget::itemDoubleClicked, this,
          [this](QTreeWidgetItem* item, int) {
            if (item &&
                !item->data(0, kRoleFilePath).toString().isEmpty()) {
              on_open_selected();
            }
          });

  connect(client_, &SimClient::bundles_fetched, this,
          &ArtifactDialog::on_bundles);
  connect(client_, &SimClient::download_progress, this,
          &ArtifactDialog::on_download_progress);
  connect(client_, &SimClient::download_finished, this,
          &ArtifactDialog::on_download_finished);

  on_refresh();
}

QString ArtifactDialog::cache_root() const {
  return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
         "/artifacts";
}

void ArtifactDialog::set_busy(bool busy, const QString& message) {
  refresh_btn_->setEnabled(!busy);
  open_btn_->setEnabled(!busy && open_btn_->isEnabled());
  progress_->setVisible(busy);
  if (busy) {
    progress_->setRange(0, 0);  // 等待服务器响应期间不确定进度
  }
  status_->setText(message);
}

void ArtifactDialog::on_refresh() {
  tree_->clear();
  status_->setText("Loading bundle list...");
  client_->fetch_bundles();
}

void ArtifactDialog::on_bundles(bool ok, const QJsonArray& bundles,
                                const QString& error) {
  if (!ok) {
    status_->setText("Failed to load bundles: " + error);
    return;
  }
  tree_->clear();
  int valid_count = 0;
  for (const auto& v : bundles) {
    const QJsonObject b = v.toObject();
    const QString job_id = b.value("job_id").toString();
    const bool valid = b.value("valid").toBool();
    const QString case_name = b.value("case_name").toString();
    const QString state = b.value("state").toString();

    QString label = job_id;
    if (!case_name.isEmpty()) {
      label += "  (" + case_name + ")";
    }
    auto* item = new QTreeWidgetItem(tree_, {label, ""});
    item->setData(0, kRoleJobId, job_id);

    if (!valid) {
      // invalid 包置灰并显示原因（CONTRACT-ART 五项校验失败详情）
      QStringList reasons;
      const QJsonArray arr = b.value("invalid_reasons").toArray();
      for (const auto& r : arr) {
        reasons << r.toString();
      }
      item->setText(0, label + "  [invalid]");
      item->setToolTip(0, reasons.join("\n"));
      item->setForeground(0, QBrush(Qt::gray));
      item->setForeground(1, QBrush(Qt::gray));
      item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
      auto* reason_item =
          new QTreeWidgetItem(item, {reasons.join("; "), ""});
      reason_item->setFlags(Qt::NoItemFlags);
      reason_item->setForeground(0, QBrush(Qt::gray));
      continue;
    }
    ++valid_count;

    bool has_exodus = false;
    const QJsonArray files = b.value("files").toArray();
    for (const auto& fv : files) {
      const QJsonObject f = fv.toObject();
      if (f.value("kind").toString() != "exodus") {
        continue;
      }
      has_exodus = true;
      const QString path = f.value("path").toString();
      auto* child = new QTreeWidgetItem(
          item, {f.value("name").toString(path),
                 human_size(f.value("size").toInteger(-1))});
      child->setData(0, kRoleJobId, job_id);
      child->setData(0, kRoleFilePath, path);
      child->setData(0, kRoleSha256, f.value("sha256").toString());
      child->setToolTip(0, path);
    }
    if (!has_exodus) {
      auto* none = new QTreeWidgetItem(item, {"(no exodus artifact)", ""});
      none->setFlags(Qt::NoItemFlags);
      none->setForeground(0, QBrush(Qt::gray));
    }
  }
  status_->setText(QString("Bundles: %1 (valid: %2). Select an exodus file.")
                       .arg(bundles.size())
                       .arg(valid_count));
}

void ArtifactDialog::on_open_selected() {
  const auto items = tree_->selectedItems();
  if (items.isEmpty()) {
    return;
  }
  QTreeWidgetItem* item = items.first();
  const QString job_id = item->data(0, kRoleJobId).toString();
  const QString path = item->data(0, kRoleFilePath).toString();
  const QString sha = item->data(0, kRoleSha256).toString();
  if (job_id.isEmpty() || path.isEmpty()) {
    return;
  }

  const QString dest = cache_root() + "/" + job_id + "/" + path;
  // 已缓存且 SHA-256 与 manifest 一致：不重复下载
  if (QFileInfo::exists(dest)) {
    bool hash_ok = false;
    const QString actual = sha256_file_hex(dest, &hash_ok);
    if (hash_ok && actual == sha) {
      status_->setText("Using cached file: " + dest);
      emit exodus_artifact_ready(dest);
      accept();
      return;
    }
  }

  pending_dest_ = dest;
  set_busy(true, "Downloading " + path + " ...");
  client_->download_bundle_file(job_id, path, sha, dest);
}

void ArtifactDialog::on_download_progress(qint64 received, qint64 total) {
  if (total > 0) {
    progress_->setRange(0, 100);
    progress_->setValue(static_cast<int>(received * 100 / total));
    status_->setText(QString("Downloading... %1 / %2")
                         .arg(human_size(received), human_size(total)));
  } else {
    status_->setText(QString("Downloading... %1").arg(human_size(received)));
  }
}

void ArtifactDialog::on_download_finished(bool ok, const QString& dest_path,
                                          const QString& error) {
  set_busy(false, "");
  progress_->setVisible(false);
  if (!ok) {
    status_->setText("Download failed: " + error);
    return;
  }
  status_->setText("Ready: " + dest_path);
  emit exodus_artifact_ready(dest_path);
  accept();
}

}  // namespace gmp
