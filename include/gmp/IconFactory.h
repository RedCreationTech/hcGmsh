// 集中式图标工厂（QtAwesome / Font Awesome 7 Free 地基层）。
// 面板接线统一通过 gmp::icons::get(key) 取图标，键名映射表见 IconFactory.cpp。
#pragma once

#include <QIcon>
#include <QString>

class QWidget;

namespace gmp::icons {

// 目标像素尺寸语义。图标由字体矢量绘制、本身可缩放；调用方渲染时按
// int(size) 取像素（如 QAction::setIcon 后由 toolButtonStyle/iconSize 决定，
// 需要固定尺寸位图时用 icon.pixmap(int(size))）。
enum class Size { Toolbar = 20, Button = 16, Tree = 16, Menu = 16 };

// 初始化全局 fa::QtAwesome 实例并加载字体；按当前 palette 设置
// color/color-disabled/color-active/color-selected 默认色（深色主题预留），
// scale-factor 默认 0.9。须在 QApplication 构造之后调用。
void init(QWidget* paletteAnchor = nullptr);

// 按映射键取图标；未知键返回空 QIcon 并对每个键 qWarning 一次。
QIcon get(const QString& key, Size size = Size::Button);

// 便捷：工具栏/回放控件用（等价 get(key, Size::Toolbar)）。
QIcon toolbar(const QString& key);

} // namespace gmp::icons
