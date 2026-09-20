# HC-Gmsh v0.2 重构成果复核与进一步优化计划

> 仓库: `RedCreationTech/hcGmsh`
> 重构前基线: `5210ba5b632bc529f63ff0ca49db8a5f0d4db275`
> 本次复核基线: `main@de30de7cfe1e54bc3fea5193379016eaeab7a59b`
> 复核日期: 2026-09-19
> 模式: 只读架构复核, 未修改仓库.
> 后续状态（2026-09-20）：本文当时记录的 RG-D 与 `v0.2-rc1` 待办已关闭；G1-08/09 已按用户长期实际使用结论验收通过。当前执行状态以 `doc/GMP-ISE项目进度与后续总体规划.md` 为准。

## 1. 总结结论

这轮重构是成功的, 而且是实质性重构, 不是简单增加抽象类或移动文件.

相对于 `5210ba5`, 当前 `main` 已前进 50 个提交, 变更 65 个文件, 新增 47 个文件, 修改 18 个文件, 约新增 15,502 行, 删除 3,458 行.

最重要的成果是, 原来集中在 `MainWindow`, `GmshPanel`, `MoosePanel`, `VtkViewer` 中的大量业务和引擎逻辑已经开始形成独立层:

- `ProjectDocument`, `ProjectObject`, `PropertyBag`.
- `DependencyGraph`.
- `TransactionManager`.
- `ProjectStore`.
- `MooseInputGenerator`.
- `SnapshotService`.
- `PhysicalGroupService`.
- `GmshMesher`.
- `AssemblyGeometryService`.
- `SketchViewport`, `MeshViewport`, `ResultViewport`.
- `hc_core`, `hc_simulation`, `hc_mesh` CMake 目标.

同时, 重构过程使用 114 步真实点击巡览, CTest, G1 `.i` 逐字一致, Snapshot manifest 对照和人工 Stage 闸门控制回归风险.

因此我会把当前版本定义为:

> `v0.2 架构地基重构基本完成, 已经从单体 Widget 驱动设计进入分层架构过渡期`.

但是, 还不能定义为:

> `ProjectDocument 已完全成为唯一真源, Domain 和 UI 已彻底解耦`.

当前仍然是一个很典型, 也很合理的 Strangler 过渡状态.

## 2. 与原重构方案的逐项确认

| 原目标 | 当前结果 | 复核结论 |
|---|---|---|
| ProjectDocument / ProjectObject | 已实现 | 完成基础层 |
| PropertyBag | 已实现 | 完成基础层 |
| DependencyGraph | 已实现并接管 stale | 完成 v0.2 范围, 仍是 kind 级规则 |
| TransactionManager | 已实现并接入 CRUD/表单审计 | 部分完成, Undo/Redo 按计划推迟 |
| Model Tree 变 Projection | 有 ModelTreeAdapter | 过渡完成, 尚未完成真源反转 |
| ProjectStore | 已抽离 | 基本完成 |
| MOOSE block generation | 已抽到 MooseInputGenerator | 完成 |
| Snapshot | 已抽到 SnapshotService | 完成 |
| Gmsh Physical Group | 已抽到 PhysicalGroupService | 完成 |
| Mesh generation | 已抽到 GmshMesher | 完成 |
| Assembly geometry | 已抽到 AssemblyGeometryService | 完成 |
| VtkViewer 拆分 | 三 Viewport + facade | 完成第一阶段 |
| CMake 模块化 | hc_core/hc_simulation/hc_mesh | 完成第一阶段 |
| Schema v2 兼容 | 保持 | 完成 |
| Phase 5 G1 零回归 | 自动化证据已齐 | 自动化通过, 最终 RG-D 人工闭环仍需确认 |

## 3. 已确认的核心成果

### 3.1 MainWindow 已经真正瘦身

对比基线, `src/MainWindow.cpp` 的 diff 为:

- additions: 761.
- deletions: 1,753.
- total changed: 2,514.

这说明 MOOSE 生成, Project persistence 等职责不是简单复制, 而是确实从 MainWindow 迁出.

当前 `MainWindow.h` 已明确把主要 block generation 标记为委托给 `MooseInputGenerator`.

这是本轮最重要的成果之一.

### 3.2 GmshPanel 的 Engine 职责显著下降

`src/GmshPanel.cpp` 对比基线:

- additions: 138.
- deletions: 1,453.

同时出现:

- `PhysicalGroupService`.
- `GmshMesher`.
- `AssemblyGeometryService`.

这说明 GmshPanel 已开始回到 UI/编排层, 这是正确方向.

### 3.3 Simulation 生成已经形成纯数据接口

`MooseInputGenerator` 输入是纯数据:

```text
ProjectModelEntry
Application Profile Context
Mapping Version
Mesh Path
PhysicalGroupManifest
```

输出是:

```text
Functions
Variables
Materials
BC
Loads
Physics
Contact
Outputs
Executioner
Generation Report
Warnings
```

它不依赖 Qt Widgets, Gmsh, VTK 或网络.

这已经具备服务级测试和未来 Headless 化的基础.

### 3.4 ProjectStore 把持久化规则集中起来

`ProjectStore` 已经接管:

- schema v2 load/save.
- typed scalar round-trip.
- mesh path migration.
- Save As project mesh rebasing.
- mesh snapshot path.
- MOOSE derived text path replacement.

此前这些规则散落在 MainWindow.

这项重构价值很高, 特别是对工程文件兼容性和未来 schema migration.

### 3.5 stale 已经从程序规则切换到 DependencyGraph

当前 `invalidate_downstream_from()` 已经通过 `DependencyGraph::markStaleFrom()` 得到传播闭包.

这完成了从:

```text
if source == X then stale Y
```

向:

```text
Dependency Graph
```

演进的第一步.

### 3.6 VtkViewer 已完成第一阶段职责拆分

现在形成:

```text
VtkViewer facade
  -> SketchViewport
  -> MeshViewport
  -> ResultViewport
  -> CameraController
  -> SelectionOverlay
```

而且保持原 84 个接口兼容, MainWindow/SketchPanel/GmshPanel 调用点不需要大规模变化.

这是典型且正确的 Strangler Refactor.

### 3.7 CMake 边界开始与架构边界对齐

当前已有:

```text
hc_core
hc_simulation
hc_mesh
gmp_ise
gmp_ise_phase0_test
```

测试目标不再重复编译核心源码, 而是链接内部库.

这意味着架构边界开始被编译器约束, 不再只存在于文档中.

## 4. 我认为当前仍然存在的关键问题

下面这些不是否定 v0.2, 而是下一阶段最值得处理的部分.

## P0. Model Tree 仍然是真实写入源, ProjectDocument 仍是反向投影

这是当前最大的剩余架构问题.

`ModelTreeAdapter.h` 明确写着:

```text
Tree 仍是唯一操作入口并继续承载 Data Role 数据.
Document 作为投影经适配器懒同步.
树变更只标脏.
document() 读取时从 Tree 全量重建.
```

因此现在的数据流仍然是:

```text
QTreeWidget
   -> mark dirty
   -> rebuild_from_tree()
   -> ProjectDocument
```

而最终应当反过来:

```text
ProjectDocument
   -> Domain Event
   -> ModelTreeAdapter
   -> QTreeWidget
```

当前的 ProjectDocument 更准确地说是:

> `Domain Projection`.

还不是:

> `Single Source of Truth`.

### 优化建议

下一阶段建立:

```text
ProjectSession
  |- ProjectDocument
  |- DependencyGraph
  |- TransactionManager
```

所有写操作先修改 Document.

Tree 只根据事件更新.

完成后删除:

```text
rebuild_from_tree()
Tree Data Role 作为业务真源
```

这应是下一轮第一优先级.

## P0. 当前所谓 Stable ObjectId 实际上会随 rename 改变

这是本次代码复核发现的一个重要问题.

`ProjectDocument` 自己支持随机 UUID ObjectId.

但是 `ModelTreeAdapter` 使用:

```text
<root>/<child name>
```

作为 ObjectId.

例如:

```text
Materials/concrete
```

如果用户把对象改名为:

```text
concrete_c40
```

ObjectId 会变成:

```text
Materials/concrete_c40
```

这不是严格意义上的 Stable ID.

它只保证:

> 同名对象在 save/reopen 后 ID 一致.

但不能保证:

> rename 后对象身份不变.

对于未来:

- Feature dependency.
- Selection reference.
- Assembly reference.
- Material reference.
- BC/Load reference.
- AI command history.
- Undo/Redo.

这是不可接受的.

### 优化建议

在 schema 中引入持久化 ID:

```yaml
- id: 5d8d...
  name: concrete
  kind: Materials
```

旧 schema v2 项目加载时:

```text
没有 id
 -> 自动生成 UUID
 -> 保存时写入
```

如果不希望立即升级 schema_version, 也可以先把 `id` 定义为 v2 向后兼容可选字段.

Object reference 最终用 ObjectId, name 只做展示.

## P0. DependencyGraph 当前仍然是 kind-level legacy graph

当前集成方式仍然是:

```text
Parts -> Assembly
Mesh -> Input Cases
Input Cases -> Jobs
...
```

节点本质上是 kind 字符串, 不是具体对象.

这对于 v0.2 等价迁移是正确的, 但不够支撑 v0.3 Feature Graph.

最终必须变为:

```text
Sketch:42
  -> Feature:100
  -> Part:12
  -> AssemblyInstance:51
  -> Mesh:91
```

而不是:

```text
Sketches -> Features -> Assembly -> Mesh
```

### 进一步问题

`legacy_stale_rule_edges()` 位于 `hc_core` 的 `DependencyGraph.cpp`, 并读取 `ProjectSchema::model_root_nodes()`.

这使 generic core graph 知道 GMP 的 CAE root kind.

这是一个边界泄漏.

建议:

```text
hc_core:
  DependencyGraph 只提供通用图算法.

hc_app / compatibility:
  build_legacy_dependency_graph()
```

把 legacy 规则移出 core.

这样 `hc_core` 就不需要因为 `ProjectSchema.h` 间接依赖 yaml-cpp.

## P0. Transaction 目前主要还是事务骨架和审计, 不是完整 Undo 系统

当前 TransactionManager 本身设计是合理的:

```text
begin
execute
commit
rollback
auditLog
```

也有:

```text
Command
SetPropertyValueCommand
ClosureCommand
```

但是现有接线中还有:

```text
record_committed()
```

这意味着部分操作是:

```text
先由旧代码完成
 -> Transaction 只记录审计
```

而不是:

```text
Transaction Command
 -> 唯一执行入口
```

这两者差别很大.

### 优化建议

v0.3 将所有模型修改入口收敛成 Command:

```text
CreateObjectCommand
DeleteObjectCommand
RenameObjectCommand
SetPropertyCommand
CreateFeatureCommand
MoveAssemblyInstanceCommand
AssignMaterialCommand
CreateSelectionCommand
```

增加:

```text
undo_stack
redo_stack
```

不要直接在现有 `record_committed()` 上加 Ctrl+Z.

必须先确保操作本身由 Command 执行, 否则无法可靠 undo.

## P1. ProjectDocument 的乱序加载实现有潜在问题

`ProjectDocument::from_variant_list()` 注释写的是:

```text
两遍装载, 允许条目乱序.
```

第一遍确实创建对象并验证 parent ID 存在于输入集合.

但第二遍仍按 `loaded` 原始顺序执行 `addObject(object, parent)`.

而 `addObject()` 要求 parent 已经实际存在于 `objects_`.

因此如果输入顺序是:

```text
child
parent
```

第二遍 child 会因为 parent 尚未挂载而失败.

当前 Project Tree 只有 root + child, 且自身序列化通常会让 root 排在 child 前, 所以现实中不容易触发.

但从 API 合同角度, "允许条目乱序" 当前并没有完全成立.

### 建议

改为:

```text
pass 1: 建所有 object, 不挂 parent
pass 2: 建 parent/children relation
```

或者做拓扑挂载.

并增加:

```text
child-before-parent round-trip test
```

## P1. ModelTreeAdapter 只支持 root -> child 两层

当前:

```cpp
id_for_item()
rebuild_from_tree()
item_for_id()
```

只处理:

```text
Root
  -> Child
```

没有递归处理任意深度.

v0.3 一旦实现真正 Feature Tree:

```text
Part
  -> Body
     -> Sketch
     -> Pad
     -> Pocket
```

当前 Adapter 会成为限制.

### 建议

在完成 Document 真源反转时一起把 Tree Adapter 改成递归 Projection.

## P1. MooseInputGenerator 仍依赖 ProjectStore 的 DTO

当前 `MooseInputGenerator.h` 直接 include:

```cpp
gmp/ProjectStore.h
```

因为它使用:

```text
ProjectModelEntry
```

这会形成一个不理想的依赖:

```text
Simulation Generator
 -> Persistence DTO
```

目标应该是:

```text
ProjectDocument
 -> SimulationModelSnapshot
 -> MooseInputGenerator
```

### 建议

引入独立 DTO:

```text
ModelObjectSnapshot
SimulationModel
SimulationSnapshot
```

放在 `hc_core` 或 `hc_simulation/model`.

`ProjectStore` 和 `MooseInputGenerator` 都依赖它.

不要让 Generator 依赖 Store.

## P1. hc_simulation 目前承载了过多基础设施

当前 `hc_simulation` 包含:

```text
MooseInputGenerator
SnapshotService
MooseSnapshot
MooseTemplates
PhysicalGroupManifest
ProjectSchema
MooseMappingRegistry
ApplicationProfile
SimClient
Env
UnitDisplay
ProjectStore
OperationLog
```

其中:

```text
ProjectStore
OperationLog
Env
SimClient
```

严格来说并不都是 simulation domain.

建议后续演进为:

```text
hc_core
hc_project
hc_cad
hc_mesh
hc_simulation
hc_execution
hc_results
hc_ui
```

其中:

```text
hc_project:
  ProjectStore
  Schema
  Migration

hc_execution:
  Runner
  SimClient
  JobService
```

## P1. hc_mesh 也混合了 CAD 和 Mesh

当前 `hc_mesh` 包含:

```text
PhysicalGroupService
GmshMesher
AssemblyGeometryService
SketchDocument
```

其中:

- `SketchDocument` 是 CAD.
- `AssemblyGeometryService` 更偏 CAD/Assembly.
- `GmshMesher` 才是 Mesh.
- `PhysicalGroupService` 是 Geometry/Mesh 之间的语义桥.

下一阶段建议拆:

```text
hc_cad:
  SketchDocument
  SketchSolver
  OccBridge
  AssemblyGeometryService

hc_mesh:
  GmshMesher
  PhysicalGroupManifest
  PhysicalGroupService
```

## P1. VtkViewer 拆分仍处于 facade 阶段

当前 Viewport 类都持有:

```text
VtkViewer* host_
```

大量实际场景状态仍在 VtkViewer.

并且出现了:

```text
ViewportInternal.h
```

约 700 行共享内部实现.

这符合第一阶段 Strangler, 但下一步不要让 `ViewportInternal.h` 变成新的 God Header.

建议引入:

```text
VtkScene
ViewportContext
RenderPipeline
PickingService
ResultDataSource
```

让 Sketch/Mesh/Result Viewport 拥有各自状态, VtkViewer 最终只做组合.

## P1. Gmsh 仍然隐含全局 Session

`GmshMesher`, `PhysicalGroupService`, `AssemblyGeometryService` 虽然不依赖 QWidget, 但大量行为仍围绕 "当前 Gmsh model" 和全局 gmsh API.

未来如果出现:

- 多 Project.
- 多 Document.
- 后台 mesh.
- 并发任务.
- 单元测试并行.

全局 session 会成为风险.

### 建议

引入:

```text
GmshSession
GmshModelContext
```

显式管理:

```text
initialize
model identity
activate
clear
finalize
```

服务对象通过 Context 操作, 不再隐式依赖 current model.

## P2. 测试目前仍集中在一个超大 CTest executable

CMake 已经消除了重复编译, 这是很好的进步.

但当前仍然是:

```text
gmp_ise_phase0_test
```

一个大测试程序承载大量合同.

建议按模块拆:

```text
hc_core_tests
hc_project_tests
hc_simulation_tests
hc_mesh_tests
hc_execution_tests
```

优点:

- 更容易定位失败.
- 可以并行执行.
- CI 更清楚.
- 模块依赖更容易验证.

## P2. 增加 CI 构建矩阵

当前功能开关很多:

```text
GMP_ENABLE_GMSH_GUI
GMP_ENABLE_VTK_VIEWER
GMP_ENABLE_MPI
GMP_ENABLE_PARAVIEW
GMP_ENABLE_CATALYST
GMP_ENABLE_WSL_RUNNER
```

建议至少覆盖:

```text
Core only
Gmsh ON / VTK OFF
Gmsh ON / VTK ON
Windows
macOS
```

以及条件允许时:

```text
ASan
UBSan
clang-tidy
```

## 5. 对当前 v0.2 准出状态的判断

如果严格按照 2026-09-18 经确认的 v0.2 Scope:

> 我认可仓库文档中的 "10 项达成 + Undo/Redo 依 Q4 调整为 Transaction 层" 的总体判断.

也就是说:

```text
v0.2 代码层准出: 可以认可.
```

但是我建议区分两种口径.

### 口径 A. 按已批准的 v0.2 过渡目标

结论:

```text
通过.
```

主要目标都已经落地.

### 口径 B. 按最终目标架构

结论:

```text
仍处于 60% 到 70% 的架构迁移阶段.
```

因为最重要的最终反转还没有发生:

```text
Tree -> Document
```

需要变成:

```text
Document -> Tree
```

同时 ObjectId, object-level graph, Command/Undo, SimulationModel 也还没有完成.

这并不是 v0.2 的失败, 而是 v0.2 成功建立了下一步能继续演进的地基.

## 6. 当前还差一个最终闭环

最新 `main@de30de7` 中已经提交:

```text
doc/v0.2-RG-D-总准出复核说明.md
```

该文档说明:

- 自动化 114 步巡览全绿.
- CTest 全绿.
- G1 `.i` 与基线逐字一致.
- Snapshot manifest 除时间字段外一致.
- 还需要用户完成 RG-D 总人工复核.
- 通过后应打 `v0.2-rc1` tag.

从当前提交历史看, 我没有看到 "RG-D 已通过并打 v0.2-rc1" 的后续证据.

因此我建议:

> 先把 RG-D 人工验收和 rc tag 关闭, 再进入 v0.3.

## 7. 下一轮推荐计划

我不建议立即继续 G2/G3 功能扩张.

建议先做一个短的 `v0.2.1 Architecture Hardening`, 再进入 `v0.3 Parametric Core`.

### v0.2.1, 架构硬化

#### H1. ProjectDocument 真源反转

目标:

```text
所有 CRUD / property edit:
Command
 -> ProjectDocument
 -> Domain Event
 -> Tree Projection
```

删除 Tree -> Document 全量 rebuild 作为正常业务路径.

#### H2. Stable ObjectId

- ID 持久化.
- rename 不改变 ID.
- name reference 逐步升级为 ObjectId reference.

#### H3. DependencyGraph 对象化

从:

```text
Parts -> Mesh
```

升级:

```text
Part:uuid -> Mesh:uuid
```

把 legacy rule builder 移出 `hc_core`.

#### H4. Transaction 强化

- 所有写操作 Command 化.
- 去掉关键路径 `record_committed` 旁路.
- 引入 undo stack / redo stack.
- 增加事务异常安全.

#### H5. ProjectDocument / ProjectStore 直接互转

目标:

```text
ProjectStore.load() -> ProjectDocument
ProjectStore.save(ProjectDocument)
```

不再经过 Tree entries.

#### H6. 修复 ProjectDocument 乱序装载合同

增加 child-before-parent 测试.

### v0.3, 参数化工程核心

在 v0.2.1 完成后进入:

#### P1. Feature Graph

```text
Sketch -> Extrude -> Boolean -> Fillet
```

每个 Feature:

```text
id
inputs
parameters
output
dependencies
status
recompute()
```

#### P2. SimulationModel

把 CAE Model 从 Tree/MOOSE DTO 中独立出来.

#### P3. Selection Identity v2

从 owner+bbox 扩展:

```text
owner
bbox
centroid
normal
area
adjacency
feature provenance
```

#### P4. Viewport 二次解耦

把状态从 VtkViewer facade 移到各子 Viewport.

#### P5. CMake 模块第二轮

形成:

```text
hc_core
hc_project
hc_cad
hc_mesh
hc_simulation
hc_execution
hc_results
hc_ui
```

## 8. 推荐实施顺序

建议按以下顺序执行:

```text
Step 0
  RG-D 人工总准出
  -> v0.2-rc1

Step 1
  Stable ObjectId
  + ProjectDocument SSOT

Step 2
  Object-level DependencyGraph

Step 3
  Command-only mutation
  + Undo/Redo

Step 4
  ProjectStore <-> Document

Step 5
  SimulationModel

Step 6
  Feature Graph / Recompute

Step 7
  Selection Identity v2

Step 8
  模块与 Viewport 第二轮解耦
```

其中最重要的是 Step 1.

如果 Step 1 不做, 后面 Feature Graph, Undo, Headless API, AI Agent 都会继续被 QTreeWidget 反向绑住.

## 9. 最终结论

这轮重构值得确认的不是 "多了多少新类", 而是三件事:

1. **核心业务开始离开 Widget.**
2. **架构边界开始进入 CMake 和自动化测试.**
3. **重构过程保住了 G1 的确定性行为和工程兼容性.**

这三个目标已经达到了.

下一阶段真正的架构拐点是:

```text
当前:
Tree -> Document Projection

下一步:
Document -> Tree Projection
```

完成这个反转后, HC-Gmsh 才会真正拥有独立于 GUI 的工程内核.

之后再做 Feature Graph, SimulationModel, Headless API 和 AI Agent, 技术路线会顺很多.

## 10. 主要源码依据

- `main@de30de7cfe1e54bc3fea5193379016eaeab7a59b`.
- `include/gmp/ProjectDocument.h`.
- `src/ProjectDocument.cpp`.
- `include/gmp/ModelTreeAdapter.h`.
- `src/ModelTreeAdapter.cpp`.
- `include/gmp/DependencyGraph.h`.
- `src/DependencyGraph.cpp`.
- `include/gmp/TransactionManager.h`.
- `src/TransactionManager.cpp`.
- `include/gmp/ProjectStore.h`.
- `include/gmp/MooseInputGenerator.h`.
- `include/gmp/GmshMesher.h`.
- `include/gmp/PhysicalGroupService.h`.
- `include/gmp/AssemblyGeometryService.h`.
- `include/gmp/SketchViewport.h`.
- `include/gmp/MeshViewport.h`.
- `include/gmp/ResultViewport.h`.
- `CMakeLists.txt`.
- `doc/v0.2重构任务清单.md`.
- `doc/v0.2-RG-D-总准出复核说明.md`.
