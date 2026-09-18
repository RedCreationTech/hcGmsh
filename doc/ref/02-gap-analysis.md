# HC-Gmsh 架构评审 v0.1 - 差距分析

> 评审模式: A - 架构评审, 只读.  
> 仓库: `RedCreationTech/hcGmsh`  
> 基线分支: `main`  
> 基线提交: `5210ba5b632bc529f63ff0ca49db8a5f0d4db275`  
> 基线日期: 2026-09-17  
> 范围: 比较架构能力, 不对商业产品做功能评分.

## 证据标记

- **已验证**: 当前源码和文档能够直接确认.
- **推断**: 根据现有实现关系做出的架构判断.
- **建议**: 面向未来版本的设计建议.

## 1. 比较目的和方法

本报告比较的是 **架构能力**, 不是简单比较功能数量.

三个参考对象各自代表不同的成熟工程软件架构思想:

- **FreeCAD**: 重点参考 `Document / DocumentObject / Property / Recompute / App-Gui 分离 / Module 扩展`.
- **Abaqus/CAE**: 重点参考 `Model Database / Model / Part / Assembly / Step / Load / Interaction / Job / ODB` 的对象归属和 CAE 工作流.
- **COMSOL**: 重点参考 `Model / Component / Geometry Sequence / Physics / Mesh / Study / Solution / Result` 的模型驱动方式.

HC-Gmsh 不需要复制任何一个产品, 但可以吸收这三类产品已经验证过的工程架构原则.

## 2. 对早期评审结论的修正

早期只依据 CMake 和文件名时, 曾经判断:

```text
缺少 Model Tree
缺少 Feature Tree
```

根据最新源码和 Phase 5 验收, 这一判断已经不准确.

当前真实情况是:

```text
已有 CAE Model Tree
已有 Parts / Sketches / Features / Assembly 等节点
已有 Feature 历史
已有 Part -> Current Feature 引用
已有 Assembly -> Part 引用
已有 stale 状态传播
```

真正的差距是:

```text
缺少独立 Document/Object 内核
缺少可重算 Dependency Graph
缺少通用 Property 系统
缺少 Transaction/Undo
Feature 历史不是参数化重放链
UI Tree 仍然承担 Runtime Model 职责
```

## 3. 与 FreeCAD 的架构差距

### 3.1 参考架构

FreeCAD 的核心思路可以抽象为:

```text
App::Document
  -> DocumentObject
      -> Property
      -> Link / LinkSub
      -> Feature

Document
  -> Dependency
  -> recompute()

Gui
  -> ViewProvider
  -> Command / Workbench
```

关键思想:

1. Document 是模型真源.
2. 对象独立于 UI 存在.
3. Property 是统一状态合同.
4. 对象依赖形成重算关系.
5. GUI 是模型的表示层.
6. 命令和 Workbench 可以扩展.

### 3.2 HC-Gmsh 当前相对位置

| 能力 | HC-Gmsh 当前 | FreeCAD 参考 | 差距 |
|---|---|---|---|
| 工程模型 | `.gmp.yaml` + QTreeWidget | `App::Document` | 缺独立运行时 Document |
| 对象 | Tree Item + `kind/params/status` | `DocumentObject` | 缺强类型对象基类 |
| 属性 | `QVariantMap` + 表单 | Property 系统 | 缺统一元数据和通知 |
| 引用 | 字符串名称/路径 | Link/LinkSub | 稳定身份较弱 |
| Feature | 历史记录 + Current Feature | 参数化 Feature | 缺重放图 |
| 重算 | 程序化 stale 传播 | recompute | 缺统一 Graph |
| UI 分离 | 较弱 | App/Gui 分离 | 明显差距 |
| 命令 | 分散 QAction/slot | Command Registry | 缺统一命令层 |
| 插件 | 暂无稳定体系 | Module/Workbench | 平台差距 |
| Headless | 很有限 | App 层可用 | 明显差距 |

### 3.3 最值得学习的 FreeCAD 思想

建议吸收原则:

```text
ProjectDocument = 真源
ProjectObject = 统一对象
Property = 统一数据接口
DependencyGraph = 重算基础
UI Tree = Projection
Command = 可记录操作
```

## 4. 与 Abaqus/CAE 的架构差距

### 4.1 参考架构

Abaqus/CAE 的对象模型可以简化为:

```text
Mdb
  -> Model
      -> Part
      -> Material
      -> Section
      -> Assembly
      -> Step
      -> Interaction
      -> Load / BC
      -> Mesh

Job
  -> Model

Odb
  -> Result Database
```

特点:

- CAE 对象归属明确.
- Part 和 Assembly 分开.
- Analysis Step 是正式领域对象.
- Job 和 Model 解耦.
- Result Database 与建模数据库分离.
- Python API 基本覆盖对象模型.

### 4.2 HC-Gmsh 当前相对位置

HC-Gmsh 当前业务词汇已经非常接近 Abaqus/CAE 主干:

```text
Parts
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
Mesh
Jobs
Results
```

差距主要在对象实现方式.

| 维度 | HC-Gmsh | 成熟 CAE 架构 |
|---|---|---|
| 模型数据库 | YAML + UI Tree | 独立 Model Database |
| Part | 已有 | 成熟 |
| Feature | 有历史快照 | 参数化体系更完整 |
| Assembly | 已有实例和变换 | 成熟实例/约束/集合 |
| Material/Section | 已有 | 成熟类型系统 |
| Step | 已有 | 多 Step 分析序列成熟 |
| Interaction | 已有 Contact | 更广类型 |
| Job | 本地+远程 | 完整 Job Model |
| Result | VTK/Exodus | 独立结果数据库 |
| API | 无统一 Headless API | Python Object API |
| 自动化 | GUI 主导 | API 驱动能力强 |

### 4.3 最值得学习的 Abaqus 思想

重点是对象归属和生命周期:

```text
ProjectDocument
  -> Model
     -> Part
     -> Assembly
     -> Analysis Setup

Job
  -> 引用一个冻结的 Analysis Snapshot

ResultSet
  -> 引用 Job / Snapshot
```

## 5. 与 COMSOL 的架构差距

### 5.1 参考架构

COMSOL 类型的架构可以抽象为:

```text
Model
  -> Component
      -> Geometry Sequence
      -> Definitions
      -> Materials
      -> Physics
      -> Mesh
  -> Study
  -> Solver
  -> Results
```

重要思想:

> Geometry, Physics, Mesh, Study, Result 都是同一个 Model Graph 的一部分.

### 5.2 HC-Gmsh 当前相对位置

HC-Gmsh 已经有:

- Geometry.
- Material.
- Physics.
- Mesh.
- Step.
- Outputs.
- Job.
- Results.

但关系主要由 `MainWindow` 和字符串引用维护.

主要差距:

```text
多 Component
多 Physics
Study / Solver 配置对象
参数扫描
求解序列
表达式系统
统一依赖关系
```

### 5.3 最值得学习的 COMSOL 思想

对于 HC 工业仿真平台, 最值得借鉴的是:

```text
物理模型不是 MOOSE 输入文本
而是独立 SimulationModel

MOOSE 只是其中一个 Adapter
```

目标:

```text
SimulationModel
  -> MooseAdapter
  -> CalculiXAdapter
  -> OpenFOAMAdapter
  -> Future Solver Adapter
```

## 6. 综合能力矩阵

| 架构能力 | HC-Gmsh 当前状态 | v0.2 目标 |
|---|---|---|
| 工程 Schema | 已有 v2 | 保留并收敛为 ProjectStore 合同 |
| Model Tree | 已有 | 变成 Document Projection |
| 独立 Document | 缺 | 必须建立 |
| 强类型 Object | 部分 | 建立基础层 |
| 通用 Property | 缺 | 建立属性元数据 |
| Feature History | 已有 | 保留兼容 |
| Feature Graph | 缺 | v0.3 重点 |
| Dependency Graph | 缺 | v0.2 建立 |
| Stale 状态 | 已有程序化传播 | 图驱动 |
| Transaction | 缺 | v0.2 建立 |
| Undo/Redo | 未发现通用实现 | v0.2 建立 |
| Sketch Domain | 较好 | 保护并模块化 |
| OCC Adapter | 已有 | 提取接口 |
| Gmsh Adapter | 已有但在 Panel | 提取 Service |
| Physical Group Contract | 较好 | 保护 |
| Assembly | 已有 | 从 UI 抽到 Domain/Application |
| Simulation Metadata | 较好 | 保护 |
| Solver-neutral Model | 缺 | v0.2/v0.3 |
| Snapshot | 已有 v2 | 独立服务化 |
| Local Runner | 已有 | 统一 Execution Backend |
| Remote Job | 已有 | 统一 Job Service |
| Result Viewer | 已有 | 分离 Result Model |
| Headless API | 缺 | v0.7 前建立 |
| Plugin System | 缺 | v0.7 |
| AI Tool API | 缺 | 依赖 Headless API |

## 7. 架构债

### P0 - 大规模继续增加功能前必须处理

#### D-001. UI Tree 是领域状态的一部分

当前 `QTreeWidgetItem` 直接保存:

```text
name
kind
params
status
```

风险:

- UI 和 Domain 无法解耦.
- 很难 Headless.
- 很难测试.
- 很难做事务.
- 很难支持多视图.

建议:

```text
ProjectDocument
  -> ProjectObject

ModelTreeView
  -> 观察 ProjectDocument
```

#### D-002. MainWindow 已具有 God Object 特征

`MainWindow` 当前负责:

- Model Tree CRUD.
- Part/Feature 关系.
- Assembly 构建.
- Project load/save.
- Mesh 路径迁移.
- Workflow Validation.
- MOOSE block 生成.
- Job 管理.
- Result 协调.
- stale 传播.
- UI Shell.

#### D-003. Dependency 是程序规则, 不是声明式图

典型依赖:

```text
Sketch
 -> Feature
 -> Part
 -> Assembly
 -> Mesh
 -> Simulation Input
 -> Snapshot
 -> Job
 -> Result
```

#### D-004. 缺少通用 Transaction

没有发现覆盖整个工程对象模型的:

```text
BeginTransaction
Commit
Rollback
Undo
Redo
```

### P1 - 参数化 CAD/CAE 必需

#### D-005. Feature 历史不是可重放 Feature Graph

成熟参数化 CAD 需要:

```text
Feature.input
Feature.parameters
Feature.output
Feature.dependencies
Feature.order
Feature.status
Feature.recompute()
```

#### D-006. 几何稳定身份只解决了一部分

当前 Assembly Physical Group 使用:

```text
owner + bbox + raw tag hint
```

但复杂 Fillet/Boolean 后仍可能失效.

#### D-007. MOOSE 生成缺少 Solver-neutral 中间模型

建议:

```text
ProjectDocument
  -> SimulationModel
      -> MooseInputGenerator
```

#### D-008. GmshPanel 同时是 UI 和 Engine Service

应拆分.

#### D-009. VtkViewer 混合 CAD Canvas 和 Post-processing

应拆分.

### P2 - 平台规模化和生态能力

#### D-010. 缺少 Plugin Runtime

未来支持行业模块时需要.

#### D-011. 缺少稳定 Headless/Scripting API

这是自动化和 AI Agent 的前置条件.

#### D-012. CMake 边界没有对应架构边界

目前只有主 executable 聚合所有模块.

## 8. 数据和合同债

### 8.1 Schema 文档漂移

当前 `ProjectSchema.cpp` 和测试已经包含:

```text
Input Cases
```

但 `doc/schema/project-v2.md` 顶层示例中的 `model` 列表没有同步展示 `Input Cases`.

建议:

- 明确代码和 Schema 文档的真源关系.
- 最好从 Schema 生成部分文档.
- 为根节点清单增加合同测试.

### 8.2 通用 Map 表示

`QVariantMap` 适合持久化边界.

但内部长期依赖:

```text
params["material"]
params["part"]
params["type"]
```

会增加拼写和类型风险.

建议:

```text
边界:
YAML <-> QVariantMap

内部:
Typed Objects / Typed Properties
```

### 8.3 Sketch 仍序列化到 params

`SketchDocument` 已经是强类型对象, 但当前仍以 YAML 字符串保存在模型树 `params["data"]`.

v0.2 可以保持兼容, 但运行时应让 `SketchObject` 真正持有 `SketchDocument`.

## 9. 产品架构差距

当前:

```text
功能对象已经很多
但是对象生命依赖 UI 和字符串参数
```

目标:

```text
对象先存在
UI, CLI, Agent, Batch 都只是调用它
```

## 10. 推荐架构立场

不建议:

```text
继续在 MainWindow 增加功能
继续在 PropertyEditor 增加 if(kind == ...)
继续在 GmshPanel 增加服务逻辑
继续在 MoosePanel 增加 Job 能力
```

推荐:

```text
先稳定 Domain Core
  -> ProjectDocument
  -> ProjectObject
  -> Property
  -> DependencyGraph
  -> Transaction

再抽 Application Service
  -> ProjectService
  -> CadService
  -> MeshService
  -> SimulationService
  -> JobService
  -> ResultService

最后让 Qt UI 变薄
```

## 11. 本文源码依据

主要依据:

- 最新 `CMakeLists.txt`.
- `MainWindow.h`.
- `ProjectSchema.cpp`.
- `project-v2.md`.
- `SketchDocument.h`.
- `GmshPanel.h`.
- `MoosePanel.h`.
- `VtkViewer.h`.
- `PropertyEditor.h`.
- `ApplicationProfile.h`.
- `MooseMappingRegistry.h`.
- `PhysicalGroupManifest.h`.
- `Runner.h`.
- `SimClient.h`.
- `tests/test_phase0.cpp`.
- `manual/test05.md`.
