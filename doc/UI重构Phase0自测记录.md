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
| — | 项目文件 v2 读写兼容 | `ProjectSchema` 递归读写层；`MainWindow` 支持嵌套单位、新节点/status、规范/早期网格摘要 | ✅ 自动与人工验收通过 |
| — | 编译集成 | `gmp_ise_phase0_test` 已注册为 CTest `gmp_ise_phase0` | ✅ 完成 |

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

### 2.2 Phase 0 合同自动测试

运行专用测试程序：

```bash
ctest --test-dir build --output-on-failure
```

测试覆盖：
1. **ProjectSchema**：新增根节点、嵌套单位/列表、规范 `mesh_snapshot.summary` 和早期扁平摘要兼容。
2. **ApplicationProfileRegistry**：3 个档案字段/单位/映射版本校验，单个损坏档案隔离及错误报告。
3. **MooseMappingRegistry**：语义版本、block/object schema、排序引用和损坏注册表拒绝。
4. **PhysicalGroupManifest**：有效清单、空组、重复组、非法路径和 SHA-256 阻断。
5. **Snapshot v2**：完整文件复制与哈希、嵌套相对路径、缺失网格、路径穿越、不可变快照和 `.e` 显式角色。

**结果**：

```
Test project .../build
    Start 1: gmp_ise_phase0
1/1 Test #1: gmp_ise_phase0 ... Passed
100% tests passed out of 1
```

✅ **验收结论：Phase 0 自动合同测试和人工 UI 验收均通过，可以进入 Phase 1。**

---

## 3. 人工验收（进入 Phase 1 前完成）

人工 UI 验收已独立整理为 `doc/UI重构Phase0人工验收清单.md`，包含 4 个可执行用例及 3 份固定测试项目。2026-09-02，用户确认 4 项用例全部通过。应用档案选择、映射驱动表单、Physical Groups 自动同步和快照提交流程尚未接入当前 UI，已移动到实际实现它们的后续阶段，不再形成 Phase 0 的不可执行准入条件。

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
  - `include/gmp/ProjectSchema.h`
  - `src/ProjectSchema.cpp`
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
  - `tests/fixtures/project-v1-minimal.gmp.yaml`
  - `tests/fixtures/project-v2-contract.gmp.yaml`
  - `tests/fixtures/project-v3-unsupported.gmp.yaml`
  - `doc/UI重构Phase0人工验收清单.md`

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
