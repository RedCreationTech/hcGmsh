# Gmsh Physical Groups 语义契约

> 状态：Phase 0 冻结基线  
> 对应需求：REQ-012

---

## 1. 目标

明确 Gmsh 几何/网格中的 Physical Groups 如何稳定地成为 MOOSE 输入中的 `block`/`subdomain`/`boundary` 语义，避免在几何布尔、重建或装配后，因 entity tag 变化导致绑定失效。

---

## 2. 核心规则

### 2.1 名称优先原则

- **Physical Group 名称**是跨 Gmsh、项目模型、`.i` 和计算结果的稳定语义键。
- **Gmsh entity tag**仅作为瞬时实现信息，不持久化为绑定键。
- 所有引用 Physical Group 的地方（材料指派、BC、Load、Contact、Output、Selection）必须使用名称。

### 2.2 维度语义

| Physical Group 维度 | MOOSE 语义 | 典型用途 |
|---|---|---|
| 3（体） | `block` / `subdomain` | 材料/区域指派、Physics `block` 限制 |
| 2（面） | `boundary` | BC、Contact 主/从面、耦合面 |
| 1（线） | `boundary` / 选择集 | 线约束、线载荷、线输出 |
| 0（点） | `boundary` / 选择集 | 参考点、点约束、点后处理 |

### 2.3 唯一性规则

- 同一维度内 Physical Group 名称必须唯一。
- 不同维度允许同名（如体 `concrete` 和面 `concrete_top` 应使用不同名称，推荐显式区分）。
- 名称为空、纯空白或与保留关键字冲突时，应阻止生成 `.i`。

### 2.4 有效性规则

Physical Group 在以下情况视为失效，必须阻止 `.i` 进入“可提交”状态：

- 名称为空或仅空白。
- 名称在项目内重复（同维度）。
- 组内未包含任何实体（空组）。
- 声明维度与实际实体维度不一致。
- 几何/装配/布尔操作后，原 tag 已失效且未重新同步。
- 被 `.i` 中引用的组在最新 `.msh` 中不存在。

---

## 3. 持久化结构

网格生成后，项目记录 `mesh_snapshot`，其中 `physical_groups` 为：

```yaml
mesh_snapshot:
  path: "mesh/case.msh"
  sha256: "..."
  physical_groups:
    - name: "concrete"
      dim: 3
      tags: [1]
      entity_count: 1
      element_count: 1000
    - name: "steel_plate"
      dim: 3
      tags: [2]
      entity_count: 1
      element_count: 500
    - name: "bottom"
      dim: 2
      tags: [3]
      entity_count: 1
      element_count: 100
    - name: "contact_master"
      dim: 2
      tags: [4]
      entity_count: 1
      element_count: 100
    - name: "contact_slave"
      dim: 2
      tags: [5]
      entity_count: 1
      element_count: 100
  summary:
    mesh_dim: 3
    node_count: 1331
    element_count: 1500
    element_type: "TET4"
    quality_min: 0.15
    quality_max: 1.0
    quality_avg: 0.82
```

### 3.1 字段说明

| 字段 | 类型 | 说明 |
|---|---|---|
| `name` | string | Physical Group 名称，项目内唯一键 |
| `dim` | int | 维度 0/1/2/3 |
| `tags` | [int] | 生成本次 `.msh` 时对应的 Gmsh entity tags（仅作审计） |
| `entity_count` | int | 该组包含的 Gmsh 实体数量 |
| `element_count` | int | 该组包含的单元数量 |

### 3.2 摘要字段

| 字段 | 类型 | 说明 |
|---|---|---|
| `mesh_dim` | int | 网格维度 1/2/3 |
| `node_count` | int | 节点总数 |
| `element_count` | int | 单元总数 |
| `element_type` | string | 主导单元类型，如 TET4/HEX8/TRI3 |
| `quality_min/max/avg` | double | 网格质量指标 |

---

## 4. 与 `.i` 的绑定

- `.i` 的 `[Mesh]` 使用 `type = FileMesh`。
- `file` 参数指向案例包根目录下的 `.msh` 相对路径。
- `block`/`boundary` 参数使用 Physical Group 名称，不使用 tag。
- 生成器在校验阶段将名称解析为 Gmsh 内部 tag；若名称不存在，报错并定位到引用节点。

---

## 5. 变更传播

- 当几何、装配、Physical Group 名称或网格参数修改后，现有 `mesh_snapshot` 标记为 `stale`。
- 标记为 `stale` 后，依赖该网格的 `.i`、Input Snapshot、Job 均标记为 `stale`，直到重新生成。
- 用户提交作业前必须重新生成网格和 `.i`。

---

## 6. 验证检查表

- [ ] `.msh` 中的 Physical Group 名称可正确显示在项目模型树 Mesh 节点下。
- [ ] 体组名称可在 Material/Section 表单的 `block` 选择中使用。
- [ ] 面组名称可在 BC/Load/Contact/Output 表单的 `boundary` 选择中使用。
- [ ] 重命名 Physical Group 后未重新生成网格，引用该组的表单显示失效提示。
- [ ] `.i` 中 `block`/`boundary` 使用名称，且能通过目标 MOOSE 应用检查。
