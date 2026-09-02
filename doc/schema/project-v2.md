# GMP-ISE 项目文件 Schema v2

> 状态：Phase 0 冻结基线  
> 版本：`2.0.0`  
> 对应需求：REQ-010、REQ-011、REQ-012、REQ-013

---

## 1. 概述

`.gmp.yaml` 是 GMP-ISE 的项目持久化文件。Schema v2 在 v1 基础上增加：

- 显式 `schema_version` 字段（替代原 `version`）。
- 活动应用档案（`application_profile`）与求解单位合同（`unit_contract`）。
- 网格快照（`mesh_snapshot`）与 Physical Groups 清单。
- 输入快照与结果追溯字段。
- 扩展的模型树节点类型：Assembly、Physics、Constraints、Selections/Sets。

旧版项目文件仍可读；加载时按 v1 处理并迁移为 v2 内存模型，保存时统一输出为 v2。

---

## 2. 顶层结构

```yaml
schema_version: 2
saved_at: "2026-09-02T10:00:00Z"

application_profile:
  profile_id: "DamSafetyApp-opt"
  profile_version: "1.0.0"
  mapping_version: "1.0.0"
  solver_program: "DamSafetyApp-opt"

unit_contract:
  name: "SI"
  length: "m"
  force: "N"
  time: "s"
  mass: "kg"
  pressure: "Pa"
  temperature: "K"
  display_to_solver_factors:
    length: 0.001          # 显示 mm -> 求解 m
    force: 1.0
    pressure: 1000000.0   # 显示 MPa -> 求解 Pa

model:
  Parts: []
  Sketches: []
  Features: []
  Datums: []
  Materials: []
  Sections: []
  Assembly: []
  Physics: []
  Steps: []
  BC: []
  Loads: []
  Interactions: []
  Constraints: []
  Selections: []
  Functions: []
  Variables: []
  Outputs: []
  Mesh: []
  Jobs: []
  Results: []

mesh_snapshot:
  path: "mesh/case.msh"
  sha256: "..."
  physical_groups:
    - name: "concrete"
      dim: 3
      tags: [1]
      entity_count: 1
      element_count: 1000
    - name: "bottom"
      dim: 2
      tags: [2]
      entity_count: 1
      element_count: 100
  summary:
    mesh_dim: 3
    node_count: 1331
    element_count: 1000
    element_type: "TET4"
    quality_min: 0.15
    quality_max: 1.0
    quality_avg: 0.82

gmsh:
  # GmshPanel 设置，与 v1 兼容

moose:
  # MoosePanel 设置，与 v1 兼容
  # 新增：input_snapshots（历史快照列表）
  input_snapshots: []

viewer:
  # VtkViewer 设置，与 v1 兼容
```

---

## 3. 模型树节点条目

每个节点条目统一格式：

```yaml
- name: "Material-1"
  kind: "Materials"
  status: "ready"   # ready | incomplete | invalid | stale | disabled
  params:
    type: "ComputeIsotropicElasticityTensor"
    youngs_modulus: "30e9"
    poissons_ratio: "0.2"
```

### 3.1 新增节点类型

| 节点类型 | kind | 说明 |
|---|---|---|
| Assembly | `Assembly` | 装配实例与变换 |
| Instance | `Assembly` 子项 | 部件引用 + 平移/旋转/缩放 |
| Physics | `Physics` | 物理场/求解方程（如 solid_mechanics） |
| Constraint | `Constraints` | 耦合/约束/接触 |
| Selection/Set | `Selections` | 命名的节点/面/体集合 |
| Input Snapshot | `InputSnapshots` | 历史输入快照引用 |

### 3.2 状态字段

节点 `status` 用于表达其与上游依赖的关系：

- `ready`：配置完整且未过期。
- `incomplete`：缺少必需参数。
- `invalid`：参数校验失败。
- `stale`：上游对象已修改，当前节点结果（网格、`.i`、快照）可能失效。
- `disabled`：用户显式禁用或目标应用不支持。

---

## 4. 兼容性与迁移

### 4.1 读取规则

1. 若文件包含 `schema_version`，按该版本解析。
2. 若文件仅包含 `version: 1`，视为 v1，所有新增字段使用默认值：
   - `schema_version = 2`
   - `application_profile` 为空（需用户后续选择或保留默认值）。
   - `unit_contract` 为空。
   - `mesh_snapshot` 为空。
3. 未知 `schema_version`（>2）应拒绝加载并提示升级。

### 4.2 保存规则

保存时始终输出 `schema_version: 2`。

### 4.3 向后兼容字段

- `gmsh`、`moose`、`viewer` 块保持 v1 字段不变。
- `model` 下各根节点保持为序列，便于 yaml-cpp 直接读取。

---

## 5. 验证检查表

- [ ] 旧版 `.gmp.yaml` 可加载，且保存后带有 `schema_version: 2`。
- [ ] 新 schema 项目保存/重开后，应用档案、单位合同、网格快照字段完整恢复。
- [ ] 未知 schema_version 被拒绝并给出可读错误。
- [ ] 缺少新增字段时，加载不崩溃，使用安全默认值。
