#include "gmp/ComboPopupFix.h"

#include <QEvent>
#include <QFontMetrics>
#include <QListView>
#include <QModelIndex>
#include <QModelIndexList>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <algorithm>

#include <QGuiApplication>
#include <QComboBox>
#include <QPointer>
#include <QSize>
#include <QTimer>
#include <QWidget>

namespace gmp {

namespace {

// 可重入保护：进入过滤器处理段时置位，退出时复位
struct ReentryGuard {
  bool& flag;
  explicit ReentryGuard(bool& f) : flag(f) { flag = true; }
  ~ReentryGuard() { flag = false; }
};

class ComboPopupFixer : public QObject {
 public:
  explicit ComboPopupFixer(QComboBox* combo) : QObject(combo), combo_(combo) {
    if (combo_) {
      // 安装前 setView() 已被调用，此处直接缓存现有视图指针。
      // 注意：绝不能在事件过滤器中调用 combo->view()，
      // 它会惰性创建弹窗容器，创建过程投递的事件会重入本过滤器，
      // 在 ComboBox 析构/容器创建路径上形成无限递归（栈溢出崩溃）。
      view_ = qobject_cast<QAbstractItemView*>(combo_->view());
      combo_->installEventFilter(this);
    }
  }

  void ensure_filter() {
    if (!view_) {
      return;
    }
    if (auto* popup = view_->window()) {
      popup->installEventFilter(this);
    } else {
      view_->installEventFilter(this);
    }
  }

 protected:
  bool eventFilter(QObject* obj, QEvent* event) override {
    if (!combo_ || in_filter_) {
      return QObject::eventFilter(obj, event);
    }

    const QEvent::Type t = event->type();
    // 析构/删除路径上的事件直接放行，不做任何 view 访问
    if (t == QEvent::ChildRemoved || t == QEvent::DeferredDelete ||
        t == QEvent::Destroy) {
      return QObject::eventFilter(obj, event);
    }
    if (!view_) {
      return QObject::eventFilter(obj, event);
    }

    if (t == QEvent::Show || t == QEvent::ShowToParent ||
        t == QEvent::MouseButtonPress || t == QEvent::MouseButtonRelease ||
        t == QEvent::Move || t == QEvent::Resize || t == QEvent::FocusIn) {
      ReentryGuard guard(in_filter_);
      QWidget* view = view_;
      QWidget* popup = view ? view->window() : nullptr;
      if (obj == combo_ || obj == view || obj == popup) {
        QTimer::singleShot(0, this, [this]() { apply_popup_geometry(0); });
      }
    }
    return QObject::eventFilter(obj, event);
  }

 private:
  void apply_popup_geometry(int attempt) {
    if (!combo_ || !view_) {
      return;
    }
    QWidget* popup = view_->window();
    if (!popup) {
      return;
    }
    if (combo_->height() <= 0 || combo_->width() <= 0) {
      if (attempt < 5) {
        QTimer::singleShot(10, this, [this, attempt]() {
          apply_popup_geometry(attempt + 1);
        });
      }
      return;
    }

    int width = combo_->width();
    if (auto* view = view_.data()) {
      const int margin = 24;
      int hint_width = view->sizeHintForColumn(0);
      if (hint_width < 0) {
        hint_width = 0;
      }
      int max_row_width = 0;
      const QFontMetrics fm(view->font());
      if (auto* model = view->model()) {
        for (int i = 0; i < model->rowCount(); ++i) {
          const QModelIndex idx = model->index(i, 0);
          const QString text = model->data(idx, Qt::DisplayRole).toString();
          const int row_w = fm.horizontalAdvance(text) + margin;
          if (row_w > max_row_width) {
            max_row_width = row_w;
          }
        }
      }
      width = std::max(width, std::max(hint_width, max_row_width));
      width = std::max(width, combo_->minimumWidth());
      width = std::max(width, 160);
      view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
      view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
      view->setTextElideMode(Qt::ElideNone);
    }

    const QPoint origin = combo_->mapToGlobal(QPoint(0, combo_->height() + 1));
    const QSize hint = popup->sizeHint();
    int popup_h = hint.height();
    if (popup_h <= 0) {
      popup_h = popup->height();
    }

    int popup_row_h = 16;
    int popup_rows = 1;
    if (auto* view = view_.data()) {
      const int row_hint = view->sizeHintForRow(0);
      if (row_hint > 0) {
        popup_row_h = row_hint;
      }
      if (auto* model = view->model()) {
        popup_rows = std::max(1, model->rowCount());
      }
    }
    const int min_popup_h = popup_row_h * qMin(popup_rows, 6) + 6;
    if (popup_h < min_popup_h) {
      popup_h = min_popup_h;
    }

    int popup_w = hint.width();
    if (popup_w < width) {
      popup_w = width;
    }

    if (const QScreen* screen = QGuiApplication::screenAt(origin)) {
      const QRect geo = screen->availableGeometry();
      popup_w = qMin(popup_w, std::max(180, geo.width() - 12));
      width = qMin(width, popup_w);

      int x = origin.x();
      int y = origin.y();
      if (x + popup_w > geo.right()) {
        x = std::max(geo.left(), geo.right() - popup_w + 1);
      }

      const int max_popup_h = std::max(90, geo.bottom() - y - 1);
      if (popup_h > max_popup_h) {
        popup_h = max_popup_h;
      }

      const QRect target_geo(x, y, popup_w, popup_h);
      if (popup->geometry() != target_geo) {
        popup->setGeometry(target_geo);
      }
      popup->setMinimumSize(popup_w, popup_h);
      popup->setMaximumSize(popup_w, popup_h);
      if (auto* view = view_.data()) {
        view->setMinimumWidth(popup_w);
      }
    } else {
      const QRect target_geo(origin.x(), origin.y(), popup_w, popup_h);
      if (popup->geometry() != target_geo) {
        popup->setGeometry(target_geo);
      }
      popup->setMinimumSize(popup_w, popup_h);
      popup->setMaximumSize(popup_w, popup_h);
      if (auto* view = view_.data()) {
        view->setMinimumWidth(popup_w);
      }
    }
  }

  QPointer<QComboBox> combo_;
  QPointer<QAbstractItemView> view_;
  bool in_filter_ = false;
};

}  // namespace

void install_combo_popup_fix(QComboBox* combo) {
  if (!combo || combo->property("_gmp_combo_popup_fix").toBool()) {
    return;
  }
  combo->setProperty("_gmp_combo_popup_fix", true);
  auto* view = new QListView(combo);
  view->setUniformItemSizes(true);
  combo->setView(view);
  auto* popup = new ComboPopupFixer(combo);
  popup->ensure_filter();
}

}  // namespace gmp
