# Phase 4 方案设计：Abaqus 式前处理工作流闭环

> 版本：2026-09-09 v1
> 目标算例（已验证基线）：`damASR/docs/examples/` 下 `cdp-v01-single-tension`、`cdp-v02-single-compression`、`cdp-r01-tension-reloading`、`cdp-r02-compression-reloading`（Abaqus inp 转换的 CDP 单轴算例）。
> 目标：人工经 GMP-ISE 的 UI 操作，即可产出与上述算例**语义等价、可直接提交运行**的 `.i` 及配套案例包。
> 依据：`doc/UI重构需求记录.md`（REQ-011～017、决策 1～8）、`doc/UI重构开发任务清单.md` W-01～W-07、目标算例块级分析、当前能力基线盘点。

---

## 1. 目标算例分析结论

### 1.1 块骨架（v01/v02 共 14 类 block，r01/r02 同骨架+变体）

| # | Block | 代表 type | 来源语义（Abaqus 阶段） | UI 可生成性 |
|---|---|---|---|---|
| 1 | `[Mesh/file]` | FileMeshGenerator | Part/Assembly/Mesh（.e 网格） | (a) 通用表单 |
| 2 | `[GlobalParams]` | displacements | （固体力学全局位移变量） | (b) 档案驱动 |
| 3 | `[Physics/SolidMechanics/QuasiStatic/*]` | QuasiStatic action | Step 物理场 + 场输出变量 + 残差 save_in | (b) 应用模板表单 |
| 4 | `[AuxVariables]` | resid_*、DamageC/T、cdp_* 诊断量 | Field Output 变量声明 | (b) 模板化成组 |
| 5 | `[AuxKernels]` | MaterialRealAux ×8 | Field Output 取数 | (b) 模板化成组 |
| 6 | `[Functions/*]` | ParsedFunction / PiecewiseLinear | 位移加载幅值（复载=加卸载曲线） | (a) 通用表单 |
| 7 | `[BCs]` | DirichletBC / FunctionDirichletBC | BC/Load（固定底面、顶面位移） | (a) 通用表单 |
| 8 | `[Materials]` 三件套 | ComputeIsotropicElasticityTensor + ComputeMultipleInelasticStress + AbaqusCDPStressUpdate | Material（弹性 + CDP 五参数 + 4 张 CSV 曲线） | (b) 应用模板表单 |
| 9 | `[Postprocessors]` ×13 | NodalSum / AverageNodalVariableValue / ElementExtremeValue | History Output（RP 反力/位移、极值统计） | (a) 通用表单 |
| 10 | `[Preconditioning/smp]` | SMP | Step 求解配置 | (a) 通用表单 |
| 11 | `[Executioner]` | Transient + IterationAdaptiveDT | Step（*Static 参数直译） | (a) 通用表单 |
| 12 | `[Times/*]` | TimeIntervalTimes | Output 时间间隔 | (a) 通用表单 |
| 13 | `[Outputs]` | Exodus + CSV（sync_times） | Field/History Output 落盘 | (a) 通用表单 |
| 14 | 输入附件 | 4 张材料 CSV + 网格文件 | Material 曲线 / Mesh | 文件选择器 + 快照打包 |

### 1.2 复载（r01/r02）与诊断块分级

- r01/r02 主体仍是同一骨架：复载语义 = **一个 Transient（0–3 s）+ PiecewiseLinear 加卸载函数**（`'0 1 2 3'→'0 2.5e-5 0 2.5e-5'`），3 个 Abaqus Step 折叠为一条位移历史，状态跨相位连续。r02 除此外无任何诊断块。
- **r01 独有的 5 个诊断块**（CDPTrialProblem、CDPAffineLoadPredictor、CDPAssemblyProbe、CDPAcceptedStateOutput、accepted_checkpoint 时间窗）绑定特定诊断分支求解器与算例敏感时间窗/硬编码单元号，属只读归因工具 → **(c) 只适合“专家扩展/自定义块”机制**（决策 8），不做结构化表单。
- 结论：**Phase 4 的 UI 结构化生成目标是 (a)+(b)**；v01/v02 全量、r01/r02 除诊断块外全部可经 UI 生成；诊断块经专家扩展注入，与生成区分离保存、合并校验（决策 8）。

### 1.3 inp→i 转换规则中必须产品化的要点

1. **单位换算**：inp 为 N·mm·MPa，.i 为 SI。E 29791.5 MPa→2.97915e10 Pa；位移 0.025 mm→2.5e-05 m；CSV 首列 stress_pa。UI 显示单位（MPa/mm）→ 求解单位由活动档案 unit_contract 声明并显式换算（决策 7）。
2. **block 命名**：转换器用 `Part名__材料名`（`concrete_cube__concrete`）。GMP-ISE 按 F-04/W-02 合同以 **Physical Volume 名**作为 MOOSE block——语义等价即可，不要求与转换器命名逐字符相同；Section 指派 = 材料 ↔ Physical Volume 绑定，生成时落到相关 Material/Physics 对象的 `block` 参数（REQ-013）。
3. **boundary 命名**：top/bottom 来自面级物理组（sideset）名；BC/Postprocessor 表单中的 boundary 必须取自命名面组下拉，禁止手输 tag。
4. **RP/耦合的等价表达**：MOOSE 无参考点概念；基线的等价做法是 top sideset 上 `NodalSum(resid_z)`（反力）+ `AverageNodalVariableValue(disp_z)`（位移）+ top_x/top_y gauge BC。该“后处理套餐”应作为 Outputs 模块的模板化组合提供；真实运动耦合（M-02）后续单独立项。
5. **网格输入角色**：v01 的 `.e` 属“显式导入已有 Exodus 网格”（决策 6 允许的例外），manifest 必须标记 `role=input_mesh`；常规自建几何仍走 `.msh + FileMeshGenerator`。

## 2. 模块交互设计：从 UI 操作到最终 .i

### 2.1 预处理操作链（模块间数据流）

```text
① 应用档案（项目级，新增）        决定：可用物理/对象类型、单位合同、校验命令、映射版本
        ↓
② 几何/部件（Sketch/Part/Mesh 模块）→ Gmsh 模型 → Physical Groups（体/面命名）
        ↓ 网格生成
③ Mesh 节点（.msh，或显式导入 .e）  记录：PG 清单/摘要（F-04 manifest，待接线）
        ↓
④ Material 模块：CDP 材料表单（弹性 + CDP 五参数 + 4 CSV + 单位换算预览）
⑤ Section 模块：材料 ↔ Physical Volume 指派（生成 block 限制，不输出 [Sections]）
⑥ Physics 模块：SolidMechanics/QuasiStatic action 表单（真实创建，替代空壳）
⑦ Step 模块：Executioner/TimeStepper/Preconditioning 求解控制表单
⑧ BC/Load 模块：DirichletBC/FunctionDirichletBC，boundary 取自面组下拉，
                位移函数引用 Functions 节点（ParsedFunction/PiecewiseLinear）
⑨ Outputs 模块：场输出（generate_output/Aux 套餐/Times/Exodus）
                + 历史输出（Postprocessor 套餐/CSV）
        ↓
⑩ 校验（W-05）：结构+语义+跨引用分级，阻断项定位跳转
        ↓
⑪ 装配 .i（W-04）：mapping-v1 驱动的结构化装配器，普通模式只读 + 专家扩展层
        ↓
⑫ 快照（W-06）：.i + 网格 + CSV + manifest.json（v2，接线已实现代码）
        ↓
⑬ 提交（W-07）：本地 Runner / 远程 SimClient（solver 名从档案读取）→ Results 回写
```

### 2.2 block → 模块 → 表单字段映射（验收粒度）

| 目标 .i 块 | 负责模块/节点 | 表单设计要点 |
|---|---|---|
| Mesh/file FileMeshGenerator | Mesh 节点 | 生成 `.msh` 或“导入 Exodus”（显式入口，role=input_mesh）；**FileMesh 旧写法升级为 FileMeshGenerator** |
| GlobalParams displacements | Physics 节点 | 档案声明位移变量名（disp_x/y/z），自动带出，不暴露手输 |
| Physics QuasiStatic action | Physics 节点子项 | action 类型（QuasiStatic；CDPQuasiStatic 仅当档案声明支持）、block（Section 指派带入）、volumetric_locking_correction、incremental、strain、generate_output 多选、save_in（残差变量，勾选“反力输出”自动带出 resid_*） |
| AuxVariables + AuxKernels 套餐 | Outputs 节点“场输出变量”多选 | 勾选 DamageC/DamageT/cdp_* → 模板化成组生成变量+MaterialRealAux（命名约定 cdp_* property 由映射注册表给出） |
| Functions | Functions 节点 | 新增 **PiecewiseLinear**（x/y 数据对表格编辑，支持复载曲线）；保留 ParsedFunction |
| BCs | BC 节点 | type 扩展：DirichletBC（value）/FunctionDirichletBC（function 下拉引用 Functions 节点）；variable 下拉（disp_x/y/z）；boundary 面组下拉 |
| Materials 三件套 | Material 节点 type=AbaqusCDP（新增） | 一个表单成组生成三对象：E/ν（MPa 显示→Pa）、CDP 五参数（ψ/e/fb0fc0/K/μ）、recovery、子步上限、4 个 CSV 文件选择器（加入快照 extra_files）；inelastic_models 引用自动连线 |
| Postprocessors | Outputs 节点“历史输出”子项 | 套餐模板：边界反力（NodalSum resid_*@面组）、边界平均位移（AverageNodalVariableValue）、场量极值（ElementExtremeValue 变量+min/max） |
| Preconditioning/SMP | Step 节点“求解配置” | type=SMP、full=true（默认折叠高级项） |
| Executioner + TimeStepper | Step 节点 | type=Transient、solve_type、line_search、automatic_scaling、容差、num_steps/dtmin/dtmax、petsc options；TimeStepper 子表（dt/optimal_iterations/growth/cutback）——与 *Static 四参数对应 |
| Times | Outputs 节点 | TimeIntervalTimes（间隔），被 Exodus/CSV 的 sync_times_object 引用 |
| Outputs | Outputs 节点 | Exodus/CSV 子块、file_base、sync_only |
| 诊断块（r01 5 块） | Custom Blocks 专家扩展层 | 原文注入 + 合并校验（名称冲突/引用/档案支持），与生成区分离保存 |

### 2.3 关键机制设计

1. **活动应用档案（接线 F-02）**：工作上下文条新增“应用”选择器（项目级单例，决策 4）；`ApplicationProfileRegistry` 已就绪但无调用方。选择后：模块可用节点类型/表单字段/校验/单位换算全部由档案 + mapping-v1 驱动；切换前生成兼容性报告。项目 YAML 记录 `application_profile`（字段已存在）。
2. **mapping-v1 驱动的装配器（接线 F-03，替代字符串拼接）**：`MooseMappingRegistry` 已就绪但只有测试调用。新装配器以“节点 kind + type → 映射注册表 schema”生成对象，按注册表 `ordering` 稳定输出；普通模式 `.i` 只读，重新生成整块替换系统生成区；专家扩展层独立存储、合并时校验（决策 8）。
3. **Physical Groups 清单生产者（接线 F-04）**：网格生成后用 Gmsh API 读回物理组清单（名称/维度/实体数/单元数/网格维度/质量摘要/SHA-256）填充 `PhysicalGroupManifest` 与 `mesh_snapshot_`（两个类都已实现、目前无生产者）；失效传播走既有 `invalidate_downstream_from`。
4. **快照 v2 与提交接线（接线 F-05）**：`export_job_snapshot_v2()` 已完整实现（input_mode 白名单、case_id/profile/PG 必填、相对路径、.e role、traceability、recommended_command），UI 从 v1 切到 v2；`SimClient` solver 名去掉硬编码 `"DamSafetyApp-opt"`，改读活动档案。
5. **单位换算层（决策 7）**：表单显示值（MPa/mm）↔ 求解值（Pa/m）在写回 params 时按档案 unit_contract 换算并记录比例因子；CDP CSV 原样打包（CSV 本身已是 Pa），换算只作用于标量字段。
6. **校验分级（W-05）**：结构（网格存在未过期/PG 有效/材料已指派/Step 完整/输出变量受支持）+ 语义（boundary 存在于面组、block 有 Section 指派、函数/变量引用可解析、维度一致、单位提示）→ 错误阻断提交、警告需确认、可跳转定位；`--check-input` 在本地有可执行程序时自动执行，check_command 模板改从档案读取。

## 3. 能力基线与缺口（盘点结论）

**Phase 0 合同层（F-01～F-05）真实存在但只有测试调用**：profile registry、mapping registry、PG manifest、snapshot v2 均未接入运行中 UI——这是 Phase 4 的第一缺口，也是性价比最高的起点。

**当前生成链路**：`sync_model_to_input()` 仅 7 个 block（Functions/Variables/Materials/BCs/Loads→Kernels/Outputs/第一个 Step），整文件正则 upsert；Physics/Constraints/Assembly 根空壳；Selections 无生产创建路径；模板为整文件替换；校验仅计数级；快照走 v1；solver 名硬编码。

## 4. Phase 4 开发任务方案（切片与验收）

> 在原 W-01～W-07 框架内细化；每个切片给出交付物与验收标准。依赖顺序即编号顺序；W-00 为新增前置。

### W-00 接线 Phase 0 合同层（前置，P0）

| 切片 | 交付物 | 验收 |
|---|---|---|
| W-00a 活动应用档案 UI | 上下文条“应用”选择器 + 项目级持久化 + 切换兼容性报告（只读提示级） | 选择 DamSafetyApp-opt 保存重开仍在；无档案时生成/提交禁用并提示 |
| W-00b PG 清单生产者 | 网格生成后填充 PhysicalGroupManifest + mesh_snapshot_ + Mesh 节点摘要 | 生成 .msh 后 Mesh 节点显示组清单/单元数/质量摘要；重开项目仍在 |
| W-00c 快照 v2 接线 | on_export_snapshot 切换到 export_job_snapshot_v2；SimClient solver 名读档案 | 导出快照含 v2 manifest 全字段；缺 profile/PG 时拒绝导出并说明 |
| W-00d mapping registry 消费 | 装配器与表单共用 mapping-v1（先读 ordering 与 block schema） | registry 损坏时 UI 给出可读错误并禁止生成 |

### W-01 节点类型与过期传播补齐（P0）

| 切片 | 交付物 | 验收 |
|---|---|---|
| W-01a Physics 真实节点 | Physics 子项创建/表单（action 类型/参数，mapping 驱动）/持久化/失效传播 | 创建 QuasiStatic 子项，保存重开恢复；改几何后标 stale |
| W-01b Section 指派语义 | Section 表单：材料下拉 + Physical Volume 多选（体组名下拉） | 指派关系持久化；删除材料/体组后 Section 标 invalid |
| W-01c Selections 生产路径 | 从舞台拾取/物理组创建 Selection 子项 | 创建后面组下拉可引用；保存重开恢复 |
| W-01d Assembly 最小语义 | Assembly 子项记录部件引用+变换；网格生成前落实到 Gmsh | 平移实例后生成的网格位置正确（v01 单部件可退化验证） |

### W-02 Gmsh → PG → .msh 语义链路（P0）

| 切片 | 交付物 | 验收 |
|---|---|---|
| W-02a 组名唯一性/维度/非空校验 | GmshPanel 创建/刷新 PG 时校验 | 同名/空组/维度错误被阻断并定位 |
| W-02b .e 显式导入 | “导入 Exodus 网格”入口 + role 标记 + Mesh 节点登记 | 导入 uniaxial_compression_mesh.e 后可用于生成与快照，manifest 记 input_mesh |
| W-02c FileMeshGenerator 升级 | inject_mesh_block 从 FileMesh 旧写法升级为 FileMeshGenerator 子块 | 生成的 [Mesh] 与基线结构一致 |

### W-03 对象→blocks 显式映射（P0，本 Phase 核心）

| 切片 | 交付物 | 验收 |
|---|---|---|
| W-03a CDP 材料模板 | Material type=AbaqusCDP 表单（弹性+五参数+recovery+子步+4 CSV 选择器+MPa→Pa 换算） | 表单值生成的三件套与 v01 `Materials` 语义一致；CSV 进快照 extra_files |
| W-03b Physics action 生成 | QuasiStatic action（block/volumetric_locking/incremental/strain/generate_output/save_in） | 勾选场输出变量与“反力输出”后生成 v01 式 action + resid_* |
| W-03c BC/Function 类型扩展 | FunctionDirichletBC + PiecewiseLinear 函数编辑 | 顶面位移加载生成 v01/r01 两种函数形式 |
| W-03d 场/历史输出套餐 | Aux 成组生成（DamageC/T、cdp_*）、Postprocessor 套餐（NodalSum/AverageNodal/ElementExtreme）、Times+sync_only | 勾选式生成与 v01 `AuxVariables/AuxKernels/Postprocessors/Outputs` 语义一致 |
| W-03e Step→Executioner 映射 | Executioner/TimeStepper/Preconditioning 表单（*Static 四参数语义） | 生成 v01 式 Executioner；多 Step 给出“不支持串联”明示而非静默取第一个 |

### W-04 确定性装配器（P0）

| 切片 | 交付物 | 验收 |
|---|---|---|
| W-04a 结构化装配服务 | mapping 驱动的块构建 + ordering 稳定输出 + 来源追踪（block→树节点/PG/模板版本） | 同项目连生成两次 diff 为空 |
| W-04b 生成区/专家层分离 | 普通模式只读 + Custom Blocks 注入合并校验 + 差异预览 | r01 诊断块经专家层注入后合并通过；冲突有报错 |
| W-04c 替换 sync_model_to_input | 旧 7-block 链路下线，菜单/按钮切到新装配器 | 旧演示案例（扩散/热力）仍可按其档案生成运行 |

### W-05 生成前校验与预检（P0）

| 切片 | 交付物 | 验收 |
|---|---|---|
| W-05a 结构+语义校验器 | 按 §2.3-6 清单实现，分级/跳转；Workflow ready 真实含义化 | 删材料指派/改组名/缺 Step 均被阻断并定位 |
| W-05b --check-input 集成 | 生成后自动预检（有本地可执行时），check_command 读档案 | v01 等价 .i 通过 check；失败输出进作业日志 |

### W-06/W-07 快照与提交闭环（P0）

| 切片 | 交付物 | 验收 |
|---|---|---|
| W-06 案例包 | v2 快照（.i+网格+CSV+manifest）+ 追溯链 UI | 复制到干净目录可按 manifest 命令运行 |
| W-07 提交闭环 | 标准链分步 + 一键入口；远程提交档案固化；结果自动登记回写 | 一键入口前置条件不满足时禁用并说明；作业完成 Results 自动登记可追溯 |

### M 级验收（本 Phase 准出）

1. **M-P4-1（核心验收）**：仅经 UI 操作，从导入 `uniaxial_compression_mesh.e` 开始，配置 CDP 材料、Section 指派、QuasiStatic action、Step、BC（底面固定+顶面位移函数）、场/历史输出，装配生成 `.i`——与 `tpl-cdpc-tension-single-recheck.i` **语义等价**（块集合、对象 type、参数值逐项一致；block/boundary 命名按 §1.3-2 允许物理组命名差异），`--check-input` 通过。
2. **M-P4-2（复载）**：同一项目改 PiecewiseLinear 复载函数 + 3 s 时长，生成 r01/r02 主体等价 `.i`（不含诊断块）。
3. **M-P4-3（专家扩展）**：r01 诊断块经 Custom Blocks 注入、合并校验通过、快照 manifest 记 `input_mode=expert`。
4. **M-P4-4（闭环）**：快照导出 →（有计算资源时）远程提交 → Results 自动登记 → 可视化加载 Exodus 并可用回放工具组播放。

## 5. 风险与边界

- **档案先行**：DamSafetyApp-opt profile 需补充声明 CDP 物理/AbaqusCDPStressUpdate/自定义 Aux property 命名约定与 supported_blocks，否则 W-03 表单无映射来源（W-00a 同步修订 profile JSON）。
- **语义等价而非文本等价**：block/boundary 命名、注释、参数顺序不追求与转换器逐字符一致；验收以块/type/参数值集合 + check-input + 运行结果对比为准。
- **真实耦合（M-02）与 CDP 高级验证（M-03）** 属 Phase 5 里程碑，不在本方案承诺范围；本 Phase 以 4 个单轴算例的 UI 可复现为闭环标准。
- **旧模板/旧项目兼容**：6 个库模板与演示案例继续可用（走各自档案）；旧 `.gmp.yaml` 按 v1→v2 迁移规则读取。

## 6. 建议实施顺序与回归策略

1. W-00（接线）→ W-03a/CDP 材料 + W-02b/.e 导入（最快可见 v01 材料与网格）→ W-01 → W-03b~e → W-04 → W-05 → W-06/W-07 → M 级验收。
2. 每个切片按 `AGENTS.md` 策略：定向巡览 + CTest；`--check-input` 类验收在本地有 DamSafetyApp-opt 时执行，无则记录跳过原因。
3. 新增巡览步骤：CDP 材料表单合同、装配器确定性（双生成 diff）、校验阻断跳转、v01 等价生成的块级断言（解析生成 .i 与基线块对照）。

---

*本方案落实后，`doc/UI重构开发任务清单.md` 的 Phase 4 章节应按本文件切片更新状态；需求追踪仍使用 REQ-011～017 编号。*
