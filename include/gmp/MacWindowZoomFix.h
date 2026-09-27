#pragma once

// macOS 原生标题栏双击验证挂点：运行时 swizzle NSWindow 的 -zoom:。
// 历史：手册工作窗（helpWorkspaceWindow）曾是 QDockWidget(Qt::Window)
// 改造品，原生 zoom 状态机异常（标题栏消失且无法恢复，用户多次确认），
// swizzle 曾把它的 -zoom: 分流到 Qt showMaximized/showNormal 往返 toggle。
// 2026-09-27 手册窗重构为普通 Qt::Window 顶层 QWidget（与主窗口同族）后
// 原生 zoom 正常，分流判断已移除；swizzle 保留为幂等安装的原实现直通
// 挂点，trigger_help_window_native_zoom 仍可直接从 AppKit 真实入口触发
// 手册窗 zoom 供巡览合同断言 isMaximized 往返。
// 非 Apple 平台为内联空实现，调用点无需宏保护。

#ifdef __APPLE__

namespace gmp {

// swizzle NSWindow -zoom:（全局一次，幂等）。须在 QApplication 构造后、
// 手册窗首次展示前调用（main.cpp）。
void install_mac_window_zoom_fix();

// 验证入口：找到 objectName=="helpWorkspaceWindow" 的顶层 QWidget，
// 取其 NSWindow 并直接发 -zoom:（AppKit 真实双击的同一入口）。无手册窗/
// 无原生窗口时返回 false（调用方降级为跳过）。
bool trigger_help_window_native_zoom();

}  // namespace gmp

#else

namespace gmp {

inline void install_mac_window_zoom_fix() {}
inline bool trigger_help_window_native_zoom() { return false; }

}  // namespace gmp

#endif
