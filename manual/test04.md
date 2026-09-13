可以按下面流程执行。建议先“另存为”一个验收项目，因为测试中会临时删除材料节点。

## 一、准备一个体物理组

如果 Section 的“物理体”列表已经有 `solid`、`unassigned_3d` 等三维组，可以直接跳到第二部分。

如果没有：

1. 在模型树展开 `Mesh`，双击对应网格子节点。
2. 进入“分组与网格场”页。
3. 打开“物理组”子页。
4. 设置：
   - 维度：`3`
   - 名称：`solid`
5. 在“实体”旁点击“拾取”。
6. 在弹窗中选择目标三维实体，例如 `3:1`，然后确认。
7. 点击“添加”。
8. 检查物理组表格中出现：
   - Dim：`3`
   - Name：`solid`
   - Entities：包含刚才选择的体
9. 重新生成一次网格，确保该物理组写入网格文件。
10. 关闭或收起网格工作窗。

如果使用现有的 `unassigned_3d`，后面的预期值就把 `solid` 替换为 `unassigned_3d`。

## 二、新建并确认 CDP 材料

1. 顶部“模块”选择“材料”。
2. 点击“新建 CDP 材料”。
3. 模型树展开 `Materials`。
4. 记下新材料名称，通常是：
   `cdp_material_1`
5. 选择该材料，打开属性编辑器。
6. 确认类型为 `AbaqusCDP`。
7. 如果之前已经完成 W03-01，可以直接使用已配置好的 CDP 材料。
8. 如果是全新材料，建议把四个 CSV 文件补齐，避免校验页出现与本用例无关的缺失提示：
   - `compression_hardening.csv`
   - `compression_damage.csv`
   - `tension_stiffening.csv`
   - `tension_damage.csv`

文件可从 `tests/fixtures/cdp-v01/` 选择。

### 四个 CSV 的来源与适用范围

- 这四个文件不是本程序临时生成的，而是从 `damASR` 的已验证
  `cdp-v01-single-tension` 算例基线逐字节复制而来；当前副本与该基线的
  SHA-256 一致。它们于 2026-09-10 随提交 `e963aa7d` 引入本仓库，作为
  UI 验收和回归测试数据。
- 更早的数据来源是专家提供的
  `damASR/docs/refs/cdp/基于GB50010-2010规范的CDP参数计算.xlsx`。项目记录确认，
  原始 Abaqus `test.inp` 中的压缩硬化、压缩损伤、拉伸 stiffening 和拉伸损伤
  四张表由该 Excel 生成，再按 MOOSE 输入要求转换为当前四个 CSV。
- 现有可审计记录只注明“专家提供”，没有专家姓名或签字文件，因此不得把这些文件
  归名到某一位具体专家。
- 这组数据被归类为 `mock/prototype`：生成时为了匹配论文中的拉压强度，调整过
  混凝土标号，并不是 Excel 默认的 C30 输出。因此，它适合本验收用例和软件回归，
  不应直接作为正式工程的 C30 材料参数。
- 正式工程计算应由材料专家根据目标混凝土、试验数据或适用规范重新生成并确认四张
  曲线，同时保存对应的 Abaqus `.inp`、Excel 输入快照和文件哈希，建立独立材料基线。

四个文件分别描述：压应力—非弹性应变、压缩损伤—非弹性应变、拉应力—开裂应变、
拉伸损伤—开裂应变。弹性模量、泊松比和 CDP 标量参数不能替代这些非线性材料曲线。

## 三、新建 Section 并完成指派

1. 顶部“模块”切换到“截面”。
2. 点击“新建实体截面”。
3. 模型树展开 `Sections`。
4. 选择新生成的 `section_1`。
5. 进入“属性”模块；也可以右键 `section_1`，选择“编辑属性”。
6. 打开“参数”页。
7. 在“快捷参数”区域确认：
   - 类型：`SolidSection`
   - 材料：从下拉框选择刚才的 `cdp_material_1`
8. 检查上方只读的“已指派物理体”输入框：首次进入时应为空；该输入框用于显示最终
   指派结果，不需要也不能手工填写。
9. 向下找到“可选物理体”区域，在列表中单击 `solid`。
   - 如果要多选，按住 Shift 或 Command。
   - 本用例建议只选一个体组，便于核对。
10. 检查“待应用选择”显示 `solid`，然后点击“应用所选物理体”。
11. 检查：
   - “待应用选择”的 chip 中出现 `solid`
   - 快捷参数里的“已指派物理体”值为 `solid`
   - 点击后页面仍保持当前界面语言，不应整体切换成英文
12. 模型树中 `section_1` 应显示“就绪”，不应显示“失效”。

如果“物理体”列表为空，返回网格工作窗点击“刷新”，确认体物理组存在；必要时重新生成网格后再打开 Section 属性。

## 四、同步并检查 `[Materials]`

1. 执行以下任一操作：
   - 菜单“模型”→“同步模型到 MOOSE 输入”
   - “属性”模块点击“同步模型到输入”
   - 快捷键 `Ctrl+Shift+R`
2. 顶部“模块”切换到“作业”，在模型树中双击具体的 Job 子节点；也可以右键该节点
   选择“打开作业工作窗”。如果当前还没有 Job，先在 `Jobs` 根节点下新建一个作业。
3. 在弹出的“作业工作窗”最上方点击“MOOSE 设置”（与“作业列表”并列的页签）。
4. 在“MOOSE 设置”内部的第二排页签中，点击最右侧的“输入文件”：
   - 第二排依次是“算例设置”“执行配置”“输入文件”。
   - 不要停留在“算例设置”；其中标为“输入文件”的单行路径框只是 `.i` 文件路径，
     不是本步骤要查看的输入内容。
5. 页面切换后会出现“输入编辑器”和一块较大的文本区域。在该文本区域中向下滚动，
   找到 `[Materials]`。当前页面没有单独的搜索框，因此不要在“算例设置”页寻找
   `[Materials]`。
   - `[Materials]` 是下方文本内容中的配置段，不是上方“模板”下拉框中的选项。
   - 本步骤不要切换模板，也不要点击“应用模板”，否则可能用模板内容覆盖刚同步的
     模型输入。
   - 如果模板下拉框已经展开，先按 `Esc` 或单击文本区域将其收起，再拖动文本区域
     右侧滚动条继续向下查找。通常会先看到 `[Mesh]`、`[Variables]`、`[Functions]`，
     `[Materials]` 位于更下方。

根据界面层级，正确路径是：

```text
作业工作窗 → MOOSE 设置 → 输入文件（第二排最右侧）→ 输入编辑器
```

应当看到类似内容：

```text
[Materials]
  [cdp_material_1_elasticity]
    type = ComputeIsotropicElasticityTensor
    block = 'solid'
    ...
  []

  [cdp_material_1_stress]
    type = ComputeMultipleInelasticStress
    block = 'solid'
    ...
  []

  [cdp_material_1_cdp_stress_update]
    type = AbaqusCDPStressUpdate
    block = 'solid'
    ...
  []
[]
```

这里需要确认：

- 三个 CDP 材料子块都存在。
- 三处 `block` 都是 `'solid'`。
- 不得出现 `block = ''`。
- 不得写成其他未选择的体组。

建议此时截一张包含三个子块及其 `block` 的截图。

## 五、验证删除材料后的失效状态

1. 回到模型树。
2. 展开 `Materials` 和 `Sections`。
3. 记住材料的准确名称，例如 `cdp_material_1`。
4. 右键 `cdp_material_1`，选择“删除”。
5. 不要修改 `section_1` 的 Material 字段，否则测试不到悬空引用。
6. 查看模型树：
   - `section_1` 状态应变为“失效”
   - `Sections` 根节点应显示“有问题 (1)”
7. 选择 `section_1`，进入“属性”模块。
8. 打开“校验”页。
9. 点击“刷新”。
10. 检查校验结果应包含：
    - `material reference`
    - 或等价的“材料引用不存在”提示

建议截一张同时包含 `section_1 → 失效` 和校验提示的截图。

## 六、验证同名材料恢复后自愈

1. 切换到“材料”模块。
2. 再次点击“新建 CDP 材料”。
3. 检查新材料名称是否与删除前完全一致，例如 `cdp_material_1`。
4. 如果名称不是原名称：
   - 右键新材料选择“重命名”
   - 改成原来的准确名称
5. 不要重新编辑 `section_1` 的 Material 字段。
6. 查看模型树：
   - `section_1` 应从“失效”恢复为“就绪”
   - `Sections` 根节点不再显示“有问题”
7. 打开 `section_1` 的“校验”页并刷新。
8. `material reference` 问题应消失。
9. 再次“同步模型到 MOOSE 输入”。
10. 确认三个 CDP 材料子块仍然包含：

```text
block = 'solid'
```

最终通过需要四组证据：

- Section 的 Material 下拉列出了 CDP 材料。
- `solid` 已通过“物理体”区域应用到 Section。
- 三个 `[Materials]` 子块均生成 `block = 'solid'`。
- 删除材料后 Section 失效；恢复同名材料后无需重新指派即可自动恢复。

## 七、确认同步会清除旧 diffusion 模板（TEST-P4-W04-01，已通过）

验收记录（2026-09-13）：操作日志出现 `Input updated from Model Tree` 和
`Model tree synced to MOOSE input`；同步后的输入仅保留 `[Mesh/file]` 与模型树当前的
`material_1`，旧 diffusion 示例块已全部消失。本次复测时 `material_1` 为
`GenericConstantMaterial`，因此该证据只关闭 W-04 清理问题，不代表 CDP 材料配置已经完成。

本步骤用于确认输入装配器已经根据模型树清理旧内容。不要在输入编辑器中手工删除旧块。

1. 使用包含本修复的新版本重新打开项目。
2. 在模型树中确认当前仅保留本算例实际创建的对象；完成前六部分后，至少应有：
   - `Materials` 下的 CDP 材料
   - `Sections` 下指派到 `solid` 的 Section
   - `Mesh` 下的有效网格
3. 执行一次“同步模型到 MOOSE 输入”。也可以直接保存项目；保存操作会先执行同一套同步。
4. 打开“作业工作窗”→“MOOSE 设置”→“输入文件”，检查完整文本。
5. 确认仍然存在：
   - `[Mesh/file]` 与当前 `.msh` 路径
   - 三个 CDP 材料子块
   - 三个子块中的 `block = 'solid'`
6. 如果模型树中的 `Variables`、`Functions`、`BC`、`Loads`、`Steps`、`Physics`
   和 `Outputs` 目前都为空，输入中不应再残留以下旧 diffusion 内容：
   - `[Variables]`、`[Functions]`、`[ICs]`、`[Kernels]`
   - `[BCs]`、`[Postprocessors]`、`[Executioner]`、`[Outputs]`
   - `[GlobalParams]`、`[Physics/SolidMechanics/...]`
   - `[AuxVariables]`、`[AuxKernels]`、`[Times/...]`
   - 名为 `diffusion` 的旧材料子块，以及 `u`、`v`、`left`、`right` 等旧示例引用
7. 再同步一次，确认文本没有新增重复块，内容与第一次同步一致。

通过标准：同步和保存都会自动删除模型树已不存在的受管块；用户不需要编辑 `.i` 文本。

> 此时只表示 W-01 Section/CDP 指派与 W-04 清理通过，还不是完整的 CDP 力学输入。
> 后续 W03-03～06 需要通过 UI 补齐 Physics、Step、函数/边界条件和输出套餐。

## 八、补齐 Tensor Mechanics Physics（TEST-P4-W03-05，已通过）

验收记录（2026-09-13）：`physic_1` 保存后显示“就绪”；同步输入包含
`[GlobalParams]`、`[Physics/SolidMechanics/QuasiStatic/physic_1]`、16 项
`generate_output`、`save_in = 'resid_x resid_y resid_z'` 及三个残差 AuxVariable；
连续同步未产生重复块。

1. 在模型树展开 `Physics`，右键根节点，选择“添加 Physics”。
2. 选择新建的 Physics 子节点，打开“属性”→“参数”。
   - Physics 当前只有一套 QuasiStatic 默认参数，新建时会自动填充，因此界面不再显示
     没有实际选择价值的“模板/应用模板”控件。
3. 在快捷参数中确认或填写：
   - Action：`QuasiStatic`
   - Block：在“可选体组”列表选择 `solid`，再点击“应用所选分组”
   - Volumetric Locking Correction：`true`
   - Add Variables：`true`
   - Incremental：`true`
   - Strain：`SMALL`
   - Generate Output：保留界面默认值
   - Save In Resid：`true`
4. 点击“确定”，模型树中的 Physics 子节点应显示“就绪”。
5. 同步到 MOOSE 输入，检查出现：

```text
[GlobalParams]
  displacements = 'disp_x disp_y disp_z'
[]

[Physics/SolidMechanics/QuasiStatic/<Physics 节点名>]
  block = 'solid'
  ...
[]
```

通过标准：`block` 为 `solid`，位移变量由 Physics action 自动建立；重复同步不产生第二个
`[GlobalParams]` 或同名 Physics action。

## 九、补齐分析步（TEST-P4-W03-04）

> 当前阶段可以在项目中保存多个 Step，但 MOOSE 同步只取顺序中的第一个 Step，并在日志中明确告警其余 Step 未串联执行。多 Step 状态继承已记录为下一阶段 `REQ-018` / `TASK-NEXT-001`。

1. 在模型树展开 `Steps`，右键根节点，选择“添加 Steps”。
2. 选择新建的 Step 子节点，打开“属性”→“参数”。
3. 按下列各组确认默认值；本轮验收可先使用默认值，不必为了测试任意修改：
   - 基本：Type=`Transient`，Start Time=`0`，End Time=`1`
   - 求解控制：Solve Type=`NEWTON`，Line Search=`bt`，Automatic Scaling=`true`
   - 非线性：Relative Tolerance=`1e-9`，Absolute Tolerance=`1e-8`，Max Iterations=`50`
   - 时间步：Time Stepper=`IterationAdaptiveDT`，Initial dt=`0.01`，dt min=`1e-15`，dt max=`1`
   - 自适应：Optimal Iterations=`8`，Iteration Window=`3`，Growth Factor=`1.15`，Cutback Factor=`0.5`
   - 预条件：Type=`SMP`，Full=`true`
4. 点击“确定”，然后同步到输入。
5. 检查 `[Executioner]` 内包含 Transient、NEWTON 与 TimeStepper 参数，并且存在一个
   `[Preconditioning/smp]` 块。
6. 再同步一次，确认 `[Executioner]` 和 `[Preconditioning/smp]` 均不重复。
7. 可选回归：新建第二个 Step 后再同步，确认日志提示“仅取第一个 Step”，且输入中仍只有一套受管求解块。

通过标准：当前阶段只生成第一个 Step 对应的唯一一套 Executioner/TimeStepper/Preconditioning；存在更多 Step 时有明确告警；删除所有 Step 后再次
同步，这些受管块也会自动删除。

## 十、建立面组、加载函数与边界条件（TEST-P4-W03-03，已通过）

验收记录（2026-09-13）：已通过 UI 创建并同步 4 个 BC；`b_1`/`b_2`/`b_3` 将 `disp_x`/`disp_y`/`disp_z` 以零值约束到 `fixed`，`b_4` 将 `disp_z` 通过 `function_1` 施加到 `load`。同步结果中 PiecewiseLinear 无残留 `expression`、FunctionDirichletBC 无残留 `value`、Mesh 块无多余 `[]`，且无 `left`/`right`/`u`/`v` 旧示例引用。

### 10.1 建立两个二维物理组

1. 双击 Mesh 子节点，进入“分组与网格场”→“物理组”。
2. 将“维度”设为 `2`，名称填 `fixed`，点击“拾取”。
3. 在弹窗中根据“面 N（Gmsh 标识 2:N）”和坐标范围选择固定端面，确认后点击“添加”。
4. 名称改为 `load`，再次拾取另一端加载面并点击“添加”。
5. 确认表格中 `fixed`、`load` 的维度均为 2，实体集合不为空且彼此不同。
6. 重新生成 3D 网格，使两个面组写入 `.msh`。

### 10.2 新建加载函数

1. 在模型树展开 `Functions`，右键根节点，选择“添加 Functions”。
2. 将节点重命名为容易识别的名称，例如 `load_curve`。
3. 打开“属性”→“参数”，类型选择 `PiecewiseLinear`。
4. 填写一组等长的时间—载荷数据，例如：
   - X：`0 1 2 3`
   - Y：`0 0.3 0.7 1.0`
5. 打开“校验”页刷新，确认没有 x/y 点数不一致提示，再点击“确定”。

### 10.3 新建固定端与加载端 BC

1. 在模型树展开 `BC`，右键根节点，选择“添加 BC”。
2. 为固定端分别建立需要约束的位移分量，例如 `fix_x`、`fix_y`、`fix_z`：
   - Type：`DirichletBC`
   - Variable：分别为 `disp_x`、`disp_y`、`disp_z`
   - Value：`0`
   - 在“可选边界组”选择 `fixed`，点击“应用所选边界”
3. 再建立加载端 BC，例如 `load_z`：
   - Type：`FunctionDirichletBC`
   - Variable：`disp_z`
   - Function：选择或填写 `load_curve`
   - 在“可选边界组”选择 `load`，点击“应用所选边界”
4. 点击“确定”，同步到输入并检查 `[Functions]` 与 `[BCs]`。
5. 特别确认 `FunctionDirichletBC` 生成 `function = load_curve`，不应同时残留无效的
   `value = ...`。

通过标准：所有 BC 的 `boundary` 都是网格中存在的命名面组；不再引用旧示例的
`left`、`right`、`u` 或 `v`。

## 十一、补齐场输出与历史输出（TEST-P4-W03-06，已通过）

> 2026-09-13 人工验收通过：`DamageC`、`DamageT`、`kappa_c` 场输出，`load`
> 面反力与 `disp_z` 平均位移，0～1/0.01 Times，以及 Exodus/CSV 均正确生成；
> 多次同步无重复块。

1. 在模型树展开 `Outputs`，右键根节点，选择“添加 Outputs”。
2. 选择新建的 Outputs 子节点，打开“属性”→“参数”。
3. 场输出至少选择本轮需要核对的 CDP 变量，例如 `DamageC`、`DamageT`、`kappa_c`。
4. 配置历史输出面组：
   - 本选择只服务于 **Reaction Force（边界反力）** 和
     **Displacement Average（边界平均位移）**；仅配置场输出变量或场量极值时不需要选择边界。
   - 按照当前界面从上到下的顺序，先在顶部“可选边界分组”中选择 `load`，
     再点击“应用所选边界”。
   - 确认“待应用选择”显示 `load`，且快捷参数中的“历史输出面组”为 `load`。
5. 按验收需要启用历史输出：
   - Reaction Force：`true`（本轮必选，因此第 4 步的 `load` 面组也必选）
   - Displacement Average：按需要设为 `true`
   - Displacement Variable：例如 `disp_z`
6. 启用输出时间控制并确认：
   - Times Enabled：`true`
   - Start=`0`，End=`1`，Interval=`0.01`
7. 确认 Exodus 与 CSV 输出均为 `true`，点击“确定”并同步。
8. 在输入中检查：
   - `[AuxVariables]` 与 `[AuxKernels]` 包含所选 CDP 场变量
   - `[Postprocessors]` 包含所选历史输出
   - `[Times/<名称>]` 的类型为 `TimeIntervalTimes`
   - `[Outputs]` 的 Exodus/CSV 输出引用同一个 times object，并包含 `sync_only = true`
9. 重复同步一次，确认以上块均没有重复。

## 十二、完整 CDP 输入检查与执行前配置（远程提交与启动已通过）

> 2026-09-13 人工验收记录：项目 `测试03.gmp.yaml` 重新打开后，系统将结构化输入写入
> `.work/case/测试03/测试03.i`，并导出 v2 快照 `case-20260913-223920`。快照包含
> `测试03.i`、`mes_1.msh`、四个 CDP CSV 与 `manifest.json`，输入 SHA-256 为
> `4c700fe09e6f0282bf6c5c5e4edcefa79df6acc98d75b586112a9079c5297d28`。
>
> 中文输入文件名上传修复复测通过：LIMS Facade 接受快照并创建作业
> `job_20260913_223929_xu10t1`，状态由 `queued` 进入 `running`。客户端、LIMS
> 监控页和计算节点状态均显示输入为 `测试03.i`；远端出现 PID `129578`、4 个 MPI
> rank，物理时间由 `0.000625` 推进到 `0.00125`，证明远端预检通过且求解器已实际启动。
>
> 本轮“完整输入物化、快照打包、网络提交、计算节点启动及状态刷新”人工验证已完成。
> 截止记录时作业仍为 `running`，因此“最终 `succeeded`、结果制品登记/下载和 Exodus
> 回放”尚未验收，不应提前标记为完成；这些结果不回退本轮提交与启动结论。

1. 通读输入，确认至少包含：Mesh、GlobalParams、Physics、Functions、BCs、Materials、
   Executioner、Preconditioning、AuxVariables/AuxKernels、Postprocessors、Times 和 Outputs。
2. 再次确认没有旧 diffusion 示例的 `u`、`v`、`MatDiffusion`、`left`、`right`。
3. 再执行一次“模型 → 同步模型到 MOOSE 输入”。对于已经保存的项目，系统应同时：
   - 将当前输入写到项目目录下 `.work/case/<项目名>/<项目名>.i`；
   - 将“算例设置 → 输入文件”更新为上述真实 `.i` 文件；
   - 将“工作目录”更新为该 `.i` 所在的 `.work/case/<项目名>`；
   - 将输入引用的四个 CDP CSV 同步到该工作目录。
4. 在“算例设置”确认输入文件和工作目录已完成上述同步，不再指向旧
   `diffusion.i`，并确认磁盘上的 `.i` 内容与“输入文件”页编辑器一致。
5. 本地预检（可选）：若当前机器安装了匹配的 MOOSE 应用，在“执行配置”设置
   可执行文件后点击“检查输入”。本地 `exec_path` 为空时不能执行本地检查或求解，
   但不阻止经 LIMS Facade 进行远程提交；远端会使用快照应用档案中的 solver 并执行预检。
6. 导出/提交快照时确认四个 CDP CSV 随任务打包，输入中使用 CSV 文件名而不是本机
   绝对路径。

### 12.1 通过网络提交计算任务（LIMS Facade）

1. 保持当前项目选择生产档案 `DamSafetyApp-opt`，完成模型同步并保存项目。
2. 打开“作业工作窗 → MOOSE 设置 → 输入文件”，点击“导出任务快照”，选择一个
   用于保存快照的父目录。系统会新建 `case-<时间戳>` 目录；确认其中包含当前 `.i`、
   `mes_1.msh`、四个 CDP CSV 和 `manifest.json`。
3. 切换到“MOOSE 设置 → 执行配置”，在“远程作业（经 LIMS Facade 提交）”中设置：
   - 服务器：`http://127.0.0.1:8200`（LIMS Facade 位于其他主机时填写其实际地址）；
   - 项目 ID：`gmp-ise`。
4. 上方“运行”区域的“运行器 / 使用 mpiexec / MPI 进程数”属于直接运行链路，
   **不会传给**下方的“提交作业”网络请求；本轮远程提交无需把运行器改为“远程”，
   远端 solver 命令取自快照 `manifest.json` 中的应用档案。
5. 点击“提交作业”。系统默认提交最近一次导出的快照；若当前会话没有快照记录，
   按弹窗选择第 2 步生成且包含 `manifest.json` 的 `case-<时间戳>` 目录。
6. 提交成功后，界面应显示 `job_<时间戳>_<随机串> : queued`，操作日志记录远程
   job ID；若提示网络错误，先确认 `http://127.0.0.1:8200` 可访问及远端代理/隧道已启动。
7. 点击“刷新状态”或切换到“作业列表”刷新，确认状态按实际执行过程从
   `queued/preparing` 进入 `running`。到达 `running` 且能够看到远端 PID、MPI rank、
   CPU/内存和物理时间推进时，“提交与远端启动”验收即通过；继续跟踪最终状态是否为
   `succeeded`。若为
   `check_input_failed`，查看作业日志中的 MOOSE 输入错误，修正模型后重新同步并导出新快照。
8. 作业成功后，在“作业列表”选择该远程作业，刷新制品并下载 Exodus 结果；也可点击
   “打开远程制品”。结果登记到 Results 后载入可视化模块检查场变量和时间步。

完成第十二部分后，才可把该项目视为“完整 CDP 力学输入已通过 UI 配置”；若求解不收敛，按
求解问题单独记录，不回退 W-01 的 Section 指派验收结论。
