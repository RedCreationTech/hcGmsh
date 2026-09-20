# GMP-ISE 项目进度与后续总体规划

> 编制日期：2026-09-19（2026-09-20 用户批准执行）
> 当前代码基线：`main@de30de7cfe1e`
> 规划模式：项目总览 + 开发任务编排
> 主要依据：`doc/UI重构开发任务清单.md`、`doc/UI重构下一阶段任务清单.md`、`manual/test05.md`、`doc/v0.2重构任务清单.md`、`doc/v0.2.1架构硬化任务清单.md`、`doc/ref/HC-Gmsh-v0.2-重构成果复核与进一步优化计划.md`
> 状态：**已批准执行；G1-08/09 按用户长期实际使用结论验收通过，不再作为遗留任务**

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

### 0.4 为什么不能马上全面补功能

- ObjectId 仍由 `<根名>/<对象名>` 派生，rename 会改变身份。
- 项目保存、生成输入和大量校验仍从 QTreeWidget 读取业务数据。
- Reference Point、Coupling、多 Step 和 Feature Graph 都会新增更多跨对象引用；继续使用名称引用会加重悬空引用和迁移风险。
- 当前 Transaction 的删除回滚只恢复本节点，属性表单仍是事后审计，不能直接接通用 Undo。

## 1. 当前基线

### 1.1 代码与验证状态

| 项目 | 当前状态 | 结论 |
|---|---|---|
| Git 基线 | `main`、`origin/main` 均为 `de30de7cfe1e` | v0.2 代码工作已结束 |
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
| 高级约束 | Phase 5 / G2 | ⚪ 未实现 | Reference Point/Coupling 的 UI、映射、传力验证均待开发 |
| 高级材料/基准 | Phase 5 / G3 | ⚪ 未实现 | M-02/M-03、CDP 认可基准与容差未冻结 |
| 多阶段 | Phase 6 / G4 | ⚪ 仅有需求 | 当前只保存多个 Step，生成器只使用首个 Step 并告警 |
| 架构 | v0.2 Stage 0～7 | ✅ 已关闭 | 7 个 Stage 已完成；RG-D 人工总准出通过；`v0.2-rc1` 已冻结 |
| 架构硬化 | v0.2.1 | 🟡 进行中 | HARD-010～040 已完成；HARD-050 为当前剩余核心切片 |
| 参数化 CAD | v0.3 | ⚪ 路线规划 | Feature Graph/Recompute/Topology Identity 尚未开始 |

### 2.2 功能能力矩阵

| 能力域 | 已交付 | 尚缺 | 下一动作 |
|---|---|---|---|
| 项目生命周期 | 新建、打开、保存、另存为、项目状态隔离、schema v2、持久 ObjectId、Document 直接持久化 | 领域写路径尚未全部切到 Document | HARD-050 |
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
| 核心对象层 | ProjectDocument/ObjectId/PropertyBag；五类可逆领域命令 | Document 尚非全部业务写入真源 | HARD-050 |
| 持久化 | ProjectStore 与 Document 直接互转；MainWindow 保存不再组装模型条目 | Adapter 仍可从 Tree 反向重建 Document | HARD-050 |
| 依赖传播 | DependencyGraph 算法与 stale 接线 | 节点仍为 kind；legacy CAE 规则泄漏到 core | HARD-060，G4 前 |
| 事务 | TransactionManager、五类可逆领域命令、失败事务闸门 | UI 仍使用 Tree 闭包/属性表单事后审计；无 Undo 栈 | HARD-050；HARD-070 后置 |
| Simulation | MooseInputGenerator/SnapshotService 无 Widget | Generator 依赖 Store DTO | v0.3 前后按实际需要处理 |
| Gmsh | PhysicalGroup/Mesher/Assembly 服务抽离 | 全局 current model/session | 多项目或后台 mesh 前处理 |
| Viewport | Sketch/Mesh/Result 子视口 + facade | 状态仍在 facade；共享 God Header 风险 | 出现真实修改冲突时处理 |
| 构建 | hc_core/hc_simulation/hc_mesh | 模块归属仍粗 | SSOT 完成后再评估 |
| 测试 | 114 步 GUI + CTest + 10 个服务合同 | 单一测试文件、平台矩阵不足 | 测试变慢或 CI 上线时处理 |

## 3. 剩余事项分级

### 3.1 P0：当前发布与后续开发前置

1. **稳定身份缺失**：rename 改变 ObjectId，新引用继续扩展会放大风险。
2. **Tree 仍是真源**：继添加对象类型会扩大未来迁移范围。

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

#### TASK-PLAN-011 完成 HARD-040～050

- **优先级**：P0。
- **范围**：最小可逆领域命令；CRUD/属性/状态先写 Document；Tree 递归投影；持久化、生成、校验从 Document/纯数据快照读取。
- **不做**：用户可见全局 Undo/Redo、通用 Event Bus、模块目录重排。
- **准出**：正常数据流为 `Command -> ProjectDocument -> Tree`，不存在 Tree 反向全量重建业务路径。

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

#### TASK-PLAN-030 冻结目标应用支持矩阵

- **优先级**：P0。
- **关联**：TASK-P5-04、M-02。
- **工作**：确认目标应用实际对象语法、参数、自由度、输出和限制；先有 `--check-input` 原型，再开 UI。
- **准出**：能力矩阵与 mapping 合同确定，不用普通面位移伪装 Coupling。

#### TASK-PLAN-031 实现领域对象、UI 与生成映射

- **优先级**：P1。
- **依赖**：TASK-PLAN-030、M1。
- **工作**：Reference Point、耦合面引用、运动学参数、预检、生成报告、snapshot 追溯。
- **架构约束**：新对象必须使用持久 ObjectId；不得新增 Tree-only 数据。
- **准出**：TEST-P5-G2-01～05 全部完成，传力和反力平衡有数值证据。

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
| 外部验证轨道 | G2 应用支持矩阵、CDP 基准准备 | 不得用未验证语法先造 UI |
| 本地开发轨道 | M1 架构硬化、P2 文档/体验小修 | 不得提前实施 G2/P6 数据模型 |

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
| Coupling 目标应用不支持 | UI 先做后废弃 | 先冻结能力矩阵和真实 `--check-input` 原型 |
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
- **最终领域真源与稳定身份：尚未完成，是下一次功能扩展前唯一必须处理的架构切片。**
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
