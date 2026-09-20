# GMP-ISE UI 重构下一阶段任务清单

> 编制日期：2026-09-13
> 启动基线：`e9df9ef`（Phase 4 / G0 收口提交）
> 依据：`doc/UI重构需求记录.md`、`doc/UI重构开发任务清单.md`、`doc/UI重构Phase4方案设计.md`、`doc/UI重构Phase4人工验收清单.md`、`manual/test04.md` 与 `manual/test05.md`
> 范围：Phase 4 收口、Phase 5 真实装配/接触算例、Phase 6 多 Step 串联

---

## 1. 当前结论

下一阶段不再重复开发已经人工通过的 Section、CDP 材料、Physics、单 Step、BC/Function、Outputs 与快照上传能力，而按以下顺序推进：

1. **Phase 4 收口**：已于 2026-09-14 完成人工准出并关闭 G0。
2. **Phase 5 真实前处理语义**：已进入 G1，先实现 Assembly，再推进 Load、Contact、Reference Point/Coupling 与混凝土块 + 钢板端到端算例。
3. **Phase 6 多 Step**：在 BC/Load/Constraint/Contact 已具备真实激活语义后，实现阶段顺序与求解状态继承。

多 Step 可以在项目中保存，但当前生成器只使用第一个 Step 并告警。这是已验收的阶段性限制，不是最终产品行为。

## 2. 已验证基线与未闭环项

| 能力域 | 关联需求/任务 | 当前结论 | 下一阶段处理 |
|---|---|---|---|
| CDP 材料与 Section | REQ-011/013，W-01b、W-03a | 已人工通过；删除材料后失效、同名恢复已通过 | 仅做回归，不重做 |
| Physical Groups 与 Mesh | REQ-012，W-02a~c | `solid/fixed/load`、FileMeshGenerator、快照网格及通用网格人工巡检均已通过 | Phase 5 装配网格回归 |
| Physics、单 Step、BC/Function、Outputs | REQ-013，W-03b~e | 已人工通过；重复同步无重复受管块 | 作为 Phase 5 新对象映射的基线 |
| 确定性装配 | REQ-014，W-04 | 结构化只读、专家扩展、来源追踪及保存重开均已人工通过 | 仅做回归，不重做 |
| 生成前校验 | REQ-015，W-05 | 统一阻断、问题定位及安全入口已人工通过 | 仅做回归，不重做 |
| 可复现快照 | REQ-016，W-06 | 中文 `.i`、网格、4 个 CSV、manifest/hash 打包与上传已通过 | 补不可变性与项目→快照→Job→Result 追溯展示 |
| 远程闭环 | REQ-017，W-07 | 提交、远端启动、4 MPI、状态监控和取消终态通过 | 2026-09-13 用户确认满足当前阶段验收；成功制品/Results 回放随 M-01 再验 |
| 多 Step | REQ-018，TASK-NEXT-001 | 当前明确告警并只取首个 Step | Phase 6 实现真正的顺序接力与历史状态继承 |

## 3. Stage A：Phase 4 收口（发布闸门 G0）

### TASK-P4-CLOSE-01 安全的一键生成/提交前置校验

> **完成状态（2026-09-14）**：代码和人工验收均已完成；`TEST-P4-G0-01`、
> `TEST-P4-G0-02` 均已人工验收通过并关闭。验收中发现的新建/临时项目沿用上次 MOOSE 输入文件、
> 工作目录和网格路径问题已完成代码修复、定向自动回归和人工复核，缺陷已关闭。
> 13.2 新发现的旧项目生成制品路径未迁移及定位属性窗被浮动作业窗覆盖问题已修复并
> 通过定向自动回归和人工复核。

- **优先级**：P0
- **关联**：REQ-011、REQ-015、W-05、W-07
- **目标**：一键流程只编排用户已经配置完成的对象，不自动创建 `u`、`left`、默认 BC/Load 等占位模型。
- **实现**：
  1. 将自动补造工作流节点改为只读的前置条件检查。
  2. 汇总 Mesh、Section、Physics、Step、BC/Load、Outputs、应用档案与文件引用问题。
  3. 错误阻断生成/快照/提交；警告可查看并按规则确认。
  4. 点击问题跳转到模型树节点和具体字段。
- **验收**：缺任一必需对象时按钮禁用且不修改项目；合法 CDP 项目可完成同步、预检、快照和提交。
- **已修复并人工回归通过**：新建项目不再继承上一项目或自动巡览的 `input_path` /
  `workdir` / `mesh_path`；打开项目先清理项目态，再只恢复当前 YAML 已保存的值。
  `Untitled` 项目的路径和生成输入默认为空，示例模板只在明确应用后写入。
  `project_context_isolation_contract` 定向巡览及 2026-09-14 人工复核均已通过。

### TASK-P4-CLOSE-02 完成 W-04 装配器与专家扩展层

> **完成状态（2026-09-14）**：结构化只读、Custom Blocks 独立保存、冲突校验、合并预览和生成来源报告均已完成；`TEST-P4-G0-03`、`TEST-P4-G0-04` 已人工验收通过并关闭。网格路径迁移后，Mesh/Job/输入/报告统一指向项目自有 `.msh`，本任务关闭。

- **优先级**：P0
- **关联**：REQ-014、W-04、M-P4-2、M-P4-3
- **目标**：系统受管输入可追溯、可重复，专家内容有边界地扩展而不覆盖模型树。
- **实现**：
  1. 为生成 block 记录模型树节点、Physical Group、模板与 mapping 版本来源。
  2. 普通模式保持只读；实现 Custom Blocks 独立存储、冲突校验与合并差异预览。
  3. 完成 PiecewiseLinear 复载 + 3 s 算例的保存重开、再次生成与幂等验证。
- **验收**：相同项目连续生成文本一致；Custom Block 冲突被定位；manifest 正确记录 `structured/expert` 输入模式。

### TASK-P4-CLOSE-03 完成远程成功结果闭环

> **实施状态：已完成（2026-09-13 用户确认）**。当前阶段以“任务可正常提交、远端求解器启动、运行状态可观察”为验收边界；已由 `job_20260913_223929_xu10t1` 提供证据。该作业后续由用户取消。成功制品下载、Results 自动登记与 Exodus 回放不再阻断 G0，转入 M-01 端到端结果验收。

- **优先级**：P0
- **关联**：REQ-016、REQ-017、W-06、W-07、M-P4-4、TEST-P4-E2E-01
- **目标**：当前阶段完成“提交—启动—监控—终态”远程生命周期。
- **实现**：
  1. 当前阶段已完成 queued/preparing/running/canceled 状态和运行资源展示。
  2. 用户取消作为正常终态记录，不误报为产品故障。
  3. succeeded/failed 终态、制品下载、Results 自动登记和 Exodus 回放在 M-01 中继续验证并收口。
- **验收**：当前阶段已按用户确认完成；成功结果自动登记、下载、加载与回放并入 TASK-P5-03 / M-01。

### TASK-P4-CLOSE-04 关闭 Phase 4 剩余人工准出项

> **完成状态（2026-09-14）**：新增定向合同、106 步全量真实点击巡览与 CTest `1/1`
> 均已通过；用户已完成人工操作并明确确认下列 6 项全部通过。Phase 4 / G0 最终准出，
> 当前无未登记的 G0 阻断缺陷。

- **优先级**：P0
- **关联**：REQ-010、REQ-012、M-P4
- **范围**：
  - `TEST-P4-W01-02`：从物理组创建 Selection、被引用、保存重开。
  - `TEST-P4-GEN-04`：大型网格规模预检、进度、取消与旧文件保护。
  - `TEST-P4-GEN-05`：结构化四边形/砖形优先与带孔回退。
  - `TEST-P4-GEN-06`：拓扑策略、严格模式与持久化。
  - `TEST-P4-GEN-07`：多个 Mesh/Part 的下拉、菜单与重开恢复。
  - `TEST-P4-GEN-08`：多个 Job 的独立网格引用与重开恢复。
- **准出标准**：定向巡览通过后执行全量真实点击巡览与 CTest；上述人工项全部有结论，无未登记 P0 缺陷。
- **准出结论**：通过。`TEST-P4-W01-02`、`TEST-P4-GEN-04`～`08` 已于
  2026-09-14 由用户统一确认完成人工验证。

## 4. Stage B：Phase 5 真实装配、接触与耦合（发布闸门 G1～G3）

逐条人工操作、负向检查、证据要求和闸门回填见 `manual/test05.md`。

### TASK-P5-01 Assembly 实例与变换

> **实施状态（2026-09-15）**：已完成。Assembly 独立根节点、Part 实例引用、变换、
> 可见性/顺序、保存重开、上游过期传播、Gmsh 实例化和装配网格均已交付；
> `TEST-P5-G1-02`～`05` 已由用户完成人工验证。`TEST-P5-G1-05` 证据包含项目自有 3D
> 装配网格、两个实例体组、两个实例面组、节点/单元/质量摘要和完整 SHA-256。
> `manual/test05.md` §4.2 专用作用面组的项目级持久化与 Assembly 稳定重绑定已实现，
> `assembly_instance_contract` 定向回归及人工验证均已通过；已进入 `TASK-P5-02` Load 与
> Interaction/Contact 真实映射。

- **优先级**：P0
- **关联**：REQ-011、REQ-012、W-01d
- **交付**：Part 实例引用、平移/旋转、可见性与顺序；变换在网格生成前落实到 Gmsh；修改后 Mesh、输入、快照与 Job 正确过期。
- **验收**：混凝土块和钢板可独立定位；保存重开一致；生成网格的体组/面组与实例位置一致。

### TASK-P5-02 Load 与 Interaction/Contact 真实映射

> **实施状态（2026-09-16）**：开发完成，等待人工验证。DamSafetyApp + mapping v1 已冻结
> `Pressure` 与 `Contact` 生产映射；Pressure 通过命名二维 Physical Group 写入 `[BCs]`，
> Contact 使用 `primary`/`secondary`、`model`、`formulation` 和
> `friction_coefficient` 写入 `[Contact]`。表单候选由活动档案与 mapping 共同注入；无映射、
> 主从面相同、维度错误、重复接触对及同面同变量的 BC/Pressure 冲突均进入预检。
> `contact_load_mapping_contract`、`bc_function_dirichlet_contract` 定向巡览和 CTest 已通过。

- **优先级**：P0
- **关联**：REQ-013、REQ-015、M-01
- **交付**：
  1. 先冻结 DamSafetyApp 档案实际支持的位移、面力/压力及接触类型和参数。
  2. Load 使用命名边界/体组选择器，不接受游离 tag。
  3. Contact 明确主从面、法向、摩擦及应用兼容性；不支持时阻断生成。
  4. 为 Load/Contact 增加引用、维度、重复与冲突校验。
- **验收**：生成的真实 MOOSE 对象通过 `--check-input`，不得用普通 BC 静默冒充未实现的 Load/Contact。

### TASK-P5-03 M-01 线弹性 + 面—面接触算例

> **实施状态（2026-09-16）**：UI 配置、确定性输入生成、来源报告、目标应用输入检查入口、
> contract v2 快照、远程作业终态与 Results 回放基础能力均已具备；现进入
> `TEST-P5-G1-06`～`09` 人工验证。最终 `--check-input` 与远程 `succeeded` 依赖用户的
> DamSafetyApp 运行环境和本轮 M-01 项目证据，不在本地自动测试中伪造。
>
> **最终回填（2026-09-20）**：`TEST-P5-G1-08/09` 按用户长期实际使用结论验收通过，
> 不再追补独立外部证据；G1 9/9 已关闭。G2 实施在 HARD-010～050 完成后恢复。

- **优先级**：P0
- **关联**：M-01、REQ-011～017
- **验收**：仅经 UI 建立混凝土块 + 钢板、Section、接触、固定与位移加载、输出；输入预检通过；远程成功；Exodus/CSV 自动登记并可回放。

### TASK-P5-04 Reference Point 与 Coupling Constraint

- **优先级**：P1
- **关联**：REQ-011～013、M-02
- **交付**：参考点节点、耦合面选择、自由度/运动学参数、真实应用映射和输出。
- **验收**：生成对象不是整面统一位移的替代实现；参考点与耦合面的位移、合力传递符合预期。

### TASK-P5-05 M-02/M-03 高级算例与基准对比

- **优先级**：P1
- **关联**：M-02、M-03、REQ-016、REQ-017
- **验收**：
  1. Reference Point + Coupling 算例远程成功并完成结果闭环。
  2. CDP 参数来源、单位换算和材料文件进入 manifest 追溯链。
  3. 与认可基准比较力—位移、损伤和反力，形成可复核报告；阈值在执行前冻结。

## 5. Stage C：Phase 6 多 Step 串联与状态继承（发布闸门 G4）

### TASK-P6-01 冻结多 Step 执行策略

- **优先级**：P1
- **父任务**：REQ-018、TASK-NEXT-001
- **决策**：根据目标 MOOSE 应用能力，在“单作业连续时间线 + Controls/函数”与“多作业 restart/recover”中选定正式策略；禁止简单生成多个冲突的 `[Executioner]`。
- **交付**：能力矩阵、状态继承合同、失败恢复和结果拼接规则。

### TASK-P6-02 Step 序列与对象激活 UI

- **优先级**：P1
- **交付**：Step 新建、重排、复制、时间区间；BC/Load/Constraint/Contact/Output 的创建、保持、修改、停用矩阵；保存重开一致。
- **验收**：时间重叠/断裂、非法停用和应用能力不兼容被定位并阻断。

### TASK-P6-03 生成、快照、作业和结果链路

- **优先级**：P1
- **交付**：每个 Step 的时间段、激活对象、输入块、继承或 restart 文件写入生成报告与 manifest；Job/Results 可按 Step 追溯。
- **验收**：相同项目重复生成幂等；恢复执行不丢失材料内部变量。

### TASK-P6-04 三阶段 CDP 回归算例

- **优先级**：P1
- **场景**：加载 → 保持 → 卸载/再加载。
- **验收**：Step N+1 从 Step N 末状态继续，位移、损伤、塑性历史不重置；分阶段输出和合并时间线正确。

## 6. 推荐迭代顺序与准出条件

| 闸门 | 建议内容 | 必须满足的准出条件 |
|---|---|---|
| G0 | TASK-P4-CLOSE-01～04 | Phase 4 剩余人工项关闭；远程提交/启动满足当前验收边界；全量巡览 + CTest 通过 |
| G1 | TASK-P5-01～03 | M-01 从 UI 建模到远程结果闭环，无手改 `.i` |
| G2 | TASK-P5-04 | Reference Point/Coupling 为真实映射且传力验证通过 |
| G3 | TASK-P5-05 | M-02/M-03 成功，基准与容差有报告 |
| G4 | TASK-P6-01～04 | 多 Step 历史状态连续、可恢复、可追溯 |

每个开发任务仍按工程约定先跑 1～2 个定向巡览；到 G0/G1/G2/G3/G4 或提交前再跑全量巡览与 CTest。

## 7. 执行前待确认问题

1. **Contact/Coupling 支持矩阵**：以实际部署的 DamSafetyApp 语法和版本为准，确认后再冻结 profile/mapping，不在 UI 中先造不可运行类型。
2. **专家模式准出范围**：建议在 G0 完成 Custom Blocks、冲突校验和 diff；若产品决定延期，必须明确标记 M-P4-3 未完成，不能将 Phase 4 整体标为完成。
3. **CDP 基准容差**：M-03 开始前确认参考输入、材料曲线版本、力—位移/损伤比较指标和容差。
4. **多 Step 技术路线**：待 TASK-P6-01 调研结论后冻结；当前“仅取第一个 Step”告警在 G4 前保留。

## 8. 后续阶段 TODO（不纳入当前 G1～G4 准出）

### TASK-NEXT-002 二维线—线（边界—边界）接触

- **状态**：TODO；当前仅完成可行性确认，不修改或集成现有 G1 三维 Contact。
- **优先级**：P2；建议在 Phase 6 / G4 完成后进入后续阶段。
- **目标**：二维模型使用一维 Physical Group 作为主/从接触边界，继续生成标准
  `[Contact]`；产品界面可称“线—线接触”，内部语义保持 MOOSE 的 primary/secondary
  boundary（离散层面可能为 node-to-segment 或二维 mortar），不新增伪 `LineContact` 类型。
- **可行性依据**：MOOSE 官方 Contact 教程明确 Contact 可用于 2D 与 3D；`primary`、
  `secondary` 接收 boundary/sideset 名称。现有工程的默认 Contact、工作流预检、Gmsh/MOOSE
  边界组读取已多处使用 `mesh_dim - 1`，具备维度自适应基础。
- **当前缺口**：
  1. `DamSafetyApp-opt` 档案目前只声明三维能力；需先用实际部署求解器冻结二维平面应变/
     平面应力、变量和 Contact formulation 支持矩阵。
  2. `mapping-v1.json` 的 `primary`/`secondary` 仍固定 `dim=2`，应扩展为“边界维度”语义，
     即二维模型取 1D 组、三维模型取 2D 组。
  3. Contact 表单标签与非法引用提示仍写死“主面/从面、2D Physical Group”，应按模型维度
     显示“主线/从线”或统一为“主边界/从边界”。
  4. 需验证二维网格清单、1D Physical Group 持久化、线拾取高亮、主从方向和法向约定。
- **交付**：二维力学 application profile；动态 boundary-dimension schema；维度感知的
  Contact 表单/校验/报告；二维接触样例与独立人工验收说明。
- **验收**：
  1. 二维模型的主/从下拉只列一维 Physical Group，三维现有行为保持只列二维组。
  2. 生成标准 `[Contact]`，通过目标 DamSafetyApp `--check-input`，不得手工修改 `.i`。
  3. 至少一个二维无摩擦算例和一个二维 Coulomb 算例远程成功，并验证接触压力、穿透量及
     反力；错误维度、同组和方向异常均能明确阻断。
  4. 现有 G1 三维 `contact_plate_concrete` 全量回归不退化。
