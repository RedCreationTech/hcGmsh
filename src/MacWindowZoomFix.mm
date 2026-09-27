// macOS：swizzle NSWindow -zoom: 的统一挂点。历史：手册工作窗
// （helpWorkspaceWindow）曾是 QDockWidget(Qt::Window) 改造品，原生 zoom
// 状态机异常（标题栏消失无法恢复），此处把它的 -zoom: 分流到 Qt
// showMaximized/showNormal。2026-09-27 手册窗重构为普通 Qt::Window 顶层
// QWidget 后原生 zoom 正常，分流判断已移除；swizzle 框架保留（幂等安装、
// 原实现保存），gmp::trigger_help_window_native_zoom 仍可从真实 AppKit
// 入口（-zoom:）触发手册窗 zoom 供巡览合同断言。
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

// NSWindow -zoom: 的替换实现（方法签名 (void)(id, SEL, id)）。
// 手册窗重构为普通顶层 QWidget 后无分流需求，直接走原实现；框架保留
// 作为全局 zoom 观测/未来分流的统一挂点。
void gmp_swizzled_zoom(id self, SEL cmd, id sender) {
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
  qInfo("[maczoom] NSWindow -zoom: swizzled (passthrough)");
}

bool trigger_help_window_native_zoom() {
  NSWindow* window = resolve_help_nswindow();
  if (!window) {
    return false;
  }
  // 真实 AppKit 入口：标题栏双击与绿色 zoom 按钮最终都走这条消息。
  // 手册窗是普通 Qt::Window 顶层 QWidget，原生 zoom 走 AppKit/Qt 正常
  // 路径（isZoomed 与 Qt isMaximized 往返一致）。
  [window zoom:nil];
  return true;
}

}  // namespace gmp
