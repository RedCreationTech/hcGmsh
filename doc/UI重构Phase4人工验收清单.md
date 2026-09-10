# GMP-ISE UI 重构 Phase 4 人工验收清单（W-00 / W-03a / W-02b）

> 版本：2026-09-10 v2（W-00a~d、W-03a、W-02b 切片用例建立；2026-09-10 全部人工验收通过）
> 前置：Phase 0~3 已全部验收关闭；自动回归基线为 89 步真实点击巡览 + CTest `1/1`
> 方案依据：`doc/UI重构Phase4方案设计.md`；任务口径：`doc/UI重构开发任务清单.md` W-00、W-02b、W-03a
> 通用检查表见 `doc/UI重构开发任务清单.md` 附录 B

## 验收前准备

1. 构建：`cmake --build build -j4`，启动 `./build/gmp_ise`。
2. 应用档案目录可用（默认 `templates/moose/profiles/`，含 DamSafetyApp-opt、blackbear-opt、combined-opt）。
3. 验收数据（已随仓库提交）：`tests/fixtures/cdp-v01/` 下有 `uniaxial_compression_mesh.e` 与 4 张材料 CSV（compression_hardening/compression_damage/tension_stiffening/tension_damage）。
4. 对照基线（可选，用于逐参数比对）：`damASR/docs/examples/cdp-v01-single-tension/input/tpl-cdpc-tension-single-recheck.i`。

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
