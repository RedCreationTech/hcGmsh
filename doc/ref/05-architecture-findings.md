# HC-Gmsh 架构评审 v0.1 - 架构发现登记表

> 评审模式: A - 架构评审, 只读.  
> 仓库: `RedCreationTech/hcGmsh`  
> 基线提交: `5210ba5b632bc529f63ff0ca49db8a5f0d4db275`  
> 目的: 记录 "为什么需要改", 与 "怎么改" 分离.

## 证据标记

- **已验证**: 源码或当前文档可直接确认.
- **推断**: 根据代码关系形成的架构判断.
- **建议**: 建议处理方向.

## 1. 用途

本文件不是 Bug 列表.

它记录的是:

```text
架构事实
架构债
合同债
可保护资产
演进机会
```

目的是让后续 v0.2 重构不变成 "凭感觉拆文件".

优先级:

```text
P0 = 必须优先处理
P1 = 下一阶段重要
P2 = 平台化需要
P3 = 文档/治理优化
Protect = 应尽量保护, 不应误删
```

## 2. 发现摘要

| ID | 发现 | 类型 | 优先级 |
|---|---|---|---|
| A-001 | `MainWindow` 承担 God Object 级职责 | 架构债 | P0 |
| A-002 | `QTreeWidget` 是当前有效领域模型的一部分 | 架构债 | P0 |
| A-003 | 工程已经形成完整 CAE 业务词汇 | 架构资产 | Protect |
| A-004 | Feature 历史已存在, 但不是参数化重放图 | 架构缺口 | P1 |
| A-005 | `SketchDocument` 是较好的独立领域边界 | 架构资产 | Protect |
| A-006 | `VtkViewer` 混合 Results 和 Sketch 编辑 | 架构债 | P1 |
| A-007 | `GmshPanel` 混合 UI, Assembly, Geometry, Physical Group, Mesh | 架构债 | P0/P1 |
| A-008 | `MoosePanel` 混合输入, Snapshot, Job 和 Remote | 架构债 | P0/P1 |
| A-009 | `SimClient` 是相对清晰的基础设施 Adapter | 架构资产 | Protect |
| A-010 | `Runner` 是有效 Port, 但本地/远程执行没有统一 | 架构机会 | P1 |
| A-011 | Schema v2 较强, 运行时对象表示仍然泛型 | 数据债 | P0 |
| A-012 | `PhysicalGroupManifest` 是有价值的网格语义合同 | 架构资产 | Protect |
| A-013 | `ApplicationProfile + MappingRegistry` 是优秀元数据设计 | 架构资产 | Protect |
| A-014 | CMake 仍然构建单体主目标 | 构建债 | P1 |
| A-015 | 核心合同测试较好, 应用服务自动化测试不足 | 测试债 | P1 |
| A-016 | 下游失效是程序规则, 不是 DependencyGraph | 架构缺口 | P0 |
| A-017 | Part/Feature/Assembly 引用可作为 Graph 迁移基础 | 架构机会 | P1 |
| A-018 | owner+bbox 比裸 tag 好, 但不等于 Topological Naming | 架构缺口 | P1 |
| A-019 | `Input Cases` 存在合同漂移 | 合同债 | P0/P3 |
| A-020 | 实现/验收文档丰富, 系统架构文档不足 | 治理债 | P2 |
| A-021 | 功能开关形成多个构建变体 | 构建/测试债 | P2 |
| A-022 | LIMS Facade 远程计算边界设计合理 | 架构资产 | Protect |
| A-023 | 未发现通用 Undo/Redo Transaction 层 | 架构缺口 | P0 |
| A-024 | 缺稳定 Headless Object API | 平台缺口 | P1 |

## 3. 详细发现

### A-001 - MainWindow 已经承担 God Object 级职责

**证据: 已验证.**

当前 `src/MainWindow.cpp` 约 0.9 MB. `MainWindow.h` 中已经包含下列职责:

- 模型树建立和 CRUD.
- Part/Feature 关系维护.
- Assembly 构建.
- Mesh 节点更新.
- Result 节点更新.
- 工程加载/保存.
- Save As 路径迁移.
- dirty 状态.
- Job 表.
- Results 导航.
- Workflow 校验.
- Issue 定位.
- stale 传播.
- MOOSE Materials/Functions/BC/Load/Interaction/Physics/Output/Executioner block 生成.
- Selection.
- Unit Conversion.
- Exodus Import.
- Job Submit 流程.

**影响.**

任何新业务功能都会扩大 UI 和领域逻辑的耦合. 非 GUI 单元测试越来越困难.

**建议.**

优先抽 Application Service, 不要只把 MainWindow 机械拆成若干 `.cpp`.

**优先级:** P0.

### A-002 - QTreeWidget 是当前有效领域模型的一部分

**证据: 已验证.**

`PropertyEditor` 直接持有:

```cpp
QTreeWidgetItem* current_item_
QPointer<QTreeWidget> model_tree_
```

并通过:

```text
KindRole
ParamsRole
StatusRole
```

读写业务状态.

`MainWindow` 同样直接修改 Tree Item 并保存工程.

**影响.**

UI 状态和业务状态绑定, 导致:

- Headless 困难.
- CLI 困难.
- AI 工具困难.
- 自动测试困难.
- Undo 困难.
- 多视图困难.

**建议.**

建立 `ProjectDocument` 真源, Tree 变成 Projection.

**优先级:** P0.

### A-003 - 项目已经形成较完整的 CAE 对象词汇

**证据: 已验证.**

当前根对象包括:

```text
Parts
Sketches
Features
Datums
Materials
Sections
Assembly
Physics
Steps
BC
Loads
Interactions
Constraints
Selections
Functions
Variables
Outputs
Mesh
Input Cases
Jobs
Results
```

**影响.**

v0.2 不需要再发明一套完全新的业务命名.

**建议.**

直接把当前词汇映射为正式 Domain Object.

**优先级:** Protect.

### A-004 - Feature 历史存在, 但不是参数化重放图

**证据: 已验证.**

Phase 5 文档明确:

- 成功操作会追加 Feature.
- `Part.feature` 指向当前 Feature.
- 旧 Feature 保留.
- Assembly 使用 Part 当前 BREP.
- 不会把所有历史 Feature 全部按顺序叠加重放.

**影响.**

当前适合作为审计历史, 但不能支持成熟参数化 CAD 的 "改 Sketch -> 整条 Feature Chain 自动重算".

**建议.**

保留现有 Feature 数据, 增加:

```text
input
output
dependencies
order
active
recompute state
```

**优先级:** P1.

### A-005 - SketchDocument 是非常值得保护的领域边界

**证据: 已验证.**

它已经拥有强类型:

```text
SketchEntity
SketchConstraint
SketchPoint2d
```

并支持:

- CRUD.
- shape identity.
- hit test.
- translate.
- closed loop.
- serialization.

其注释明确了三个消费者:

```text
Sketch drawing
PlaneGCS solve
OCC Part feature
```

**影响.**

它已经是未来 `hc_cad` 的内核雏形.

**建议.**

迁移时尽量少改行为, 先增加 Transaction/Event.

**优先级:** Protect.

### A-006 - VtkViewer 混合 Results Viewer 和 Sketch Editor

**证据: 已验证.**

`VtkViewer` 同时承担:

- Exodus Reader.
- Mesh Rendering.
- Scalar.
- Deformation.
- Slice.
- Picking.
- Screenshot.
- Sketch Tool.
- Snap.
- Drag.
- Constraint.
- Dimension.
- Sketch Solve.

**影响.**

CAD 交互和后处理迭代速度不同, 但现在绑在一个大类.

**建议.**

拆成:

```text
SketchViewport
MeshViewport
ResultViewport
```

**优先级:** P1.

### A-007 - GmshPanel 混合多层职责

**证据: 已验证.**

它同时提供:

- 几何导入.
- Primitive.
- Transform.
- Boolean.
- Assembly Instance.
- Assembly Build.
- Custom Physical Group.
- Physical Group 恢复.
- Mesh Field.
- Mesh Generation.
- UI 状态.

**影响.**

Gmsh 能力无法在无 Widget 环境复用.

**建议.**

提取:

```text
GmshGeometryAdapter
AssemblyGeometryService
PhysicalGroupService
GmshMesher
```

**优先级:** P0/P1.

### A-008 - MoosePanel 混合多个应用服务

**证据: 已验证.**

它同时负责:

- 输入编辑.
- 模板.
- Model Block 应用.
- Local Run.
- Check Input.
- Snapshot.
- Remote Submit.
- Remote Monitor.
- Download.
- Expert Mode.
- Project Input Materialize.

**影响.**

求解配置, Job 和 UI 无法独立测试.

**建议.**

拆为:

```text
MooseInputGenerator
SnapshotService
JobApplicationService
MoosePanel View/Controller
```

**优先级:** P0/P1.

### A-009 - SimClient 是相对清晰的 Infrastructure Adapter

**证据: 已验证.**

`SimClient` 明确只调用 LIMS Facade, 不直接连接 C06.

接口包含:

- submit.
- fetch_job.
- fetch_jobs.
- execution status.
- files.
- log.
- cancel.
- download.
- bundles.

**影响.**

它很适合作为 `IJobGateway` 的一个实现.

**建议.**

保持网络职责单一, 不把 Job 领域逻辑继续塞进 SimClient.

**优先级:** Protect.

### A-010 - Runner 是有效 Port, 但执行模型仍然分裂

**证据: 已验证.**

`Runner` 已经有:

```cpp
start(const RunSpec&)
stop()
```

和标准事件.

本地路径使用 Runner, 远程路径主要经过 `SimClient/MoosePanel`.

**影响.**

同一个 Job 在本地和远程走不同编排体系.

**建议.**

引入 `IExecutionBackend`, 让 Local 和 LIMS 成为同级 Adapter.

**优先级:** P1.

### A-011 - Schema v2 很有价值, 运行时表示仍然过于泛型

**证据: 已验证.**

Schema v2 已经有:

- Profile.
- Units.
- Model.
- Mesh Snapshot.
- Status.
- Assembly.
- Traceability.

但运行时大量使用:

```text
QVariant
QVariantMap
QString key
```

**影响.**

业务正确性分散到很多 Validator 和 `if(kind)` 中.

**建议.**

YAML 边界继续宽容, 内部逐步强类型.

**优先级:** P0.

### A-012 - PhysicalGroupManifest 是有价值的语义网格合同

**证据: 已验证.**

它记录:

- Name.
- Dim.
- Tags.
- Entity Count.
- Element Count.
- Bound Object IDs.
- Mesh Hash.
- Mesh Dim.
- Node/Element Count.
- Element Type.
- Quality Summary.

并提供 Validation.

**影响.**

Simulation 层不需要直接理解 Gmsh 内部状态.

**建议.**

正式提升为 `hc_mesh` Domain Contract.

**优先级:** Protect.

### A-013 - ApplicationProfile + MappingRegistry 是很好的元数据架构

**证据: 已验证.**

ApplicationProfile 描述:

- Physics.
- Supported Blocks.
- Solver Program.
- Mapping Version.
- Unit Contract.
- Support Level.

MappingRegistry 描述:

- Block.
- Object Type.
- Required Params.
- Parameter Schema.
- Output Ordering.

测试已经验证版本和 Pressure/Contact 语法.

**影响.**

这是后续多求解应用的良好基础.

**建议.**

保留元数据, 但让 MOOSE 文本成为 typed SimulationModel 的下游产物.

**优先级:** Protect.

### A-014 - CMake 仍然以单体主目标为主

**证据: 已验证.**

`gmp_ise` 直接列出几乎所有源文件.

测试目标又重复列出部分源文件.

**影响.**

编译器无法帮助约束模块依赖.

**建议.**

按真实抽取进度增加 internal library.

**优先级:** P1.

### A-015 - 自动测试覆盖核心合同, 但服务级覆盖不足

**证据: 已验证.**

`gmp_ise_phase0_test` 已覆盖:

- ProjectSchema.
- SketchDocument.
- Profiles.
- Mapping.
- Physical Groups.
- Snapshot.
- SimClient 合同.

Phase 5 则有大量人工验收.

**影响.**

大型 GUI 类重构风险较高.

**建议.**

抽服务前将关键 G1 业务转成 service-level tests.

**优先级:** P1.

### A-016 - 下游失效存在, 但没有 DependencyGraph

**证据: 已验证/推断.**

已有:

```text
invalidate_downstream_from()
```

以及 Schema/验收中的 stale 语义.

但没有独立通用 Graph.

**影响.**

每新增一种对象关系都需要手工增加传播规则.

**建议.**

建立 `DependencyGraph`.

**优先级:** P0.

### A-017 - 当前 Part/Feature/Assembly 引用是很好的迁移基础

**证据: 已验证.**

当前已有:

```text
Feature.part
Part.feature
Part.brep
Assembly.part
```

**影响.**

无需推倒重建全部对象.

**建议.**

给现有 Name Reference 补 Stable Object ID, 然后逐步迁移.

**优先级:** P1.

### A-018 - owner+bbox 重绑定有价值, 但不是完整 Topological Naming

**证据: 已验证.**

Assembly 自定义 Physical Group 保存:

```text
owner
bbox
tag hint
```

恢复时优先 owner+bbox.

**影响.**

可以抵抗简单 tag 变化, 但无法保证复杂 Boolean/Fillet 后稳定匹配.

**建议.**

短期保留, 中期增加:

```text
area
normal
centroid
adjacency
feature provenance
```

**优先级:** P1.

### A-019 - `Input Cases` 存在合同漂移

**证据: 已验证.**

`ProjectSchema.cpp::model_root_nodes()` 和测试包含:

```text
Input Cases
```

但 `doc/schema/project-v2.md` 顶层 model 示例没有同步列出.

**影响.**

代码和合同文档可能出现不同真源.

**建议.**

修正文档, 并增加 Schema Contract Test.

**优先级:** P0/P3.

### A-020 - 实现和验收文档丰富, 架构治理文档不足

**证据: 已验证.**

仓库已经有大量:

- UI 重构文档.
- 人工验收.
- 用户手册.
- 缺陷汇总.
- Contract.
- Schema.

但缺系统化:

```text
C4
Module Boundary
Dependency Rules
Architecture Decision Record
```

**影响.**

新开发人员容易从功能角度继续堆代码.

**建议.**

把本轮文档作为 `doc/architecture` 的候选基础.

**优先级:** P2.

### A-021 - 功能开关形成多个架构变体

**证据: 已验证.**

存在:

```text
GMP_ENABLE_GMSH_GUI
GMP_ENABLE_VTK_VIEWER
GMP_ENABLE_MPI
GMP_ENABLE_PARAVIEW
GMP_ENABLE_CATALYST
GMP_ENABLE_WSL_RUNNER
```

**影响.**

不同组合可能只在部分开发环境编译.

**建议.**

CI 建立最小组合矩阵.

**优先级:** P2.

### A-022 - LIMS Facade 远程边界设计合理

**证据: 已验证.**

客户端不直接保存计算 Agent 凭据, 只连接 LIMS.

**影响.**

企业安全和部署边界更清楚.

**建议.**

后续把它正式定义为 Remote Execution Adapter.

**优先级:** Protect.

### A-023 - 未发现通用 Undo/Redo Transaction 层

**证据: 本次审计未发现.**

现有 UI 有大量修改操作, 但没有发现覆盖 Project Object 的统一事务机制.

**影响.**

CAD 产品可用性, Agent 可控性, 错误恢复都会受限.

**建议.**

v0.2 建立 Command + Transaction.

**优先级:** P0.

### A-024 - 缺稳定 Headless Object API

**证据: 已验证/推断.**

当前主要交互入口是 QWidget/slot 和模型树.

**影响.**

无法自然支撑:

- Python.
- Batch.
- Regression.
- MCP.
- AI Agent.

**建议.**

先建立 Application Service C++ API, 再考虑 Python/MCP.

**优先级:** P1.

## 4. 收益最高的第一批动作

按建议优先级:

```text
1. ProjectDocument + ObjectId
2. QTreeWidget -> Projection
3. DependencyGraph
4. ProjectStore
5. MooseInputGenerator
6. AssemblyGeometryService / GmshMesher
7. Transaction / Undo
8. Headless Application API
```

不建议第一步就拆 VTK 或大规模改 UI, 因为业务真源问题更重要.

## 5. 不应该被 "修掉" 的东西

下面这些是资产, 不是问题:

- `SketchDocument`.
- PlaneGCS.
- OpenCASCADE.
- `PhysicalGroupManifest`.
- ApplicationProfile.
- MooseMappingRegistry.
- Snapshot v2.
- Runner 接口.
- SimClient/LIMS 边界.
- Schema Versioning.
- Phase 5 验收体系.
- Current Feature History.
- Assembly 的 owner+bbox 恢复策略.

正确做法是:

```text
提升层次
明确边界
保留兼容
```

而不是重写.

## 6. 本文源码依据

主要依据:

- Git Tree 文件规模.
- `MainWindow.h`.
- `GmshPanel.h`.
- `MoosePanel.h`.
- `VtkViewer.h`.
- `PropertyEditor.h`.
- `ProjectSchema.cpp`.
- `SketchDocument.h`.
- `PhysicalGroupManifest.h`.
- `ApplicationProfile.h`.
- `MooseMappingRegistry.h`.
- `Runner.h`.
- `SimClient.h`.
- `manual/test05.md`.
- `tests/test_phase0.cpp`.
