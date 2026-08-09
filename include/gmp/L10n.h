#pragma once

class QWidget;
class QString;

namespace gmp {
namespace l10n {

enum class Language { English, Chinese };

// 当前语言（从 QSettings 读取，默认中文）
Language current_language();
// 设置并持久化语言偏好
void set_language(Language lang);
// 按当前语言翻译 root 下的整个控件树
// (按钮/标签/复选框/分组框/页签/菜单/动作/下拉项/列表项/表头/占位符等;
//  未收录的文本保持原样; 动态拼接串按前缀表仅译前缀)
void apply(QWidget* root);

}  // namespace l10n
}  // namespace gmp
