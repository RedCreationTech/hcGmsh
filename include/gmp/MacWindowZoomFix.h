#pragma once

// macOS 原生标题栏双击修复：AppKit 层 NSWindow 消费标题栏双击（Qt
// eventFilter 收不到真实双击），原生 zoom 状态机对手册工作窗
// （helpWorkspaceWindow）异常（标题栏消失且无法恢复，用户两次确认）。
// 这里运行时 swizzle NSWindow 的 -zoom:：目标是手册窗时分流到 Qt
// showMaximized/showNormal 往返 toggle，其余窗口走原实现。
// 非 Apple 平台为内联空实现，调用点无需宏保护。

#ifdef __APPLE__

namespace gmp {

// swizzle NSWindow -zoom:（全局一次，幂等）。须在 QApplication 构造后、
// 手册窗首次展示前调用（main.cpp）。
void install_mac_window_zoom_fix();

// 验证入口：找到 objectName=="helpWorkspaceWindow" 的顶层 QWidget，
// 取其 NSWindow 并直接发 -zoom:（AppKit 真实双击的同一入口，经 swizzle
// 分流）。无手册窗/无原生窗口时返回 false（调用方降级为跳过）。
bool trigger_help_window_native_zoom();

}  // namespace gmp

#else

namespace gmp {

inline void install_mac_window_zoom_fix() {}
inline bool trigger_help_window_native_zoom() { return false; }

}  // namespace gmp

#endif
