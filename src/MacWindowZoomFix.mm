// macOS：swizzle NSWindow -zoom:，把手册工作窗（helpWorkspaceWindow）的
// 原生 zoom（标题栏双击在 AppKit 层的真实入口）分流到 Qt
// showMaximized/showNormal。其余窗口调用原实现。
// 见 include/gmp/MacWindowZoomFix.h 头文件注释。

#include "gmp/MacWindowZoomFix.h"

#import <AppKit/AppKit.h>
#include <objc/runtime.h>

#include <QApplication>
#include <QDebug>
#include <QWidget>

namespace {

QWidget* find_help_widget() {
  const QWidgetList tops = QApplication::topLevelWidgets();
  for (QWidget* widget : tops) {
    if (widget->objectName() == QLatin1String("helpWorkspaceWindow")) {
      return widget;
    }
  }
  return nullptr;
}

// 手册窗的 NSWindow。winId() 会强制创建原生句柄（即使窗口尚未展示），
// Qt6 cocoa 的 WId 即 NSWindow*；用 isKindOfClass 校验并对 NSView 兜底，
// 兼容 WId 语义在不同 Qt 版本间的差异。不 retain：由 Qt 侧拥有生命周期。
NSWindow* resolve_help_nswindow() {
  QWidget* widget = find_help_widget();
  if (!widget) {
    return nil;
  }
  const WId wid = widget->winId();
  if (!wid) {
    return nil;
  }
  id object = reinterpret_cast<id>(wid);
  if ([object isKindOfClass:[NSWindow class]]) {
    return static_cast<NSWindow*>(object);
  }
  if ([object isKindOfClass:[NSView class]]) {
    return [static_cast<NSView*>(object) window];
  }
  return nil;
}

IMP g_original_zoom = nullptr;

// 重入护栏：分流分支里调 Qt toggle 会重入 -zoom:。正常情况下重入调用
// 因 Qt/AppKit 两态错开而命中"放行"分支，此 flag 只是兜底，防止状态
// 意外一致时无限递归。主线程专属，普通 bool 即可。
bool g_inside_qt_toggle = false;
struct QtToggleGuard {
  QtToggleGuard() { g_inside_qt_toggle = true; }
  ~QtToggleGuard() { g_inside_qt_toggle = false; }
};

// NSWindow -zoom: 的替换实现（方法签名 (void)(id, SEL, id)）。
// 每次调用实时解析目标窗口（顶层窗数量极小，开销可忽略），避免缓存
// 悬垂指针。
//
// 分流判据 = Qt 状态与 AppKit isZoomed 是否一致（实测 Qt 6.11
// setWindowStates 先更新 QWindow 状态再调平台层，QCocoaWindow 的
// 注释"状态尚未更新"已过时）：
//   Qt=0 AppKit=0  → AppKit 发起 maximize（真实双击/green 按钮）→ 分流
//   Qt=1 AppKit=1  → AppKit 发起 restore → 分流
//   Qt=1 AppKit=0  → Qt showMaximized 自己发起的 zoom: → 放行原实现
//   Qt=0 AppKit=1  → Qt showNormal 自己发起的 zoom: → 放行原实现
// 分流走 Qt toggle 后，Qt 会先置自身状态再重入 zoom:（两态错开），
// 重入经 guard 命中"放行"分支，最终由 AppKit 原生 zoom 在 Qt 状态机
// 内完成——与 Qt 自用的稳定路径完全一致；而 AppKit 直接发起的原生
// zoom（用户双击的真实路径）正是状态机错位、把窗口改写成异常尺寸并
// 丢标题栏（用户两次确认）的路径，被 swizzle 接管。
void gmp_swizzled_zoom(id self, SEL cmd, id sender) {
  NSWindow* help_window = resolve_help_nswindow();
  if (help_window && (NSWindow*)self == help_window &&
      !g_inside_qt_toggle) {
    if (QWidget* widget = find_help_widget()) {
      const bool qt_maximized = widget->isMaximized();
      const bool appkit_zoomed =
          [static_cast<NSWindow*>(self) isZoomed];
      if (qt_maximized == appkit_zoomed) {
        qInfo("[maczoom] NSWindow -zoom: routed to Qt %s for "
              "helpWorkspaceWindow (qt=%d zoomed=%d)",
              qt_maximized ? "showNormal" : "showMaximized",
              int(qt_maximized), int(appkit_zoomed));
        const QtToggleGuard guard;
        if (qt_maximized) {
          widget->showNormal();
        } else {
          widget->showMaximized();
        }
        return;
      }
    }
  }
  if (g_original_zoom) {
    reinterpret_cast<void (*)(id, SEL, id)>(g_original_zoom)(self, cmd,
                                                             sender);
  }
}

}  // namespace

namespace gmp {

void install_mac_window_zoom_fix() {
  static bool installed = false;
  if (installed) {
    return;
  }
  installed = true;
  Method method =
      class_getInstanceMethod([NSWindow class], @selector(zoom:));
  if (!method) {
    qWarning("[maczoom] NSWindow -zoom: not found; swizzle skipped");
    return;
  }
  g_original_zoom = method_getImplementation(method);
  method_setImplementation(method,
                           reinterpret_cast<IMP>(gmp_swizzled_zoom));
  qInfo("[maczoom] NSWindow -zoom: swizzled for helpWorkspaceWindow");
}

bool trigger_help_window_native_zoom() {
  NSWindow* window = resolve_help_nswindow();
  if (!window) {
    return false;
  }
  // 真实 AppKit 入口：标题栏双击与绿色 zoom 按钮最终都走这条消息，
  // 因此命中 swizzle 分流（修复后的行为），未修复时则复现原生 bug。
  [window zoom:nil];
  return true;
}

}  // namespace gmp
