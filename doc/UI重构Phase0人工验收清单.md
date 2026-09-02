# UI 重构 Phase 0 人工验收清单

> 版本：2026-09-02 v1
> 范围：Phase 0 数据合同与项目兼容性
> 自动测试：`ctest --test-dir build --output-on-failure`
> 验收结果：2026-09-02 用户确认 TEST-P0-M01～TEST-P0-M04 全部通过
> 准入结论：Phase 0 通过，可以正式进入 Phase 1。

## 1. 验收范围

- 覆盖：应用启动、v1 项目迁移、v2 嵌套字段和新增节点往返、未知版本拒绝、既有入口冒烟。
- 不覆盖：应用档案选择 UI、映射驱动表单、Physical Groups 自动提取、快照 UI/SimClient 提交；这些能力分别在 L-04、I-01、W-02/W-03、W-07 验收。
- 原则：不要直接修改 `tests/fixtures/` 中的基准文件，应复制到临时目录后操作。

## 2. 环境与准备

1. 在项目根目录执行：

   ```bash
   cmake -S . -B build
   cmake --build build --target gmp_ise gmp_ise_phase0_test -j4
   ctest --test-dir build --output-on-failure
   ```

2. 确认 CTest 显示 `100% tests passed`，且至少发现 `gmp_ise_phase0` 1 个测试。
3. 创建临时验收目录，将以下文件复制进去：
   - `tests/fixtures/project-v1-minimal.gmp.yaml`
   - `tests/fixtures/project-v2-contract.gmp.yaml`
   - `tests/fixtures/project-v3-unsupported.gmp.yaml`
4. 启动 `build/gmp_ise`。

## 3. 人工用例

### TEST-P0-M01 应用启动与模型根节点

- **关联需求**：REQ-010、REQ-011
- **前置条件**：应用首次启动或新建空项目。
- **步骤**：
  1. 启动应用，观察主窗口和左侧模型树。
  2. 依次查找 `Assembly`、`Physics`、`Constraints`、`Selections` 根节点。
  3. 展开、折叠上述节点，再切换一个现有模块页。
- **预期结果**：
  - [x] 应用启动无崩溃、无未处理异常弹窗。
  - [x] 4 个新增根节点均存在且可正常展开/折叠。
  - [x] Parts、Materials、Mesh、Jobs、Results 等原有根节点仍存在。
  - [x] 原有模块入口仍可打开，没有因新增节点失联。

### TEST-P0-M02 v1 项目迁移与重开

- **关联需求**：REQ-010
- **前置条件**：使用临时目录中的 `project-v1-minimal.gmp.yaml` 副本。
- **步骤**：
  1. 通过 File → Open 打开 v1 项目副本。
  2. 确认 Parts 下存在 `cube`，Materials 下存在 `steel`。
  3. 使用 Save As 保存为 `project-v1-migrated.gmp.yaml`。
  4. 关闭后重新打开迁移文件。
  5. 用文本编辑器检查迁移文件。
- **预期结果**：
  - [x] 打开、保存和重开均无报错。
  - [x] `cube`、`steel` 及材料参数未丢失。
  - [x] 文件包含 `schema_version: 2`。
  - [x] 文件包含 `application_profile`、`unit_contract`、`mesh_snapshot`，即使内容为空也不被省略。
  - [x] `mesh_snapshot` 使用 `summary` 子节点，不新增早期扁平摘要字段。

### TEST-P0-M03 v2 嵌套单位与新增节点无损往返

- **关联需求**：REQ-010、REQ-011、REQ-012、REQ-013
- **前置条件**：使用临时目录中的 `project-v2-contract.gmp.yaml` 副本。
- **步骤**：
  1. 打开 v2 合同项目副本。
  2. 检查树中以下对象：Assembly/`assembly-1`、Physics/`solid-mechanics`、Constraints/`coupling-1`、Selections/`bottom`。
  3. 使用 Save As 保存为 `project-v2-roundtrip.gmp.yaml`。
  4. 关闭后重新打开保存文件，再次检查上述对象。
  5. 用文本编辑器对照原文件检查关键字段。
- **预期结果**：
  - [x] 打开时不出现 `bad conversion` 或其他 YAML 异常。
  - [x] 4 个新增对象保存、重开后仍存在，名称和参数不丢失。
  - [x] `unit_contract.display_to_solver_factors` 仍为嵌套对象。
  - [x] `length: 0.001`、`pressure: 1000000.0` 等比例因子保持数值语义。
  - [x] `mesh_snapshot.summary`、Physical Group `concrete` 和 `bound_object_ids` 均保留。
  - [x] 节点 `status` 分别保持 `ready`、`incomplete`、`disabled` 等原值。

### TEST-P0-M04 未知 Schema 版本安全拒绝

- **关联需求**：REQ-010
- **前置条件**：先打开 TEST-P0-M03 的有效项目，并记住树中的 `concrete-cube`。
- **步骤**：
  1. 尝试打开 `project-v3-unsupported.gmp.yaml`。
  2. 关闭警告对话框。
  3. 检查当前项目和模型树。
- **预期结果**：
  - [x] 显示可读的“不支持 schema version 3”提示。
  - [x] 应用不崩溃。
  - [x] 不加载 `must-not-load` 节点。
  - [x] 原有效项目仍保持打开，`concrete-cube` 等数据不被清空。

## 4. 验收结果记录

| 用例 | 结果（通过/失败/阻塞） | 证据或截图路径 | 问题说明 |
|---|---|---|---|
| TEST-P0-M01 | 通过 | 用户于 2026-09-02 确认 | 无 |
| TEST-P0-M02 | 通过 | 用户于 2026-09-02 确认 | 无 |
| TEST-P0-M03 | 通过 | 用户于 2026-09-02 确认 | 无 |
| TEST-P0-M04 | 通过 | 用户于 2026-09-02 确认 | 无 |

## 5. 问题反馈格式

若任一项失败，请记录：用例编号、操作到第几步、实际现象、是否稳定复现、保存后的 YAML 或截图路径。不要在失败后继续覆盖同一个输出文件，以便保留现场。
