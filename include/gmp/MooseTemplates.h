#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace gmp {

// templates/moose/<key>/ 下单个模板的元数据（template.json + input.i）
struct MooseTemplateInfo {
  QString key;
  QString display_name;
  QString status;       // "verified" | "prototype"
  QString status_note;  // prototype 的限制说明（可空）
  QString source_desc;  // 来源仓/路径/commit 的可读摘要
  QString dir;          // 模板目录绝对路径
  QString input_path;   // <dir>/input.i
  QStringList mesh_files;   // template.json 声明的网格文件（相对模板目录）
  QStringList extra_files;  // template.json 声明的其他附件
  bool valid = false;
};

// 模板库根目录定位顺序：环境变量 GMP_MOOSE_TEMPLATES >
// 编译期 GMP_TEMPLATE_DIR > 可执行文件旁/源码树内 templates/moose。
// 找不到时返回空串。
QString moose_templates_root();

// 读取 <root>/<key>/template.json 并确认 input.i 存在；失败返回 valid=false。
MooseTemplateInfo load_moose_template(const QString& root, const QString& key);

}  // namespace gmp
