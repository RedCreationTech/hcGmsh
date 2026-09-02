#include "gmp/ProjectSchema.h"

#include <QMetaType>

namespace gmp::project_schema {

namespace {

QVariant scalar_to_variant(const YAML::Node& node) {
  const QString raw = QString::fromStdString(node.as<std::string>(""));
  const QString lower = raw.toLower();
  if (lower == QStringLiteral("true") || lower == QStringLiteral("false")) {
    return lower == QStringLiteral("true");
  }

  bool ok_int = false;
  const qlonglong int_value = raw.toLongLong(&ok_int);
  if (ok_int && !raw.contains('.') && !raw.contains('e', Qt::CaseInsensitive)) {
    return int_value;
  }

  bool ok_double = false;
  const double double_value = raw.toDouble(&ok_double);
  if (ok_double) {
    return double_value;
  }
  return raw;
}

int int_or(const YAML::Node& node, const char* key, int fallback) {
  return node && node[key] ? node[key].as<int>(fallback) : fallback;
}

QString string_or(const YAML::Node& node, const char* key) {
  return node && node[key]
             ? QString::fromStdString(node[key].as<std::string>(""))
             : QString();
}

}  // namespace

QStringList model_root_nodes() {
  return {
      "Parts",       "Sketches",    "Features",     "Datums",
      "Materials",   "Sections",    "Assembly",     "Physics",
      "Steps",       "BC",          "Loads",        "Interactions",
      "Constraints", "Selections",  "Functions",    "Variables",
      "Outputs",     "Mesh",        "Input Cases",  "Jobs",
      "Results",
  };
}

QVariant yaml_to_variant(const YAML::Node& node) {
  if (!node || node.IsNull()) {
    return QVariant();
  }
  if (node.IsScalar()) {
    return scalar_to_variant(node);
  }
  if (node.IsSequence()) {
    QVariantList values;
    for (const auto& item : node) {
      values.append(yaml_to_variant(item));
    }
    return values;
  }
  if (node.IsMap()) {
    return yaml_map_to_variant_map(node);
  }
  return QVariant();
}

YAML::Node variant_to_yaml(const QVariant& value) {
  if (!value.isValid() || value.isNull()) {
    return YAML::Node(YAML::NodeType::Null);
  }

  if (value.typeId() == QMetaType::QVariantMap) {
    return variant_map_to_yaml(value.toMap());
  }
  if (value.typeId() == QMetaType::QStringList) {
    YAML::Node sequence(YAML::NodeType::Sequence);
    for (const QString& item : value.toStringList()) {
      sequence.push_back(item.toStdString());
    }
    return sequence;
  }
  if (value.typeId() == QMetaType::QVariantList) {
    YAML::Node sequence(YAML::NodeType::Sequence);
    for (const QVariant& item : value.toList()) {
      sequence.push_back(variant_to_yaml(item));
    }
    return sequence;
  }

  YAML::Node scalar;
  switch (value.typeId()) {
    case QMetaType::Bool:
      scalar = value.toBool();
      break;
    case QMetaType::Int:
    case QMetaType::LongLong:
      scalar = value.toLongLong();
      break;
    case QMetaType::UInt:
    case QMetaType::ULongLong:
      scalar = value.toULongLong();
      break;
    case QMetaType::Float:
    case QMetaType::Double:
      scalar = value.toDouble();
      break;
    default:
      scalar = value.toString().toStdString();
      break;
  }
  return scalar;
}

QVariantMap yaml_map_to_variant_map(const YAML::Node& node) {
  QVariantMap map;
  if (!node || !node.IsMap()) {
    return map;
  }
  for (const auto& item : node) {
    const QString key = QString::fromStdString(item.first.as<std::string>());
    map.insert(key, yaml_to_variant(item.second));
  }
  return map;
}

YAML::Node variant_map_to_yaml(const QVariantMap& map) {
  YAML::Node node(YAML::NodeType::Map);
  for (auto it = map.cbegin(); it != map.cend(); ++it) {
    node[it.key().toStdString()] = variant_to_yaml(it.value());
  }
  return node;
}

PhysicalGroupManifest mesh_snapshot_from_yaml(const YAML::Node& node) {
  PhysicalGroupManifest manifest;
  if (!node || !node.IsMap()) {
    return manifest;
  }

  manifest.mesh_path = string_or(node, "path");
  manifest.mesh_sha256 = string_or(node, "sha256");

  const YAML::Node groups = node["physical_groups"];
  if (groups && groups.IsSequence()) {
    for (const auto& group_node : groups) {
      PhysicalGroupEntry group;
      group.name = string_or(group_node, "name");
      group.dim = int_or(group_node, "dim", -1);
      if (group_node["tags"] && group_node["tags"].IsSequence()) {
        for (const auto& tag : group_node["tags"]) {
          group.tags.append(tag.as<int>(0));
        }
      }
      group.entity_count = int_or(group_node, "entity_count", 0);
      group.element_count = int_or(group_node, "element_count", 0);
      if (group_node["bound_object_ids"] &&
          group_node["bound_object_ids"].IsSequence()) {
        for (const auto& object_id : group_node["bound_object_ids"]) {
          group.bound_object_ids.append(
              QString::fromStdString(object_id.as<std::string>("")));
        }
      }
      manifest.groups.append(group);
    }
  }

  // v2 规范使用 summary；兼容早期实现写在 mesh_snapshot 顶层的字段。
  const YAML::Node summary =
      node["summary"] && node["summary"].IsMap() ? node["summary"] : node;
  manifest.mesh_dim = int_or(summary, "mesh_dim", -1);
  manifest.node_count = int_or(summary, "node_count", 0);
  manifest.element_count = int_or(summary, "element_count", 0);
  manifest.element_type = string_or(summary, "element_type");

  if (node["quality_summary"] && node["quality_summary"].IsMap()) {
    for (const auto& value : node["quality_summary"]) {
      manifest.quality_summary.insert(
          QString::fromStdString(value.first.as<std::string>()),
          value.second.as<double>(0.0));
    }
  }
  for (const QString& key : {QStringLiteral("quality_min"),
                             QStringLiteral("quality_max"),
                             QStringLiteral("quality_avg")}) {
    const std::string yaml_key = key.toStdString();
    if (summary[yaml_key]) {
      manifest.quality_summary.insert(key, summary[yaml_key].as<double>(0.0));
    }
  }
  return manifest;
}

YAML::Node mesh_snapshot_to_yaml(const PhysicalGroupManifest& manifest) {
  YAML::Node node(YAML::NodeType::Map);
  node["path"] = manifest.mesh_path.toStdString();
  node["sha256"] = manifest.mesh_sha256.toStdString();

  YAML::Node groups(YAML::NodeType::Sequence);
  for (const auto& group : manifest.groups) {
    YAML::Node group_node(YAML::NodeType::Map);
    group_node["name"] = group.name.toStdString();
    group_node["dim"] = group.dim;
    YAML::Node tags(YAML::NodeType::Sequence);
    for (int tag : group.tags) {
      tags.push_back(tag);
    }
    group_node["tags"] = tags;
    group_node["entity_count"] = group.entity_count;
    group_node["element_count"] = group.element_count;
    if (!group.bound_object_ids.isEmpty()) {
      YAML::Node ids(YAML::NodeType::Sequence);
      for (const QString& id : group.bound_object_ids) {
        ids.push_back(id.toStdString());
      }
      group_node["bound_object_ids"] = ids;
    }
    groups.push_back(group_node);
  }
  node["physical_groups"] = groups;

  YAML::Node summary(YAML::NodeType::Map);
  summary["mesh_dim"] = manifest.mesh_dim;
  summary["node_count"] = manifest.node_count;
  summary["element_count"] = manifest.element_count;
  summary["element_type"] = manifest.element_type.toStdString();
  for (auto it = manifest.quality_summary.cbegin();
       it != manifest.quality_summary.cend(); ++it) {
    summary[it.key().toStdString()] = it.value();
  }
  node["summary"] = summary;
  return node;
}

}  // namespace gmp::project_schema
