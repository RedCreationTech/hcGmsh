#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace gmp {

// 单个 Physical Group 的持久化记录。
struct PhysicalGroupEntry {
  QString name;          // 稳定语义键
  int dim = -1;          // 0/1/2/3
  QList<int> tags;       // 本次 .msh 生成时的 entity tags（仅审计）
  int entity_count = 0;  // 包含的 Gmsh 实体数
  int element_count = 0; // 包含的单元数
  QStringList bound_object_ids; // 引用此组的对象 ID（可选）

  bool is_valid() const {
    return !name.trimmed().isEmpty() && dim >= 0 && dim <= 3 &&
           !tags.isEmpty() && entity_count > 0 && element_count > 0;
  }

  QVariantMap to_variant_map() const;
  static PhysicalGroupEntry from_variant_map(const QVariantMap& map);
};

// Physical Groups 清单与网格摘要。
// 对应需求 REQ-012：从 .msh 提取并持久化物理组语义。
struct PhysicalGroupManifest {
  QString mesh_path;          // 案例包/项目内相对路径
  QString mesh_sha256;        // .msh 文件 SHA-256
  QList<PhysicalGroupEntry> groups;
  int mesh_dim = -1;
  int node_count = 0;
  int element_count = 0;
  QString element_type;       // 主导单元类型
  QMap<QString, double> quality_summary; // min/max/avg 等

  bool is_valid() const;
  bool has_group(const QString& name, int dim = -1) const;
  PhysicalGroupEntry group(const QString& name) const;
  QStringList group_names(int dim = -1) const;

  QVariantMap to_variant_map() const;
  static PhysicalGroupManifest from_variant_map(const QVariantMap& map);

  // 校验唯一性、非空、维度一致性。
  struct ValidationIssue {
    QString severity; // error | warning
    QString message;
    QString group_name;
  };
  QList<ValidationIssue> validate() const;
};

}  // namespace gmp
