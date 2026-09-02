# UI 重构 Phase 0 自测记录

> 日期：2026-09-02  
> 对应任务清单：Phase 0（数据合同与架构冻结）  
> 对应需求：REQ-010、REQ-011、REQ-012、REQ-013、REQ-016、REQ-017

---

## 1. 本次交付物清单

| 编号 | 任务 | 交付物 | 状态 |
|---|---|---|---|
| F-01 | 冻结项目模型 YAML schema v2 | `doc/schema/project-v2.md` | ✅ 完成 |
| F-02 | 设计应用档案注册表 | `include/gmp/ApplicationProfile.h`、`src/ApplicationProfile.cpp`、3 个示例 profile JSON | ✅ 完成 |
| F-03 | 设计版本化 MOOSE 映射注册表 | `templates/moose/mapping-v1.json`、`include/gmp/MooseMappingRegistry.h`、`src/MooseMappingRegistry.cpp` | ✅ 完成 |
| F-04 | 冻结 Physical Groups 语义契约 | `doc/contracts/physical-groups.md`、`include/gmp/PhysicalGroupManifest.h`、`src/PhysicalGroupManifest.cpp` | ✅ 完成 |
| F-05 | 设计输入快照与 manifest.json 合同 v2 | `doc/contracts/input-snapshot-v2.md`、更新 `MooseSnapshot.h/cpp` 新增 `export_job_snapshot_v2` | ✅ 完成 |
| — | 项目文件 v2 读写兼容 | 更新 `MainWindow.h/cpp` 支持 `schema_version` / `application_profile` / `unit_contract` / `mesh_snapshot` / `input_snapshots` | ✅ 完成 |
| — | 编译集成 | 更新 `CMakeLists.txt` 加入新源文件与 `GMP_PROFILE_DIR`；新增 `gmp_ise_phase0_test` 测试目标 | ✅ 完成 |

---

## 2. 自测方法

### 2.1 编译测试

```bash
mkdir -p build && cd build
cmake ..
cmake --build . --target gmp_ise -j4
cmake --build . --target gmp_ise_phase0_test -j4
```

**结果**：
- `gmp_ise` 主程序成功链接，无编译错误。
- `gmp_ise_phase0_test` 测试程序成功链接，无编译错误。

### 2.2 Phase 0 单元/集成自测

运行专用测试程序：

```bash
./build/gmp_ise_phase0_test
```

测试覆盖：
1. **ApplicationProfileRegistry**：从 `templates/moose/profiles/` 加载 3 个 profile，校验 production 状态与 block 支持。
2. **MooseMappingRegistry**：加载 `templates/moose/mapping-v1.json`，校验版本、block 存在性、对象类型存在性、输出顺序。
3. **PhysicalGroupManifest**：构造有效清单、校验通过；构造重复名称清单、校验报错。
4. **Snapshot v2**：调用 `export_job_snapshot_v2()` 生成临时快照，验证 manifest 包含 `contract_version=2.0.0`、`application_profile`、`traceability`、`unit_contract` 等字段。

**结果**：

```
Loaded 3 profiles
  DamSafetyApp-opt: DamSafetyApp（优化版） (production)
  blackbear-opt: BlackBear（优化版） (production)
  combined-opt: MOOSE Combined（优化版） (prototype)
Mapping registry version: 1.0.0
Generated v2 manifest:
{
    "application_profile": { ... },
    "base_model_hash": "abc123",
    "case_id": "test-001",
    "case_name": "test-case",
    "contract": "CONTRACT-JOB",
    "contract_version": "2.0.0",
    "created_at": "...",
    "extension_hash": "",
    "final_input_hash": "...",
    "generator_version": "gmp-ise-test",
    "input_mode": "structured",
    "input_snapshot": { ... },
    "recommended_command": "DamSafetyApp-opt -i case.i --check-input",
    "schema_version": "2.0.0",
    "traceability": { ... },
    "unit_contract": { ... }
}

Phase 0 self-tests PASSED
```

✅ **自测结论：Phase 0 基础合同与注册表实现通过自测。**

---

## 3. 人工验证步骤（必须在进入 Phase 1 前完成）

以下步骤建议在完整 GUI 环境或命令行下由测试人员执行。

### 3.1 项目文件 v2 读写与兼容

**步骤**：
1. 启动 `build/gmp_ise`。
2. 打开一个旧版 `.gmp.yaml`（仅含 `version: 1`）。
3. 执行一次保存。
4. 用文本编辑器检查保存后的 YAML。

**通过标准**：
- [ ] 文件包含 `schema_version: 2`。
- [ ] 文件包含 `application_profile`（即使为空结构也允许）、`unit_contract`、`mesh_snapshot` 字段。
- [ ] 旧模型数据（Parts、Materials 等）完整保留。
- [ ] 保存后重新打开不报错，树结构与参数一致。

### 3.2 应用档案注册表

**步骤**：
1. 启动 `build/gmp_ise`。
2. 在项目设置或工作上下文条中查看“应用选择”入口（如已接入 UI）。
3. 切换应用到 `blackbear-opt` 并保存项目。
4. 检查项目 YAML 中 `application_profile.profile_id` 是否变为 `blackbear-opt`。

**通过标准**：
- [ ] 至少可见 `DamSafetyApp-opt`、`blackbear-opt`、`combined-opt` 三个选项。
- [ ] `combined-opt` 明确显示为 prototype 状态。
- [ ] 切换应用后保存，YAML 字段同步更新。
- [ ] 损坏或删除一个 profile JSON 后启动，应用不崩溃，状态栏/日志给出提示。

### 3.3 映射注册表

**步骤**：
1. 编辑 `templates/moose/mapping-v1.json`，修改某个枚举值（例如 `Executioner/Transient/solve_type` 增加一个选项）。
2. 重新启动应用（或热加载注册表）。
3. 打开对应表单查看下拉选项是否同步。

**通过标准**：
- [ ] 修改注册表后，表单/校验器能读取到新值，无需改代码。
- [ ] 损坏注册表文件后启动，应用给出明确错误或不使用相关生成能力。

### 3.4 Physical Groups 契约

**步骤**：
1. 在 GmshPanel 中导入 `.geo` 并生成 `.msh`。
2. 创建 Physical Volume 和 Physical Surface，确保名称唯一。
3. 保存项目，检查 `mesh_snapshot.physical_groups` 字段。
4. 重命名一个 Physical Group 后未重新生成网格，尝试“生成 MOOSE 输入”。

**通过标准**：
- [ ] `mesh_snapshot` 记录正确的名称、维度、tag、单元数。
- [ ] 体组名称可在 Material/Section 表单中选择。
- [ ] 面组名称可在 BC/Load/Contact/Output 表单中选择。
- [ ] 重命名后未重新生成网格，系统提示引用失效并定位问题对象。

### 3.5 输入快照 v2

**步骤**：
1. 准备一个合法案例，执行“生成快照”。
2. 检查快照目录中的 `manifest.json`。
3. 将快照目录复制到另一路径，按 `recommended_command` 运行（本地有 MOOSE 时）。
4. 修改项目后再次生成快照。

**通过标准**：
- [ ] `manifest.json` 包含 `contract_version: 2.0.0`、`application_profile`、`unit_contract`、`traceability`。
- [ ] `.i`、`.msh` 和附件均有 SHA-256 且与实际文件一致。
- [ ] `.i` 中相对路径在快照根目录下可解析，无建模机绝对路径。
- [ ] 复制到干净目录后可按推荐命令运行（本地有 MOOSE 时）。
- [ ] 再次生成快照产生新目录，旧快照内容不变。

---

## 4. 已知限制与待办

| 问题 | 说明 | 计划处理阶段 |
|---|---|---|
| UI 尚未接入应用档案选择 | 注册表已可加载，但顶部工作上下文条和项目设置界面尚未替换 | Phase 1 |
| UI 尚未接入 Physical Group 清单 | `PhysicalGroupManifest` 数据结构已就绪，但 GmshPanel 生成 `.msh` 后尚未自动填充全部摘要字段 | Phase 1/Phase 4（W-02） |
| 映射注册表仅覆盖 Phase 0 基线 blocks | Contact、Constraint 等高级对象类型仅列出占位 schema，参数细节需在 Phase 4 补齐 | Phase 4（W-03） |
| 项目 YAML 中 mesh_snapshot 手工同步 | 当前由 GmshPanel/调用方负责填充；后续应在网格生成成功后自动写入 | Phase 4（W-02） |
| 输入快照 v2 尚未被 SimClient 默认使用 | `export_job_snapshot_v2()` 已提供，但 MoosePanel 提交流程仍调用 v1；需显式切换 | Phase 4（W-07） |

---

## 5. 相关文件路径

- 合同文档：
  - `doc/schema/project-v2.md`
  - `doc/contracts/physical-groups.md`
  - `doc/contracts/input-snapshot-v2.md`
- 注册表与数据结构：
  - `include/gmp/ApplicationProfile.h`
  - `src/ApplicationProfile.cpp`
  - `include/gmp/MooseMappingRegistry.h`
  - `src/MooseMappingRegistry.cpp`
  - `include/gmp/PhysicalGroupManifest.h`
  - `src/PhysicalGroupManifest.cpp`
- 模板/档案：
  - `templates/moose/mapping-v1.json`
  - `templates/moose/profiles/DamSafetyApp-opt.json`
  - `templates/moose/profiles/blackbear-opt.json`
  - `templates/moose/profiles/combined-opt.json`
- 快照/项目读写：
  - `include/gmp/MooseSnapshot.h`
  - `src/MooseSnapshot.cpp`
  - `include/gmp/MainWindow.h`
  - `src/MainWindow.cpp`
- 测试程序：
  - `tests/test_phase0.cpp`
  - `build/gmp_ise_phase0_test`

---

## 6. 附录：旧版项目兼容说明

旧版 `.gmp.yaml` 的顶层字段 `version: 1` 仍被 `load_project()` 识别，加载后内存统一按 schema v2 维护。保存后输出 `schema_version: 2`，同时保留 `version: 2` 字段以兼容只读取 `version` 的外部工具。

测试人员应至少准备一个 v1 项目文件做回归验证：

```yaml
version: 1
saved_at: "2026-08-01T00:00:00Z"
model:
  Parts:
    - name: cube
      kind: Parts
      params: {}
  Materials:
    - name: steel
      kind: Materials
      params:
        type: ComputeIsotropicElasticityTensor
        youngs_modulus: "200e9"
        poissons_ratio: "0.3"
```

加载并保存后，应出现 `schema_version: 2` 以及空的 `application_profile`/`unit_contract`/`mesh_snapshot` 字段，且 `Parts`、`Materials` 数据完整保留。
