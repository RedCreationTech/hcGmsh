# MOOSE 官方算例 Round B CAE 人工复刻与闭环验证手册

> 版本：2026-10-06 v7（MC01、MC02 人工闭环通过；补充官方教程链接与 MC02 验收记录）
> 计算节点：本次 MC02 使用 `192.168.0.121`；后续提交前确认实际节点与运行任务
> 应用：`HC MOOSE CAE [prototype]` / `hc_moose-opt`
> 参考产物：`examples/hcMooseApp/qualification/MC01～MC05/`

## 1. Round B 的验收目标

Round B 不只是验证求解器路由。完整的验收对象是 MC01～MC05 五个官方算例，每个算例都必须完成同一条人工闭环：

```text
空项目选择 HC MOOSE CAE
  -> 通过 CAE 图形化操作完成几何、网格、材料、物理、边界、分析步和输出
  -> 结构化生成 .i
  -> 导出不可变 Job Snapshot
  -> 通过 LIMS/C06 提交到本次确认的计算节点
  -> hc_moose-opt 完成有限元求解
  -> 通过 LIMS 下载结果目录
  -> 导入 CAE Results 工作窗
  -> 查看场结果、时间步和历史曲线
  -> 与 Round A 官方参考进行语义和数值对比
```

严格禁止下列替代方式：

- 不把整份官方 `.i` 粘贴到输入编辑器；
- 不手改 CAE 生成的 `.i`、`manifest.json` 或快照文件；
- 不用 Custom Block 补齐结构化 UI 未支持的对象；
- 不在计算节点终端手动运行 `.i` 冒充 CAE Job；
- 不用 ParaView 单独查看替代 CAE 内的 Results 导入与回放。

## 2. 当前可执行性

2026-09-28 的代码已完成下列 Round B 基础设施：

- CAE 可选择 `HC MOOSE CAE [prototype]`；
- Job 显式传递 `solver_id = hc_moose-opt`；
- C06 可按 Job 选择 HcMooseApp，旧调用未传 `solver_id` 时仍默认 DamSafetyApp；
- Job 可追溯 requested/resolved solver id、Application commit、MOOSE commit 和二进制 SHA-256；
- 成功作业的 Exodus/CSV 可由 LIMS 下载并登记到 Results。

但是，五个算例的结构化 CAE 对象尚未全部实现。因此本文档是本轮的**目标操作手册和准出清单**，不是宣告五例已可全部通过。

| 用例 | 当前状态 | 首个必须解决的阻断项 |
|---|---|---|
| MC01 瞬态热传导 | 已人工通过 | 用户于 2026-09-29 验收；Job `job_20260929_081949_gxr4ra` |
| MC02 热-结构耦合 | 已人工闭环通过 | 用户于 2026-10-06 确认验证、远程任务执行及 CAE 结果导入均 OK；见 5.14.1 |
| MC03 无摩擦接触 | 部分具备 | 精确的双立柱二维网格、二维 Contact 支持、预测器及结果量 |
| MC04 J2 各向同性塑性 | 部分具备 | `IsotropicPlasticityStressUpdate` 及自动材料链、塑性应变场输出 |
| MC05 Newmark 动力学 | 阻断 | InertialForce、Newmark 速度/加速度辅助链、动力历史输出 |

人工执行到标注为“阻断闸门”的界面能力不存在时，记录为 `BLOCKED` 并停止该例；不应选一个近似对象继续。

## 3. 通用验证流程

### 3.1 环境与证据

1. 确认现有 LIMS API 可用，CAE 的 Server 使用 `http://127.0.0.1:8200`；服务已运行时不重复启动或重启。
2. 启动 CAE，新建独立项目，选择 `HC MOOSE CAE [prototype]`。
3. 每例使用独立项目：`roundb-mc01.gmp.yaml`～`roundb-mc05.gmp.yaml`。
4. 每例记录项目路径、生成 `.i`、生成报告、快照目录、Job ID、终态、制品清单和结果截图。
5. 数值参考只使用本仓库 `examples/hcMooseApp/qualification/MCxx/` 内的 Round A 文件。

### 3.2 每例的通用操作

1. 按本文对应章节在 CAE 建立几何、网格和 Physical Groups。
2. 在模型树中逐项创建 Variables、Functions、Materials、Sections、Physics/Kernels、BC/Loads、Step 和 Outputs。
3. 保存、关闭并重开项目，确认对象、引用和数值未丢失。
4. 点击“校验工作流”，必须为 `0 errors`；警告必须逐条记录。
5. 点击“同步模型到 MOOSE 输入”两次，两份生成文本必须完全一致。
6. 按各例的“`.i` 语义检查”核对关键 Block；不要求与官方 `.i` 逐字相同。
7. 点击“检查输入”，目标应用 `hc_moose-opt --check-input` 必须成功。
8. 点击“导出任务快照”，检查快照包含 `.i`、网格及依赖文件、`manifest.json`，所有引用均为包内相对路径。
9. 在“执行配置 -> Remote Job”确认 Project ID，提交作业并记录 Job ID。
10. 等待 `queued -> preparing/running -> succeeded`；只进入 `running` 不算通过。
11. 在 Job 详情核对 requested/resolved solver id 均为 `hc_moose-opt`，实际应用为 `HcMooseApp`。
12. 通过 LIMS 下载该 Job 的完整结果目录，不与 Round A 目录混放。
13. 在 CAE 的 Results 工作窗导入该目录中的 Exodus/CSV，确认来源 Job、快照和输入哈希可追溯。
14. 执行本文对应的后处理检查，填写第 9 节验收表。

## 4. MC01：二维瞬态热传导

### 4.1 用例身份与验证边界

| 项目 | 内容 |
|---|---|
| 人工测试编号 | `TEST-MOOSE-B-MC01-01` |
| 官方教程 | [Step 3 — Adding additional terms to the heat equation](https://mooseframework.inl.gov/modules/heat_transfer/tutorials/introduction/therm_step03.html) |
| 官方输入 | `examples/hcMooseApp/qualification/MC01/therm_step03.i` |
| Round A 参考结果 | `examples/hcMooseApp/qualification/MC01/therm_step03_out.e` |
| Round A 参考曲线 | `examples/hcMooseApp/qualification/MC01/therm_step03_out_t_sampler_0006.csv` |
| 目标项目文件 | `roundb-mc01.gmp.yaml` |
| 目标求解器 | `hc_moose-opt` |

官网同页还介绍增加体热源的 `therm_step03a.i`；MC01 对齐的是不含体热源的 `therm_step03.i`。官网可能随版本更新，本轮数值基准仍使用上述 Round A 冻结文件。

官方算例的物理问题是：一块长 `2`、高 `1` 的二维材料，初始温度均为 `300`；左边始终保持 `300`，右边温度按 `300+5*t` 上升；在 `0～5` 内以 `dt=1` 做瞬态热传导计算。

本轮已交付“通用结构化对象 + MC01 预设”能力。MC01 已于 2026-09-29 由用户完成人工闭环：不再使用 Round A 的 `.e` 作为输入网格，也不需要切换专家/自定义模式手改 `.i`。

**能力边界**：变量、函数、材料、Kernel、分析步、向量后处理和输出仍由通用结构化对象、mapping 与生成器承载；只有 MC01 的参考网格、可选类型、最小默认值和输出预设绑定 `hc_moose-opt`。本轮不扩展成“可编辑任意 MOOSE block/参数”的通用编辑器，因为那会同时引入完整语法覆盖、版本兼容和所有现有应用的回归面，却不是 MC01 闭环验收所需。后续只有在多个算例重复需要同一种能力时，才把对应预设上提为跨算例通用能力。

### 4.2 新建项目并选择应用

1. 启动 GMP-ISE，选择“文件 -> 新建项目”。
2. 在顶部“应用”下拉框选择 `HC MOOSE CAE [prototype]`。
3. 保存为 `roundb-mc01.gmp.yaml`。必须先保存，MC01 参考网格会写入项目自己的工作目录。
4. 再次查看顶部“应用”，确认没有回退为 `DamSafetyApp-opt` 或 `combined-opt`。
5. 确认模型树中有 `Variables`、`Functions`、`Materials`、`Loads`、`Steps`、`VectorPostprocessors`、`Outputs` 和 `Mesh`，记录 `MC01-01-project-profile.png`。

**预期结果**：项目成功保存，顶部应用始终为 `HC MOOSE CAE [prototype]`。

### 4.3 准备二维网格

1. 打开顶部“网格”（`Mesh`）菜单，点击 `Create MC01 Reference Mesh`。
2. 等待状态栏显示 `MC01 reference mesh created: 121 nodes, 100 QUAD4.`。
3. 确认视口中是 `2 x 1` 的二维矩形，X/Y 方向各 10 段，共 `121` 个节点和 `100` 个 `QUAD4` 单元。
4. 确认物理组为二维域 `domain` 及边界 `bottom`、`right`、`top`、`left`。
5. 确认 `Mesh` 根节点下已登记 `mc01_therm_step03.msh`，保存项目并记录 `MC01-02-reference-mesh.png`。

**等价性**：官方输入使用 `GeneratedMeshGenerator(dim=2,nx=10,ny=10,xmax=2,ymax=1)`；CAE 生成与其几何、拓扑、网格密度和边界名一致的 Gmsh 网格，属于 CAE Native Equivalent。

### 4.4 创建温度变量

1. 在模型树中右键 `Variables`，选择“添加 Variables”。
2. 将新对象重命名为 `T`，双击 `T` 打开属性窗口。
3. 进入“参数”页，勾选“高级参数”（`Advanced Parameters`）。
4. 保持同步方式为“双向（推荐）”（`Bidirectional (Recommended)`）。
5. 使用“添加参数”逐项填写：

   | Key | Value |
   |---|---|
   | `family` | `LAGRANGE` |
   | `order` | `FIRST` |
   | `initial_condition` | `300` |

6. 切换到“预览”页，确认对象名为 `T`，三个参数均未丢失。
7. 关闭属性窗口并保存项目。

**预期结果**：生成语义为 `[Variables/T]`，初始温度为 `300`。本步使用 CAE 自带的结构化高级参数表，不是手改 `.i`。

### 4.5 创建右边界温度函数

1. 右键 `Functions`，选择“添加 Functions”。
2. 将对象重命名为 `right_temperature`，双击打开属性。
3. 在“参数 -> 快捷参数”中，将 Type 设为 `ParsedFunction`。
4. 将 Expression/表达式填为 `300+5*t`。
5. 打开“预览”，确认函数名和表达式正确，保存项目。

**语义说明**：官方 `.i` 在 `FunctionDirichletBC` 内直接写表达式；CAE 建立命名的 `ParsedFunction` 后由边界条件引用，属于可接受的 CAE Native Equivalent。

### 4.6 创建左、右温度边界

#### 4.6.1 左边界 `t_left`

1. 右键 `BC`，选择“添加 BC”，重命名为 `t_left`。
2. 双击 `t_left`，在快捷参数中选择 `DirichletBC`，或选用“Fixed (Dirichlet 0)”模板后修改数值。
3. Variable/变量选择 `T`，Value/值填写 `300`。
4. 在“可选边界分组”（`Available Boundary Groups`）中只选中 `left`。
5. 点击“应用所选边界”（`Apply Selected Boundaries`），确认 Boundary 字段显示 `left`。

#### 4.6.2 右边界 `t_right`

1. 再次右键 `BC`添加对象，重命名为 `t_right`。
2. 选择“Prescribed Function”模板，或将 Type 设为 `FunctionDirichletBC`。
3. Variable/变量选择 `T`，Function/函数选择 `right_temperature`。
4. 在可选边界中只选中 `right`，点击“应用所选边界”。
5. 进入“预览”，确认引用的是 `T`、`right_temperature` 和 `right`。
6. 记录截图 `MC01-03-temperature-bcs.png`，保存项目。

**预期结果**：左边界始终为 `300`；右边界在 `t=0` 为 `300`，在 `t=5` 为 `325`。

### 4.7 创建材料与热传导 Kernel

#### 4.7.1 材料

1. 在 `Materials` 下添加 `thermal`。
2. Type 选择 `HeatConductionMaterial`，填写 Thermal Conductivity=`45`、Specific Heat=`0.5`。
   - 若通过双击对象打开浮动属性窗，Type 下拉也必须包含 `HeatConductionMaterial`；缺失则记录 `MC01-PHY-FLOATING-01`，不要改用近似材料。
3. 在 `Materials` 下添加 `density`。
4. Type 选择 `GenericConstantMaterial`，填写 Prop Names=`density`、Prop Values=`8000`。
5. 两个材料的“可选物理体”均保持不选，“待应用选择”显示“（无）”正是本例预期。省略 `block` 代表对唯一域全局生效。

#### 4.7.2 Kernel

1. 在 `Loads` 下添加 `heat_conduction`，Type 选择 `HeatConduction`，Variable 选择 `T`。
2. 勾选高级参数，确认只有 `type` 和 `variable`，不应残留 `BodyForce` 的 `value=0`。
3. 在 `Loads` 下添加 `time_derivative`，Type 选择 `HeatConductionTimeDerivative`，Variable 选择 `T`。
4. 两个对象都保持不选物理体，确认预览无 `value` 和 `block`。

**说明**：这两个对象在 CAE 树中统一由 `Loads` 管理，同步后会正确生成在 MOOSE `[Kernels]` 中。

### 4.8 创建瞬态分析步

1. 进入 `Step` 模块，点击“瞬态步”（`Add Transient Step`），将对象重命名为 `heat_transient`。
2. 双击该分析步，确认 Type=`Transient`、Start Time=`0`、End Time=`5`、Time Step=`1`、Scheme=`implicit-euler`。
3. 勾选“高级参数”，表中应且只应有 `type`、`start_time`、`end_time`、`dt`、`scheme`。
4. “预览”中不应有 `IterationAdaptiveDT`、`TimeStepper`、PETSc 选项或 `Preconditioning`。
5. 本步不需要手工删除通用结构分析默认参数；如仍出现，记录 `MC01-STEP-REGRESSION` 并停止提交。

### 4.9 创建中心线采样与输出

#### 4.9.1 中心线采样

1. 在 `VectorPostprocessors` 下添加对象，重命名为 `t_sampler`。
2. 双击对象打开属性窗，保持“高级参数”未勾选；快捷参数区必须显示 Type、Variable、Start Point、End Point、Number of Points、Sort By。若只看到高级参数表或空白区域，记录 `MC01-VPP-FORM-01` 并停止提交。
3. Type 选择 `LineValueSampler`，Variable 选择 `T`。
4. Start Point 填写 `0 0.5 0`，End Point 填写 `2 0.5 0`。
5. Number of Points 填写 `20`，Sort By 选择 `x`。
6. 预览应完整显示六个键，Validation 为 `No issues`。

#### 4.9.2 输出

1. 在 `Outputs` 下添加对象，重命名为 `mc01_outputs`。
2. “场输出变量”中的 CDP 变量全部不勾选；主变量 `T` 会由 Exodus 正常写出。
3. History Output Preset 保持 `custom`，边界反力、平均位移和场量极值全部为 `false` 或留空。
4. Enable Times 保持 `false`；本例不建立额外 Times 对象。
5. Exodus 选择 `true`，CSV 选择 `true`，CSV execute_on 选择 `final`。
6. `file_base` 填写 `therm_step03_out`，不选任何边界分组。
7. 预览后点击“确定”，记录 `MC01-04-vpp-outputs.png`。

#### 4.9.3 历史记录（修复前）

> 本小节仅保留 2026-09-28 的缺口证据，已不是复验操作指令；当前操作以 4.9.1～4.9.2 为准。

对 `mc01_outputs_probe` 的界面走查已确认：

- “场输出变量”只提供 `DamageC`、`DamageT`、`kappa_c`、`kappa_t` 及 CDP 迭代诊断量，它们与 MC01 无关，应全部保持未勾选。温度 `T` 作为主变量可由 Exodus 默认写出，不需要伪装成 CDP 场输出。
- “历史输出”只提供边界反力、边界平均位移和场量极值，不能建立 `T` 的中心线采样。
- “输出时间”可启用 Times，但它不是 `LineValueSampler`，也不能替代 CSV 的 `execute_on=final`，因此保持 `false`。
- “输出文件”确实可设 Exodus、CSV 和 `file_base`，但没有为 CSV 设置 `execute_on=final` 的控件；开启 CSV 也不会自动产生 `LineValueSampler` 数据。
- 顶部“可选边界分组”与本例全域 Exodus/中心线采样无关，保持“待应用选择：（无）”是正确的。

**修复前判定（已失效）**：当时记录为 `BLOCKED MC01-VPP-01` + `BLOCKED MC01-OUT-01`；两项已在本轮关闭，不得再作为停止当前复验的依据。

#### 4.9.4 修复前 `.i` 记录（已失效）

> 下表描述修复前的残缺输入，不用于判定当前版本是否可提交。当前准出以 4.11～4.12 为准。

2026-09-28 实际操作日志已记录项目新建、Exodus 诊断网格导入、`T`、函数、两个 BC、`density`、`heat_conduction`、`heat_transient` 的添加/编辑，以及 `mc01_outputs_probe` 的添加后删除。日志中没有 `Check Input`、Job Snapshot 或远端提交成功记录。

修复前的结构化 `.i` 只生成了 Variable、Function、两个 BC、密度、`HeatConduction` 和固定时间步，当时的缺口如下：

| 检查项 | 当前 `.i` | 影响 |
|---|---|---|
| `HeatConductionTimeDerivative` | 缺失 | 没有热容量/瞬态项，不是目标瞬态热传导 |
| `HeatConductionMaterial(k=45, cp=0.5)` | 缺失 | `HeatConduction` 缺导热材料属性，可能在输入检查/材料属性初始化时失败 |
| `LineValueSampler` | 缺失 | 无法生成中心线 20 点数据 |
| `[Outputs]` | 缺失 | 无法产生预期 Exodus 和 final-only CSV |
| Mesh 引用 | macOS 绝对路径 | 计算节点不能直接访问；正式快照必须打包网格并改为包内相对路径 |
| 网格来源 | Round A 已求解 `.e` | 只是表单诊断网格，不能作为从空项目复刻的正式证据 |

**当时决策（已由本轮修复取代）**：

- 不点击“运行”或提交远端 Job。
- 不切换高级/自定义输入模式手工补写官方 block；这样可以测求解器，但不能证明 CAE 结构化能力，而 Round A 已经完成求解器本身的直接实算验证。
- 先完成第 4.11 节的五个结构化缺口修复，再从 CAE 重新生成 `.i`、导出快照并提交 `hc_moose-opt`。这一步现已完成，当前应直接按第 4.10 节复验。

### 4.10 从保存复查到远端求解

> 本节是修复后的正式人工复验路径。输入模式必须保持 Structured/结构化，不需要手改 `.i`。

#### 4.10.1 保存、重开与生成 `.i`

1. 保存项目，关闭项目后重新打开 `roundb-mc01.gmp.yaml`。
2. 逐项复查 `T`、`right_temperature`、两个 BC、两个 Kernel、两个 Material、Transient Step、`t_sampler` 和 Outputs，确认名称、引用和数值未丢失。
3. 进入 `Job`模块，点击“打开作业工作区”（`Open Job Workspace`）。
4. 点击“校验工作流”（`Validate Workflow`）；要求 `0 errors`。HC 档案下不要为通过校验而补建 `Sections` 或 `Physics`。
5. 点击“同步到输入”（`Sync to Input`）。
6. 在 MOOSE 工作窗的“输入文件”页查看 `Generated Input` 和 `Generation Report`。按第 4.12 节核对后，不需要切换高级模式。
7. 再次执行“同步到输入”，确认生成内容稳定，没有随机重命名或引用漂移。
8. 生成输入中的 Mesh 可能显示本机项目工作目录的绝对路径，这是结构化编辑阶段的正常显示；导出 Job Snapshot 时必须改写为包内文件名。
9. 点击“写入输入文件”（`Write Input`），将生成文件保存为 `therm_step03.i`，然后点击“检查输入”（`Check Input`）。

#### 4.10.2 导出快照并提交计算节点

1. 在“执行配置”页中，Runner 选择 Remote Job/LIMS Facade。
2. Server 填写当前 LIMS API 地址，本地默认为 `http://127.0.0.1:8200`；Project ID 选择本次验证项目。
3. 确认目标求解器为 `hc_moose-opt`，不是 `dam-safety-app`。
4. 点击“导出任务快照”（`Export Job Snapshot`），记录快照目录。
   - 每次同步或修改输入后都必须重新导出；“提交作业”会按输入哈希拒绝过期快照。
5. 打开快照制品清单，确认包含 `.i`、`mc01_therm_step03.msh`、`manifest.json`，且快照内 `.i` 不再引用 `/Users/...`。
6. 点击“提交作业”（`Submit Job`），记录 Job ID。
7. 点击“刷新状态”（`Refresh Status`），直到状态为 `succeeded`。`running` 不算完成。
8. 在 Job 详情中确认 requested solver id 和 resolved solver id 都为 `hc_moose-opt`，应用为 `HcMooseApp`，计算节点为 `192.168.0.138`。

#### 4.10.3 通过 LIMS 下载并导入 CAE Results

1. 在 Job 工作区点击“刷新文件”（`Refresh Files`）。
2. 在制品清单中至少勾选 `therm_step03_out.e`、`therm_step03_out_t_sampler_0006.csv`、求解日志和作业 manifest。
3. 点击“下载选中项”（`Download Selected`）。默认下载目录为 `~/Downloads/gmp_remote/<job_id>/`。
4. 进入 `Results` 模块，点击“导入任务目录...”（`Import Task Directory...`），选择刚下载的 Job 根目录。
5. MC01 的预期结果是一个 `therm_step03_out.e` 场结果和一个 `therm_step03_out_t_sampler_0006.csv` 最终时刻空间剖面；CSV 表头为 `T,id,x,y,z`，不要求 `time` 列。
6. 导入后在场变量中选择 `T`，从 `t=0` 逐帧播放到 `t=5`，记录首帧和末帧截图。
7. 切换到表格/曲线视图，查看 CSV 的 `T-x` 中心线数据。单文件导入仅作为目录导入异常时的诊断手段。
8. 不要直接导入 `examples/` 中的 Round A 文件作为 Round B 结果；Round B 证据必须来自刚记录的 Job ID。

### 4.11 本轮修复记录

| 编号 | 修复结果 | 人工复验点 |
|---|---|---|
| `MC01-GEO-01` | 增加 `Create MC01 Reference Mesh` | 121 节点、100 QUAD4、`domain/left/right/top/bottom` |
| `MC01-PHY-01` | HC 独立 mapping 增加 `HeatConductionMaterial` 和 `HeatConductionTimeDerivative`，并向浮动属性窗传递同一份档案类型候选 | 主编辑器与浮动窗均可选类型，切换 Kernel 后无残留 `value=0` |
| `MC01-STEP-01` | HC 档案使用最小固定步长默认值，删除高级行后立即刷新表单 | 只生成 `type/start/end/dt/scheme` |
| `MC01-VPP-01` | 增加 `VectorPostprocessors/LineValueSampler` 结构化表单，并补入快捷表单启用白名单 | 浮动窗六个快捷控件可见；`T`、起终点、20 点、x 排序均可保存 |
| `MC01-OUT-01` | Outputs 增加 CSV `execute_on` | Exodus 启用，CSV 为 `final`，`file_base=therm_step03_out` |
| `MC01-RESULT-01` | 目录导入同时识别时间历程 CSV 和 `x/y/z` 空间采样 CSV，不再强制结果包同时具备 Exodus 与 CSV | 直接选择 MC01 Job 根目录，`.e` 与 `T-x` 采样均进入 Results |

自动定向巡览 `moosecase_mc01_transient_heat_contract` 已通过：它从空模型树创建网格和全部 MC01 对象，检查生成 `.i`、无关默认值清理以及工作流预检。这不替代第 4.10 节的真实远程人工复验。

### 4.12 生成 `.i` 的语义检查

不要按行和排版与官方 `.i` 做文本比对，应核对下列语义：

- `[Mesh/file]` 在编辑阶段引用项目自己的 `mc01_therm_step03.msh`，在快照内必须改为包内引用；网格语义等价于官方 `GeneratedMeshGenerator(dim=2,nx=10,ny=10,xmax=2,ymax=1)`。
- `[Variables/T]` 存在，且 `initial_condition=300`。
- `HeatConduction(variable=T)` 和 `HeatConductionTimeDerivative(variable=T)` 同时存在，且块内无 `value=0`。
- `HeatConductionMaterial` 的导热系数为 `45`、比热为 `0.5`；`density` 为 `8000`。
- `t_left` 为 `DirichletBC(T=300, boundary=left)`。
- `t_right` 为 `FunctionDirichletBC`，引用等价于 `300+5*t` 的函数，边界为 `right`。
- Executioner 为 `Transient`，`start_time=0`、`end_time=5`、`dt=1`、`scheme=implicit-euler`，无 `TimeStepper` 和 `Preconditioning`。
- `LineValueSampler` 采样 `T`，从 `(0,0.5,0)` 到 `(2,0.5,0)`，`num_points=20`，`sort_by=x`。
- Exodus 和 CSV 均存在，CSV 的输出前缀为 `therm_step03_out` 且只在 `final` 执行。

### 4.13 预期产物与数值验收

| 文件 | 必须存在 | 用途 |
|---|---|---|
| `therm_step03.i` | 是 | CAE 结构化生成的求解输入 |
| `mc01_therm_step03.msh` | 是 | CAE 生成并随快照打包的输入网格 |
| `therm_step03_out.e` | 是 | `T` 温度场和 `t=0～5` 时间帧 |
| `therm_step03_out_t_sampler_0006.csv` | 是 | `t=5` 时 `y=0.5` 中心线的 20 个温度采样点 |
| 求解日志 | 是 | 确认正常收敛和实际求解器身份 |
| Job manifest/身份记录 | 是 | 追溯 Job、快照、HcMooseApp 和 MOOSE commit |

CAE 后处理必须完成以下检查：

1. `t=0` 时整个域为 `T=300`。
2. `t=5` 时左边界为 `300`，右边界为 `325`，温度由右向左平滑传播，不应有越界震荡或空场。
3. CSV 应有表头 `T,id,x,y,z` 和 20 行数据；`x` 从 `0` 单调增加到 `2`。
4. 与 Round A 参考的末时刻关键点对比：

   | 位置 | Round A 参考 `T` | 验收建议 |
   |---|---:|---|
   | `x=0` | `300` | 边界值必须一致 |
   | `x≈1.05263` | `300.05801473129` | 相对误差 `<= 1e-6` |
   | `x≈1.89474` | `316.52664137136` | 相对误差 `<= 1e-6` |
   | `x=2` | `325` | 边界值必须一致 |

5. 如果 CAE 生成的网格节点编号不同，不按节点 ID 对比，按采样线的 `x` 坐标对齐。
6. 将项目文件、生成 `.i`、Generation Report、Job ID、下载目录、三张关键截图和 PASS/BLOCKED/FAIL 结论填入第 9 节验收表。

## 5. MC02：二维热-结构耦合

### 5.1 用例身份、当前状态与执行边界

| 项目 | 内容 |
|---|---|
| 人工测试编号 | `TEST-MOOSE-B-MC02-01` |
| 官方教程 | [Step 1 — Basic Thermal/Mechanical Coupling](https://mooseframework.inl.gov/modules/combined/tutorials/introduction/thermomech_step01.html) |
| 官方冻结输入 | `examples/hcMooseApp/qualification/MC02/thermomech_step01.i` |
| Round A 参考结果 | `examples/hcMooseApp/qualification/MC02/thermomech_step01_out.e` |
| 目标项目 | `roundb-mc02.gmp.yaml` |
| 目标求解器 | `HC MOOSE CAE [prototype]` / `hc_moose-opt` |
| 生成网格 | `mc02_thermomech_step01.msh` |
| 结果前缀 | `thermomech_step01_out` |
| 当前状态 | 2026-10-06 用户确认人工验证、远程任务执行与 CAE 结果导入通过；详细数值比较证据待补 |

2026-10-05 用户指定本轮测试由用户自行执行，AI 完成代码、编译和人工说明，不运行自动巡览、CTest、远程输入检查或求解，不修改 LIMS/C06 配置、注册表或服务。2026-10-06 用户确认已验证通过，提交任务执行后导入结果也均 OK。本次闭环由用户执行，验收范围与证据见 5.14.1；以下步骤保留作为后续复验说明。

官方教程以 `thermal_mechanical/thermomech_step01.i` 为输入。该文件的旧注释存在 `thermoech_step01.html` 拼写，本表使用已核验可访问的 `thermomech_step01.html`。官网用于查验建模语义，数值参考仍使用 Round A 冻结输入和结果。

物理问题：`2 x 1` 的二维矩形，`10 x 10` 四边形网格，初温 `300 K`。左边恒温 `300`、右边 `300+5*t`，全域体热源 `5e4`。材料为有限应变线弹性、热膨胀系数 `0.001`，左下角固定 X 位移，底边固定 Y 位移。时间范围 `0～5`，固定 `dt=1`。

### 5.2 官方语义到 CAE 的能力对照

| 官方块/对象 | CAE 结构化入口 | 生成语义 | 人工验证位置 |
|---|---|---|---|
| GeneratedMeshGenerator + ExtraNodesetGenerator | Mesh → Create MC02 Reference Mesh | FileMeshGenerator；MSH 内置左下角 `pin` nodeset | 5.3、5.8、5.10 |
| Variables/T、热传导、热容、热学材料与密度 | Variables、Loads、Materials | 保留 MC01 热学链 | 5.4～5.5 |
| HeatSource | Loads → Heat Source (MC02) 模板或 Type | `HeatSource(T, value=5e4)` | 5.5、5.10 |
| GlobalParams + QuasiStatic | Physics 快捷表单 | 二维位移、FINITE、自动本征应变、von Mises | 5.7、5.10 |
| 弹性张量、热膨胀、有限应变应力 | Materials 的三个 MC02 模板 | 三对象材料链 | 5.6、5.10 |
| pin_x / bottom_y | BC 快捷表单 + 分组指派 | `disp_x=0 @ pin`、`disp_y=0 @ bottom` | 5.8 |
| SMP / LU / Transient | Steps 快捷表单 | `SMP(full=true)`；PETSc `-pc_type lu`；固定步长 | 5.9 |
| Outputs/Exodus | Outputs 快捷表单 | 仅 Exodus，无强制采样 CSV | 5.9、5.12 |

`pin` 采用 CAE Native Equivalent：MSH 2.2 中的 0 维物理组与一个 POINT 单元，挂在已有的 `(0,0,0)` 节点上，不增加节点或求解域单元。libMesh 将其读为命名 nodeset，与官方 ExtraNodesetGenerator 的点约束语义一致。实现依据见 [libMesh GmshIO 源码](https://mooseframework.inl.gov/docs/doxygen/libmesh/gmsh__io_8C_source.html)。该路径已包含在用户确认通过的本次远程实算中。

QuasiStatic 负责生成有限应变计算对象；**不另行手工添加 ComputeFiniteStrain 材料**，避免与 action 生成的对象重复。

### 5.3 新建项目与参考网格

1. 启动更新后的 GMP-ISE，新建空项目，应用选择 `HC MOOSE CAE [prototype]`。
2. 保存为 `roundb-mc02.gmp.yaml`；先保存再生成网格。
3. 打开顶部“网格 / Mesh”菜单，点击 **Create MC02 Reference Mesh**。
4. 状态栏应显示 `MC02 reference mesh created: 121 nodes, 100 QUAD4.`。
5. 确认 Mesh 下登记的是 `mc02_thermomech_step01.msh`，视口为 `2 x 1` 二维矩形。
6. 网格清单应有 `domain(dim=2)`，`bottom/right/top/left(dim=1)`，`pin(dim=0)`。`pin` 只含左下角 `(0,0,0)` 的一个节点。
7. 总记录数为 `141`：100 QUAD4 域单元 + 40 LINE2 边界单元 + 1 POINT；求解域仍只有 100 单元、121 节点。
8. 保存，记录 `MC02-01-project-mesh.png`。

不要选 MC01 网格代替本步：它没有 pin 点组。

### 5.4 温度变量、函数与温度边界

按 MC01 第 4.4～4.6 节的同一路径，从模型树分别建立：

| 分类 | 对象名 | 参数 |
|---|---|---|
| Variables | `T` | `family=LAGRANGE`、`order=FIRST`、`initial_condition=300` |
| Functions | `right_temperature` | Type=`ParsedFunction`、Expression=`300+5*t` |
| BC | `t_left` | Type=`DirichletBC`、Variable=`T`、Value=`300`；选 `left` 后应用 |
| BC | `t_right` | Type=`FunctionDirichletBC`、Variable=`T`、Function=`right_temperature`；选 `right` 后应用 |

变量使用 CAE 的结构化高级参数表；其余使用快捷表单。对象编辑完成后点击“确定”，确认修改已提交到模型树。此时不创建 disp_x/disp_y 变量节点，后续由 Physics 自动创建。

### 5.5 热学材料、热传导与体热源

1. Materials 添加 `thermal`：Type=`HeatConductionMaterial`，导热系数 `45`、比热 `0.5`。
2. Materials 添加 `density`：Type=`GenericConstantMaterial`，Prop Names=`density`、Prop Values=`8000`。
3. 两个材料都不指派 block，作用于唯一全域。
4. Loads 添加 `heat_conduction`：Type=`HeatConduction`、Variable=`T`。
5. Loads 添加 `time_derivative`：Type=`HeatConductionTimeDerivative`、Variable=`T`。
6. Loads 添加 `heat_source`：选择 **Heat Source (MC02)** 模板，点击“应用模板”；或选择 Type=`HeatSource` 后手动填写 Variable=`T`、Value=`5e4`。Function 下拉选择空白项，不指派物理体。应用常量模板会主动清除该对象已有的 function 引用。
7. 预览检查：前两个 Kernel 没有残留 `value`，只有 heat_source 有 `value=5e4`；热源没有 `function` 引用。
8. 保存，记录 `MC02-02-thermal-source.png`。

#### 5.5.1 Function 空值与常量模板定向人工复验（2026-10-05 修复）

本次用户截图中 Function 显示 right_temperature，但实际保存的 heat_source 参数和生成的 .i 均未包含 function。原因是共用下拉回显将“未设置”显示为第一项；另外常量模板曾未明确移除已经保存的 function，两个问题分别修复。以下复验由用户执行，AI 未运行 GUI/CTest 或远程求解。

1. 重启更新后的应用，打开当前 roundb-mc02.gmp.yaml，双击 heat_source。若高级参数没有 function，快捷表单也必须显示空白，不能自动显示 right_temperature；T 和 5e4 仍正确回显。
2. 在 Function 下拉主动选择 right_temperature，再应用 Heat Source (MC02) 模板。Function 必须清空，高级参数/预览没有 function，type=HeatSource、variable=T、value=5e4。
3. 再主动选 right_temperature，然后从同一下拉选择第一条空白项，确认可人工撤销引用；点击确定、保存并重开，Function 应继续为空。
4. 同步模型，检查 [Kernels/heat_source] 仅含 type、variable、value（本例不指派 block），不能残留 function=right_temperature。该测试无需提交作业。
5. 打开 t_right，已保存的 right_temperature 应正常显示。临时选 Function 空白项时，FunctionDirichletBC 必须报告缺少 function；取消编辑，保留原有温度边界。
6. 对 Body Force 常量模板同样做“先选择函数、再应用常量模板”的检查；Function 应清空。其他快捷下拉在高级参数不存在对应值时，应显示空白；已有有效值应继续回显。必填项为空仍按原有校验规则处理。

观察数据以对象高级参数和生成输入为准；操作日志只记录对象提交时间，不包含字段明细，不能单凭日志判断某个函数引用是否已保存。

### 5.6 弹性与热膨胀材料链

在 Materials 下依次添加三个对象。双击打开属性，在模板下拉中选择对应模板后点击“应用模板”，再核对快捷字段：

| 对象名 | 模板 / Type | 必须核对的值 |
|---|---|---|
| `elasticity` | Isotropic Elasticity (MC02) / ComputeIsotropicElasticityTensor | Young's Modulus **1000 MPa**；Poisson's Ratio `0.3`；存储/预览为 `youngs_modulus=1e9 Pa` |
| `expansion1` | Thermal Expansion (MC02) / ComputeThermalExpansionEigenstrain | `temperature=T`、`thermal_expansion_coeff=0.001`、`stress_free_temperature=300`、`eigenstrain_name=thermal_expansion` |
| `stress` | Finite Strain Elastic Stress (MC02) / ComputeFiniteStrainElasticStress | 仅 Type；不携带 Prop Names/Prop Values、E/nu 或热膨胀字段 |

三个对象均保持不选物理体，不创建 Sections。本例为单域全局材料链，Physics 在下一步指派 domain。

**单位检查**：弹性表单按 MPa 显示，输入 1000 对应求解器的 1e9 Pa；不要在 MPa 控件中填写 1e9。热膨胀材料必须使用 0.001，不使用通用 Thermal Expansion 模板的 1e-5 默认值。

记录 `MC02-03-elasticity.png`、`MC02-04-thermal-expansion.png`。

### 5.7 有限应变 QuasiStatic Physics

1. 在模型树 Physics 根节点下添加对象，命名为 `all`，双击打开属性。
2. Action=`QuasiStatic`，Block=`domain`；若 Block 为空，在可选物理体中选 domain 并应用。
3. Strain=`FINITE`，add_variables=`true`。
4. automatic_eigenstrain_names=`true`，generate_output=`vonmises_stress`，save_in_resid=`false`。
5. volumetric_locking_correction、incremental 保持空白，表示不覆盖 MOOSE 默认值。
6. 预览应包含上述关键语义，不应出现 CDP 场输出、resid_x/y/z 或手工额外的有限应变材料。
7. 保存，记录 `MC02-05-physics.png`。

本例通过全局参数自动生成 `displacements='disp_x disp_y'`；二维网格不得生成第三个位移 disp_z。自动本征应变引用将 expansion1 的 `thermal_expansion` 纳入有限应变计算。

### 5.8 点组与底边位移约束

1. BC 添加 `pin_x`，Type=`DirichletBC`、Variable=`disp_x`、Value=`0`。
2. 在可选边界分组中选择 **pin**，点击“应用所选边界”，Boundary 回显必须为 `pin`。
3. BC 添加 `bottom_y`，Type=`DirichletBC`、Variable=`disp_y`、Value=`0`；只选 bottom 后应用。
4. 两个对象都不得带 function 引用；pin_x 不能误选 left，bottom_y 不能误选整个 domain。
5. pin 点组可用于 DirichletBC / FunctionDirichletBC；压力、接触和 Neumann 面载荷不应提供该点组作为候选。
6. 保存，记录 `MC02-06-displacement-bcs.png`。

### 5.9 瞬态步和输出

1. Steps 添加 Transient Step，命名为 `thermomechanical_transient`。
2. Type=`Transient`、Start=`0`、End=`5`、dt=`1`、scheme=`implicit-euler`。
3. petsc_options_iname 填 `-pc_type`，petsc_options_value 填 `lu`。不添加 MUMPS 选项。
4. preconditioning_type 选择 `SMP`，preconditioning_full 选择 `true`；两个下拉均需显式选定。
5. timestepper_type、solve_type、line_search、automatic_scaling 和其他非线性控制保持空白；不启用 IterationAdaptiveDT。
6. Outputs 添加 `mc02_outputs`：Exodus=`true`、CSV=`false`，file_base=`thermomech_step01_out`。
7. CDP 场输出全不选，历史套餐保持 custom，各历史开关为 false，Times=false，不选边界。
8. 不创建 VectorPostprocessors：官方 MC02 只要求 Exodus，工作流应允许它为空。
9. 保存，记录 `MC02-07-step-outputs.png`。

### 5.10 本地工作流、生成语义与保存重开

1. 点击“校验工作流”，应为 **0 errors**。MC02 不应出现“缺少 MC01 line sampler”或“pin 维度不符”的错误。
2. 点击“同步模型到 MOOSE 输入”，保存为项目自己的 `thermomech_step01.i`；再同步一次，文本应完全一致。
3. 仅在预览中核对下表；发现缺项先记录并停止该项，不手改 .i。

| 生成位置 | 预期语义 |
|---|---|
| Mesh/file | FileMeshGenerator，引用本项目的 mc02_thermomech_step01.msh；pin 已包含在网格中，因此不另外生成 ExtraNodesetGenerator |
| GlobalParams | `displacements = 'disp_x disp_y'` |
| Variables/T | 初值 300、FIRST/LAGRANGE；无手工 disp_z |
| Kernels | HeatConduction、HeatConductionTimeDerivative、HeatSource，全部引用 T；热源值 5e4 |
| Materials | thermal(45,0.5)、density(8000)、elasticity(1e9,0.3)、expansion1(T,0.001,300,thermal_expansion)、stress(ComputeFiniteStrainElasticStress) |
| Physics/SolidMechanics/QuasiStatic/all | domain、FINITE、add_variables=true、automatic_eigenstrain_names=true、generate_output='vonmises_stress' |
| BCs | t_left(T=300@left)、t_right(T由right_temperature驱动@right)、pin_x(disp_x=0@pin)、bottom_y(disp_y=0@bottom) |
| Executioner | Transient、start=0、end=5、dt=1、implicit-euler、PETSc -pc_type / lu，无 TimeStepper |
| Preconditioning/smp | type=SMP、full=true，重复同步后只存在一份 |
| Outputs | Exodus 开启，file_base=thermomech_step01_out；没有 history_csv、CDP AuxKernels 或反力残差变量 |

4. 保存项目，关闭并重新打开 roundb-mc02.gmp.yaml。
5. 确认应用仍为 hc_moose-opt、五个材料/三个 Loads/四个 BC/一个 Physics/一个 Step/一个 Outputs 全部恢复，网格路径指向自己的项目。
6. 再打开 pin_x 属性，pin 候选仍可见且已指派；打开 Physics、Step 核对自动本征应变和 SMP/LU。
7. 重开后重新校验并同步，文本应与重开前相同。记录 `MC02-08-generated-input.png` 和 `MC02-09-reopen.png`。

另做三个本地定向人工检查，使用副本或完成后恢复原值：

- 将 heat_source 切为 HeatConduction，预览中的 value 应清除；再应用 Heat Source (MC02) 模板，恢复 T/5e4。
- 将 expansion1.temperature 临时改成 T_missing，工作流校验必须报错；恢复 T 后错误消失。
- MC02 保存后新建独立 MC01 项目并生成 MC01 网格：候选不应残留 pin，Transient 仍为最小 0～5/dt=1，无 SMP/LU；再打开 MC02 时 pin 和所有参数应恢复。若检查 MC01 完整工作流，仍须按第 4 节建立中心线采样。

### 5.11 用户择时执行输入检查、快照和远程 Job

本节会触发真实计算，应在用户决定 121 上其他任务的资源安排后执行。应用选 hc_moose-opt 只决定本作业的求解器，不需要切换 C06 默认求解器或重启服务。

1. 确认 CAE Server 是本次使用的 LIMS 地址、Project ID 正确，目标 C06 已注册 hc_moose-opt。
2. 按通用流程点击“检查输入”，由目标应用执行 --check-input。若本机没有该应用，记录当前检查入口的执行位置与原始日志，不把本机缺二进制误判为 MC02 语义错误；远程 Job 的 C06 输入检查必须成功。
3. 导出**新 Job Snapshot**，确认输入和网格都已打包，Mesh/file 为包内相对路径，solver_id 为 hc_moose-opt。
4. 不手改快照；若修改任何模型参数，重新同步、保存并导出新快照。
5. 提交 Job，记录 Job ID；等待 succeeded，并核对 requested/resolved solver_id、HcMooseApp 身份与输入检查日志。
6. 确认完成 t=1～5 共五个求解步，日志没有 Solve Did NOT Converge。
7. 通过 LIMS 下载完整任务目录，至少包含生成输入、输入网格、结果 Exodus、求解日志及身份/快照记录。没有采样 CSV 是本例预期。

### 5.12 CAE 结果导入与物理检查

1. 在 Results 选择“导入任务目录”，选择刚下载的 Job 根目录，不能选择 Round A 参考目录替代本次计算。
2. 确认识别 thermomech_step01_out.e；仅有 Exodus 也应导入成功。
3. 播放应有六帧 t=0,1,2,3,4,5。逐一查看 T、disp_x、disp_y 和 **单元场** vonmises_stress。
4. t=0：T 为 300，位移和热应力为零或求解精度下的近零。
5. t=5：左边 T=300，右边 T=325；内部因 5e4 体热源可以高于 325，不能套用 MC01 的温度上界。
6. pin 节点 disp_x=0，bottom 全边 disp_y=0；结构对热载荷产生位移/应力响应，没有整体刚体漂移。
7. vonmises_stress 作为单元输出存在且数值有限；不能用 CDP 诊断量代替它。
8. 如启用变形显示，记录缩放倍率；数值检查使用原始场值，不能按夸大的显示位移判断误差。
9. 记录 MC02-10-T-final.png、MC02-11-disp-x.png、MC02-12-disp-y.png、MC02-13-vonmises.png。

### 5.13 与 Round A 参考的人工数值核对

真实 Job 结果登记后，可以另外打开 Round A 参考 thermomech_step01_out.e 用于比较。参考文件不计入本次 Job 产物。

1. 相同物理时间、相同 Point/Cell 关联、相同分量进行比较，优先用数据表，Digits 设为 12 以上。
2. 先比较六帧的 T、disp_x、disp_y、vonmises_stress 最小值/最大值和均值；可在结果曲线页导出这些统计时间曲线的 CSV，保留两组来源文件。
3. 对左下角点、四个角点和中心区域做原始场值抽查；节点/单元编号可能不同，按坐标位置或单元所在区域对应，不直接用表格 Index 一一配对。
4. Round A compare.log 的场量比较容差为相对 5.5e-6、floor=1e-10。人工非零量抽查沿用相对 5.5e-6；参考为零的约束量单列绝对误差并以 1e-10 作为人工检查阈值。后者是本轮人工检查规则，不等同于 Exodiff floor 的定义。
5. 统计量/几个点相符属于人工数值抽查，**不应写成全场 Exodiff PASS**。如要宣告完整全场数值等价，仍需保存坐标对齐后的完整比较证据；无法完成的行保持待验证，并提供实际输入、结果和日志继续核对。

### 5.14 验收记录与常见阻断

| 检查项 | 状态（PASS/BLOCKED/FAIL/待验证） | 证据 |
|---|---|---|
| 空项目、HC 档案、MC02 参考网格和 pin | PASS | 用户确认；项目、网格与快照只读核对通过，见 5.14.1 |
| 热源与三对象力学材料链 | PASS | 保存项目、当前输入与快照一致；用户确认验证通过 |
| 二维 Physics、自动本征应变和位移 BC | PASS | bottom_y 已修正为 disp_y；输入与快照核对通过 |
| SMP/LU、Exodus-only、工作流与远程提交 | PASS | 用户确认已完成远程任务执行 |
| 幂等同步、保存重开和项目隔离 | 同步一致性 PASS；其他专项待补证据 | 导出后再次同步仍匹配快照；重开/隔离专项记录未单独提供 |
| 远程输入检查及 hc_moose-opt Job | PASS（用户确认闭环） | Job ID、原始输入检查/求解日志及身份记录待归档 |
| LIMS 下载与 CAE 目录导入 | PASS（用户确认） | 用户确认执行后导入结果均 OK；结果目录与截图待归档 |
| 四个场量、时间帧、约束和物理趋势 | 后处理整体 PASS；逐项证据待补 | 用户确认导入和验证 OK；MC02-10～13 未单独提供 |
| 人工数值抽查 / 全场比较（分开记录） | 待补证据 | 尚未提供比较数据与误差，不声明全场 Exodiff PASS |

发生以下问题时记录原始证据并停止相应阶段：Type/模板缺失、pin 不可选择、生成三维位移、材料残留异类参数、重复有限应变计算、MC02 被要求创建 line sampler、SMP.full 未生成、快照过期、unknown solver_id、输入检查失败、求解不收敛或结果变量缺失。界面缺口填 BLOCKED，真实计算/结果不符合填 FAIL；未执行的项不填 PASS。

#### 5.14.1 本次人工闭环记录（2026-10-06）

用户确认：“已经验证通过了，并且提交了计划任务执行后，又导入结果，都是 OK 的”。据此记录 MC02 的 CAE 建模、生成输入、快照、远程执行和结果导入闭环通过。该记录来自用户人工验收，不代表 AI 执行了自动测试或全场数值比较。

| 追溯项 | 本次记录 |
|---|---|
| 项目 | `/Users/a123/Desktop/test/moose/workspace/1005/roundb-mc02.gmp.yaml` |
| 已核对快照 | `.work/case/roundb-mc02/case-20261005-234323` |
| 快照导出时间 | 2026-10-05 23:43:23（北京时间） |
| 应用 / mapping / profile | `hc_moose-opt` / `1.2.0` / `0.3.0`；保持 prototype |
| 输入 SHA-256 | `fe6998373900bfe12535a9b6149c18b185ca527bcae44612d11550f5d0c2a43d` |
| 网格 SHA-256 | `004b78a98666af7bce245976cf597598a06d4e16ca72379f18c0ec3d0623c6f3` |
| 网格组成 | 121 节点、100 QUAD4 域单元、40 LINE2 边界单元、1 POINT pin |
| 提交前核对 | 保存项目与当前输入一致；网格引用归一化后与快照输入一致；输入、网格哈希均匹配清单 |
| 用户执行结果 | 人工验证通过、远程任务执行后结果导入 OK |
| 待归档证据 | 实际 Job ID、执行身份/日志、结果目录、截图及数值比较数据；本次未单独提供，不填写推测值 |

本轮改动：新增 MC02 参考网格与 pin 点组；补齐 HeatSource、热膨胀、有限应变弹性应力模板和 HC mapping；生成二维位移与自动本征应变 Physics；允许 Exodus-only 工作流；保存/重开恢复点组候选；修复可选下拉空值回显和常量模板清除 function。人工走查发现的 bottom_y 误配为 T 已由用户在界面改为 disp_y，修正进入上述快照。

## 6. MC03：二维无摩擦接触

### 6.1 CAE 预处理

1. 新建两根平行立柱，每根宽 `0.5`、高 `5`，中间间隙 `0.2`。
2. 每根立柱使用 `5 x 15` 的二维网格，竖向 bias 为 `0.9`。
3. 建立两个体组、两个底面组、两个外侧受压面组，以及面向间隙的 `pillar1_right` 和 `pillar2_left`。
4. 创建有限应变 QuasiStatic Physics，位移为 `disp_x disp_y`，输出 von Mises。
5. 创建线弹性材料：`E=1e9`、`nu=0.3`，使用有限应变弹性应力。
6. 两根立柱底部的 `disp_x=0`、`disp_y=0`。
7. 创建函数 `pressure_curve=1e4*t^2`，在两个外侧面上施加指向间隙的压力。
8. 创建 Contact：`primary=pillar1_right`、`secondary=pillar2_left`、`model=frictionless`、`formulation=penalty`、`penalty=1e9`、`normalize_penalty=true`。
9. 创建 Transient Step：`0～5`、`dt=0.5`、`solve_type=NEWTON`、`line_search=none`，LU，SimplePredictor `scale=1`。
10. 启用 Exodus，`file_base=step01_out`。

**阻断闸门 MC03-G1**：必须确认当前 HC profile 对二维 Contact、双体 FileMesh 和 SimplePredictor 有正式映射。现有三维 Contact 表单通过不等于本例自动通过。

### 6.2 `.i` 语义检查与后处理

- Contact 必须引用两个不同的命名面组，且为 frictionless + penalty `1e9`。
- 两个压力方向相反，但都把立柱压向中间；不得因面法线误判而向外加载。
- 下载并导入 `step01_out.e`，播放 `0～5`。
- 查看变形和 von Mises：两柱向中间靠拢，进入接触后不得明显互相穿透；日志不得出现 `Solve Did NOT Converge`。

## 7. MC04：三维 J2 各向同性塑性

### 7.1 CAE 预处理

1. 新建单位立方体，使用一个三维体单元。Round A 中的 `1x1x1cube.e` 只是官方输入网格，不是求解输出。
2. 建立顶面、X 固定面、Y 固定面和 Z 固定面的命名 Physical Groups。
3. 创建有限应变 QuasiStatic Physics，位移为 `disp_x disp_y disp_z`，输出 `stress_yy plastic_strain_xx plastic_strain_yy plastic_strain_zz`。
4. 创建函数 `top_pull=0.0625*t`。
5. 创建硬化函数 `hardening_function`：

   | x | y |
   |---|---|
   | `0 0.001 0.003 0.023` | `50 52 54 56` |

6. 创建塑性材料：`E=2.1e5`、`nu=0.3`、`yield_stress=50`，引用 `hardening_function`。CAE 应将“各向同性塑性”展开为弹性张量、`IsotropicPlasticityStressUpdate` 和 `ComputeMultipleInelasticStress`。
7. 在顶面对 `disp_y` 施加 `top_pull`；分别约束 X/Y/Z 对应面的 `disp_x/disp_y/disp_z=0`。
8. 创建 Transient Step：`0～0.075`、`dt=0.00125`、`dtmin=0.0001`、`solve_type=PJFNK`，并按官方参考保留非线性容差和迭代上限。
9. 启用 Exodus，将单元场以节点方式显示，`file_base=isotropic_plasticity_finite_strain_out`。

**阻断闸门 MC04-G1**：用户只选择一个“各向同性塑性”材料，生成器必须自动展开完整材料链。不应让用户在高级参数中手工组装三个 MOOSE 对象。

### 7.2 `.i` 语义检查与后处理

- 硬化曲线、屈服应力、塑性更新器和多非弹性应力链必须完整引用。
- 下载并导入 `isotropic_plasticity_finite_strain_out.e`；`1x1x1cube.e` 不能当作计算结果。
- 播放 `0～0.075`，查看 `stress_yy`和三个塑性应变分量；屈服后塑性应变应从 0 发展，应力趋势与硬化曲线一致。

## 8. MC05：三维 Newmark 动力学

### 8.1 CAE 预处理

1. 新建 `0.1 x 1.0 x 0.1` 的三维细长杆，建立体组 `bar`、顶面 `top` 和底面 `bottom`。
2. 创建 `disp_x disp_y disp_z`，以及对应的 `vel_x/y/z`、`accel_x/y/z`辅助变量；增加单元级 `stress_yy`、`strain_yy`。
3. 创建 Solid Mechanics 小应变 Physics，弹性参数为 `E=210`、`nu=0`，密度 `7750`。
4. 三个位移方向均增加 InertialForce，Newmark 参数 `beta=0.25`、`gamma=0.5`，引用各自的速度和加速度变量。
5. 为每个方向创建 NewmarkAccelAux 和 NewmarkVelAux；使用 RankTwoAux 生成 `stress_yy` 和 `strain_yy`。
6. 固定 `top` 的三个位移方向。
7. 创建压力函数：`x='0 0.2 1 5'`、`y='0 0.2 1 1'`，系数 `1e3`；在 `bottom` 施加压力。
8. 创建 Transient Step：`0～2`、`dt=0.1`。
9. 创建历史输出：底部位移、速度、加速度，以及平均 `stress_yy`、`strain_yy`。
10. 启用 Exodus，`file_base=newmark_out`。

**阻断闸门 MC05-G1**：InertialForce、NewmarkAccelAux、NewmarkVelAux 和对应历史输出必须全部由结构化 UI 生成，不得只配置 Transient Step 就称为 Newmark 动力学。

### 8.2 `.i` 语义检查与后处理

- 三组位移-速度-加速度引用必须一一对应，`beta=0.25`、`gamma=0.5`。
- 压力必须作用于 `bottom`，`top` 三向固定，时间范围 `0～2`。
- 下载并导入 `newmark_out.e`。
- 播放位移场，查看 `stress_yy` 和 `strain_yy`；在历史曲线中切换底部位移、速度、加速度，确认三者有时间相位和幅值关系，不是空曲线或同一数据的重复命名。

## 9. 验收记录

每个算例只有全部行通过才可标记 `PASS`。界面能力未交付填 `BLOCKED`，计算或结果不符合填 `FAIL`。

| 用例 | CAE 预处理 | `.i` 语义 | `--check-input` | 远程 Job | LIMS 下载 | CAE 后处理 | Job ID / 备注 |
|---|---|---|---|---|---|---|---|
| MC01 | PASS | PASS | PASS | PASS | PASS | PASS | 用户 2026-09-29 确认；job_20260929_081949_gxr4ra |
| MC02 | PASS | PASS | PASS（用户确认） | PASS | PASS | PASS | 用户 2026-10-06 确认人工闭环；Job ID 与数值比较证据待归档，见 5.14.1 |
| MC03 | BLOCKED | 待验证 | 待验证 | 待验证 | 待验证 | 待验证 | MC03-G1 |
| MC04 | BLOCKED | 待验证 | 待验证 | 待验证 | 待验证 | 待验证 | MC04-G1 |
| MC05 | BLOCKED | 待验证 | 待验证 | 待验证 | 待验证 | 待验证 | MC05-G1 |

### 9.1 通过标准

1. 五个项目都从空项目通过 CAE 结构化操作建立。
2. 生成 `.i` 满足 CAE Model Semantic 和 MOOSE Physics Semantic 等价。
3. 五个 Job 均由 `hc_moose-opt` 运行至 `succeeded`，无串用求解器。
4. 五个结果目录均可由 LIMS 下载并由 CAE 导入。
5. 各例指定的场量、时间步和历史曲线可查看，物理趋势正确。
6. MC01、MC02、MC04、MC05 按各自官方 gold 冻结数值容差；MC03 按收敛、接触响应和无明显穿透准出。容差尚未冻结前，不得只凭视觉类似宣告数值通过。

## 10. 与 Round A 的边界

Round A 已证明同一 `hc_moose-opt` 可以直接求解五个官方输入，其产物是 Round B 的参考答案。Round B 需要另外证明：CAE 能从工程语义创建等价输入，并经真实产品链路完成求解和后处理。

因此，将 Round A 的 `.i` 直接提交给 C06，或把 Round A 的 `.e` 直接导入 CAE，可用于单独检查路由/读取器，但不计入五个算例的 Round B 闭环通过记录。
