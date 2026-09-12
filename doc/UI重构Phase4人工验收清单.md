# GMP-ISE UI 重构 Phase 4 人工验收清单（W-00 / W-03a / W-02b）

> 版本：2026-09-12 v3（补充多 Mesh、按部件生成、Job 网格选择、项目恢复与三阶段实时进度验收）
> 前置：Phase 0~3 已全部验收关闭；自动回归基线为 99 步真实点击巡览 + CTest `1/1`
> 方案依据：`doc/UI重构Phase4方案设计.md`；任务口径：`doc/UI重构开发任务清单.md` W-00、W-02b、W-03a
> 通用检查表见 `doc/UI重构开发任务清单.md` 附录 B

## 验收前准备

1. 构建：`cmake --build build -j4`，启动 `./build/gmp_ise`。
2. 应用档案目录可用（默认 `templates/moose/profiles/`，含 DamSafetyApp-opt、blackbear-opt、combined-opt）。
3. 验收数据（已随仓库提交）：`tests/fixtures/cdp-v01/` 下有 `uniaxial_compression_mesh.e` 与 4 张材料 CSV（compression_hardening/compression_damage/tension_stiffening/tension_damage）。
4. 对照基线（可选，用于逐参数比对）：`damASR/docs/examples/cdp-v01-single-tension/input/tpl-cdpc-tension-single-recheck.i`。

---

## 当前仍需人工验证（2026-09-12）

本轮自动回归已通过 99 步真实点击巡览与 CTest `1/1`。以下 13 项仍需要人工确认；建议先验证本次 Mesh 改动对应的前三项。

| 优先级 | 用例 | 人工确认重点 |
|---|---|---|
| P0 | TEST-P4-GEN-07 | 多 Mesh、部件下拉、节点菜单/双击、项目重开恢复 |
| P0 | TEST-P4-GEN-08 | 不同 Job 独立选择并恢复各自网格 |
| P0 | TEST-P4-GEN-04 | 三阶段内实时进度、界面响应与取消手感 |
| P1 | TEST-P4-GEN-05 | 无孔六面体结构化网格与带孔体回退 |
| P1 | TEST-P4-GEN-06 | 单元拓扑策略、严格模式拒绝与持久化 |
| P1 | TEST-P4-E2E-01 | 真实几何到远端求解、结果回放的端到端流程 |
| P2 | TEST-P4-W02-04 | FileMeshGenerator 输入形态 |
| P2 | TEST-P4-W01-01 | Section 指派与失效引用 |
| P2 | TEST-P4-W01-02 | 从物理组创建选择集及重开恢复 |
| P2 | TEST-P4-W03-03 | BC/函数扩展及输入同步 |
| P2 | TEST-P4-W03-04 | Step 到 Executioner 映射及幂等同步 |
| P2 | TEST-P4-W03-05 | Physics action 生成 |
| P2 | TEST-P4-W03-06 | 场/历史输出套餐 |

---

## W-00a 活动应用档案

## TEST-P4-W00-01 选择器存在与新建默认选中

1. 启动应用，查看工作上下文条：“模块 / 项目 / 应用 / 当前对象”中应有“应用”选择器。
2. 执行“文件 ▶ 新建项目”。
3. 查看“应用”选择器与状态栏右侧“应用：”标签。

通过标准：选择器列出 3 个档案（combined-opt 等 prototype 档案带 `[prototype]` 标注）；新建项目默认选中 DamSafetyApp-opt；状态栏显示“应用：DamSafetyApp-opt”；悬停选择器 tooltip 显示档案名称与状态说明。

## TEST-P4-W00-02 档案切换与 mapping 重载

1. 在“应用”选择器中切换到 combined-opt（prototype）。
2. 观察状态栏提示与“应用：”标签；悬停标签查看 tooltip（应显示档案版本与 mapping 注册表加载状态）。
3. 切回 DamSafetyApp-opt。

通过标准：切换即时生效；prototype 档案出现“原型档案，不建议用于正式提交”状态栏提示；tooltip 显示 mapping registry 已加载及版本；项目被标记为已修改（状态栏脏标记）。

## TEST-P4-W00-03 档案持久化与旧项目兼容

1. 选中 DamSafetyApp-opt 后保存项目为 `test-p4.gmp.yaml`，关闭并重开，检查选择器与状态栏。
2. 打开一个 Phase 4 之前创建的旧项目（无档案字段），确认正常加载、选择器显示“未选择”，然后在选择器中补选一个档案并保存。

通过标准：新保存的项目重开后档案恢复；旧项目加载不报错、不强制要求档案；补选后保存重开档案生效。

## W-00b 物理组清单生产者

## TEST-P4-W00-04 网格生成后的清单摘要

路径 A（几何 → 生成网格）：
1. 在网格工作窗“模型”页打开一个含物理组的 `.geo` 几何。注意“导入后自动剖分”默认勾选，打开后会自动完成一次网格生成（与手动点“生成网格”是同一路径）；想验手动路径时先取消该勾选再打开。
2. 展开模型树 Mesh 节点，双击生成的网格子项查看属性（或在属性页查看 summary）。
3. 对一个没有任何物理组的模型执行生成网格，观察状态栏与 Mesh 节点状态。

路径 B（现成网格 → 显式导入）：
1. 菜单“网格 ▶ Import Exodus Mesh...”选择 `tests/fixtures/cdp-v01/uniaxial_compression_mesh.e`（.e 是网格不是几何，无需再生成）。
2. 展开模型树 Mesh 节点查看导入子项的属性。

通过标准：路径 A 含物理组时 Mesh 节点摘要显示“维度 · 节点数 · 单元数 (类型) · 物理组数 · 质量 minSICN 区间”，params 含 physical_group_names 与 sha256，状态为“已生成”；无物理组时生成不被阻断，但状态栏警告“网格清单不完整”，节点状态显示“不完整”。路径 B 节点登记 role=input_mesh、source=exodus_import、状态“已生成”。

## W-00c 快照 v2 与提交接线

## TEST-P4-W00-05 快照 v2 导出与拒绝路径

1. 前置：已选活动档案（W-00a）且已生成含物理组的网格（W-00b），`.i` 编辑器中有引用该网格的输入内容。
2. 在作业工作窗 MOOSE 设置页执行“导出作业快照”，选择输出目录。
3. 检查导出目录：应有 `case-<时间戳>` 子目录，内含 `.i`、网格文件、`manifest.json`；打开 manifest 检查字段（case_id、application_profile、unit_contract、物理组清单、文件 SHA-256、input_mode=structured、recommended_command）。
4. 拒绝路径 A：新建空项目（不清档案但无网格清单），直接导出快照；拒绝路径 B：在 `.i` 中引用一个不存在的文件后导出。

通过标准：成功路径 manifest 为 v2 全字段、`.i` 中网格/CSV 引用已归一化为包内相对文件名；两条拒绝路径均不产生快照目录，并在作业日志/状态栏给出可读原因（缺档案/缺物理组清单/文件缺失）。

## TEST-P4-W00-06 提交清单 solver/档案字段（有远程环境时）

1. 用 W-00-05 导出的 v2 快照执行远程提交。
2. 再找一个 Phase 4 之前导出的 v1 旧快照目录执行提交（如有）。

通过标准：v2 快照提交按活动档案的 solver_program 提交（不再是固定写死值）；v1 旧快照可提交并回落到缺省 solver，日志中有回落说明。

## W-03a CDP 材料表单

## TEST-P4-W03-01 表单字段与单位换算

1. 材料模块点击“New CDP Material”（或材料根节点右键添加后把 type 改为 AbaqusCDP）。
2. 检查参数表单：弹性模量（MPa）、泊松比、CDP 五参数（膨胀角/偏心率/双轴单轴抗压比/受拉子午线比/粘度）、拉压恢复、最大子步，及 4 个 CSV 文件选择行（Browse 按钮）。
3. 填入 v01 参数：E=29791.5（MPa）、ν=0.2、36/0.1/1.16/0.667/0.0005、恢复 0/1、子步 256；4 个 CSV 分别选择 `tests/fixtures/cdp-v01/` 下对应文件。
4. 确定后重新打开该材料表单，检查回显。

通过标准：14 个字段齐全且仅 AbaqusCDP 类型显示；E 以 MPa 显示与回显（29791.5），内部存储为 SI（可在高级参数表看到 2.97915e10）；4 个 CSV 路径持久化；切换 type 后 CDP 字段正确显隐。

## TEST-P4-W03-02 生成 Materials 三件套（对照 v01 基线）

1. 完成 W03-01 后，执行“模型 ▶ 同步模型到 MOOSE 输入”。
2. 在 `.i` 编辑器中检查 `[Materials]` 段。
3. 与基线 `tpl-cdpc-tension-single-recheck.i` 的 [Materials] 逐项比对（type、E/ν、CDP 五参数、recovery、substeps、CSV 文件名）。

通过标准：生成 elasticity / stress / cdp_stress_update 三个对象；type 分别为 ComputeIsotropicElasticityTensor / ComputeMultipleInelasticStress / AbaqusCDPStressUpdate；`inelastic_models` 自动连线；E=2.97915e10（SI）；CSV 以纯文件名（basename）引用，无绝对路径泄漏；未做 Section 指派时 `block = ''` 且控制台有警告（指派属后续 W-01b）。

## W-02b Exodus 网格显式导入

## TEST-P4-W02-01 导入登记与侧集名

1. 执行“网格 ▶ Import Exodus Mesh...”，选择 `tests/fixtures/cdp-v01/uniaxial_compression_mesh.e`。
2. 检查：模型树 Mesh 节点新子项（属性中 role=input_mesh、source=exodus_import、状态“已生成”）；中央舞台显示该网格。
3. 打开任意 BC 表单，检查 boundary/组选择中是否出现 top、bottom。

通过标准：Mesh 节点登记 role=input_mesh；舞台正确显示 Exodus 网格；boundary 组选择 chips/下拉含 top、bottom；项目标记为已修改。

## TEST-P4-W02-02 输入/结果角色不混淆

1. 用 TEST-P4-W02-01 导入的网格配合 W-03a 材料导出快照，检查 manifest 中该 `.e` 的角色为 input_mesh。
2. 在结果模块“导入结果文件...”选择同一个 `.e`，确认它只登记为 Results 结果节点，不会被当作输入网格。

通过标准：显式导入的 `.e` 在快照中标记 input_mesh；结果导入路径不产生第二个输入网格引用，两条路径互不干扰。

## 通用回归

## TEST-P4-GEN-01 中英文切换新文案

1. 切换 English，检查：应用选择器与 tooltip、Import Exodus Mesh 菜单项、New CDP Material 按钮、CDP 表单全部标签、快照导出提示。
2. 切回中文复查。

通过标准：新增界面文案双语完整，无残留未翻译项、无截断。

## TEST-P4-GEN-02 既有流程不回归

1. 运行一个演示案例（工具 ▶ 演示案例 ▶ 瞬态扩散），生成网格并同步到输入。
2. 打开/关闭各工作窗，切换 13 个模块。

通过标准：演示案例照常可生成可同步（非 CDP 材料仍按原逻辑生成）；工作窗与模块切换无异常；无档案硬阻断影响旧流程。

## TEST-P4-GEN-03 新建项目对话框（名称/目录/未保存提示）

1. 修改当前项目使其处于未保存状态（如新建一个材料），点击工具栏“新建”按钮：应先弹出“当前项目有未保存的修改”提示，选“取消”后项目保持不变。
2. 再次点击“新建”，选“不保存”：弹出“新建项目”对话框，应包含“项目名称”和“存储目录”（带“浏览...”按钮）两个字段。
3. 输入名称 `test-p4-new`，选择一个目录，点“创建”。
4. 检查：标题栏显示 `test-p4-new.gmp.yaml`；状态栏右侧“项目：”显示完整路径；该目录下已生成 `test-p4-new.gmp.yaml` 文件；文件 ▶ 最近项目 中出现该路径。
5. 再次“新建项目”，输入相同名称和目录，确认出现“文件已存在，是否覆盖？”提示。

通过标准：未保存修改有提示且不静默丢失；名称/目录由用户在创建时确定并可修改；创建后项目文件真实落盘、路径在标题栏/状态栏/最近项目中可见；同名覆盖有二次确认。

---

## 验收记录

| 用例 | 结果 | 备注 |
|---|---|---|
| TEST-P4-W00-01 | 通过 | 选择器与默认选中 |
| TEST-P4-W00-02 | 通过 | 切换与 mapping 重载 |
| TEST-P4-W00-03 | 通过 | 持久化与旧项目兼容 |
| TEST-P4-W00-04 | 通过 | 网格清单摘要 |
| TEST-P4-W00-05 | 通过 | 快照 v2 导出与拒绝 |
| TEST-P4-W00-06 | 通过 | 提交清单字段（需远程环境） |
| TEST-P4-W03-01 | 通过 | CDP 表单与单位换算 |
| TEST-P4-W03-02 | 通过 | 三件套生成对照 v01 |
| TEST-P4-W02-01 | 通过 | Exodus 导入与侧集名 |
| TEST-P4-W02-02 | 通过 | 输入/结果角色隔离 |
| TEST-P4-GEN-01 | 通过 | 中英文新文案 |
| TEST-P4-GEN-02 | 通过 | 既有流程不回归 |
| TEST-P4-GEN-03 | 通过 | 新建项目对话框 |

*2026-09-10 全部用例人工验收通过。验收中发现并当日修复的缺陷：快照导出被绝对路径误拒、提交清单与服务端严格 schema 冲突（7 键/文件条目 2 键）、内置扩散模板 hit 语法错误（服务端 --check-input 预检发现）。端到端证据：快照 case-20260910-171821 提交成功，远端 job_20260910_171821_hsrg0x 计算成功（21 时间步 u/v 场结果 + CSV 历史量），diffusion_out.e 已在应用内载入验证（M-P4-4 场景）。已知边界：本次用 GeneratedMesh 演示模板，几何→物理组→真实 BC 绑定属 W-01/W-02/W-03 后续切片。*

---

# 第二轮用例（W-02a/W-02c/W-01b/W-01c/W-03b~e，2026-09-10）

## TEST-P4-W02-03 物理组命名/维度/非空校验（W-02a）

前置：网格工作窗“模型”页已有几何模型（“打开几何”加载 `.geo`，或勾选“使用示例盒”）；“拾取”对话框的实体列表来自当前 Gmsh 模型，无模型时为空属预期。

1. 网格工作窗“分组与网格场 → 物理组”页，依次尝试：名称为空点“添加”；有名称但不选实体点“添加”；创建与既存组同维度同名的组；“维度”选 3 但实体填 `2:5` 这类不一致的 `dim:tag`。
2. 对既存组执行“更新所选”，名称保持不变（应允许），再改成与其他组重名（应拒绝）。

通过标准：四种非法输入都被拒绝并在日志/状态栏给出可读原因，且 gmsh 状态无半残留（被拒绝的组不出现在组下拉里）；名称不变的自更新被允许；合法创建（维度+名称+实体齐全）正常出现在分组下拉与统计表。

## TEST-P4-W02-04 FileMeshGenerator 注入形态（W-02c）

1. 生成或导入网格后，打开作业工作窗“输入文件”页查看 `[Mesh]` 段。

通过标准：`[Mesh]` 为 `[Mesh/file]` 子块形态且 `type = FileMeshGenerator`；不会出现旧式 `type = FileMesh` 顶层块与新块并存。

## TEST-P4-W01-01 Section 指派（W-01b）

1. 新建一个 CDP 材料（W03-01），新建一个 Section：Material 下拉应列出该材料；Physical Volumes 用组选择 chips 选一个体组（如无体组先在几何里建）。
2. 执行“同步模型到 MOOSE 输入”，检查 `[Materials]` 三件套的 `block =`。
3. 删除该材料，查看 Section 节点状态与校验提示。

通过标准：`block = '<指派的体组名>'` 不再是空串；删除材料后 Section 状态显示失效、校验提示 material 引用问题；恢复同名材料后自愈。

## TEST-P4-W01-02 从物理组新建选择集（W-01c）

1. Selections 根右键“New Selection from Physical Group...”，选维度与组名创建。
2. 点击该子项，观察视口过滤；保存项目重开。

通过标准：子项创建成功（type=PhysicalGroup、维度/组名正确）；点击后视口按组过滤（tag 可解析时）；保存重开后仍在。

## TEST-P4-W03-03 BC/函数扩展（W-03c）

1. 新建 Functions 子项，type 选 PiecewiseLinear，填 x=`0 1 2 3`、y=`0 2.5e-05 0 2.5e-05`；再试 x/y 个数不一致。
2. 新建 BC，type 选 FunctionDirichletBC，function 下拉选到该函数，boundary 用面组 chips 选择。
3. 同步到输入检查 `[Functions]`/`[BCs]`。

通过标准：个数不一致被校验拦截；`.i` 中函数为 `type = PiecewiseLinear` 且 x/y 带引号；BC 为 `type = FunctionDirichletBC` 且 `function = <函数名>`、无残留 value 行；DirichletBC 路径不受影响。

## TEST-P4-W03-04 Step→Executioner 映射（W-03e）

1. 新建 Step（默认即 v01 参数），检查表单四组字段（基本/求解控制/时间步进/预处理）。
2. 同步到输入检查 `[Executioner]`、`[TimeStepper]` 子块、`[Preconditioning/smp]`。
3. 再建第二个 Step 后同步；连续同步两次。

通过标准：`[Executioner]` 含 NEWTON/line_search=bt/automatic_scaling/容差/petsc options（带引号）；`[TimeStepper]` 含 dt/optimal_iterations 等五项；`[Preconditioning/smp] full = true`；两个 Step 时给出“仅取第一个 Step”警告；重复同步不产生重复块。

## TEST-P4-W03-05 Physics action 生成（W-03b）

1. 完成 CDP 材料 + Section 指派后，Physics 根右键 Add Physics：block 应自动带出指派体组名。
2. 检查表单（action 下拉、strain、三个布尔、generate_output、save_in_resid），同步到输入。

通过标准：`.i` 含 `[GlobalParams] displacements = 'disp_x disp_y disp_z'` 与 `[Physics/SolidMechanics/QuasiStatic/<名>]`；block 为指派组名；generate_output 16 项与 save_in='resid_x resid_y resid_z' 正确；重复同步幂等。

## TEST-P4-W03-06 场/历史输出套餐（W-03d）

1. 新建 Outputs 子项，勾选场输出变量 3 项以上（DamageC/DamageT/kappa_c）、历史输出反力（边界选面组）、Times（间隔 0.01）、Exodus+CSV。
2. 同步到输入检查各块；连续同步两次。

通过标准：生成对应 AuxVariables（CONSTANT MONOMIAL）与 AuxKernels（`property = DamageC`、`property = cdp_kappa_c` 命名正确）；Postprocessors 含 NodalSum；`[Times]` 为 TimeIntervalTimes；Exodus/CSV 带 sync_times_object 与 sync_only = true；重复同步无重复块。

## TEST-P4-E2E-01 端到端：真实几何全流程（第二轮准出）

1. 打开你的几何（如 demo_fixed.geo），在“分组与网格场”页给表面建命名面组（如 fixed/load）、给体建体组（如 solid），生成网格。
2. 新建 CDP 材料（填 v01 参数与 4 张 CSV）→ 新建 Section 指派材料到 solid → Add Physics（确认 block）→ 新建 Step → 新建 PiecewiseLinear 或 ParsedFunction 位移函数 → 新建 BC（固定面 DirichletBC + 加载面 FunctionDirichletBC，boundary 用 chips 选组）→ 新建 Outputs 勾选场/历史套餐。
3. 同步到输入，通读 `.i` 全文：与 v01 基线同骨架（Mesh/GlobalParams/Physics/Aux/Functions/BCs/Materials/Postprocessors/Preconditioning/Executioner/Times/Outputs）。
4. 导出快照 → 提交远端（服务器 http://127.0.0.1:8200）→ 作业列表监控到结束 → 结果登记后载入可视化，用回放工具组播放。

通过标准：全流程不手改 `.i` 文本；远端 --check-input 预检通过、作业成功；结果 .e 可载入并回放（若几何/边界与物理场景本身不收敛，以“预检通过 + 作业正常启动”为准，并在备注记录求解表现）。

## 第二轮验收记录

| 用例 | 结果 | 备注 |
|---|---|---|
| TEST-P4-W02-03 | 通过 | 物理组命名/维度/非空校验已完成人工验收 |
| TEST-P4-W02-04 | 待验证 | FileMeshGenerator 形态 |
| TEST-P4-W01-01 | 待验证 | Section 指派 |
| TEST-P4-W01-02 | 待验证 | 选择集创建 |
| TEST-P4-W03-03 | 待验证 | BC/函数扩展 |
| TEST-P4-W03-04 | 待验证 | Executioner 映射 |
| TEST-P4-W03-05 | 待验证 | Physics action |
| TEST-P4-W03-06 | 待验证 | 输出套餐 |
| TEST-P4-E2E-01 | 待验证 | 端到端真实几何全流程 |

---

## TEST-P4-GEN-04 大型 3D 网格规模预检、进度与取消（2026-09-11-001）

前置：通过“草图 → 部件 → 拉伸”创建约 `100 × 70 × 10` 的实体，在网格模块引用该部件；准备一个已有有效内容的输出 `.msh` 以验证失败不覆盖。

1. 将全局网格尺寸设为 `0.2`，点击“生成 3D 网格”。
2. 记录规模预检提示中的预计单元数和建议尺寸；关闭提示后检查生成按钮与其他网格按钮。
3. 把尺寸调至提示建议值以上（建议先用 `1~2`），再次点击“生成 3D 网格”。
4. 观察进度窗：第 1/3 步“生成 2D 面网格”应在总进度 `0%~33%` 内实时推进；第 2/3 步“生成 3D 体网格”应根据实际剖分里程碑在 `33%~67%` 内推进；第 3/3 步“优化网格质量”应根据实际优化事件在 `67%~100%` 内推进。确认百分比与已用时持续更新、总进度不倒退；在一次生成中点击“取消”。
5. 再次生成并等待完成，检查中央舞台和日志；随后不重建部件，直接再次生成一次。

通过标准：过细尺寸在剖分前被快速拒绝，提示可读且按钮立即恢复，原有效输出不被覆盖；正常生成时三个步骤内部都有可见的实时百分比变化，阶段切换连续、总进度单调递增，窗口不呈系统“未响应”；取消可终止且不损坏当前 OCC 部件；成功后舞台显示真实 3D 体网格，日志含 3D 单元与质量统计且不再出现 element type 1 质量错误；同一部件可连续生成，不退化为 12 节点空网格。

## 网格生成缺陷回归记录

| 用例 | 结果 | 备注 |
|---|---|---|
| TEST-P4-GEN-04 | 待人工验证 | 自动回归已覆盖生成、舞台、模型恢复、超限拒绝；人工确认三个步骤内的实时进度、界面响应与取消手感 |

## TEST-P4-GEN-05 优先结构化四边形/砖形（W-02d）

1. 在网格工作窗“网格”页确认“优先结构化四边形/砖形立方体网格”开关可见；悬停阅读可映射拓扑、立方体成立条件和回退提示。
2. 对一个无孔六面长方体开启该选项，使用合理全局尺寸生成 3D 网格，检查日志和单元排列。
3. 再对带圆孔拉伸体保持开启状态生成，检查回退原因、中央舞台和 `Top-dimensional element composition`。
4. 在舞台 Dim 选择 3，确认默认只显示最高维单元的外表面；切换 All/2 可分别检查混合维或边界面。

通过标准：六面块体输出方向对齐的 Hexahedron，不出现由四面体细分得到的蜂窝扭曲单元；带孔体明确说明其拓扑不可映射并回退为 Tetrahedron，不伪报“全六面体”；舞台默认 Dim=3 时外壳完整、质量值可计算；项目保存/重开后选项保持。

| 用例 | 结果 | 备注 |
|---|---|---|
| TEST-P4-GEN-05 | 待人工验证 | 自动回归覆盖带孔体回退 Tetrahedron 与六面块体结构化 Hexahedron 两条路径 |

## TEST-P4-GEN-06 单元拓扑策略与严格模式（W-02e）

1. 打开网格工作窗“网格”页，确认原复选框已替换为“单元拓扑策略”下拉框，包含自动、通用三角形/四面体、严格结构化三个选项。
2. 对带孔沿 Z 拉伸体选择“自动”并生成，确认成功回退为 Tetrahedron，日志说明边界面数量不满足映射条件。
3. 对同一实体选择“结构化四边形/六面体（严格）”并生成。
4. 对无孔六面长方体选择严格模式并生成；保存、重开项目后检查策略保持。

通过标准：严格模式下 2D/3D 算法框禁用；带孔体在剖分前明确失败、不写出新文件且生成按钮恢复，不得回退后报告成功；六面块体输出 Hexahedron；自动模式仍可回退；旧项目的 `recombine` 布尔值可迁移。

| 用例 | 结果 | 备注 |
|---|---|---|
| TEST-P4-GEN-06 | 待人工验证 | 自动回归覆盖自动回退、严格拒绝、严格六面体生成与控件恢复 |

## TEST-P4-GEN-07 多 Mesh、按部件生成、节点交互与项目恢复（2026-09-12）

1. 创建两个形态明显不同的部件，例如实心 `part_1` 与中空 `part_2`。
2. 右键 Mesh 根节点，确认菜单中有“添加 Mesh”，没有“打开网格工作窗”；连续创建 `mes_1`、`mes_2`。
3. 右键任意 Mesh 子节点，确认菜单中有“打开网格工作窗”、复制、重命名、删除，没有“添加 Mesh”；再分别双击 `mes_1`、`mes_2`，确认打开对应标题的网格工作窗。
4. 在两个网格工作窗的“模型”页，通过“几何”下拉框分别选择 `part_1`、`part_2` 并生成网格。
5. 切换 Mesh 子节点查看结果，确认 `part_1` 为实心、`part_2` 保留中空特征，且两个网格文件路径互不覆盖。
6. 保存并重新打开项目，再次打开两个 Mesh 子节点，检查几何下拉框、已选部件和已有网格结果。

通过标准：可连续创建多个 Mesh；根节点与子节点菜单职责正确；双击总是打开所点子节点；每个 Mesh 按自身选择的部件生成且结果形态正确；保存重开后部件列表、每个 Mesh 的几何来源与结果均恢复，不显示 `No geometry loaded`。

## TEST-P4-GEN-08 Job 独立网格选择与恢复（2026-09-12）

1. 前置：至少有两个已生成且路径不同的 Mesh 子节点；创建 `job_1`、`job_2`。
2. 打开 `job_1` 的“作业工作窗 → MOOSE 设置 → 算例设置”，确认“网格文件”为下拉选择并列出两个网格；选择 `mes_1`。
3. 打开 `job_2`，选择 `mes_2`；在两个 Job 之间往返切换并检查选择值与输入文件中的网格引用。
4. 保存并重新打开项目，再次检查 `job_1`、`job_2` 的网格选择和物理组摘要。

通过标准：不同 Job 可独立选择不同网格，切换 Job 不会互相覆盖；下拉项与 Mesh 节点及实际文件一致；选择后物理组信息随对应网格刷新；保存重开后两项选择分别恢复。

| 用例 | 结果 | 备注 |
|---|---|---|
| TEST-P4-GEN-07 | 待人工验证 | 自动回归已覆盖多节点创建、部件选择、双击打开和项目恢复；人工确认菜单细节与实心/中空视觉结果 |
| TEST-P4-GEN-08 | 待人工验证 | 自动回归已覆盖 Job 网格下拉合同；人工确认多 Job 独立选择、物理组刷新与重开恢复 |
