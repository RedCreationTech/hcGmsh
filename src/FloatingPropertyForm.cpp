#include "gmp/FloatingPropertyForm.h"

#include "gmp/L10n.h"
#include "gmp/PropertyEditor.h"

#include <QDialogButtonBox>
#include <QEvent>
#include <QGuiApplication>
#include <QLabel>
#include <QLayout>
#include <QMap>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
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
      {"Input Cases", "输入算例"},
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
    const QStringList& volume_groups,
    const QStringList& physics_action_options,
    const QStringList& load_type_options,
    const QStringList& interaction_type_options, QWidget* parent)
    : QDialog(parent), target_item_(target) {
  setObjectName("floatingPropertyForm");
  setModal(true);
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setMinimumSize(560, 420);

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
  // The modal editor works on a cloned tree, but must use the same active
  // application-profile choices as the main editor before building its form.
  editor_->set_physics_action_options(physics_action_options);
  editor_->set_load_type_options(load_type_options);
  editor_->set_interaction_type_options(interaction_type_options);
  editor_->set_boundary_groups(boundary_groups);
  editor_->set_volume_groups(volume_groups);
  editor_->set_item(buffer_item_);

  // 弹窗内废除内部滚动区（如 paramsTabScroll）：内容 widget 直接并入页面
  // 布局按自身高度全部展开，杜绝"外层+内层"双滚动条；唯一滚动由下方
  // 外层 QScrollArea 承载。共享编辑器（模块窗内）不受影响，仍保留其
  // 内部滚动层。
  for (auto* scroll : editor_->findChildren<QScrollArea*>()) {
    QWidget* content = scroll->takeWidget();
    QLayout* host_layout = scroll->parentWidget()
                               ? scroll->parentWidget()->layout()
                               : nullptr;
    if (!content || !host_layout) {
      continue;
    }
    // 手动换位：replaceWidget 对仍带父对象的 content 不可靠。
    int index = -1;
    for (int i = 0; i < host_layout->count(); ++i) {
      if (host_layout->itemAt(i)->widget() == scroll) {
        index = i;
        break;
      }
    }
    content->setParent(scroll->parentWidget());
    if (auto* box = qobject_cast<QBoxLayout*>(host_layout)) {
      if (index >= 0) {
        box->insertWidget(index, content);
      } else {
        box->addWidget(content);
      }
    } else {
      host_layout->addWidget(content);
    }
    // 立即删除（构造期内无重入风险）：deleteLater 在巡览/断言的同一事件
    //  pass 内尚未处理，残留的滚动区会被误判为双滚动条。
    delete scroll;
  }

  // 弹窗唯一的外层滚动承载：内容按当前 TAB 自然展开（wrap_content），
  // 高度不超上限时滚动条隐藏；内容超高时窗口按上限定高、由这里滚动。
  // 内部（如参数页的 paramsTabScroll）因此永远拿到足够高度，不再出现
  // 第二条滚动条。
  outer_scroll_ = new QScrollArea(this);
  outer_scroll_->setObjectName("floatingPropertyFormScroll");
  outer_scroll_->setWidgetResizable(true);
  outer_scroll_->setFrameShape(QFrame::NoFrame);
  outer_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  outer_scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scroll_host_ = new QWidget(outer_scroll_);
  auto* scroll_host_layout = new QVBoxLayout(scroll_host_);
  scroll_host_layout->setContentsMargins(0, 0, 0, 0);
  scroll_host_layout->addWidget(editor_, 1);
  outer_scroll_->setWidget(scroll_host_);
  scroll_host_->installEventFilter(this);
  layout->addWidget(outer_scroll_, 1);

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
  resize(remembered.isValid() ? remembered : QSize(760, 560));

  if (auto* tabs = editor_->findChild<QTabWidget*>("propertyEditorTabs")) {
    connect(tabs, &QTabWidget::currentChanged, this, [this]() {
      // 立即同步宿主最小尺寸（eventFilter 的 LayoutRequest 路径延迟一帧，
      // 断言/检查在切换后立刻发生时内部滚动区尚未被撑开）。
      sync_scroll_host_minimum();
      QTimer::singleShot(0, this,
                         [this]() { fit_to_current_tab(); });
    });
  }
}

QString FloatingPropertyForm::settings_key() const {
  QString kind = buffer_item_
                     ? buffer_item_->data(0, PropertyEditor::kKindRole).toString()
                     : QString("unknown");
  kind.replace('/', '_');
  return QString("ui/property_form/v2/%1/size").arg(kind);
}

void FloatingPropertyForm::set_display_unit_factors(
    const QMap<QString, double>& factors) {
  if (editor_) {
    editor_->set_display_unit_factors(factors);
  }
}

void FloatingPropertyForm::place_over_stage(QWidget* stage) {
  if (!stage) {
    return;
  }
  stage_ = stage;
  fit_to_current_tab();
  const QRect stage_rect(stage->mapToGlobal(QPoint(0, 0)), stage->size());
  QPoint candidate(stage_rect.center().x() - width() / 2,
                   stage_rect.center().y() - height() / 2);
  QRect target(candidate, size());
  QScreen* screen = stage->screen();
  const QRect safe = screen ? screen->availableGeometry().adjusted(
                                 12, 12, -12, -12)
                            : stage_rect.adjusted(12, 12, -12, -12);
  if (!safe.contains(target)) {
    candidate.setX(qBound(safe.left(), candidate.x(),
                          qMax(safe.left(), safe.right() - width() + 1)));
    candidate.setY(qBound(safe.top(), candidate.y(),
                          qMax(safe.top(), safe.bottom() - height() + 1)));
  }
  move(candidate);
}

QSize FloatingPropertyForm::content_aware_editor_hint() {
  // QTabWidget/QScrollArea 的 sizeHint 会低估当前页内容高度（实测参数页
  // 内容 580px 而 editor sizeHint 仅 414px），导致内部 paramsTabScroll
  // 被压出第二条滚动条。这里以编辑器 sizeHint 为基准，再按当前页内滚动区
  // 内容 widget 的真实 sizeHint 补足差额。
  if (auto* editor_layout = editor_->layout()) {
    editor_layout->activate();
  }
  QSize hint = editor_->sizeHint();
  if (auto* tabs = editor_->findChild<QTabWidget*>("propertyEditorTabs")) {
    if (QWidget* page = tabs->currentWidget()) {
      if (auto* page_layout = page->layout()) {
        page_layout->activate();
      }
      const int page_hint_h = page->sizeHint().height();
      int extra = 0;
      for (auto* scroll : page->findChildren<QScrollArea*>()) {
        if (!scroll->widget()) {
          continue;
        }
        if (auto* content_layout = scroll->widget()->layout()) {
          content_layout->activate();
        }
        extra = qMax(extra,
                     scroll->widget()->sizeHint().height() - page_hint_h);
      }
      if (extra > 0) {
        hint.rheight() += extra;
      }
    }
  }
  return hint;
}

QSize FloatingPropertyForm::preferred_size_for_current_tab() {
  if (!editor_ || !outer_scroll_) {
    return QSize(760, 560);
  }
  const QSize editor_hint = content_aware_editor_hint();

  const QMargins dialog_margins = layout()->contentsMargins();
  const int dialog_spacing = layout()->spacing();
  const QSize buttons_hint = buttons_ ? buttons_->sizeHint() : QSize();
  return QSize(qMax(760, editor_hint.width() + dialog_margins.left() +
                             dialog_margins.right()),
               qMax(560, editor_hint.height() + buttons_hint.height() +
                             dialog_spacing + dialog_margins.top() +
                             dialog_margins.bottom()));
}

void FloatingPropertyForm::sync_scroll_host_minimum() {
  if (!scroll_host_ || !editor_ || !outer_scroll_) {
    return;
  }
  // 宿主最小尺寸 = 编辑器按内容展开的真实高度；窗口被上限压住时
  // QScrollArea 据 minimumSize 出外层滚动条，内部 paramsTabScroll
  // 始终拿到全高，不会退化成第二条滚动条。
  const QSize hint = content_aware_editor_hint();
  // 最小尺寸设在编辑器自身：编辑器随宿主拉伸填满，内部滚动区拿到全高；
  // 仅设宿主最小尺寸时编辑器按 AlignTop 保持原高，内部仍会退化出滚动条。
  if (editor_->minimumSize() != hint) {
    editor_->setMinimumSize(hint);
  }
}

bool FloatingPropertyForm::eventFilter(QObject* watched, QEvent* event) {
  if (watched == scroll_host_ && event->type() == QEvent::LayoutRequest) {
    // 内容高度动态变化（高级参数展开/收起、校验行变化等）后，延迟一帧
    // 同步宿主最小尺寸，避免内部滚动区退化成第二条滚动条。
    QTimer::singleShot(0, this,
                       [this]() { sync_scroll_host_minimum(); });
  }
  return QDialog::eventFilter(watched, event);
}

void FloatingPropertyForm::fit_to_current_tab() {
  const QPoint center = frameGeometry().center();
  QScreen* screen = stage_ ? stage_->screen()
                           : QGuiApplication::screenAt(center);
  if (!screen) {
    screen = QGuiApplication::primaryScreen();
  }
  const QRect safe = screen ? screen->availableGeometry().adjusted(
                                 12, 12, -12, -12)
                            : QRect(QPoint(0, 0), preferred_size_for_current_tab());
  // 内容超高时窗口只给到屏幕可用高度的 70%，由外层滚动区滚动，
  // 而不是把弹窗撑到接近全屏。
  const int height_cap = qMax(420, safe.height() * 7 / 10);
  QSize target = preferred_size_for_current_tab();
  target.setWidth(qMin(target.width(), safe.width()));
  target.setHeight(qMin(target.height(), height_cap));
  resize(target.expandedTo(minimumSize().boundedTo(safe.size())));
  sync_scroll_host_minimum();

  if (isVisible()) {
    QPoint position(center.x() - width() / 2, center.y() - height() / 2);
    position.setX(qBound(safe.left(), position.x(),
                         qMax(safe.left(), safe.right() - width() + 1)));
    position.setY(qBound(safe.top(), position.y(),
                         qMax(safe.top(), safe.bottom() - height() + 1)));
    move(position);
  }
}

void FloatingPropertyForm::set_commit_callback(
    std::function<bool(QTreeWidgetItem*, const QString&,
                       const QVariantMap&)> callback) {
  commit_callback_ = std::move(callback);
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
  if (commit_callback_ &&
      !commit_callback_(target_item_, buffer_item_->text(0),
                        buffer_item_->data(0, PropertyEditor::kParamsRole)
                            .toMap())) {
    return;
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
