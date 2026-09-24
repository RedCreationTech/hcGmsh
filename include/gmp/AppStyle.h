// 应用级统一样式表（QApplication 级，覆盖独立顶级窗体/浮动表单）。
// 单一来源：main.cpp 设置到 QApplication；FloatingPropertyForm 等
// QDialog 独立顶层路径未继承应用表的场合显式同步。
#pragma once

#include <QString>

namespace gmp {

inline QString app_style_sheet() {
  return QStringLiteral(
      "QToolTip { background: #1f2937; color: #f9fafb; border: none; "
      "padding: 4px 8px; }"
      // 滚动条：带上下箭头的经典细条样式，全应用统一。
      "QScrollBar:vertical { background: #f0f2f5; width: 14px; margin: 0; }"
      "QScrollBar::handle:vertical { background: #c8ced6; border: 1px solid "
      "#b8bfc8; min-height: 24px; }"
      "QScrollBar::handle:vertical:hover { background: #aeb6c0; }"
      "QScrollBar::add-line:vertical { height: 14px; background: #e4e8ee; "
      "subcontrol-position: bottom; subcontrol-origin: margin; "
      "border-top: 1px solid #d5dbe3; }"
      "QScrollBar::sub-line:vertical { height: 14px; background: #e4e8ee; "
      "subcontrol-position: top; subcontrol-origin: margin; "
      "border-bottom: 1px solid #d5dbe3; }"
      "QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical { "
      "width: 8px; height: 8px; }"
      "QScrollBar:horizontal { background: #f0f2f5; height: 14px; margin: 0; }"
      "QScrollBar::handle:horizontal { background: #c8ced6; border: 1px solid "
      "#b8bfc8; min-width: 24px; }"
      "QScrollBar::handle:horizontal:hover { background: #aeb6c0; }"
      "QScrollBar::add-line:horizontal { width: 14px; background: #e4e8ee; "
      "subcontrol-position: right; subcontrol-origin: margin; "
      "border-left: 1px solid #d5dbe3; }"
      "QScrollBar::sub-line:horizontal { width: 14px; background: #e4e8ee; "
      "subcontrol-position: left; subcontrol-origin: margin; "
      "border-right: 1px solid #d5dbe3; }"
      "QScrollBar::left-arrow:horizontal, QScrollBar::right-arrow:horizontal { "
      "width: 8px; height: 8px; }"
      "QScrollBar::add-page, QScrollBar::sub-page { background: none; }"
      // 勾选/单选：强调蓝选中态，全应用一致。
      "QCheckBox, QRadioButton { spacing: 6px; }"
      "QCheckBox::indicator, QRadioButton::indicator { width: 14px; "
      "height: 14px; border: 1px solid #aab4c0; border-radius: 3px; "
      "background: #ffffff; }"
      "QRadioButton::indicator { border-radius: 7px; }"
      "QCheckBox::indicator:checked { background: #2f6fed; "
      "border: 1px solid #2f6fed; image: url(\":/icons/check.png\"); }"
      "QRadioButton::indicator:checked { background: #2f6fed; "
      "border: 1px solid #2f6fed; image: url(\":/icons/dot.png\"); }"
      "QCheckBox::indicator:hover, QRadioButton::indicator:hover { "
      "border-color: #2f6fed; }"
      // 滑块：与强调蓝一致（播放进度条等）。
      "QSlider::groove:horizontal { height: 4px; background: #d5dbe3; "
      "border-radius: 2px; }"
      "QSlider::handle:horizontal { width: 14px; height: 14px; "
      "margin: -5px 0; border-radius: 7px; background: #2f6fed; }"
      "QSlider::handle:horizontal:hover { background: #1d4ed8; }");
}

} // namespace gmp
