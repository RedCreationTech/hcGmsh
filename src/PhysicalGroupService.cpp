#include "gmp/PhysicalGroupService.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

#ifdef GMP_ENABLE_GMSH_GUI
#include <gmsh.h>
#endif

namespace gmp {

QString PhysicalGroupService::to_json() const {
  return QString::fromUtf8(
      QJsonDocument::fromVariant(definitions_).toJson(QJsonDocument::Compact));
}

bool PhysicalGroupService::load_json(const QString& json, QString* error) {
  definitions_.clear();
  const QByteArray raw = json.toUtf8();
  QJsonParseError parse_error;
  const QJsonDocument doc = QJsonDocument::fromJson(raw, &parse_error);
  if (parse_error.error == QJsonParseError::NoError && doc.isArray()) {
    definitions_ = doc.toVariant().toList();
    return true;
  }
  if (!json.trimmed().isEmpty() && json.trimmed() != "[]") {
    if (error) {
      *error = QStringLiteral(
          "Saved custom Physical Groups are invalid and were ignored.");
    }
    return false;
  }
  return true;
}

void PhysicalGroupService::forget(const QString& name) {
  if (name.trimmed().isEmpty()) {
    return;
  }
  QVariantList kept;
  for (const QVariant& value : definitions_) {
    if (value.toMap().value("name").toString() != name) {
      kept.append(value);
    }
  }
  definitions_ = kept;
}

bool PhysicalGroupService::remember(const QString& name, int dim,
                                    const std::vector<int>& tags,
                                    bool is_assembly_active, QStringList* log) {
#ifdef GMP_ENABLE_GMSH_GUI
  if (!is_assembly_active || name.trimmed().isEmpty() || tags.empty()) {
    return false;
  }
  QVariantList entities;
  for (const int tag : tags) {
    const QVariantList bbox = entity_bounding_box(dim, tag);
    const QString owner = assembly_owner_for_entity(dim, tag);
    if (bbox.size() != 6 || (dim == 2 && owner.isEmpty())) {
      if (log) {
        log->append(QString("Custom Physical Group '%1' was not persisted: "
                            "an entity has no Assembly owner or geometry signature.")
                        .arg(name));
      }
      return false;
    }
    entities.append(
        QVariantMap{{"tag", tag}, {"owner", owner}, {"bbox", bbox}});
  }
  forget(name);
  definitions_.append(
      QVariantMap{{"version", 1},
                  {"name", name.trimmed()},
                  {"dim", dim},
                  {"entities", entities}});
  if (log) {
    log->append(QString("Custom Physical Group saved: %1 (%2 entity/entities).")
                    .arg(name)
                    .arg(entities.size()));
  }
  return true;
#else
  Q_UNUSED(name);
  Q_UNUSED(dim);
  Q_UNUSED(tags);
  Q_UNUSED(is_assembly_active);
  Q_UNUSED(log);
  return false;
#endif
}

void PhysicalGroupService::restore(QStringList* log) {
#ifdef GMP_ENABLE_GMSH_GUI
  for (const QVariant& value : definitions_) {
    const QVariantMap spec = value.toMap();
    const QString name = spec.value("name").toString().trimmed();
    const int dim = spec.value("dim", -1).toInt();
    const QVariantList entity_specs = spec.value("entities").toList();
    if (name.isEmpty() || dim < 0 || dim > 3 || entity_specs.isEmpty()) {
      continue;
    }

    bool name_in_use = false;
    std::vector<std::pair<int, int>> existing_groups;
    gmsh::model::getPhysicalGroups(existing_groups, dim);
    for (const auto& group : existing_groups) {
      std::string existing_name;
      gmsh::model::getPhysicalName(group.first, group.second, existing_name);
      if (QString::fromStdString(existing_name) == name) {
        name_in_use = true;
        break;
      }
    }
    if (name_in_use) {
      if (log) {
        log->append(QString("Custom Physical Group '%1' was not restored: "
                            "the name is already in use.")
                        .arg(name));
      }
      continue;
    }

    std::vector<int> resolved;
    std::set<int> used;
    bool complete = true;
    for (const QVariant& entity_value : entity_specs) {
      const QVariantMap entity_spec = entity_value.toMap();
      const QVariantList expected = entity_spec.value("bbox").toList();
      const QString owner = entity_spec.value("owner").toString();
      const int original_tag = entity_spec.value("tag", -1).toInt();
      if (expected.size() != 6) {
        complete = false;
        break;
      }

      std::vector<int> candidates = assembly_owner_entities(owner, dim);
      if (owner.isEmpty()) {
        std::vector<std::pair<int, int>> all;
        gmsh::model::getEntities(all, dim);
        for (const auto& entity : all) {
          candidates.push_back(entity.second);
        }
      }
      double scale = 1.0;
      for (const QVariant& coordinate : expected) {
        scale = std::max(scale, std::abs(coordinate.toDouble()));
      }
      const double tolerance = 1e-6 * scale;
      auto distance = [dim, &expected](int candidate) {
        const QVariantList actual = entity_bounding_box(dim, candidate);
        if (actual.size() != 6) {
          return std::numeric_limits<double>::infinity();
        }
        double maximum = 0.0;
        for (int i = 0; i < 6; ++i) {
          maximum = std::max(
              maximum,
              std::abs(actual.at(i).toDouble() - expected.at(i).toDouble()));
        }
        return maximum;
      };

      std::vector<std::pair<double, int>> matches;
      for (const int candidate : candidates) {
        if (used.count(candidate)) {
          continue;
        }
        const double candidate_distance = distance(candidate);
        if (candidate_distance <= tolerance) {
          // tag 只用于距离完全相同时提供稳定排序；存在多个等价候选时
          // 下方仍会拒绝，不以 tag 代替几何身份。
          matches.push_back({candidate_distance, candidate});
        }
      }
      std::sort(matches.begin(), matches.end(),
                [original_tag](const auto& lhs, const auto& rhs) {
                  if (lhs.first != rhs.first) {
                    return lhs.first < rhs.first;
                  }
                  return lhs.second == original_tag &&
                         rhs.second != original_tag;
                });
      int selected = -1;
      if (matches.size() == 1 ||
          (matches.size() > 1 &&
           matches.at(1).first - matches.at(0).first > tolerance * 0.01)) {
        selected = matches.front().second;
      }
      if (selected < 0) {
        complete = false;
        break;
      }
      used.insert(selected);
      resolved.push_back(selected);
    }

    if (!complete || resolved.size() !=
                         static_cast<std::size_t>(entity_specs.size())) {
      if (log) {
        log->append(QString("Custom Physical Group '%1' was not restored: "
                            "the owning instance geometry has changed.")
                        .arg(name));
      }
      continue;
    }
    const int group_tag = gmsh::model::addPhysicalGroup(dim, resolved);
    gmsh::model::setPhysicalName(dim, group_tag, name.toStdString());
    if (log) {
      log->append(
          QString("Custom Physical Group restored: %1 (%2 entity/entities).")
              .arg(name)
              .arg(resolved.size()));
    }
  }
#else
  Q_UNUSED(log);
#endif
}

bool PhysicalGroupService::validate_group_input(
    int dim, const QString& name, const QString& entities_text,
    bool has_existing_entities, int exclude_tag, bool chinese,
    QString* error) const {
#ifdef GMP_ENABLE_GMSH_GUI
  auto fail = [error, chinese](const QString& en, const QString& zh_msg) {
    if (error) {
      *error = chinese ? zh_msg : en;
    }
    return false;
  };
  if (dim < 0 || dim > 3) {
    return fail(QString("invalid dimension %1 (expected 0-3).").arg(dim),
                QString::fromUtf8("维度 %1 非法（应为 0~3）。").arg(dim));
  }
  if (name.isEmpty()) {
    return fail("name is empty; please enter a group name.",
                QString::fromUtf8("名称为空，请输入物理组名称。"));
  }
  if (entities_text.trimmed().isEmpty()) {
    return fail("no entities selected; pick at least one entity.",
                QString::fromUtf8("未选择实体，请至少选择一个实体。"));
  }
  const auto tokens = parse_dim_tag_tokens(entities_text);
  if (tokens.empty()) {
    return fail("Entities contains no valid entity IDs.",
                QString::fromUtf8("实体列表中没有合法的实体编号。"));
  }
  for (const auto& token : tokens) {
    if (token.has_dim && token.dim != dim) {
      return fail(QString("entity %1:%2 does not match group dimension %3.")
                      .arg(token.dim)
                      .arg(token.tag)
                      .arg(dim),
                  QString::fromUtf8("实体 %1:%2 的维度与物理组维度 %3 不一致。")
                      .arg(token.dim)
                      .arg(token.tag)
                      .arg(dim));
    }
  }
  if (!has_existing_entities) {
    return fail(
        QString("selection has no existing entities of dimension %1.").arg(dim),
        QString::fromUtf8("所选内容中没有维度 %1 的现存实体。").arg(dim));
  }
  std::vector<std::pair<int, int>> groups;
  gmsh::model::getPhysicalGroups(groups, dim);
  for (const auto& g : groups) {
    if (g.second == exclude_tag) {
      continue;
    }
    std::string existing;
    gmsh::model::getPhysicalName(dim, g.second, existing);
    if (QString::fromStdString(existing).trimmed() == name) {
      return fail(QString("name \"%1\" is already used by physical group "
                          "%2:%3.")
                      .arg(name)
                      .arg(dim)
                      .arg(g.second),
                  QString::fromUtf8("名称“%1”已被物理组 %2:%3 使用。")
                      .arg(name)
                      .arg(dim)
                      .arg(g.second));
    }
  }
  return true;
#else
  Q_UNUSED(dim);
  Q_UNUSED(name);
  Q_UNUSED(entities_text);
  Q_UNUSED(has_existing_entities);
  Q_UNUSED(exclude_tag);
  Q_UNUSED(chinese);
  Q_UNUSED(error);
  return false;
#endif
}

std::vector<PhysicalGroupService::DimTagToken>
PhysicalGroupService::parse_dim_tag_tokens(const QString& text) {
  QString cleaned = text;
  cleaned.replace(",", " ");
  const QStringList parts =
      cleaned.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
  std::vector<DimTagToken> tokens;
  tokens.reserve(parts.size());
  for (const auto& part : parts) {
    const int colon = part.indexOf(':');
    if (colon > 0) {
      bool ok_dim = false;
      bool ok_tag = false;
      const int dim = part.left(colon).toInt(&ok_dim);
      const int tag = part.mid(colon + 1).toInt(&ok_tag);
      if (ok_dim && ok_tag) {
        tokens.push_back({dim, tag, true});
      }
    } else {
      bool ok = false;
      const int tag = part.toInt(&ok);
      if (ok) {
        tokens.push_back({-1, tag, false});
      }
    }
  }
  return tokens;
}

QVariantList PhysicalGroupService::entity_bounding_box(int dim, int tag) {
#ifdef GMP_ENABLE_GMSH_GUI
  try {
    double xmin = 0.0;
    double ymin = 0.0;
    double zmin = 0.0;
    double xmax = 0.0;
    double ymax = 0.0;
    double zmax = 0.0;
    gmsh::model::getBoundingBox(dim, tag, xmin, ymin, zmin, xmax, ymax, zmax);
    return {xmin, ymin, zmin, xmax, ymax, zmax};
  } catch (...) {
  }
#else
  Q_UNUSED(dim);
  Q_UNUSED(tag);
#endif
  return {};
}

QString PhysicalGroupService::assembly_owner_for_entity(int dim, int tag) {
#ifdef GMP_ENABLE_GMSH_GUI
  if (dim < 0 || dim >= 3) {
    return {};
  }
  std::vector<std::pair<int, int>> owner_groups;
  gmsh::model::getPhysicalGroups(owner_groups, dim + 1);
  for (const auto& owner_group : owner_groups) {
    std::string owner_name;
    gmsh::model::getPhysicalName(owner_group.first, owner_group.second,
                                 owner_name);
    if (owner_name.empty()) {
      continue;
    }
    std::vector<int> owner_entities;
    gmsh::model::getEntitiesForPhysicalGroup(
        owner_group.first, owner_group.second, owner_entities);
    for (const int owner_tag : owner_entities) {
      gmsh::vectorpair boundary;
      gmsh::model::getBoundary({{dim + 1, owner_tag}}, boundary, false, false,
                               false);
      for (const auto& entity : boundary) {
        if (entity.first == dim && std::abs(entity.second) == tag) {
          return QString::fromStdString(owner_name);
        }
      }
    }
  }
#else
  Q_UNUSED(dim);
  Q_UNUSED(tag);
#endif
  return {};
}

std::vector<int> PhysicalGroupService::assembly_owner_entities(
    const QString& owner, int dim) {
  std::vector<int> result;
#ifdef GMP_ENABLE_GMSH_GUI
  if (owner.isEmpty() || dim < 0 || dim >= 3) {
    return result;
  }
  std::vector<std::pair<int, int>> owner_groups;
  gmsh::model::getPhysicalGroups(owner_groups, dim + 1);
  std::set<int> unique;
  for (const auto& owner_group : owner_groups) {
    std::string owner_name;
    gmsh::model::getPhysicalName(owner_group.first, owner_group.second,
                                 owner_name);
    if (QString::fromStdString(owner_name) != owner) {
      continue;
    }
    std::vector<int> owner_top_entities;
    gmsh::model::getEntitiesForPhysicalGroup(
        owner_group.first, owner_group.second, owner_top_entities);
    for (const int owner_tag : owner_top_entities) {
      gmsh::vectorpair boundary;
      gmsh::model::getBoundary({{dim + 1, owner_tag}}, boundary, false, false,
                               false);
      for (const auto& entity : boundary) {
        if (entity.first == dim) {
          unique.insert(std::abs(entity.second));
        }
      }
    }
  }
  result.assign(unique.begin(), unique.end());
#else
  Q_UNUSED(owner);
  Q_UNUSED(dim);
#endif
  return result;
}

}  // namespace gmp
