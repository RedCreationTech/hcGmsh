#pragma once

#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include <yaml-cpp/yaml.h>

#include "gmp/PhysicalGroupManifest.h"

namespace gmp::project_schema {

inline constexpr int kCurrentVersion = 2;

// project-v2.md 中声明的模型根节点。旧节点保持原顺序，新节点追加到相邻语义位置。
QStringList model_root_nodes();

// YAML 与 QVariant 的递归转换，用于保留单位比例因子、列表及后续扩展结构。
QVariant yaml_to_variant(const YAML::Node& node);
YAML::Node variant_to_yaml(const QVariant& value);
QVariantMap yaml_map_to_variant_map(const YAML::Node& node);
YAML::Node variant_map_to_yaml(const QVariantMap& map);

// mesh_snapshot 的 v2 规范读写。读取兼容 Phase 0 早期实现的扁平摘要字段，
// 写入统一使用 project-v2.md 中的 summary 子节点。
PhysicalGroupManifest mesh_snapshot_from_yaml(const YAML::Node& node);
YAML::Node mesh_snapshot_to_yaml(const PhysicalGroupManifest& manifest);

}  // namespace gmp::project_schema
