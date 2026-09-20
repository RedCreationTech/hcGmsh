# GMP-ISE 项目进度与后续总体规划

> 编制日期：2026-09-19（2026-09-20 用户批准执行并持续回填）
> 当前代码基线：`main@4c184f4a3d5b`
> 规划模式：项目总览 + 开发任务编排
> 主要依据：`doc/UI重构开发任务清单.md`、`doc/UI重构下一阶段任务清单.md`、`manual/test05.md`、`doc/v0.2重构任务清单.md`、`doc/v0.2.1架构硬化任务清单.md`、`doc/ref/HC-Gmsh-v0.2-重构成果复核与进一步优化计划.md`
> 状态：**已批准执行；G1 已关闭，HARD-010～050 已完成；G2 目标应用支持矩阵已冻结，进入产品实现阶段**

---

## 0. 执行结论

### 0.1 核心判断

下一步不应选择以下两个极端：

- 不继续做无边界的架构重构；当前模块命名、测试拆分、Viewport/Gmsh 二次解耦都不是近期交付阻塞。
- 不立即把 Reference Point/Coupling、多 Step、Feature Graph 继续堆在 Tree 真源和 name reference 上；这会放大迁移成本并制造新的悬空引用。

推荐采用：

> **先完成一个有限的架构硬化切片，再立即恢复既定功能开发。**

有限切片只包含：

```text
ProjectDocument 装载合同
  -> 持久 ObjectId
  -> ProjectStore <-> ProjectDocument
  -> 最小领域命令
  -> Document 写入 / Tree 投影
```

即执行 `TASK-HARD-010～050` 后停止架构扩张。对象级 DependencyGraph（HARD-060）和内部 Undo/Redo（HARD-070）保留为后续任务，不阻塞 G2/G3 功能恢复。

### 0.2 推荐的产品顺序

1. 关闭 v0.2 RG-D，冻结 `v0.2-rc1`。
2. G1-08/09 按用户持续实际使用结论验收通过，不再保留外部证据待办。
3. 完成 `HARD-010～050` 的最小架构硬化。
4. 恢复 Phase 5：Reference Point/Coupling（G2）→ CDP 基准闭环（G3）。
5. 在多 Step 前完成对象级依赖图，再实施 Phase 6（G4）。
6. 工业 CAE 闭环稳定后，再进入 v0.3 Feature Graph/参数化 CAD。

### 0.3 为什么不是继续全量重构

- v0.2 已把 ProjectStore、MOOSE 生成、Snapshot、Gmsh 服务、Viewport 第一阶段和 CMake 边界抽离；继续拆模块的边际收益已明显下降。
- 当前 CTest 单目标不足 1 秒，拆测试目标没有现实收益。
- `VtkViewer` facade、Gmsh 全局 Session、模块目录不理想，但尚未阻断单项目、串行网格和既定 G1～G3。
- 当前最有产品价值的缺口是“真实求解成功和结果闭环”，不是再增加一批内部类。

### 0.4 架构硬化后如何恢复功能开发

`HARD-010～050` 已解决持久 ObjectId、Document 真源、Store 直连 Document 和最小可逆领域命令问题。Reference Point/Coupling 可以进入实现，但必须继续遵守：

- 新对象和跨对象引用使用持久 ObjectId，不新增 Tree-only 数据。
- 目标应用语法先经真实版本验证，再进入 profile/mapping 和 UI。
- G2 只实现 Reference Point/Coupling 闭环，不顺带扩展多 Step、Feature Graph 或通用约束框架。
- `HARD-060`、`HARD-070` 分别保留到 G4 与 Feature Graph 前，不重新阻塞 G2/G3。

## 1. 当前基线

### 1.1 代码与验证状态

| 项目 | 当前状态 | 结论 |
|---|---|---|
| Git 基线 | `main`、`origin/main` 均为 `4c184f4a3d5b` | HARD-010～050 与当前 C4 总结已推送 |
| 构建 | `cmake --build build -j4` 无待编译项 | 通过 |
| CTest | `gmp_ise_phase0`，`1/1`，约 `0.94 s` | 通过 |
| GUI 巡览 | 114 步真实点击基线 | 最近 v0.2 提交记录全绿 |
| G1 确定性 | `.i` 与基线逐字一致 | 通过 |
| Snapshot | manifest 除时间字段外一致 | 通过 |
| RG-D | 2026-09-20 用户确认通过 | ✅ 已关闭 |
| tag | `v0.2-rc1` | ✅ 已冻结 |

### 1.2 代码规模只作为风险信号

| 文件 | 当前行数 | 判断 |
|---|---:|---|
| `src/MainWindow.cpp` | 19,223 | 仍大，但生成/存储/Gmsh 主逻辑已外移；下一步只迁领域真源，不做全面拆窗 |
| `src/GmshPanel.cpp` | 4,168 | 服务已抽离，UI/会话编排仍集中 |
| `src/MoosePanel.cpp` | 3,218 | 快照已抽离，Job/Remote/UI 仍共置 |
| `src/VtkViewer.cpp` | 1,948 | 第一阶段 facade 已完成 |
| `src/ViewportInternal.h` | 705 | 需观察，当前不单独治理 |
| `tests/test_phase0.cpp` | 2,188 | 单目标可维护性开始下降，但执行时间尚非问题 |

代码行数不作为拆分任务的充分理由。只有职责变化频繁、回归难定位或构建/测试时间成为瓶颈时才继续拆分。

## 2. 整体进度总览

### 2.1 里程碑状态

| 轨道 | 里程碑 | 当前状态 | 精确结论 |
|---|---|---|---|
| UI | Phase 0～3 | ✅ 完成 | 数据合同、布局、交互、视觉/可访问性均已自动与人工准出 |
| 前处理 | Phase 4 / G0 | ✅ 完成 | 应用档案、Physical Groups、材料/Section、Physics/Step、BC/Load、Outputs、生成校验、快照、提交入口已准出 |
| 真实算例 | Phase 5 / G1 | ✅ 9/9 通过 | G1-01～07 按原证据通过；G1-08/09 于 2026-09-20 按用户长期实际使用结论验收通过，不另补外部证据 |
| 高级约束 | Phase 5 / G2 | 🟡 实施中 | 目标语法与最小传力探针已验证；产品领域对象、UI、生成、校验和端到端准出待完成 |
| 高级材料/基准 | Phase 5 / G3 | ⚪ 未实现 | M-02/M-03、CDP 认可基准与容差未冻结 |
| 多阶段 | Phase 6 / G4 | ⚪ 仅有需求 | 当前只保存多个 Step，生成器只使用首个 Step 并告警 |
| 架构 | v0.2 Stage 0～7 | ✅ 已关闭 | 7 个 Stage 已完成；RG-D 人工总准出通过；`v0.2-rc1` 已冻结 |
| 架构硬化 | v0.2.1 | ✅ 核心切片完成 | HARD-010～050 已完成；恢复 G2/G3 功能开发 |
| 参数化 CAD | v0.3 | ⚪ 路线规划 | Feature Graph/Recompute/Topology Identity 尚未开始 |

### 2.2 功能能力矩阵

| 能力域 | 已交付 | 尚缺 | 下一动作 |
|---|---|---|---|
| 项目生命周期 | 新建、打开、保存、另存为、项目状态隔离、schema v2、持久 ObjectId、Document 直接持久化与领域写入 | 无当前功能阻塞 | G2/G3 继续复用 |
| UI 工作台 | 两栏布局、工具组、模型/结果导航、浮动属性窗、独立工作窗、布局恢复 | 少量低优先级 UX 改进 | 只修真实缺陷 |
| Sketch | 绘制、约束、尺寸、逻辑形状、Undo/Redo、保存恢复 | DoF/过约束诊断、Trim/Extend、Mirror/Pattern 等高级能力 | 参数化 CAD 阶段 |
| Part/Feature | 拉伸等特征结果、Feature 历史、BREP/mesh 产物 | 不是可重放 Feature Graph；旧 Feature 只是快照历史 | v0.3 Feature Graph |
| Assembly | 实例引用、平移/旋转/缩放、可见性/顺序、预览、网格 | 高级装配约束、干涉检查 | G2 后再排 |
| Physical Groups | 自定义组、owner+bbox 恢复、维度校验、网格清单 | 完整 Topological Naming | Selection Identity v2 |
| Mesh | 2D/3D、结构化优先、规模预检、取消、原子落盘、摘要 | 多 Session/后台并发 | 有实际并发需求时再做 |
| 材料/Section | 线弹性、CDP 表单、Section 指派、单位合同 | CDP 真实基准闭环 | G3 |
| Physics/Step | Physics action、单 Step Executioner、表单/校验 | 多 Step 状态接力 | G4 |
| BC/Load/Contact | Dirichlet/Function、Pressure、3D Contact、冲突/维度校验 | Reference Point/Coupling；二维接触 | G2；二维接触后置 |
| MOOSE 输入 | 结构化生成、Custom Blocks、diff/冲突、来源报告、确定性 | 多 Step | G4 |
| Snapshot | contract v2、hash、材料 CSV、网格/输入打包、上传 | 无当前 G1 遗留 | G2/G3 继续复用 |
| Job | 本地/远端提交、状态、4 MPI、取消、日志与文件列表基础 | 无当前 G1 遗留 | G2/G3 继续复用 |
| Results | Exodus/CSV 登记与 VTK 回放基础、变量/时间轴/切片 | 无当前 G1 遗留 | G2/G3 继续复用 |

### 2.3 架构进度矩阵

| 架构目标 | 已完成 | 剩余问题 | 处理时点 |
|---|---|---|---|
| 核心对象层 | ProjectDocument/ObjectId/PropertyBag；五类可逆领域命令；Document 为业务真源 | 引用字段尚未全部 ObjectId 化 | 按功能触达渐进迁移 |
| 持久化 | ProjectStore 与 Document 直接互转；Tree 仅作递归投影 | 无当前阻塞 | 保持 schema v2 兼容 |
| 依赖传播 | DependencyGraph 算法与 stale 接线 | 节点仍为 kind；legacy CAE 规则泄漏到 core | HARD-060，G4 前 |
| 事务 | TransactionManager、五类可逆领域命令、UI 真实命令提交、失败事务闸门 | 无 Undo 栈 | HARD-070，Feature Graph 前 |
| Simulation | MooseInputGenerator/SnapshotService 无 Widget | Generator 依赖 Store DTO | v0.3 前后按实际需要处理 |
| Gmsh | PhysicalGroup/Mesher/Assembly 服务抽离 | 全局 current model/session | 多项目或后台 mesh 前处理 |
| Viewport | Sketch/Mesh/Result 子视口 + facade | 状态仍在 facade；共享 God Header 风险 | 出现真实修改冲突时处理 |
| 构建 | hc_core/hc_simulation/hc_mesh | 模块归属仍粗 | SSOT 完成后再评估 |
| 测试 | 114 步 GUI + CTest + 10 个服务合同 | 单一测试文件、平台矩阵不足 | 测试变慢或 CI 上线时处理 |

## 3. 剩余事项分级

### 3.1 P0：当前发布与后续开发前置

HARD-010～050 已关闭稳定身份与 Tree 真源问题；当前无阻塞 G2/G3 的架构 P0。

### 3.2 P1：既定产品功能

1. G2 Reference Point/Coupling。
2. G3 CDP 认可基准与高级算例。
3. G4 多 Step 激活、状态继承、快照/Job/Result 追溯。
4. v0.3 Feature Graph/Recompute 与 Topology Identity。

### 3.3 P1：后置架构硬化

1. Object-level DependencyGraph（HARD-060）：应在多 Step/Feature Graph 前完成。
2. 内部 Undo/Redo 栈（HARD-070）：应在 Feature Graph 大量可编辑操作前完成，不阻塞 G2/G3。
3. SimulationModelSnapshot：当 Store DTO 开始限制新求解器或 Headless API 时再抽。

### 3.4 P2：已登记体验改进

| 缺陷/改进 | 状态 | 建议 |
|---|---|---|
| 2026-09-17-020 Feature 摘要缺 brep | 已登记 | 与 Feature Graph 一起处理 |
| 2026-09-17-022 phys_id 缺组名图例 | 已登记 | Results 体验批次处理 |
| 2026-09-17-023 缺装配预览/体网格来源切换 | 已登记 | G2/G3 使用反馈后处理 |
| 2026-09-19-028 统计表预览计数易混淆 | 已登记 | 文案/状态展示小修 |

下列项目代码已修复但文档仍标记“待人工复验”：2026-09-06-013/014、2026-09-13-001、2026-09-15-003/004、2026-09-16-017/018。应在下一轮人工回归中批量核对并回填，不单独开启开发阶段。

## 4. 三种路线比较

| 路线 | 收益 | 主要代价 | 判断 |
|---|---|---|---|
| A. 继续完整架构重构 | 边界更整齐；为 Headless/多项目打底 | 长时间没有用户价值；容易扩展到模块重排、Viewport/Gmsh 等非阻塞项 | 不推荐 |
| B. 立即全面补功能 | G2/G3 表面进度最快 | 新增更多 name reference、Tree 写路径和不可逆编辑，随后返工 | 不推荐 |
| C. 最小硬化后恢复功能 | 消除身份/真源两个根阻塞；快速回到算例闭环 | 需要接受部分架构债继续存在 | **推荐** |

路线 C 的止损规则：

- HARD-010～050 完成后，不得以“顺手”为理由继续拆模块。
- HARD-060/070 只有在进入 G4/Feature Graph 前才升级为前置。
- 新功能若能复用现有服务和 schema，不新增框架或抽象层。
- 每个阶段必须产生可运行算例、测试证据或明确的兼容性收益。

## 5. 后续执行路线

### Milestone M0：关闭现有基线

#### TASK-PLAN-001 关闭 v0.2 RG-D

- **优先级**：P0。
- **关联**：TASK-HARD-000、`doc/v0.2-RG-D-总准出复核说明.md`。
- **工作**：完成人工总复核；发现问题先登记修复；通过后回填并打 `v0.2-rc1`。
- **产出**：明确的冻结提交和发布基线。
- **准出**：RG-D 结论有用户确认，tag 存在。
- **状态**：✅ 已完成（2026-09-20）；用户确认 RG-D 通过，冻结 `v0.2-rc1`。

#### TASK-PLAN-002 清理“已修复待复验”状态

- **优先级**：P1，可并入 RG-D。
- **工作**：批量核对 §3.4 的 7 项记录，只更新验收结论，不重复开发已修复功能。
- **准出**：每项状态改为“人工通过”或登记新的可复现差异。

### Milestone M1：最小架构硬化

#### TASK-PLAN-010 完成 HARD-010～030

- **优先级**：P0。
- **范围**：ProjectDocument 乱序/层级/顺序/原子装载；schema v2 可选持久 ID；ProjectStore 与 Document 直接互转。
- **不做**：schema v3、全量 name reference 迁移、SimulationModel 新层。
- **准出**：旧项目无 ID 可读；首次保存固化 ID；rename ID 不变；保存不遍历 Tree 采集模型。
- **状态**：✅ 已完成（2026-09-20）。

#### TASK-PLAN-011 完成 HARD-040～050

- **优先级**：P0。
- **范围**：最小可逆领域命令；CRUD/属性/状态先写 Document；Tree 递归投影；持久化、生成、校验从 Document/纯数据快照读取。
- **不做**：用户可见全局 Undo/Redo、通用 Event Bus、模块目录重排。
- **准出**：正常数据流为 `Command -> ProjectDocument -> Tree`，不存在 Tree 反向全量重建业务路径。
- **状态**：✅ 已完成（2026-09-20）；114/114 真实点击巡览与 CTest 1/1 通过。

#### M1 停止条件

完成 TASK-PLAN-010/011 后立即进入功能轨道。以下项目明确不属于 M1：

- HARD-060 对象级依赖图。
- HARD-070 Undo/Redo 栈。
- VtkViewer/Gmsh Session 第二轮。
- CMake 模块重命名和测试目标拆分。

### Milestone M2：关闭 G1 真实算例

#### TASK-PLAN-020 补齐 TEST-P5-G1-08

- **优先级**：P0。
- **依赖**：可用的目标 `DamSafetyApp-opt` 环境。
- **工作**：对冻结的 M-01 快照执行真实 `--check-input`；不得手工修改 `.i`。
- **准出**：目标应用返回成功；日志、应用版本、mapping 版本进入证据。
- **状态**：✅ 用户验收通过（2026-09-20）；依据长期实际使用结论关闭，不再补独立外部证据。

#### TASK-PLAN-021 补齐 TEST-P5-G1-09

- **优先级**：P0。
- **依赖**：远端计算环境、TASK-PLAN-020。
- **工作**：提交同一快照，取得 `succeeded`、Exodus/CSV/log 制品，自动登记 Results 并回放；检查接触量和反力不是空占位。
- **准出**：G1 9/9 关闭，项目→快照→Job→Result 可追溯。
- **调度规则**：外部环境若在 M1 前恢复，先执行一次；M1 完成后再做最小回归确认。
- **状态**：✅ 用户验收通过（2026-09-20）；依据长期实际使用结论关闭，不再补独立外部证据。G1 按 9/9 关闭。

### Milestone M3：G2 Reference Point / Coupling

#### M3 阶段目标

仅通过 GMP-ISE UI 建立可持久化的 Reference Point 与 Coupling，生成目标 DamSafetyApp 接受的真实约束对象，并形成从领域对象、输入、快照、Job 到 CSV/Exodus 的完整追溯。G2 准出必须同时证明：

1. Reference Point 是带持久 ObjectId 的独立工程对象，保存重开和 rename 不丢失身份。
2. Coupling 通过 ObjectId 引用参考点，通过 Physical Group 引用耦合面；引用、维度和自由度错误在提交前阻断。
3. 生成使用已验证的 `EqualValueBoundaryConstraint`，不得退化为加载面统一 `DirichletBC`。
4. UI 生成项目在锁定目标版本上预检和求解成功，参考点与耦合面位移一致，反力输出非空且可追溯。
5. `TEST-P5-G2-01～05`、CTest 和不少于 114 步真实点击巡览全部通过。

本阶段不实现多 Step、CDP 基准、对象级 DependencyGraph、全局 Undo/Redo 或通用求解器中立 Constraint 框架。

#### TASK-PLAN-030 冻结目标应用支持矩阵

- **优先级**：P0。
- **关联**：TASK-P5-04、M-02。
- **工作**：确认目标应用实际对象语法、参数、自由度、输出和限制；先有 `--check-input` 原型，再开 UI。
- **准出**：能力矩阵与 mapping 合同确定，不用普通面位移伪装 Coupling。
- **状态**：✅ 已完成（2026-09-20）。
- **冻结合同**：
  - 目标版本：DamSafetyApp `8e0ddf5c165bbae1e83b3b4f66e284bc36d3d7c3`、MOOSE `4bce02d91b56c7ed845a5747df4d24f415592504`。
  - 约束对象：`EqualValueBoundaryConstraint`。
  - 关键参数：`variable`、`primary_node_coord`、`secondary`、`penalty`、`formulation = kinematic`。
  - 小应变线弹性探针使用 total small strain；`ComputeLinearElasticStress` 不得与 `incremental = true` 组合。
  - 参考点输出按稳定边界名 `rp_load` 读取，不依赖网格导入后可能变化的 node ID。
- **外部证据**：
  - 首轮 `job_20260920_115426_4wamaj`：`--check-input` 通过；因探针误设 `incremental = true` 在材料初始化阶段失败，未产生 CSV，不属于接口读取故障或 Coupling 语法失败。
  - 最终 `job_20260920_141020_cch4er`：`succeeded`；输入 SHA-256 `fcd9082302b37151fdda477927c88d56f6c6eb172948f9eeeaba72f5fb942ab9`，网格 SHA-256 `3dcda34fbf09a945632140e539e40ac3407c1692a7093ca0cf574d3decbe3edb`。
  - 实测 `rp_disp_z = -0.001 m`、`top_disp_z = -0.0010000000000659 m`，绝对差约 `6.6e-14 m`；`top_reaction_z = -3.1252958216062e7 N`。
  - 结果 SHA-256：CSV `4cf769bcc30df7d89c84f251a0b8afd193eba9f8784fd46f6dd76bdb020ab1d1`，Exodus `30cb5bae8d270c8f28f13b75a09550666c6bafb19482a520a26223d1d40a556f`，solve log `0a4c1e5ec739758a80dbcb7b2bedb8b4f97067cbf12f3849f83044fcc9a8de99`。
- **证据边界**：该手工探针只关闭目标语法和最小传力风险，不替代 UI 生成、持久化、负向校验和正式 G2 端到端准出。

#### TASK-PLAN-031 实现领域对象、UI 与生成映射

- **优先级**：P1。
- **依赖**：TASK-PLAN-030、M1。
- **工作**：Reference Point、耦合面引用、运动学参数、预检、生成报告、snapshot 追溯。
- **架构约束**：新对象必须使用持久 ObjectId；不得新增 Tree-only 数据。
- **准出**：TEST-P5-G2-01～05 全部完成，传力和反力平衡有数值证据。
- **状态**：⏳ 待实施；按以下子任务持续回填。

##### TASK-PLAN-031A 领域对象与稳定引用

- **优先级**：P0。
- **目标**：Reference Point 成为独立可持久化对象；Coupling 使用 ObjectId 引用参考点。
- **工作**：保存参考点坐标、名称、ObjectId、状态；保存耦合面、自由度、formulation、penalty 和参考点 ID；删除、重命名、复制与保存重开语义明确。
- **准出**：rename 不改变引用；删除参考点使 Coupling 明确失效；新建同名参考点不得静默重绑。
- **状态**：⬜ 未开始。

##### TASK-PLAN-031B 应用档案与 Mapping

- **优先级**：P0。
- **依赖**：TASK-PLAN-031A、TASK-PLAN-030。
- **目标**：把冻结的真实对象合同写入 `DamSafetyApp-opt` profile 与 mapping registry。
- **工作**：以 `EqualValueBoundaryConstraint` 替换未验证的 Coupling 占位映射；声明关键参数、自由度和支持级别；不支持档案保留对象但阻断生成。
- **准出**：表单、验证器和生成器共用同一 mapping；不存在 `CoupledDirichletBC` 与真实映射并存歧义。
- **状态**：⬜ 未开始。

##### TASK-PLAN-031C UI 与属性表单

- **优先级**：P1。
- **依赖**：TASK-PLAN-031A/031B。
- **目标**：通过 UI 创建、编辑 Reference Point 和 Coupling，不手改 `.i`。
- **工作**：提供参考点坐标、参考点选择、二维耦合面选择、自由度、kinematic formulation 和 penalty；候选受活动档案和 Physical Group 维度约束。
- **准出**：完成 TEST-P5-G2-01/02 的创建、编辑、保存重开和引用恢复路径。
- **状态**：⬜ 未开始。

##### TASK-PLAN-031D 工作流校验与负向合同

- **优先级**：P0。
- **依赖**：TASK-PLAN-031A～031C。
- **目标**：错误配置在生成/提交前可定位阻断。
- **工作**：覆盖参考点缺失、耦合面缺失/维度错误、自由度为空、同自由度 BC 冲突、活动档案不支持五类错误。
- **准出**：TEST-P5-G2-05 五类负向路径全部通过，恢复配置后重新同步能力恢复。
- **状态**：⬜ 未开始。

##### TASK-PLAN-031E 确定性生成与来源报告

- **优先级**：P0。
- **依赖**：TASK-PLAN-031B～031D。
- **目标**：按所选自由度生成真实 `[Constraints]` 对象和稳定输出。
- **工作**：生成 `primary_node_coord`、`secondary`、`penalty`、`formulation`；位移加载施加到参考点；参考点输出按稳定边界名读取；来源报告关联 RP、Coupling、Physical Group 和 mapping。
- **准出**：两次同步逐字一致；不存在加载面统一 DirichletBC 替代；TEST-P5-G2-03 通过。
- **状态**：⬜ 未开始。

##### TASK-PLAN-031F Snapshot 与追溯

- **优先级**：P1。
- **依赖**：TASK-PLAN-031E。
- **目标**：Reference Point/Coupling 进入快照和 Job 证据链。
- **工作**：manifest 固化应用版本、mapping 版本和对象来源；上游变化使旧输入、快照和 Job stale。
- **准出**：项目对象、`.i`、manifest、Job 和结果可以互相追溯。
- **状态**：⬜ 未开始。

##### TASK-PLAN-031G 自动合同与定向巡览

- **优先级**：P0。
- **依赖**：TASK-PLAN-031A～031F。
- **工作**：CTest 增加 Coupling 生成/负向合同；新增 `reference_point_coupling_contract` 定向 UI 巡览，覆盖创建、保存重开、引用失效/恢复和确定性生成。
- **执行规则**：日常仅运行直接相关的 1～2 个定向用例；提交前执行全量真实点击巡览和 CTest。
- **准出**：本任务合同全绿，既有基线不减少。
- **状态**：⬜ 未开始。

##### TASK-PLAN-031H G2 端到端准出

- **优先级**：P0 闸门。
- **依赖**：TASK-PLAN-031A～031G。
- **工作**：执行 TEST-P5-G2-01～05；用 UI 生成的新快照完成真实 `--check-input`、远端求解、CSV/Exodus 下载和 Results 回放。
- **准出**：参考点与耦合面位移满足冻结容差，反力方向和量值可解释；无未关闭 P0 缺陷；CTest 与不少于 114 步真实点击巡览通过。
- **产出**：执行记录、证据索引、计划/手册回填、独立提交并推送 `origin/main`。
- **状态**：⬜ 未开始。

#### M3 跟踪总表

| 工作项 | 当前状态 | 关联验收 | 下一动作 |
|---|---|---|---|
| TASK-PLAN-030 支持矩阵 | ✅ 完成 | G2 目标应用原型 | 回填后冻结，不再重复探测 |
| TASK-PLAN-031A 领域对象 | ⬜ 未开始 | TEST-P5-G2-01/02 | 建立 RP 与 Coupling 的 ObjectId 引用 |
| TASK-PLAN-031B Profile/Mapping | ⬜ 未开始 | TEST-P5-G2-03/05 | 写入已验证对象合同 |
| TASK-PLAN-031C UI | ⬜ 未开始 | TEST-P5-G2-01/02 | 创建属性表单与选择器 |
| TASK-PLAN-031D 校验 | ⬜ 未开始 | TEST-P5-G2-05 | 五类阻断合同 |
| TASK-PLAN-031E 生成 | ⬜ 未开始 | TEST-P5-G2-03 | 确定性 Constraints/输出/报告 |
| TASK-PLAN-031F 追溯 | ⬜ 未开始 | TEST-P5-G2-04 | Snapshot/Job/Result 链 |
| TASK-PLAN-031G 自动合同 | ⬜ 未开始 | G2 回归 | CTest + 定向巡览 |
| TASK-PLAN-031H 总准出 | ⬜ 未开始 | TEST-P5-G2-01～05 | UI 生成项目真实求解与全量回归 |

### Milestone M4：G3 CDP 与认可基准

#### TASK-PLAN-040 冻结基准和容差

- **优先级**：P0。
- **关联**：TASK-P5-05、M-03。
- **工作**：确认参考输入、材料曲线版本、单位、力—位移/损伤/反力指标及容差。
- **准出**：执行前形成不可变的对比合同；没有合同不启动“调参到通过”。

#### TASK-PLAN-041 完成 M-02/M-03 端到端闭环

- **优先级**：P1。
- **依赖**：TASK-PLAN-040、G2、远端环境。
- **工作**：Reference Point/Coupling 成功求解；CDP 材料来源和 CSV/hash 入 manifest；输出基准对比报告。
- **准出**：TEST-P5-G3-01～05 关闭，结果差异在预先冻结的容差内。

### Milestone M5：G4 前架构补强与方案冻结

#### TASK-PLAN-050 完成对象级 DependencyGraph

- **优先级**：P1，G4 前置。
- **关联**：HARD-060。
- **工作**：legacy 规则移出 core；ObjectId→ObjectId 图；按显式引用传播 stale。
- **准出**：修改一个对象只使真实下游 stale，不相关同类对象不受影响。

#### TASK-PLAN-051 冻结多 Step 技术路线

- **优先级**：P0。
- **关联**：TASK-P6-01、REQ-018。
- **工作**：在单作业连续时间线与 restart/recover 中按目标应用能力选定正式策略；定义状态继承、失败恢复、结果拼接。
- **准出**：能力矩阵、数据合同和负向行为明确；禁止多个冲突 Executioner 的简单拼接。

#### TASK-PLAN-052 内部 Undo/Redo（可选前置）

- **优先级**：P2 对 G4，P1 对 Feature Graph。
- **关联**：HARD-070。
- **决策**：不阻塞多 Step；必须在 v0.3 Feature Graph 大量编辑操作前完成。

### Milestone M6：G4 多 Step

#### TASK-PLAN-060 实现 Step 序列与对象激活

- **优先级**：P1。
- **关联**：TASK-P6-02。
- **工作**：Step 顺序、时间区间、激活/保持/修改/停用矩阵；保存重开；非法时间线和能力不兼容阻断。
- **准出**：配置语义可持久化且确定性生成。

#### TASK-PLAN-061 打通生成、快照、Job 与 Results

- **优先级**：P1。
- **关联**：TASK-P6-03/04。
- **工作**：生成每 Step 时间段和继承策略；manifest/Job/Results 可追溯；加载→保持→卸载/再加载算例。
- **准出**：后续 Step 不重置材料内部变量，时间线连续，恢复执行可复现。

### Milestone M7：v0.3 参数化 CAD

#### TASK-PLAN-070 Feature Graph 最小闭环

- **优先级**：P1，G1～G4 稳定后启动。
- **首批范围**：Sketch→Extrude/Pad→Pocket/Boolean；参数变更、依赖、状态、recompute、失败回滚。
- **前置**：M1、TASK-PLAN-050、HARD-070。
- **不做**：第一批不同时实现全部 Revolve/Fillet/Loft/Sweep/Pattern/Mirror。
- **准出**：至少一条真实特征链可修改上游参数并确定性重算，下游 BC/Selection 不静默错绑。

## 6. 并行与资源策略

虽然任务顺序总体串行，以下两条可独立推进：

| 轨道 | 可并行内容 | 不可越过的边界 |
|---|---|---|
| 外部验证轨道 | G2 UI 生成算例准出、CDP 基准准备 | G2 原型已冻结；不得用手工探针替代产品闭环 |
| 本地开发轨道 | TASK-PLAN-031A～031G | 不扩展到 G3/G4 或 Feature Graph |

单个代码变更仍遵守 `AGENTS.md`：日常跑 1～2 个相关定向巡览；提交前全量 114 步巡览 + CTest。

## 7. 每阶段质量闸门

| 闸门 | 自动证据 | 人工/外部证据 | 不通过时 |
|---|---|---|---|
| RG-D | 构建、CTest、114 步、G1 `.i`/manifest | 完整人工复核 | 修复后重申 RG-D |
| H-Gate | 核心合同、保存重开、投影合同、全量回归 | rename/保存重开抽查 | 不进入新对象开发 |
| G1 | 确定性、预检、快照合同 | 2026-09-20 用户按长期实际使用结论验收 | ✅ 9/9 已关闭 |
| G2 | 引用/维度/冲突合同 | 真实传力与反力平衡 | 不进入 G3 |
| G3 | manifest/hash、比较脚本 | 冻结基准内的结果报告 | 登记差异，不事后改容差 |
| G4 | Step 合同、确定性、恢复合同 | 三阶段历史状态连续 | 不进入参数化 CAD 扩展 |
| v0.3 | Graph/recompute/undo 合同 | 真实特征链人工回归 | 缩减 Feature 范围 |

## 8. 风险与缓解

| 风险 | 影响 | 缓解 |
|---|---|---|
| SSOT 反转修改面过大 | UI 回归或项目损坏 | 按 CRUD→属性→读取→持久化分小提交；失败装载原子替换 |
| 旧项目没有 ID | 引用在迁移期不稳定 | 加载生成、首次保存固化；rename 保持会话 ID |
| 外部环境持续不可用 | G3 真实基准无法准出 | 本地工作与外部证据分轨；G1 已按用户验收结论关闭 |
| Coupling 产品实现偏离已验证原型 | UI 可用但目标应用拒绝或传力错误 | profile/mapping/生成器共用冻结合同；正式准出重复真实求解 |
| CDP 基准定义不清 | 无限调参、结论不可复核 | 执行前冻结模型、曲线、指标、容差 |
| 多 Step 路线选错 | 状态丢失或生成无效 | 先做 P6-01，不先写多个 Executioner |
| 架构任务再次膨胀 | 功能长期停滞 | M1 明确停止于 HARD-050，其他债触发式处理 |

## 9. 已确认与后续待确认决策

### D-01 产品优先级：工业 CAE 闭环还是参数化 CAD？

- **推荐**：先完成 G1～G4，再进入 Feature Graph。
- **理由**：G1 已 9/9 关闭；Feature Graph 是更大的新产品阶段。
- **决策**：✅ 已确认（2026-09-20），先完成 G2～G4，再进入 Feature Graph。

### D-02 v0.2.1 是否只包含 HARD-010～050？

- **推荐**：是。HARD-060 放到 G4 前，HARD-070 放到 Feature Graph 前。
- **理由**：二者重要但不阻塞 G2/G3，继续把它们作为功能恢复闸门会延迟用户价值。
- **决策**：✅ 已确认（2026-09-20），v0.2.1 核心切片只含 HARD-010～050。

### D-03 G1 外部证据是否继续追补？

- **决策**：✅ 不再追补（2026-09-20）。用户基于长期实际使用确认 G1-08/09 通过，不再保留独立外部证据任务。

### D-04 G3 采用哪一份认可基准？

- **推荐**：在 G2 完成前指定参考输入、材料曲线和比较容差；无认可基准则不启动 G3 实施。
- **决策**：待确认。

### D-05 版本命名是否调整？

- **推荐**：暂不改旧文档中的 v0.3/v0.5 名称，以 G1～G4 和 M0～M7 作为执行顺序；等 G1 关闭后再决定下一个版本承诺。
- **理由**：当前“版本路线”和“功能 Phase”交叉，先改名字不会增加交付价值。
- **决策**：待确认。

## 10. 项目完成度的正确口径

不建议给整个项目一个单一百分比。当前最准确的表达是：

- **工作台/UI：已完成并进入维护。**
- **单 Step 前处理与确定性生成：已完成。**
- **G1 线弹性接触：✅ 9/9 已按用户验收结论关闭。**
- **高级约束、CDP 基准、多 Step：尚未交付。**
- **v0.2 架构抽离：✅ 代码与人工总准出均已关闭，冻结为 `v0.2-rc1`。**
- **最终领域真源与稳定身份：✅ HARD-010～050 已完成。**
- **参数化 CAD/Feature Graph：尚未开始。**

## 11. 总体完成定义

近期阶段达到“可继续扩展的工业 CAE 基线”需要同时满足：

1. v0.2 RG-D 关闭并有冻结 tag。
2. ProjectDocument 成为业务真源，对象 rename 不改变身份。
3. G1 9/9 保持回归通过（G1-08/09 按 2026-09-20 用户验收结论）。
4. G2 Reference Point/Coupling 有真实传力证据。
5. G3 CDP 结果在预先冻结的认可基准容差内。
6. G4 多 Step 不丢失材料内部状态且可追溯。
7. 全程保持 schema v2 兼容、114 步巡览基线只增不减、G1 确定性不退化。

完成以上条件后，再投入 Feature Graph、Headless API、多求解器和 AI Engineering Layer，收益与风险才匹配。
