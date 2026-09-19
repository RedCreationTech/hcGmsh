#include "gmp/AssemblyGeometryService.h"

#include <QFileInfo>

#include <algorithm>
#include <set>
#include <stdexcept>

#ifdef GMP_ENABLE_GMSH_GUI
#include <gmsh.h>
#endif

#include "gmp/PhysicalGroupService.h"

namespace gmp {

AssemblyBuildResult AssemblyGeometryService::build(
    const QVariantList& instances, PhysicalGroupService* groups) {
  AssemblyBuildResult result;
#ifndef GMP_ENABLE_GMSH_GUI
  Q_UNUSED(instances);
  Q_UNUSED(groups);
  result.error = "Gmsh is not enabled in this build.";
  return result;
#else
  if (instances.isEmpty()) {
    result.error = "No assembly instances are configured.";
    return result;
  }

  QList<QVariantMap> sorted_instances;
  for (const QVariant& value : instances) {
    sorted_instances.append(value.toMap());
  }
  std::stable_sort(sorted_instances.begin(), sorted_instances.end(),
                   [](const QVariantMap& lhs, const QVariantMap& rhs) {
                     return lhs.value("order").toInt() <
                            rhs.value("order").toInt();
                   });

  try {
    gmsh::clear();
    gmsh::model::add("assembly");

    struct InstanceEntities {
      QString name;
      int dim = -1;
      std::vector<int> tags;
    };
    QList<InstanceEntities> instance_groups;
    int visible_count = 0;
    for (const QVariantMap& instance : sorted_instances) {
      const QString visible =
          instance.value("visible", "true").toString().trimmed().toLower();
      if (visible == "false" || visible == "0") {
        continue;
      }
      const QString name = instance.value("name").toString().trimmed();
      const QString source =
          instance.value("source_path").toString().trimmed();
      if (name.isEmpty() || source.isEmpty() || !QFileInfo::exists(source)) {
        throw std::runtime_error(
            QString("Instance '%1' has no readable Part BREP source.")
                .arg(name.isEmpty() ? QString("(unnamed)") : name)
                .toStdString());
      }

      gmsh::vectorpair imported;
      gmsh::model::occ::importShapes(source.toStdString(), imported, true,
                                     "brep");
      if (imported.empty()) {
        throw std::runtime_error(
            QString("Instance '%1' produced no geometry.").arg(name).toStdString());
      }

      const double sx = instance.value("scale_x", "1").toDouble();
      const double sy = instance.value("scale_y", "1").toDouble();
      const double sz = instance.value("scale_z", "1").toDouble();
      if (sx <= 0.0 || sy <= 0.0 || sz <= 0.0) {
        throw std::runtime_error(
            QString("Instance '%1' scale must be positive.").arg(name).toStdString());
      }
      if (sx != 1.0 || sy != 1.0 || sz != 1.0) {
        gmsh::model::occ::dilate(imported, 0.0, 0.0, 0.0, sx, sy, sz);
      }
      const double pi = 3.14159265358979323846;
      const double rx = instance.value("rotate_x", "0").toDouble() * pi / 180.0;
      const double ry = instance.value("rotate_y", "0").toDouble() * pi / 180.0;
      const double rz = instance.value("rotate_z", "0").toDouble() * pi / 180.0;
      if (rx != 0.0) {
        gmsh::model::occ::rotate(imported, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, rx);
      }
      if (ry != 0.0) {
        gmsh::model::occ::rotate(imported, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, ry);
      }
      if (rz != 0.0) {
        gmsh::model::occ::rotate(imported, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, rz);
      }
      gmsh::model::occ::translate(
          imported, instance.value("translate_x", "0").toDouble(),
          instance.value("translate_y", "0").toDouble(),
          instance.value("translate_z", "0").toDouble());

      int top_dim = -1;
      for (const auto& entity : imported) {
        top_dim = std::max(top_dim, entity.first);
      }
      InstanceEntities group;
      group.name = name;
      group.dim = top_dim;
      for (const auto& entity : imported) {
        if (entity.first == top_dim) {
          group.tags.push_back(entity.second);
        }
      }
      instance_groups.append(group);
      ++visible_count;
    }
    if (visible_count == 0) {
      throw std::runtime_error("No visible assembly instances are configured.");
    }

    gmsh::model::occ::synchronize();
    for (const auto& group : instance_groups) {
      if (group.dim < 0 || group.tags.empty()) {
        continue;
      }
      const int volume_group =
          gmsh::model::addPhysicalGroup(group.dim, group.tags);
      gmsh::model::setPhysicalName(group.dim, volume_group,
                                   group.name.toStdString());

      gmsh::vectorpair top_entities;
      for (int tag : group.tags) {
        top_entities.emplace_back(group.dim, tag);
      }
      gmsh::vectorpair boundary;
      gmsh::model::getBoundary(top_entities, boundary, true, false, false);
      std::set<int> boundary_tags;
      for (const auto& entity : boundary) {
        if (entity.first == group.dim - 1) {
          boundary_tags.insert(std::abs(entity.second));
        }
      }
      if (!boundary_tags.empty()) {
        const std::vector<int> tags(boundary_tags.begin(), boundary_tags.end());
        const int surface_group =
            gmsh::model::addPhysicalGroup(group.dim - 1, tags);
        gmsh::model::setPhysicalName(group.dim - 1, surface_group,
                                     (group.name + "_surface").toStdString());
      }
    }

    // 用户定义的接触/固定/加载专用面组在默认实例组之后恢复。恢复逻辑
    // 只接受所属实例内几何签名一致的实体，防止 Part 修改后把旧组静默
    // 绑定到同 tag 的另一张面。
    if (groups) {
      groups->restore(&result.log_lines);
    }

    result.ok = true;
    result.visible_count = visible_count;
    return result;
  } catch (const std::exception& ex) {
    try {
      gmsh::clear();
    } catch (...) {
    }
    result.ok = false;
    result.error = QString::fromUtf8(ex.what());
    return result;
  }
#endif
}

}  // namespace gmp
