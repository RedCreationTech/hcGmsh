#include "gmp/PhysicalGroupManifest.h"

#include <QMap>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace gmp {

namespace {

int safe_int(const QVariantMap& map, const QString& key, int fallback = 0) {
  bool ok = false;
  const int v = map.value(key).toInt(&ok);
  return ok ? v : fallback;
}

double safe_double(const QVariantMap& map, const QString& key, double fallback = 0.0) {
  bool ok = false;
  const double v = map.value(key).toDouble(&ok);
  return ok ? v : fallback;
}

}  // namespace

QVariantMap PhysicalGroupEntry::to_variant_map() const {
  QVariantMap map;
  map.insert("name", name);
  map.insert("dim", dim);
  QVariantList tag_list;
  for (int t : tags) {
    tag_list.append(t);
  }
  map.insert("tags", tag_list);
  map.insert("entity_count", entity_count);
  map.insert("element_count", element_count);
  map.insert("bound_object_ids", bound_object_ids);
  return map;
}

PhysicalGroupEntry PhysicalGroupEntry::from_variant_map(const QVariantMap& map) {
  PhysicalGroupEntry entry;
  entry.name = map.value("name").toString();
  entry.dim = safe_int(map, "dim", -1);
  const QVariantList tag_list = map.value("tags").toList();
  for (const QVariant& v : tag_list) {
    bool ok = false;
    const int t = v.toInt(&ok);
    if (ok) {
      entry.tags.append(t);
    }
  }
  entry.entity_count = safe_int(map, "entity_count", 0);
  entry.element_count = safe_int(map, "element_count", 0);
  entry.bound_object_ids = map.value("bound_object_ids").toStringList();
  return entry;
}

bool PhysicalGroupManifest::is_valid() const {
  const auto issues = validate();
  for (const auto& issue : issues) {
    if (issue.severity == QStringLiteral("error")) {
      return false;
    }
  }
  return true;
}

bool PhysicalGroupManifest::has_group(const QString& name, int dim) const {
  for (const auto& g : groups) {
    if (g.name == name && (dim < 0 || g.dim == dim)) {
      return true;
    }
  }
  return false;
}

PhysicalGroupEntry PhysicalGroupManifest::group(const QString& name) const {
  for (const auto& g : groups) {
    if (g.name == name) {
      return g;
    }
  }
  return PhysicalGroupEntry();
}

QStringList PhysicalGroupManifest::group_names(int dim) const {
  QStringList names;
  for (const auto& g : groups) {
    if (dim < 0 || g.dim == dim) {
      names.append(g.name);
    }
  }
  names.removeDuplicates();
  return names;
}

QVariantMap PhysicalGroupManifest::to_variant_map() const {
  QVariantMap map;
  map.insert("mesh_path", mesh_path);
  map.insert("mesh_sha256", mesh_sha256);
  QVariantList group_list;
  for (const auto& g : groups) {
    group_list.append(g.to_variant_map());
  }
  map.insert("physical_groups", group_list);
  map.insert("mesh_dim", mesh_dim);
  map.insert("node_count", node_count);
  map.insert("element_count", element_count);
  map.insert("element_type", element_type);
  QVariantMap quality_map;
  for (auto it = quality_summary.begin(); it != quality_summary.end(); ++it) {
    quality_map.insert(it.key(), it.value());
  }
  map.insert("quality_summary", quality_map);
  return map;
}

PhysicalGroupManifest PhysicalGroupManifest::from_variant_map(const QVariantMap& map) {
  PhysicalGroupManifest manifest;
  manifest.mesh_path = map.value("mesh_path").toString();
  manifest.mesh_sha256 = map.value("mesh_sha256").toString();
  const QVariantList group_list = map.value("physical_groups").toList();
  for (const QVariant& v : group_list) {
    manifest.groups.append(
        PhysicalGroupEntry::from_variant_map(v.toMap()));
  }
  manifest.mesh_dim = safe_int(map, "mesh_dim", -1);
  manifest.node_count = safe_int(map, "node_count", 0);
  manifest.element_count = safe_int(map, "element_count", 0);
  manifest.element_type = map.value("element_type").toString();
  const QVariantMap quality_map = map.value("quality_summary").toMap();
  for (auto it = quality_map.begin(); it != quality_map.end(); ++it) {
    manifest.quality_summary.insert(it.key(), it.value().toDouble());
  }
  return manifest;
}

QList<PhysicalGroupManifest::ValidationIssue> PhysicalGroupManifest::validate() const {
  QList<ValidationIssue> issues;

  if (mesh_path.isEmpty()) {
    issues.append({"error", "mesh_path is empty", QString()});
  } else {
    const QString clean = QDir::cleanPath(mesh_path);
    if (QFileInfo(mesh_path).isAbsolute() || clean == QStringLiteral("..") ||
        clean.startsWith(QStringLiteral("../"))) {
      issues.append({"error", "mesh_path must be relative to the project/snapshot root",
                     QString()});
    }
  }
  if (mesh_sha256.isEmpty()) {
    issues.append({"error", "mesh_sha256 is empty", QString()});
  } else {
    static const QRegularExpression sha256_re(QStringLiteral("^[0-9a-fA-F]{64}$"));
    if (!sha256_re.match(mesh_sha256).hasMatch()) {
      issues.append({"error", "mesh_sha256 must contain 64 hexadecimal characters",
                     QString()});
    }
  }
  if (mesh_dim < 1 || mesh_dim > 3) {
    issues.append({"error", "mesh_dim must be 1, 2 or 3", QString()});
  }

  QMap<QString, int> seen_dim;
  for (const auto& g : groups) {
    if (g.name.isEmpty()) {
      issues.append({"error", "Physical group has empty name", g.name});
      continue;
    }
    if (g.name.trimmed().isEmpty()) {
      issues.append({"error", "Physical group name is whitespace", g.name});
      continue;
    }
    if (g.dim < 0 || g.dim > 3) {
      issues.append({"error", "Physical group dimension out of range", g.name});
    } else if (mesh_dim >= 1 && g.dim > mesh_dim) {
      issues.append({"error", "Physical group dimension exceeds mesh dimension",
                     g.name});
    }
    if (g.tags.isEmpty()) {
      issues.append({"error", "Physical group has no entity tags", g.name});
    } else {
      QSet<int> seen_tags;
      for (int tag : g.tags) {
        if (tag <= 0) {
          issues.append({"error", "Physical group contains a non-positive entity tag",
                         g.name});
        } else if (seen_tags.contains(tag)) {
          issues.append({"error", "Physical group contains duplicate entity tags",
                         g.name});
        }
        seen_tags.insert(tag);
      }
    }
    if (g.entity_count <= 0) {
      issues.append({"error", "Physical group contains no entities", g.name});
    }
    if (g.element_count <= 0) {
      issues.append({"error", "Physical group contains no mesh elements", g.name});
    }

    const QString key = QString("%1:%2").arg(g.dim).arg(g.name);
    if (seen_dim.contains(key)) {
      issues.append({"error", "Duplicate physical group name within same dimension", g.name});
    } else {
      seen_dim.insert(key, 1);
    }
  }

  return issues;
}

}  // namespace gmp
