# 输入快照（Input Snapshot）与 manifest.json 合同 v2

> 状态：Phase 0 冻结基线  
> 版本：`2.0.0`  
> 对应需求：REQ-016、REQ-017

---

## 1. 目标

把前处理结果冻结为作业系统可以完整上传、校验和重跑的输入快照。快照一经提交即不可被后续 UI 编辑原地修改；再次生成形成新快照或新版本。

---

## 2. 快照目录结构

```
case-20260902-100000/
├── manifest.json
├── case.i              # 主输入文件
├── case.msh            # Gmsh 网格
├── extra/
│   └── material_data.csv
└── (可选)
    ├── project-snapshot.gmp.yaml
    ├── generation-report.json
    └── physical-groups.json
```

所有相对文件引用在快照根目录下可解析。

- 主 `.i` 和所有引用文件必须使用快照根目录内的相对路径；绝对路径、空路径和 `..` 穿越直接拒绝。
- 保留引用文件的相对子目录结构，不把不同目录文件压平到同一 basename。
- 目标快照目录已存在时拒绝写入；再次生成必须使用新的版本目录，旧快照内容保持不变。
- 任一必需引用文件缺失时整个快照生成失败，不得产出“成功但 `mesh_files` 为空”的 manifest。

---

## 3. manifest.json 字段

```json
{
  "contract": "CONTRACT-JOB",
  "contract_version": "2.0.0",
  "schema_version": "2.0.0",
  "case_name": "concrete-steel-contact",
  "case_id": "case-20260902-100000",

  "application_profile": {
    "profile_id": "DamSafetyApp-opt",
    "profile_version": "1.0.0",
    "mapping_version": "1.0.0",
    "solver_program": "DamSafetyApp-opt",
    "support_level": "production"
  },

  "unit_contract": {
    "name": "SI",
    "length": "m",
    "force": "N",
    "time": "s",
    "mass": "kg",
    "pressure": "Pa",
    "temperature": "K",
    "display_to_solver_factors": {
      "length": 0.001,
      "force": 1.0,
      "pressure": 1000000.0
    }
  },

  "input_mode": "structured",
  "generator_version": "gmp-ise 2026.09",
  "base_model_hash": "sha256-of-project-model",
  "extension_hash": "sha256-of-custom-objects",
  "final_input_hash": "sha256-of-case.i",
  "created_at": "2026-09-02T10:00:00Z",

  "input_snapshot": {
    "input_file": "case.i",
    "input_sha256": "...",
    "input_role": "input_config",
    "mesh_files": [
      { "name": "case.msh", "sha256": "...", "role": "input_mesh" }
    ],
    "extra_files": [
      { "name": "extra/material_data.csv", "sha256": "...", "role": "extra" }
    ]
  },

  "traceability": {
    "project_path": "/path/to/project.gmp.yaml",
    "project_version": "sha256-of-project-file",
    "mesh_snapshot_sha256": "...",
    "physical_groups": [
      { "name": "concrete", "dim": 3 },
      { "name": "bottom", "dim": 2 }
    ]
  },

  "recommended_command": "DamSafetyApp-opt -i case.i --check-input",
  "notes": ""
}
```

### 3.1 字段说明

| 字段 | 类型 | 说明 |
|---|---|---|
| `contract` | string | 固定 `CONTRACT-JOB` |
| `contract_version` | string | 合同版本，当前 `2.0.0` |
| `schema_version` | string | 本 schema 版本 `2.0.0` |
| `case_name` | string | 案例名 |
| `case_id` | string | 唯一案例标识 |
| `application_profile` | object | 应用档案快照 |
| `unit_contract` | object | 求解单位合同 |
| `input_mode` | string | `structured` / `expert` / `manual` |
| `generator_version` | string | 生成器版本 |
| `base_model_hash` | string | 结构化模型哈希 |
| `extension_hash` | string | 专家扩展层哈希（无则为空） |
| `final_input_hash` | string | 最终 `.i` 哈希 |
| `created_at` | string | ISO 8601 时间 |
| `input_snapshot` | object | 输入文件清单与 SHA-256 |
| `traceability` | object | 可追溯信息 |
| `recommended_command` | string | 推荐运行命令 |

### 3.2 文件角色（role）

| role | 说明 |
|---|---|
| `input_mesh` | 输入网格 `.msh`/`.e` |
| `input_config` | 主输入 `.i` |
| `extra` | 附件（材料表、函数数据等） |
| `result` | 计算结果 `.e`/CSV |
| `initial_state` | 初始状态/多阶段初始化 |
| `restart_data` | 重启/checkpoint 数据 |
| `project_snapshot` | 只读项目快照 |
| `generation_report` | 生成报告 |
| `physical_groups` | Physical Groups 清单 |

`.e` 默认视为计算结果，不按扩展名自动作为普通输入。只有显式导入 Exodus 网格或多阶段初始化/重启场景，且 manifest 明确标记为 `input_mesh`、`initial_state` 或 `restart_data` 时，才允许进入输入快照。

---

## 4. 与 LIMS Facade/SimClient 提交合同的兼容

SimClient 将 `manifest.json` 转换为提交侧 manifest：

```json
{
  "project_id": "...",
  "case_name": "...",
  "input_file": "case.i",
  "input_sha256": "...",
  "mesh_files": ["case.msh"],
  "extra_files": ["extra/material_data.csv"],
  "command": "DamSafetyApp-opt -i case.i"
}
```

- 新增字段（如 `application_profile`、`unit_contract`）向后兼容，原提交侧可忽略。
- 命令参数使用白名单，不得将任意 UI 文本拼接为执行命令。

---

## 5. 不可变性

- 快照目录一旦创建，禁止 UI 直接原地修改其中文件。
- 用户修改项目后，执行“生成快照”将创建新的 `case-<timestamp>/` 目录。
- 重新生成结构化 `.i` 时，若存在手工模式副本，不覆盖，而是生成新文件并展示差异。

---

## 6. 验证检查表

- [ ] 快照包含 `manifest.json`、`.i`、`.msh` 和全部引用的 extra files。
- [ ] 所有文件均有 SHA-256，且与实际文件一致。
- [ ] `.i` 中相对路径在快照根目录下可解析，无建模机绝对路径。
- [ ] `application_profile`、`unit_contract`、`traceability` 字段完整。
- [ ] 复制快照到干净目录后，可按 `recommended_command` 运行（本地有 MOOSE 时）。
- [ ] 项目修改后再次生成快照产生新版本，旧快照内容不变。
