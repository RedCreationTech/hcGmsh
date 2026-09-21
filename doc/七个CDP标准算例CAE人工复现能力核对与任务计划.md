# 七个 CDP 标准算例 CAE 人工组成复现能力核对与任务计划

> 编制日期：2026-09-20
> 核对对象：GMP-ISE `main@631a77505b1d`
> 标准算例来源：`/Users/a123/Desktop/code/damASR/docs/examples/`
> 主目标：**用户从空项目开始，通过 CAE 的结构化 UI 逐项建立与标准 `.i` 语义等价的有限元模型，并完成求解与后处理。**
> 辅助目标：提供“导入已有 `.i`/算例包”入口，用于对照、复算和兼容，不代替结构化建模能力。

---

## 0. 修正后的范围与验收口径

### 0.1 主流程：人工组成 `.i`

每个标准算例必须能够仅通过 CAE UI 完成：

1. 新建项目并选择目标 DamSafetyApp 档案；
2. 导入或创建 1000 个 HEX8 单元网格，识别体组 `concrete_cube__concrete` 和边界 `bottom/top`；
3. 新建 CDP 材料，填写弹性、塑性、损伤、恢复、积分控制参数及四张材料 CSV；
4. 新建 Section，并把材料指派到 `concrete_cube__concrete`；
5. 新建 QuasiStatic/CDPQuasiStatic Physics，配置位移变量、应变模式、体积锁定修正和输出变量；
6. 新建 ParsedFunction 或 PiecewiseLinear 加载历程；
7. 新建底面/顶面位移边界条件；
8. 新建 Transient Step、IterationAdaptiveDT 和 SMP 预处理；
9. 新建场输出、历史输出、Times、CSV 和 Exodus 输出；
10. 由 Model Tree 同步生成完整 `.i`，与冻结标准逐块比较；
11. 经 UI 校验、快照、远端提交、制品下载和 Results 回放形成闭环。

验收时允许对象名称或等价数值格式不同，但对象类型、参数、引用、边界、时间历程和输出语义必须一致。不得通过粘贴整份 `.i`、Expert Custom Blocks 或直接修改生成文本来补齐缺失的正式 UI 能力。

### 0.2 辅助流程：导入已有 `.i`/算例包

“导入 MOOSE 算例”仍作为标准入口，但用途限定为：

- 查看和复算已有模型；
- 给结构化生成结果提供逐块对照；
- 验证快照打包、远端兼容性和历史算例可回放性；
- 对尚未结构化的专家诊断对象提供临时只读兼容。

导入成功不代表 CAE 已具备对应的人工建模能力，也不能计入本里程碑的“结构化复现通过”。

### 0.3 四级状态

| 状态 | 含义 |
|---|---|
| A：表单可表达 | UI 有对应对象和字段，可保存重开 |
| B：结构化生成等价 | Model Tree 可生成与冻结 `.i` 语义等价的全部块 |
| C：目标环境可运行 | 生成输入通过锁定版本预检并求解成功 |
| D：人工闭环通过 | 用户按操作文档完成建模、提交和结果核对 |

只有达到 D 才记为“该标准算例已由 CAE UI 人工复现”。

## 1. 七个算例的共同骨架

### 1.1 输入基线

7 个算例共用：

- 网格 `uniaxial_compression_mesh.e`；SHA-256 `51d296cd1e43d8bb47e2984f49b9ca8aef29db47caf9bb34dbe93974a212f03e`；
- 3D、1331 节点、1000 个 HEX8、体组 `concrete_cube__concrete`；
- node set `bottom/top`，各 121 节点；
- `compression_hardening.csv`、`compression_damage.csv`、`tension_stiffening.csv`、`tension_damage.csv`；
- CDP 材料参数、8 个 CDP AuxVariables/AuxKernels、SMP、IterationAdaptiveDT、CSV/Exodus 同步输出。

### 1.2 `.i` 块到 CAE 人工操作的映射

| 冻结 `.i` 组成 | 预期 CAE 操作 | 当前状态 | 当前差距 |
|---|---|---|---|
| `[Mesh/file]` | Mesh → 导入 Exodus 输入网格 | V01 达 A/B | 已提取体组、边界、维度、节点/单元计数、HEX8 和 SHA-256，并进入正式 manifest |
| `[GlobalParams]` + `[Physics]` | Physics → 新建 Solid Mechanics | V01/V02/R02/G33 达 A | `CDPQuasiStatic` 尚未由当前档案开放 |
| `[Materials]` 三件套 | Materials → 新建 CDP 材料 | A 具备 | 生成器可自动组成 elasticity/stress/CDP update；高级积分参数主要在高级参数表查看 |
| Section/block | Sections → 材料指派 | V01 达 A/B | `concrete_cube__concrete` 已作为 3D 体组候选，可由正式 UI 指派 |
| `[Functions]` | Functions → Parsed/PiecewiseLinear | A 具备 | 7 例加载曲线均可表达 |
| `[BCs]` | BC → Dirichlet/FunctionDirichlet | A 具备 | `top/bottom` 可选择；依赖完整网格 manifest 才能通过工作流校验 |
| `[Executioner]` | Steps → Transient | A 具备 | 现有表单覆盖冻结参数 |
| `[TimeStepper]` | Steps → IterationAdaptiveDT | A 具备 | 现有表单覆盖冻结参数 |
| `[Preconditioning/smp]` | Steps → SMP | A 具备 | 现有表单覆盖冻结参数 |
| `[AuxVariables]/[AuxKernels]` | Outputs → CDP 场输出 | A/B 基本具备 | 8 个共同诊断量均可生成 |
| `[Postprocessors]` | Outputs → 历史输出请求 | V01 达 A/B | 已增加最小 `cdp_uniaxial_z` 预设，逐项生成 V01 的 13 个冻结历史量；其他算例的差异化输出仍待后续扩展 |
| `[Times]` + `[Outputs]` | Outputs → Times/Exodus/CSV | A/B 具备 | 可配置文件名、0.01 s 间隔和同步输出 |
| Predictor/Checkpoint/Problem/诊断输出 | 专用高级对象 | 缺失 | G32 Predictor 与 R01 专用诊断尚无结构化 UI |

### 1.3 冻结输入与结果身份

| 算例 | 主输入（相对 `damASR/docs/examples/`） | 预期主结果名 | 冻结 DamSafetyApp |
|---|---|---|---|
| V01 | `cdp-v01-single-tension/input/tpl-cdpc-tension-single-recheck.i` | `uniaxial_tension_single.csv/.e` | `f99f18d1e24e5168ef6b88857d06b8c01f0c0fee` |
| V02 | `cdp-v02-single-compression/input/tpl-cdpc-compression-single-recheck.i` | `uniaxial_compression_single.csv/.e` | `f99f18d1e24e5168ef6b88857d06b8c01f0c0fee` |
| R01 | `cdp-r01-tension-reloading/input/tpl-cdpc-tension-reloading.i` | `uniaxial_tension_reloading.csv/.e` + 诊断制品 | `8e0ddf5c165bbae1e83b3b4f66e284bc36d3d7c3` |
| R02 | `cdp-r02-compression-reloading/input/tpl-cdpc-compression-reloading.i` | `uniaxial_compression_reloading.csv/.e` | `f99f18d1e24e5168ef6b88857d06b8c01f0c0fee` |
| G32 | `cdp-g32-tension-compression/input/tpl-cdpc-tension-compression.i` | `uniaxial_tension_compression.csv/.e` | `8e0ddf5c165bbae1e83b3b4f66e284bc36d3d7c3` |
| G33 | `cdp-g33-compression-tension/input/tpl-cdpc-compression-tension.i` | `uniaxial_compression_tension.csv/.e` | `8e0ddf5c165bbae1e83b3b4f66e284bc36d3d7c3` |
| G34 | `cdp-g34-uniaxial-shear/input/tpl-cdpc-uniaxial-shear.i` | `uniaxial_shear.csv/.e` | `8e0ddf5c165bbae1e83b3b4f66e284bc36d3d7c3` |

MOOSE 版本统一锁定为 `4bce02d91b56c7ed845a5747df4d24f415592504`。结构化生成验收必须记录实际运行身份，不把不同 release 的结果混为同一基线。

## 2. 各算例需要用户在 UI 中组成的差异

| 算例 | Physics | 加载函数 | 约束差异 | Step/输出差异 | 当前结构化覆盖 |
|---|---|---|---|---|---|
| V01 单次拉伸 | QuasiStatic | ParsedFunction：`2.5e-05*t` | bottom Z；top X/Y 固定，top Z 加载 | 1 s；`nl_abs_tol=1e-8`；16 项 Physics 输出 | **已达到 B，待用户执行 C/D** |
| V02 单次压缩 | QuasiStatic | ParsedFunction：`-0.0025*t` | 同 V01 | 1 s；其余同 V01 | 高，未达到 B |
| R02 压缩卸载再加载 | QuasiStatic | PiecewiseLinear：`0 1 2 3` / `0 -0.0025 0 -0.0025` | 同 V01 | 3 s；其余同 V01 | 高，未达到 B |
| G33 压—拉 | QuasiStatic | PiecewiseLinear：`0 1 2 3` / `0 -0.0025 0 0.0005` | 同 V01 | 3 s；增加 3 个主应变输出 | 高，未达到 B |
| G32 拉—压 | CDPQuasiStatic | PiecewiseLinear：`0 1 2 3` / `0 2.5e-05 0 -0.0025` | bottom XYZ；top X/Y 固定，top Z 加载 | 3 s；`nl_abs_tol=1e-12`；主应变输出；Predictor | 中，Physics/Predictor 缺失 |
| G34 单轴剪切 | CDPQuasiStatic | PiecewiseLinear：`0 1` / `0 2.5e-04` | bottom XYZ；top Y/Z 固定，top X 加载 | 1 s；X/Z 两个位移平均和 X/Y/Z 反力 | 中，Physics/多位移历史缺失 |
| R01 拉伸卸载再加载 | CDPQuasiStatic | PiecewiseLinear：`0 1 2 3` / `0 2.5e-05 0 2.5e-05` | 与 G32 相同 | 3 s；Predictor、Checkpoint、TrialProblem、Probe、AcceptedState | 低，专家诊断对象缺失 |

## 3. 当前结论

### 3.1 已经具备的人工建模能力

现有 CAE 已经可以通过结构化 UI 完成七例共同骨架的大部分组成：

- CDP 材料及四张 CSV；
- Section/Physics/Function/BC/Step/Outputs 的对象表单；
- ParsedFunction 与 PiecewiseLinear；
- Transient、IterationAdaptiveDT、SMP；
- QuasiStatic、8 个 CDP 场变量、CSV/Exodus/Times；
- V01/V02/R02/G33 所需的主要加载和求解参数。

这说明正确的下一步不是优先建设 `.i` 黑盒透传，而是补齐现有结构化链路的少量共用缺口，先让 V01 达到 B/C/D。

### 3.2 当前首例状态

V01 的三个直接结构化阻塞已关闭：

1. Exodus 导入会建立 `concrete_cube__concrete` 体组及 `bottom/top` 边界，并持久化完整 manifest；
2. Outputs 的 `cdp_uniaxial_z` 预设逐项生成 V01 冻结的 13 个 Postprocessor；
3. `cdp_v01_structured_reproduction_contract` 已从空模型树组装 V01，完成同步和本地工作流预检。

因此当前状态为：

> **V01 已达到 B（结构化生成等价），人工操作文档已提供；目标环境运行 C 和用户人工闭环 D 待执行，因此计数仍为 0/7 达到 D。**

## 4. 修正后的开发任务顺序

### TASK-STDCAE-010 冻结七例结构化等价清单

- **目标**：把每个冻结 `.i` 拆成可追溯的 UI 对象/字段清单，作为生成比较真源。
- **最小实现**：记录块、type、参数、引用、文件、允许的名称/格式差异；不建设通用 `.i` AST。
- **验收**：`TEST-STDCAE-01`；七例均能指出每一块由哪个 UI 对象生成，不能映射的块明确进入后续任务。

### TASK-STDCAE-020 补齐 Exodus 输入网格 manifest

- **目标**：导入共同 `.e` 后，直接得到可供 Section/Physics/BC 使用的网格集合。
- **工作**：读取并持久化维度、节点/单元数、HEX8、element block、node/side set、集合计数和 SHA-256；体组 `concrete_cube__concrete` 进入 volume groups，`bottom/top` 进入 boundary groups。
- **验收**：`TEST-STDCAE-02`；用户无需伪造 Gmsh Physical Group 即可完成材料指派和边界选择，保存重开不丢失。
- **状态**：✅ 2026-09-20 完成；V01 fixture 定向巡览验证 3D、1331 节点、1000 HEX8、1 个体组、2 个边界和网格 SHA-256。

### TASK-STDCAE-030 补齐标准算例输出请求

- **目标**：通过 Outputs UI 组成冻结算例需要的全部 Aux、Postprocessor、Times 和文件输出。
- **最小实现**：先为首例增加 `cdp_uniaxial_z` 历史输出预设，复用现有 NodalSum、AverageNodalVariableValue、ElementExtremeValue 生成逻辑；其他算例出现新差异时再扩展，不预建通用请求设计器。
- **不做**：不建设任意 MOOSE Postprocessor IDE。
- **验收**：`TEST-STDCAE-03`；V01 的顶/底反力、顶面 X/Y 反力、Z 位移平均和各自极值逐项等价；G34 的 X/Z 位移平均留到 TASK-STDCAE-070。
- **状态**：✅ V01 最小范围完成；13 个历史量已有生成器单测与真实 UI 定向巡览。

### TASK-STDCAE-040 V01 结构化 UI 首例准出

- **目标**：从空项目开始，经 UI 组成 V01 全部 `.i` 内容。
- **流程**：导入网格 → CDP 材料 → Section → QuasiStatic → ParsedFunction → 4 个 BC → Step → Outputs → 同步输入 → 语义比较 → 预检 → Job → Results。
- **产出**：创建 `doc/人工算例操作说明文档.md`，写入 V01 的逐次点击、字段值、预期生成块、结果核对和验收记录。
- **验收**：`TEST-STDCAE-04`；生成输入达到 B，在锁定版本达到 C，用户人工执行后达到 D。
- **状态**：🟡 B 已完成；C/D 待用户按操作文档验证，不自动占用远端计算节点。

### TASK-STDCAE-050 V02/R02/G33 结构化复现

- **目标**：复用 V01 项目骨架，仅通过 UI 修改加载函数、时间和输出项完成三例。
- **顺序**：V02 → R02 → G33。
- **验收**：`TEST-STDCAE-05～07`；每例生成输入逐块等价并完成真实 Job；通过一例就在人工操作文档追加一章。

### TASK-STDCAE-060 CDPQuasiStatic 与 Predictor

- **目标**：补齐 G32/R01/G34 的正式 Physics 选择，并为需要的算例增加 Predictor。
- **工作**：在锁定 DamSafetyApp profile/mapping 中开放 `CDPQuasiStatic`；为 `CDPAffineLoadPredictor` 增加仅含冻结算例字段的最小表单/对象。
- **验收**：`TEST-STDCAE-08`；不得用 QuasiStatic 或手写 Custom Block 静默替代。

### TASK-STDCAE-070 G32/G34 结构化复现

- **依赖**：TASK-STDCAE-060。
- **工作**：分别组成拉—压路径与 X 向剪切路径；G34 验证 X/Z 位移和三向反力输出。
- **验收**：`TEST-STDCAE-09/10`；生成、求解和后处理均与冻结语义一致。

### TASK-STDCAE-080 R01 专用诊断对象

- **目标**：结构化组成 R01 的 Predictor、Checkpoint、AssemblyProbe、TrialProblem 和 AcceptedStateOutput。
- **最小策略**：普通 CAE 对象用正式表单；仅服务该诊断算例的对象使用一个“R01 诊断配置”受控表单，不拆成五套通用设计器。
- **验收**：`TEST-STDCAE-11`；所有诊断参数可在 UI 设置、保存重开并生成，不粘贴输入片段。

### TASK-STDCAE-090 导入已有 `.i`/算例包

- **优先级**：P1 辅助能力，不阻塞 V01 结构化首例。
- **目标**：提供查看、对照、复算已有算例的标准入口。
- **工作**：选择主 `.i` 或目录，解析文件引用闭包，按 `.i` 所在目录打包；明确标记为 imported/read-only source，不冒充 Model Tree 结构化模型。
- **验收**：`TEST-STDCAE-12`；输入 hash 不变，依赖齐全，可与结构化生成结果并排比较。

### TASK-STDCAE-100 回归与七例人工文档

- **工作**：增加结构化生成对照合同、项目保存重开、网格 manifest、远端任务与结果回放验证；逐例更新人工操作文档和总体规划。
- **执行规则**：日常只跑相关 1～2 个定向用例；提交前执行完整 CTest 和不少于现有 114 步的真实点击巡览。
- **准出**：7/7 达到 D；导入入口只作为辅助证据，不替代任何一例的 UI 组成证据。

## 5. 任务状态与里程碑

| 顺序 | 工作项 | 当前状态 | 阶段结果 |
|---:|---|---|---|
| 1 | TASK-STDCAE-010 七例等价清单 | ✅ 首轮完成 | 建立逐块追溯基线 |
| 2 | TASK-STDCAE-020 Exodus manifest | ✅ 完成 | Section/Physics 可绑定真实体组 |
| 3 | TASK-STDCAE-030 输出请求补齐 | ✅ V01 范围完成 | V01 输出无损组成 |
| 4 | TASK-STDCAE-040 V01 首例 | 🟡 B 完成、C/D 待人工 | 首个结构化人工闭环 + 操作文档 |
| 5 | TASK-STDCAE-050 V02/R02/G33 | ⬜ 未开始 | 4/7 结构化闭环 |
| 6 | TASK-STDCAE-060 Physics/Predictor | ⬜ 未开始 | G32/G34/R01 前置 |
| 7 | TASK-STDCAE-070 G32/G34 | ⬜ 未开始 | 6/7 结构化闭环 |
| 8 | TASK-STDCAE-080 R01 诊断 | ⬜ 未开始 | 7/7 结构化闭环 |
| 9 | TASK-STDCAE-090 `.i` 导入 | ⬜ 未开始 | 已有算例查看/对照/复算入口 |
| 10 | TASK-STDCAE-100 总回归 | ⬜ 未开始 | 自动、外部、人工证据闭合 |

## 6. 人工操作文档的创建时点

`doc/人工算例操作说明文档.md` 已在 TASK-STDCAE-020/030 和 V01 结构化生成合同通过后创建，供用户实际执行 TASK-STDCAE-040。

首版必须逐项写明：

- 从空项目开始的点击路径；
- V01 每个对象的名称、类型和字段值；
- 共同 `.e` 与四张 CSV 的选择位置；
- 生成 `.i` 的逐块核对点；
- 预检、快照、Job、日志、CSV/Exodus 和时间步检查；
- 实际 `job_id`、运行版本、偏差和人工验收结论。

其他算例只在相应结构化生成合同通过后追加，不能用“导入已有 `.i`”步骤替代。
