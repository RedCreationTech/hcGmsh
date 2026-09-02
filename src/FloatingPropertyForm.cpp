#include "gmp/FloatingPropertyForm.h"

#include "gmp/L10n.h"
#include "gmp/PropertyEditor.h"

#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QMap>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace gmp {
namespace {

QString localized_kind(const QString& kind) {
  if (l10n::current_language() != l10n::Language::Chinese) {
    QString singular = kind;
    if (singular.endsWith('s')) {
      singular.chop(1);
    }
    return singular;
  }
  const QMap<QString, QString> names = {
      {"Materials", "材料"},       {"Sections", "截面"},
      {"Steps", "分析步"},          {"BC", "边界条件"},
      {"Loads", "载荷"},              {"Interactions", "相互作用"},
      {"Functions", "函数"},          {"Variables", "变量"},
      {"Outputs", "输出"},            {"Features", "特征"},
      {"Datums", "基准"},             {"Assembly", "装配"},
      {"Constraints", "约束"},        {"Selections", "选择集"},
  };
  return names.value(kind, kind);
}

QList<int> item_path(QTreeWidgetItem* item) {
  QList<int> path;
  for (auto* cursor = item; cursor; cursor = cursor->parent()) {
    path.prepend(cursor->parent() ? cursor->parent()->indexOfChild(cursor)
                                  : cursor->treeWidget()->indexOfTopLevelItem(cursor));
  }
  return path;
}

QTreeWidgetItem* resolve_path(QTreeWidget* tree, const QList<int>& path) {
  if (!tree || path.isEmpty()) {
    return nullptr;
  }
  QTreeWidgetItem* item = tree->topLevelItem(path.first());
  for (int i = 1; item && i < path.size(); ++i) {
    item = item->child(path.at(i));
  }
  return item;
}

}  // namespace

FloatingPropertyForm::FloatingPropertyForm(
    QTreeWidgetItem* target, const QStringList& boundary_groups,
    const QStringList& volume_groups, QWidget* parent)
    : QDialog(parent), target_item_(target) {
  setObjectName("floatingPropertyForm");
  setModal(true);
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setMinimumSize(620, 560);

  const QString kind = target
                           ? target->data(0, PropertyEditor::kKindRole).toString()
                           : QString();
  const QString action = l10n::current_language() == l10n::Language::Chinese
                             ? "编辑"
                             : "Edit";
  setWindowTitle(QString("%1%2 — %3")
                     .arg(action, localized_kind(kind),
                          target ? target->text(0) : QString()));

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  // Keep a complete hidden clone so type templates and cross-node validation
  // see the same model context while all writes remain isolated.
  buffer_tree_ = new QTreeWidget(this);
  buffer_tree_->hide();
  if (target && target->treeWidget()) {
    const QList<int> path = item_path(target);
    for (int i = 0; i < target->treeWidget()->topLevelItemCount(); ++i) {
      buffer_tree_->addTopLevelItem(
          target->treeWidget()->topLevelItem(i)->clone());
    }
    buffer_item_ = resolve_path(buffer_tree_, path);
  }

  editor_ = new PropertyEditor(this);
  editor_->setObjectName("floatingPropertyEditor");
  editor_->set_boundary_groups(boundary_groups);
  editor_->set_volume_groups(volume_groups);
  editor_->set_item(buffer_item_);
  layout->addWidget(editor_, 1);

  buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok |
                                      QDialogButtonBox::Cancel,
                                  Qt::Horizontal, this);
  buttons_->button(QDialogButtonBox::Ok)->setObjectName("propertyFormOk");
  buttons_->button(QDialogButtonBox::Cancel)->setObjectName(
      "propertyFormCancel");
  if (l10n::current_language() == l10n::Language::Chinese) {
    buttons_->button(QDialogButtonBox::Ok)->setText("确定");
    buttons_->button(QDialogButtonBox::Cancel)->setText("取消");
  }
  layout->addWidget(buttons_);
  connect(buttons_, &QDialogButtonBox::accepted, this,
          &FloatingPropertyForm::commit_if_valid);
  connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);

  QSettings settings("gmp-ise", "gmp_ise");
  const QSize remembered = settings.value(settings_key()).toSize();
  resize(remembered.isValid() ? remembered : QSize(760, 700));
}

QString FloatingPropertyForm::settings_key() const {
  QString kind = buffer_item_
                     ? buffer_item_->data(0, PropertyEditor::kKindRole).toString()
                     : QString("unknown");
  kind.replace('/', '_');
  return QString("ui/property_form/v2/%1/size").arg(kind);
}

void FloatingPropertyForm::place_over_stage(QWidget* stage) {
  if (!stage) {
    return;
  }
  const QRect stage_rect(stage->mapToGlobal(QPoint(0, 0)), stage->size());
  QSize fitted = size().boundedTo(
      QSize(qMax(420, stage_rect.width() - 24),
            qMax(360, stage_rect.height() - 24)));
  resize(fitted);
  QPoint candidate(stage_rect.left() + qMax(12, stage_rect.width() / 10),
                   stage_rect.top() + (stage_rect.height() - height()) / 2);
  QRect target(candidate, size());
  const QRect safe = stage_rect.adjusted(12, 12, -12, -12);
  if (!safe.contains(target)) {
    candidate.setX(qBound(safe.left(), candidate.x(),
                          qMax(safe.left(), safe.right() - width() + 1)));
    candidate.setY(qBound(safe.top(), candidate.y(),
                          qMax(safe.top(), safe.bottom() - height() + 1)));
  }
  move(candidate);
}

void FloatingPropertyForm::commit_if_valid() {
  QStringList issues;
  if (!editor_ || !editor_->validate_current(&issues)) {
    return;
  }
  if (!target_item_ || !buffer_item_) {
    reject();
    return;
  }
  QTreeWidget* tree = target_item_->treeWidget();
  {
    const QSignalBlocker blocker(tree);
    target_item_->setText(0, buffer_item_->text(0));
    target_item_->setData(0, PropertyEditor::kParamsRole,
                          buffer_item_->data(0, PropertyEditor::kParamsRole));
  }
  emit committed(target_item_);
  accept();
}

void FloatingPropertyForm::done(int result) {
  QSettings settings("gmp-ise", "gmp_ise");
  settings.setValue(settings_key(), size());
  QDialog::done(result);
}

}  // namespace gmp
