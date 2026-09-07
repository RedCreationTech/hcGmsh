#pragma once

class QComboBox;
class QApplication;

namespace gmp {

void install_combo_popup_fix(QComboBox* combo);
// 应用级滚轮拦截：所有 QComboBox 与 QAbstractSpinBox（QSpinBox/
// QDoubleSpinBox）不响应鼠标滚轮，避免滚动页面时误改数值与选项。
// 下拉弹窗内列表滚轮不受影响。
void install_combo_wheel_block(QApplication* app);

}  // namespace gmp
