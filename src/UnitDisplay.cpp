#include "gmp/UnitDisplay.h"

namespace gmp {

QString format_unit_display_value(double stored, double factor) {
  if (factor <= 0.0) {
    factor = 1.0;
  }
  return QString::number(stored / factor, 'g', 15);
}

bool unit_key_info(const QString& key, UnitKeyInfo* info) {
  if (key != QStringLiteral("youngs_modulus")) {
    return false;
  }
  if (info) {
    info->quantity = QStringLiteral("pressure");
    info->stored_unit = QStringLiteral("Pa");
    info->display_unit = QStringLiteral("MPa");
  }
  return true;
}

}  // namespace gmp
