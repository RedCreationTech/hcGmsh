# MOOSE 官方算例 Round B CAE 人工复刻与闭环验证手册

> 版本：2026-09-28 v3
> 计算节点：`192.168.0.138`
> 应用：`HC MOOSE CAE [prototype]` / `hc_moose-opt`
> 参考产物：`examples/hcMooseApp/qualification/MC01～MC05/`

## 1. Round B 的验收目标

Round B 不只是验证求解器路由。完整的验收对象是 MC01～MC05 五个官方算例，每个算例都必须完成同一条人工闭环：

```text
空项目选择 HC MOOSE CAE
  -> 通过 CAE 图形化操作完成几何、网格、材料、物理、边界、分析步和输出
  -> 结构化生成 .i
  -> 导出不可变 Job Snapshot
  -> 通过 LIMS/C06 提交到 192.168.0.138
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
| MC01 瞬态热传导 | 阻断 | 二维面网格、`HeatConductionTimeDerivative`、`HeatConductionMaterial`、`LineValueSampler` 和精确输出合同 |
| MC02 热-结构耦合 | 阻断 | MC01 能力，再加 HeatSource、热膨胀和热-力耦合材料链 |
| MC03 无摩擦接触 | 部分具备 | 精确的双立柱二维网格、二维 Contact 支持、预测器及结果量 |
| MC04 J2 各向同性塑性 | 部分具备 | `IsotropicPlasticityStressUpdate` 及自动材料链、塑性应变场输出 |
| MC05 Newmark 动力学 | 阻断 | InertialForce、Newmark 速度/加速度辅助链、动力历史输出 |

人工执行到标注为“阻断闸门”的界面能力不存在时，记录为 `BLOCKED`并停止该例；不应选一个近似对象继续。

## 3. 通用验证流程

### 3.1 环境与证据

1. 启动 LIMS API，CAE 的 Server 使用 `http://127.0.0.1:8200`。
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

### 4.1 用例身份与当前结论

| 项目 | 内容 |
|---|---|
| 人工测试编号 | `TEST-MOOSE-B-MC01-01` |
| 官方输入 | `examples/hcMooseApp/qualification/MC01/therm_step03.i` |
| Round A 参考结果 | `examples/hcMooseApp/qualification/MC01/therm_step03_out.e` |
| Round A 参考曲线 | `examples/hcMooseApp/qualification/MC01/therm_step03_out_t_sampler_0006.csv` |
| 目标项目文件 | `roundb-mc01.gmp.yaml` |
| 目标求解器 | `hc_moose-opt` |

官方算例的物理问题是：一块长 `2`、高 `1` 的二维材料，初始温度均为 `300`；左边始终保持 `300`，右边温度按 `300+5*t` 上升；在 `0～5` 内以 `dt=1` 做瞬态热传导计算。

当前 CAE 版本还不能从空项目完成本例闭环。本节因此分为两条路径：

- **当前诊断路径**：导入 Round A 的 Exodus 作为输入网格，实际操作已有的结构化表单，用于确认项目、分组、变量、函数、边界、分析步、作业和结果窗口；它不是正式验收。
- **正式验收路径**：必须从空项目建立二维网格，并用专用结构化表单建立全部热传导对象。第 4.11 节列出了当前阻断项。

> 执行规则：找不到文档指定的对象类型或按钮时，立即记录对应的 `BLOCKED` 编号。不使用 Custom Block，不粘贴官方 `.i`，不用名称相近但物理意义不同的对象替代。

### 4.2 新建项目并选择应用

1. 启动 GMP-ISE，在顶部“应用”下拉框选择 `HC MOOSE CAE [prototype]`。
2. 选择“文件 -> 新建项目”，保存为 `roundb-mc01.gmp.yaml`。
3. 查看模型树，确认至少能看到 `Variables`、`Functions`、`Materials`、`Steps`、`BC`、`Loads`、`Outputs`、`Mesh`、`Jobs` 和 `Results`。
4. 再次查看顶部“应用”，确认没有回退为 `DamSafetyApp-opt`或 `combined-opt`。
5. 记录截图 `MC01-01-project-profile.png`，画面中应同时包含项目名和 HC 应用名。

**预期结果**：项目成功保存，顶部应用始终为 `HC MOOSE CAE [prototype]`。

### 4.3 准备二维网格

#### 4.3.1 正式验收应建立的网格

正式的 CAE 操作入口应建立以下模型：

| 项目 | 目标值 |
|---|---|
| 几何 | XY 平面矩形，左下角 `(0,0,0)`，右上角 `(2,1,0)` |
| 网格 | X 方向 10 段，Y 方向 10 段，共 100 个四边形单元、11×11 个节点 |
| 二维域 | 单一材料域；可命名为 `domain`，但 MC01 的全局材料不依赖该名称 |
| 边界组 | `left`、`right`、`top`、`bottom` |

当前草图可绘制矩形，但尚无“草图平面 -> 二维 Gmsh 面网格”的正式通路；网格模块的 Box 是三维实体，不能用它的外表面代替本例的二维域。因此当前记录 `BLOCKED MC01-GEO-01`。

#### 4.3.2 当前版本可执行的诊断网格路径

1. 进入 `Mesh` 模块，选择“网格 -> 导入 Exodus 网格...”（`Import Exodus Mesh...`）。
2. 选择 `examples/hcMooseApp/qualification/MC01/therm_step03_out.e`。
3. 等待导入完成，查看网格摘要：应为二维、`121` 个节点、`100` 个 `QUAD4` 单元。
4. 在 Physical Groups/分组清单中确认 `left`、`right`、`top`、`bottom` 都存在。
5. 保存项目，记录截图 `MC01-02-diagnostic-mesh.png`。

**预期结果**：可以在 CAE 中选择左右边界并继续测试后续表单。

**限制**：该 `.e` 是 Round A 已求解结果。把它作为输入网格只是诊断手段，不能计入“从空项目一比一复刻”的正式验收证据。

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

#### 4.7.1 密度材料：当前可操作

1. 右键 `Materials`添加材料，重命名为 `density`。
2. 在属性中将 Type 设为 `GenericConstantMaterial`。
3. 填写 `prop_names = density`，`prop_values = 8000`。
4. 顶部“可选物理体”就是材料的域/block 指派入口，不会再显示一个名为“域”的参数。
5. 使用第 4.3.2 节的诊断网格时，列表中会出现 `Unnamed block ID: 0`。这表示 Exodus 中已识别到唯一的未命名单元块，不是网格缺失。
6. 本例保持不选任何物理体，使“待应用选择”显示“（无）”。不需要点选 `Unnamed block ID: 0`，也不需要为它补造 `domain`。
7. 进入“预览”或后续检查生成 `.i`：`density` 块应只有 `type`、`prop_names`、`prop_values`，不应生成 `block = ...`。省略 `block` 表示材料对全部网格块生效，与官方 MC01 输入一致。

**截图判定**：当前界面显示 `Unnamed block ID: 0` 且“待应用选择：（无）”，符合 MC01 预期，可继续执行第 4.7.2 节。

#### 4.7.2 热传导项：当前阻断

正式模型必须通过结构化 UI 创建下列三个对象：

| 对象名 | MOOSE Type | 参数 |
|---|---|---|
| `thermal` | `HeatConductionMaterial` | `thermal_conductivity=45`，`specific_heat=0.5` |
| `heat_conduction` | `HeatConduction` | `variable=T` |
| `time_derivative` | `HeatConductionTimeDerivative` | `variable=T` |

当前版本的 Load/Kernel 类型中可看到 `HeatConduction`，但没有 `HeatConductionTimeDerivative`；Material 类型中也没有 `HeatConductionMaterial`。因此：

1. 在左侧模型树中找到“载荷”（`Loads`）根节点。
2. 右键“载荷”，选择“添加载荷”；也可先单击“载荷”，再点模型树上方的 `+` 按钮。
3. 将新建对象重命名为 `heat_conduction`，双击它打开属性窗口。
4. 进入“参数”页，在“快捷参数”中将“类型”选择为 `HeatConduction`，将“变量”选择为 `T`。
5. 勾选“高级参数”。新建载荷最初会携带 `BodyForce` 的默认 `value=0`；切换为 `HeatConduction` 后如果该行仍存在，选中 `value` 行并点击“移除”（`Remove Param`）。
6. `Value`、`Factor`、`Function`、`Component`、`Diffusivity` 和 `Displacements` 都不属于该 Kernel，最终高级参数表中不应有这些键。
7. 顶部“可选物理体”保持不选，使 Kernel 对唯一网格域全局生效。
8. 切换到“预览”，应只看到对象名 `heat_conduction`、Type=`HeatConduction`、Variable=`T`，不应有 `value` 或 `block`。
9. 这个对象虽然在 CAE 树中位于“载荷”，但生成器会将它写入 MOOSE `[Kernels/heat_conduction]`，不会写成边界载荷。
10. 保存项目。由于同一“类型”下拉框中找不到 `HeatConductionTimeDerivative`，记录 `BLOCKED MC01-PHY-01`并停止正式求解流程。
11. **不要**用普通 `TimeDerivative` 替代 `HeatConductionTimeDerivative`；后者会正确引用密度和比热，两者物理语义不同。
12. **不要**把导热系数和比热塞进任意材料类型，或通过 Custom Block 绕过表单。

**截图判定**：Type=`HeatConduction` 和 Variable=`T` 已正确，但高级参数中如仍有 `value=0`，只能判定为“待清理”；删除 `value` 后才算完成当前可执行的 `heat_conduction` 诊断建立。

### 4.8 创建瞬态分析步

1. 进入 `Step` 模块，点击“瞬态步”（`Add Transient Step`），将对象重命名为 `heat_transient`。
2. 双击该分析步，填写 Start Time `0`、End Time `5`、Time Step `1`。
3. 展开“高级参数”，本例应保留的核心语义只有：

   | Key | Value |
   |---|---|
   | `type` | `Transient` |
   | `start_time` | `0` |
   | `end_time` | `5` |
   | `dt` | `1` |

4. `scheme=implicit-euler` 是 MOOSE 的默认时间积分方式；当前生成器会自动补出该行，与官方算例语义一致，可保留。
5. 当前通用 Transient 模板还会带入大量本例不需要的默认参数。在高级参数表中删除下列键：

   | 类别 | 需删除的 Key | 原因 |
   |---|---|---|
   | 自适应时间步 | `timestepper_type`、`optimal_iterations`、`iteration_window`、`growth_factor`、`cutback_factor`、`dtmin`、`dtmax` | 会把官方的固定 `dt=1` 改成可变时间步，必须删除 |
   | 额外求解控制 | `num_steps`、`solve_type`、`line_search`、`automatic_scaling`、`nl_rel_tol`、`nl_abs_tol`、`nl_max_its` | 官方 MC01 未显式指定，本例应使用 MOOSE 默认值 |
   | PETSc/预处理 | `petsc_options_iname`、`petsc_options_value`、`preconditioning_type`、`preconditioning_full` | 通用非线性结构求解默认，不是 MC01 官方输入的一部分 |

6. 删除后，高级参数表应只保留 `type`、`start_time`、`end_time`、`dt`，可选保留 `scheme=implicit-euler`。
7. 切换到“预览”，再切回“参数”，确认已删除的参数没有自动恢复；然后点击“确定”、保存项目，重新打开该 Step 再检查一次。

**本次实测记录**：界面默认预览中出现 `IterationAdaptiveDT`、全套非线性求解参数、PETSc LU/MUMPS 和 SMP，但 Validation 仍显示 `No issues`。这只说明参数形式合法，不代表它与官方 MC01 语义等价。

2026-09-28 人工删除多余高级参数后，Input Preview 已只保留 `type=Transient`、`start_time=0`、`end_time=5`、`dt=1`、`scheme=implicit-euler`，该预览语义符合 MC01。删除后快捷表单仍可显示旧的 `preconditioning_full=true` 等值，而高级参数表和 Input Preview 已不包含它们；这是界面同步显示问题，生成语义暂以 Input Preview 为准。

**阻断判定**：当前 Step 对象可用于继续人工走查，但该现象已记录为 `BLOCKED MC01-STEP-01`。如果表单强制恢复上述参数，不要用它提交正式 MC01 求解。

### 4.9 创建中心线采样与输出

正式验收应创建以下对象：

| 类别 | 对象/参数 |
|---|---|
| VectorPostprocessor | `t_sampler`，Type=`LineValueSampler`，Variable=`T` |
| 采样线 | Start=`0 0.5 0`，End=`2 0.5 0`，`num_points=20`，`sort_by=x` |
| Exodus | 启用，生成 `therm_step03_out.e` |
| CSV | `file_base=therm_step03_out`，`execute_on=final` |

当前模型树没有 `VectorPostprocessors` 结构化根节点，且现有 mapping 只有 `NodalValueSampler`，没有本例需要的 `LineValueSampler`；当前 Outputs 套餐也不能精确表达“Exodus 常规输出 + CSV 仅在 final 执行”。

#### 4.9.1 当前版本的人工取证操作

1. 检查左侧模型树：确认没有 `VectorPostprocessors` 或“线采样”根节点/模块。记录 `BLOCKED MC01-VPP-01`。
2. 展开“输出”（`Outputs`）节点。为了检查现有界面，右键“输出”添加一个临时对象，重命名为 `mc01_outputs_probe`。
3. 双击 `mc01_outputs_probe`，进入“参数”，滚动到“Output Files/输出文件”区域。
4. 确认可以看到 Exodus、CSV 和 `file_base`；但界面中没有分别为 Exodus/CSV 设置 `execute_on` 的入口，也没有只针对 CSV 选择 `final` 的控件。
5. 可为取证将 Exodus 设为 `true`、CSV 设为 `true`、`file_base` 填为 `therm_step03_out`，然后查看“预览”。当前生成器不会由这三个字段单独生成官方所需的 `CSV(execute_on=final)` 合同。
6. 记录截图 `MC01-04-output-gap.png` 和 `BLOCKED MC01-OUT-01`。
7. 取证完成后删除 `mc01_outputs_probe`，不要将它作为正式 MC01 对象保留。

#### 4.9.2 停止规则

- 不用 `NodalValueSampler` 替代 `LineValueSampler`。
- 不手改生成 `.i` 的 Outputs 块。
- 不通过启用与 MC01 无关的 History/Times 套餐来迫使 CSV 生成。
- 待专用入口完成后，按本节首表建立正式对象，再继续第 4.10 节。

#### 4.9.3 2026-09-28 人工实测结论

对 `mc01_outputs_probe` 的界面走查已确认：

- “场输出变量”只提供 `DamageC`、`DamageT`、`kappa_c`、`kappa_t` 及 CDP 迭代诊断量，它们与 MC01 无关，应全部保持未勾选。温度 `T` 作为主变量可由 Exodus 默认写出，不需要伪装成 CDP 场输出。
- “历史输出”只提供边界反力、边界平均位移和场量极值，不能建立 `T` 的中心线采样。
- “输出时间”可启用 Times，但它不是 `LineValueSampler`，也不能替代 CSV 的 `execute_on=final`，因此保持 `false`。
- “输出文件”确实可设 Exodus、CSV 和 `file_base`，但没有为 CSV 设置 `execute_on=final` 的控件；开启 CSV 也不会自动产生 `LineValueSampler` 数据。
- 顶部“可选边界分组”与本例全域 Exodus/中心线采样无关，保持“待应用选择：（无）”是正确的。

**最终判定**：4.9 已完成当前版本能做的全部人工取证，结论为 `BLOCKED MC01-VPP-01` + `BLOCKED MC01-OUT-01`。删除临时 `mc01_outputs_probe`后停止，不执行第 4.10 节的生成、快照和远端提交。

#### 4.9.4 当前同步 `.i` 与提交决策

2026-09-28 实际操作日志已记录项目新建、Exodus 诊断网格导入、`T`、函数、两个 BC、`density`、`heat_conduction`、`heat_transient` 的添加/编辑，以及 `mc01_outputs_probe` 的添加后删除。日志中没有 `Check Input`、Job Snapshot 或远端提交成功记录。

当前结构化同步的 `.i` 已正确生成 Variable、Function、两个 BC、密度、`HeatConduction` 和固定时间步，但仍不可提交：

| 检查项 | 当前 `.i` | 影响 |
|---|---|---|
| `HeatConductionTimeDerivative` | 缺失 | 没有热容量/瞬态项，不是目标瞬态热传导 |
| `HeatConductionMaterial(k=45, cp=0.5)` | 缺失 | `HeatConduction` 缺导热材料属性，可能在输入检查/材料属性初始化时失败 |
| `LineValueSampler` | 缺失 | 无法生成中心线 20 点数据 |
| `[Outputs]` | 缺失 | 无法产生预期 Exodus 和 final-only CSV |
| Mesh 引用 | macOS 绝对路径 | 计算节点不能直接访问；正式快照必须打包网格并改为包内相对路径 |
| 网格来源 | Round A 已求解 `.e` | 只是表单诊断网格，不能作为从空项目复刻的正式证据 |

**决策**：

- 不点击“运行”或提交远端 Job。
- 不切换高级/自定义输入模式手工补写官方 block；这样可以测求解器，但不能证明 CAE 结构化能力，而 Round A 已经完成求解器本身的直接实算验证。
- 先完成第 4.11 节的五个结构化缺口修复，再从 CAE 重新生成 `.i`、导出快照并提交 `hc_moose-opt`。

### 4.10 从保存复查到远端求解

> 本节是正式闭环操作。只有第 4.11 节所有阻断项解决后才执行；当前同步的残缺 `.i` 不得提交，也不得通过高级/自定义输入手工补块后冒充结构化验收。

#### 4.10.1 保存、重开与生成 `.i`

1. 保存项目，关闭项目后重新打开 `roundb-mc01.gmp.yaml`。
2. 逐项复查 `T`、`right_temperature`、两个 BC、两个 Kernel、两个 Material、Transient Step、`t_sampler` 和 Outputs，确认名称、引用和数值未丢失。
3. 进入 `Job`模块，点击“打开作业工作区”（`Open Job Workspace`）。
4. 点击“校验工作流”（`Validate Workflow`）；要求 `0 errors`。如有 warning，逐条记录，不直接忽略。
5. 点击“同步到输入”（`Sync to Input`）。
6. 在 MOOSE 工作窗的“输入文件”页查看 `Generated Input` 和 `Generation Report`。输入模式必须是 Structured/结构化。
7. 再次执行“同步到输入”，确认生成内容稳定，没有随机重命名或引用漂移。
8. 点击“写入输入文件”（`Write Input`），将生成文件保存为 `therm_step03.i`。
9. 按第 4.12 节检查语义，然后点击“检查输入”（`Check Input`）。

#### 4.10.2 导出快照并提交计算节点

1. 在“执行配置”页中，Runner 选择 Remote Job/LIMS Facade。
2. Server 填写当前 LIMS API 地址，本地默认为 `http://127.0.0.1:8200`；Project ID 选择本次验证项目。
3. 确认目标求解器为 `hc_moose-opt`，不是 `dam-safety-app`。
4. 点击“导出任务快照”（`Export Job Snapshot`），记录快照目录。
5. 打开快照制品清单，确认包含 `.i`、输入网格、`manifest.json`，且 `.i` 引用的网格为快照内相对路径。
6. 点击“提交作业”（`Submit Job`），记录 Job ID。
7. 点击“刷新状态”（`Refresh Status`），直到状态为 `succeeded`。`running` 不算完成。
8. 在 Job 详情中确认 requested solver id 和 resolved solver id 都为 `hc_moose-opt`，应用为 `HcMooseApp`，计算节点为 `192.168.0.138`。

#### 4.10.3 通过 LIMS 下载并导入 CAE Results

1. 在 Job 工作区点击“刷新文件”（`Refresh Files`）。
2. 在制品清单中至少勾选 `therm_step03_out.e`、`therm_step03_out_t_sampler_0006.csv`、求解日志和作业 manifest。
3. 点击“下载选中项”（`Download Selected`）。默认下载目录为 `~/Downloads/gmp_remote/<job_id>/`。
4. 进入 `Results` 模块，点击“导入结果文件...”（`Import Result File...`），先选择刚下载的 `.e`。
5. 在场变量中选择 `T`，从 `t=0` 逐帧播放到 `t=5`，记录首帧和末帧截图。
6. 再次点击“导入结果文件...”，选择下载的 CSV，切换到表格/曲线视图查看 `T-x` 中心线数据。
7. 不要直接导入 `examples/` 中的 Round A 文件作为 Round B 结果；Round B 证据必须来自刚记录的 Job ID。

### 4.11 当前阻断清单与开发准出

| 编号 | 当前缺口 | 开发后的可见准出条件 |
|---|---|---|
| `MC01-GEO-01` | 无草图/参数矩形到二维面网格的通路 | 能从空项目生成 `2 x 1`、`10 x 10` 网格及四条命名边界 |
| `MC01-PHY-01` | 缺 `HeatConductionMaterial` 和 `HeatConductionTimeDerivative` 表单/mapping | 类型可选，参数可保存重开，生成引用正确 |
| `MC01-STEP-01` | 通用 Transient 模板强制带入自适应步长、非线性控制、PETSc 和 SMP，校验不提示与 MC01 语义不等价；从高级表删除后，快捷表单仍显示旧值 | 用户只需设置 `0/5/1`，生成固定 `dt=1`的最小 Executioner；不相容模板需警告或自动清理，快捷/高级/预览三处显示必须一致 |
| `MC01-VPP-01` | 无 `VectorPostprocessors/LineValueSampler` 结构化入口；现有场/历史输出只有 CDP 诊断量和结构反力/位移/极值 | 可设置变量、起终点、采样点数和排序方式 |
| `MC01-OUT-01` | Outputs 虽有 Exodus、CSV、`file_base`，但无 CSV 专属 `execute_on=final`；开启 CSV 不会补出线采样数据 | 能生成 Exodus，且 CSV 为 `file_base=therm_step03_out`、`execute_on=final` |

只有五项全部解决，并通过 `moosecase_mc01_transient_heat_contract` 定向测试，才可把 MC01 从 `BLOCKED` 改为可人工验收。

### 4.12 生成 `.i` 的语义检查

不要按行和排版与官方 `.i` 做文本比对，应核对下列语义：

- Mesh 引用快照内的 Gmsh/Exodus 网格，网格物理语义等价于官方 `GeneratedMeshGenerator(dim=2, nx=10, ny=10, xmax=2, ymax=1)`。
- `[Variables/T]` 存在，且 `initial_condition=300`。
- `HeatConduction(variable=T)` 和 `HeatConductionTimeDerivative(variable=T)` 同时存在。
- `HeatConductionMaterial` 的导热系数为 `45`、比热为 `0.5`；`density` 为 `8000`。
- `t_left` 为 `DirichletBC(T=300, boundary=left)`。
- `t_right` 为 `FunctionDirichletBC`，引用等价于 `300+5*t` 的函数，边界为 `right`。
- Executioner 为 `Transient`，`start_time=0`、`end_time=5`、`dt=1`，不应启用自适应时间步。
- `LineValueSampler` 采样 `T`，从 `(0,0.5,0)` 到 `(2,0.5,0)`，`num_points=20`，`sort_by=x`。
- Exodus 和 CSV 均存在，CSV 的输出前缀为 `therm_step03_out` 且只在 `final` 执行。

### 4.13 预期产物与数值验收

| 文件 | 必须存在 | 用途 |
|---|---|---|
| `therm_step03.i` | 是 | CAE 结构化生成的求解输入 |
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

### 5.1 CAE 预处理

1. 以新的空项目建立与 MC01 相同的 `2 x 1`、`10 x 10` 网格；另建立底边界 `bottom` 和左下角点组 `pin`。
2. 创建 `T`，初始值 300，并使用 MC01 的热传导、材料、密度和两个温度边界。
3. 增加体热源：`HeatSource`，`variable=T`，`value=5e4`。
4. 创建二维 QuasiStatic 有限应变 Physics，自动创建 `disp_x disp_y`，输出 von Mises 应力。
5. 创建材料链：

   | 对象 | 关键参数 |
   |---|---|
   | ComputeIsotropicElasticityTensor | `youngs_modulus=1e9`，`poissons_ratio=0.3` |
   | ComputeThermalExpansionEigenstrain | `thermal_expansion_coeff=0.001`，`stress_free_temperature=300`，`temperature=T` |
   | ComputeFiniteStrain | 有限应变，引用热膨胀本征应变 |
   | ComputeFiniteStrainElasticStress | 弹性应力 |

6. 约束 `pin` 的 `disp_x=0`，约束 `bottom` 的 `disp_y=0`。
7. 创建 Transient Step：`0～5`、`dt=1`，SMP `full=true`，线性求解使用 LU。
8. 启用 Exodus，`file_base=thermomech_step01_out`。

**阻断闸门 MC02-G1**：必须通过 MC01-G1，且 HeatSource、热膨胀材料链、二维点组约束均可由 UI 建立。

### 5.2 `.i` 语义检查与后处理

- 热场必须同时作为热膨胀的温度耦合量；不能是两个无引用关系的 Physics。
- 位移、有限应变、热膨胀和 von Mises 输出必须共存。
- 下载并导入 `thermomech_step01_out.e`。
- 分别查看 `T`、`disp_x`、`disp_y` 和 von Mises；温度随时间上升，位移/应力对热载荷产生响应，且约束位置不得出现刚体漂移。

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
| MC01 | BLOCKED | 待验证 | 待验证 | 待验证 | 待验证 | 待验证 | MC01-G1 |
| MC02 | BLOCKED | 待验证 | 待验证 | 待验证 | 待验证 | 待验证 | MC02-G1 |
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
