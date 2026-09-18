#pragma once

// app 层单位显示工具（v0.2，仅依赖 Qt Core，可无 Widget 测试）。
// 缺陷 2026-09-19-025/026：显示侧换算与单位提示的单一实现点。

#include <QString>

namespace gmp {

// 存储值（SI 求解单位）→ 快捷字段显示文本。15 位有效数字：不暴露
// double 伪精度（如 29791.459780000001），显示值回解析后与存储值在
// 双精度噪声内一致。factor <= 0 时按 1 处理。
QString format_unit_display_value(double stored, double factor);

// 已知带单位换算机制的参数键描述。当前只有 Materials.youngs_modulus
// 走 display_unit_factors_/unit_factor_stress 机制（quantity=pressure，
// 存储 Pa，显示 MPa）；后续接入新键时在此登记。返回 false = 无单位机制。
struct UnitKeyInfo {
  QString quantity;      // display_unit_factors_ 的键（如 "pressure"）
  QString stored_unit;   // 存储/求解单位（如 "Pa"）
  QString display_unit;  // 快捷字段显示单位（如 "MPa"）
};
bool unit_key_info(const QString& key, UnitKeyInfo* info);

}  // namespace gmp
