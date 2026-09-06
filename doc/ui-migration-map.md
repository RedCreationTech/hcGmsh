# UI 右栏页面迁移清单

> 阶段：Phase 1 / L-00 建立；Phase 2 / I-04（TASK-P2-I04-01）复核更新
> 基线提交：`285699a`（L-00）；2026-09-06 按 I-04 代码基线复核
> 目标：移除永久右栏前，保证 `property_stack_` 中每个页面都有临时承载、最终承载和回归入口；I-04 拆分通用 Module Workspace 前确认每页入口、实例规则和关闭语义。

## 1. 迁移原则

- 中央 3D 舞台始终存在；模块、Job、Results、Plot、Table 不得替换舞台。
- Phase 1 先将旧页面整体迁入非模态“模块工作窗”，保持对象、信号和现有功能可用。
- Phase 2 再将对象编辑页替换为带确定/取消语义的 `FloatingPropertyForm`，不得在 Phase 1 临时实现字段即时写回的新表单。
- I-01A 将兼容工作窗从旧右侧窄栏排版调整为宽窗任务布局：禁止嵌套滚动，主要操作横排，复杂表单和复合工作窗按页签分区。
- I-02 将左侧导航拆为 Model/Results 两个页签，增加名称过滤、状态列和 Input Cases；导航切换不替换中央舞台。
- Job 和 Results 最终使用各自的非模态单实例工作窗；Visualization、Mesh 使用独立工作窗加舞台高频工具。
- 页面完成最终迁移并通过入口回归前，不删除旧对象、不断开信号、不复用其索引。

## 2. 承载结构总览（I-04 基线）

| 承载容器 | objectName | 说明 |
|---|---|---|
| `module_work_window_` | `moduleWorkspaceWindow` | 通用浮动 QDockWidget，承载 `property_stack_`（13 页），最小 620×540，默认 760×700；Sketch 编辑期切换紧凑 profile（最小 640×320，默认 680×350） |
| `job_work_window_` | `jobWorkspaceWindow` | 独立非模态单实例工作窗，900×720，View 菜单可切换 |
| `visualization_work_window_` | `visualizationWorkspaceWindow` | 独立非模态单实例工作窗，720×740 |
| `results_work_window_` | `resultsWorkspaceWindow` | 独立非模态单实例工作窗，900×720，内含 Results/Plot/Table 页签（`resultsWorkspaceTabs`） |
| `mesh_work_window_` | （I-04 待建） | I-04 目标：Mesh 专用独立工作窗（TASK-P2-I04-02） |

模块选择器顺序（`module_tabs_`，隐藏的内部状态机）：Sketch(0)、Part(1)、Property(2)、Material(3)、Section(4)、Assembly(5)、Step(6)、Interaction(7)、Load(8)、Mesh(9)、Job(10)、Visualization(11)、Results(12)。
模块索引 → 栈页映射（`module_to_property`，`src/MainWindow.cpp:2877`）：`{8, 1, 0, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12}`。

已知行为约定：

- `module_tabs_::currentChanged` 只切页不弹窗；弹窗由 `tabBarClicked`（`src/MainWindow.cpp:2978-2995`）、显式编辑器、右键菜单、工具栏/舞台动作触发。编程式 `setCurrentIndex` 后须显式调用 `tabBarClicked` 才会显示窗口（模块选择器与巡览均如此）。
- Job/Visualization/Results 三个模块页签点击时直接打开对应独立工作窗，不显示 Module Workspace。
- 三个 launcher 页（栈 10/11/12）仅含说明文字与“Open … Workspace”按钮（`make_workspace_launcher`，2775–2797），无业务控件；真正的页面已通过 `setWidget` 重挂到独立工作窗（2267–2269）。

## 3. `property_stack_` 页面清单

| 栈索引 | 模块页 | 当前入口/用途 | Phase 1 临时承载 | 最终承载 | 回归重点 | 状态 |
|---:|---|---|---|---|---|---|
| 0 | PropertyEditor | Property 页签；树节点参数查看/编辑 | 模块工作窗 | Phase 2 模态浮动属性表单 | 树选择、参数表、校验、预览、引用选项 | ✅ I-01 事务式表单与 I-01A 四页签布局验收通过 |
| 1 | Part | Part 页签；部件清单与 PartFeaturePanel | 模块工作窗 | 顶部/舞台命令 + 创建向导/编辑表单 | 草图转部件、拉伸/旋转/扫掠/放样、节点 CRUD | ✅ I-01A 四类特征页签验收通过 |
| 2 | Material | Material 页签；材料清单与快捷动作 | 模块工作窗 | 模态材料编辑表单 | 新建、复制、删除、参数模板、材料引用 | 已临时迁移 |
| 3 | Section | Section 页签；截面清单与指派 | 模块工作窗 | 模态截面/指派表单 | 材料引用、Physical Volume 指派 | 已临时迁移 |
| 4 | Assembly | Assembly 页签；当前以 Parts 中兼容节点承载 | 模块工作窗 | 装配工作窗 + 实例变换向导 | 实例创建、平移、持久化 | 已临时迁移 |
| 5 | Step | Step 页签；分析步清单和序列预览 | 模块工作窗 | 模态 Step 表单 + 序列工作窗 | 新建静力/瞬态步、顺序、参数预览 | 已临时迁移 |
| 6 | Interaction | Interaction 页签；接触/相互作用清单 | 模块工作窗 | 模态 Interaction/Contact 表单 | 主从面、摩擦属性、支持级别 | 已临时迁移 |
| 7 | Load | Load 页签；Load/BC 快捷动作 | 模块工作窗 | 模态 Load/BC 表单 | 载荷、边界条件、Step 关联 | 已临时迁移 |
| 8 | SketchPanel | Sketch 页签；草图列表和 2D 编辑命令 | 模块工作窗，编辑会话进入 2D 舞台 | 舞台左侧 Sketch 工具 + 草图工作窗 | 新建/树双击/列表双击/打开编辑、完成草图、撤销重做、2D/3D 切换 | ✅ 入口与预览回归通过 |
| 9 | GmshPanel | Mesh 页签；导入、生成、预览网格 | 模块工作窗 | **I-04 迁入专用 Mesh 工作窗** | `.geo`、`.msh`、Physical Groups、预览、生成进度/质量摘要 | ⏳ TASK-P2-I04-02（高频入口与 I-01A 五类任务页签已验收） |
| 10 | Job 容器/MoosePanel | Job 页签；生成、校验、运行、表格、日志 | 模块工作窗 | 独立非模态单实例 Job 工作窗 | 运行/停止/重试、日志、作业状态；关闭窗不停止作业 | ✅ 独立工作窗已建成；I-04 补合同断言（TASK-P2-I04-03） |
| 11 | Visualization | Visualization 页签；显示控制、Plot/Table | 模块工作窗 | 舞台高频工具 + Visualization 工作窗 | 数据加载、显示参数、相机与结果不卸载 | ✅ 舞台工具与独立窗回归通过；I-04 补合同断言 |
| 12 | Results | Results 页签；结果筛选、详情、打开方式 | 模块工作窗 | 独立非模态单实例 Results 工作窗 + “新建对比窗口”多实例 | 筛选、Plot/Table、文本、追溯；关闭窗不卸载结果 | ⏳ 单实例已迁移；对比窗口待建（TASK-P2-I04-04） |

## 4. I-04 逐页详审（入口 / 承载 / 实例规则 / 关闭语义 / 回归步骤）

### 栈 0 — Property（`property_editor_`，PropertyEditor）

- **入口**：Property 模块页签；模块列表双击非 Part 节点跳 Property 页（692–718）；Material/Section 页工具按钮“Open Property Editor”（1513–1518、1543–1548）。
- **当前承载**：Module Workspace（仅 `tabBarClicked`/模块选择器路径显示窗口）。
- **I-04 目标承载**：保持 Module Workspace + 双击树节点的 `FloatingPropertyForm`；无变化。
- **实例规则**：单页。**关闭语义**：隐藏窗口即可。
- **回归步骤**：切换 Property 模块确认窗口弹出且表单跟随当前树节点；双击 Material 节点确认打开 FloatingPropertyForm 而非本页。

### 栈 1 — Part（part_page）

- **入口**：Part 模块页签；树双击 Part 子节点 → `open_part_editor`（1115–1140，标题“Part Editor — <名称>”）；Part 列表双击（1233–1240）；“Open Selected part”（1249–1264）；工具栏“New Part”/`create_part_from_sketch`；右键“Open Part Editor”。
- **当前承载 / I-04 目标承载**：Module Workspace；无变化。
- **实例规则**：单页。**关闭语义**：隐藏窗口，Part 数据不受影响。
- **回归步骤**：双击 Part 节点确认窗口标题与特征页签；执行一次拉伸确认写回当前 Part、不新建 Part（不可回归行为）。

### 栈 2–7 — Material / Section / Assembly / Step / Interaction / Load

- **入口**：对应模块页签；列表双击跳 Property 页；各页工具栏新建按钮（2438–2532）。
- **当前承载 / I-04 目标承载**：Module Workspace；Phase 4 再评估是否拆独立工作窗。
- **实例规则**：单页。**关闭语义**：隐藏窗口。
- **回归步骤**：逐页签切换确认列表刷新、新建按钮创建对应根节点子项、双击跳转 Property 页加载同一对象。

### 栈 8 — Sketch（`sketch_panel_`，SketchPanel）

- **入口**：Sketch 模块页签；树双击 Sketch 子节点 → `open_sketch_editor`（1765–1813）；草图列表双击；`new_sketch_requested`/`open_edit_requested`；舞台工具自动进入编辑（2312–2334）。
- **当前承载 / I-04 目标承载**：Module Workspace 紧凑 profile（`gmpWorkspaceProfile=sketch`，独立几何键 `ui/layout/v6/sketch_editor_geometry`）；草图编辑会话保护（I-03）不动。
- **实例规则**：单页单会话。**关闭语义**：关闭窗口 = 保存并结束草图编辑会话（`visibilityChanged` → `close_sketch_editor`，1832–1837）。
- **回归步骤**：新建草图确认紧凑尺寸与工具高亮；关闭窗口确认编辑会话结束、模块控件恢复。

### 栈 9 — Mesh（`mesh_page`，GmshPanel）

- **内容**：`gmshWorkspaceTabs`（模型/几何/分组与网格场/网格/日志 5 页签）、`gmshGeometryTabs`（3）、`gmshGroupsTabs`（2）；生成参数、进度、质量摘要、运行态。
- **入口**：Mesh 模块页签；舞台左栏 `mesh_workspace_requested`（2369–2376）；树右键 Mesh 根“Open Mesh Workspace”（4487–4505）与 Mesh 子节点（4541–4545，**已知缺陷：未显式切栈页**）；工具栏 Generate Mesh/2D/3D、Generate & Submit（2569–2601）。
- **当前承载**：Module Workspace（通用 profile）。
- **I-04 目标承载**：迁入专用 `mesh_work_window_`（TASK-P2-I04-02），独立版本化几何键，越界恢复复用主窗规则；右键入口改为显式打开新窗口。
- **实例规则**：单实例，重复打开激活同一窗口。**关闭语义**：隐藏窗口，进行中的网格生成任务不得中断；重开状态与日志完整。
- **回归步骤**：Mesh 页签/舞台按钮/树右键三入口均激活同一窗口；生成网格期间关闭窗口任务继续；重新打开状态与日志完整。

### 栈 10 — Job launcher → `job_work_window_`

- **入口**：Job 模块页签（直接开 `job_work_window_`，2983–2984）；工具栏/模块动作“Open Job Workspace”；树右键 Jobs 根/子节点；Results 导航双击 Jobs 项（884–887）。
- **当前承载 / I-04 目标承载**：launcher 留 Module Workspace；真实作业页（`job_container`：作业列表/MOOSE 设置页签 + 底部运行按钮）在 `job_work_window_`；I-04 仅补合同断言。
- **实例规则**：单实例，重复打开激活。**关闭语义**：关闭窗口不停止运行中的作业，重开可查看状态与日志。
- **回归步骤**：两个入口重复打开确认单实例；运行 demo 作业后关闭再打开确认状态/日志保留。

### 栈 11 — Visualization launcher → `visualization_work_window_`

- **入口**：Visualization 模块页签（2985–2986）；舞台左栏 `visualization_workspace_requested`；模块工具动作。
- **当前承载 / I-04 目标承载**：launcher 留 Module Workspace；真实页在 `visualization_work_window_`；I-04 仅补合同断言。
- **实例规则**：单实例。**关闭语义**：隐藏窗口，舞台显示状态不变。
- **回归步骤**：打开窗口调整显示参数确认舞台同步；重复打开确认单实例。

### 栈 12 — Results launcher → `results_work_window_`

- **入口**：Results 模块页签（2987–2991，顺带把 `results_work_tabs_` 复位到 0）；舞台左栏 `results_workspace_requested`；Results 导航树双击（862–889）；树右键 Results 根/子节点；Plot/Table “Focus Viewport”按钮（2001–2014）。
- **当前承载**：launcher 留 Module Workspace；真实页（Results/Plot/Table 页签）在 `results_work_window_`。
- **I-04 目标承载**：默认工作窗保持单实例；新增“新建对比窗口”多实例（TASK-P2-I04-04），对比窗口独立标题与几何记忆。
- **实例规则**：默认单实例 + N 个对比窗口。**关闭语义**：关闭任何 Results 窗口不卸载舞台已加载结果；关闭对比窗口不影响主窗口与其他对比窗口。
- **回归步骤**：加载结果后关闭主窗口确认舞台结果保留；“新建对比窗口”确认标题可区分、关闭互不影响。

## 5. 几何与可见性持久化键（QSettings `gmp-ise`/`gmp_ise`）

| 窗口 | 几何键 | 可见性键 |
|---|---|---|
| 主窗口 / 分割条 / 工具组 | `ui/layout/v1/*`、`ui/layout/v3/main_window_state` | — |
| Module Workspace（通用 profile） | `ui/layout/v6/module_workspace_geometry`（旧键 v2 保留降级） | `ui/layout/v2/module_workspace_visible` |
| Module Workspace（Sketch profile） | `ui/layout/v6/sketch_editor_geometry` | 同上 |
| Job | `ui/layout/v3/job_workspace_geometry` | `ui/layout/v3/job_workspace_visible` |
| Visualization | `ui/layout/v2/visualization_workspace_geometry` | `ui/layout/v2/visualization_workspace_visible` |
| Results | `ui/layout/v2/results_workspace_geometry` | `ui/layout/v2/results_workspace_visible` |
| Mesh（I-04 新建） | 待建：`ui/layout/v1/mesh_workspace_geometry`（版本化新键） | `ui/layout/v1/mesh_workspace_visible` |
| Results 对比窗口（I-04 新建） | 待建：按实例编号版本化键 | 不持久化可见性 |

越界恢复：Module Workspace 已有实现（`apply_module_workspace_profile`，4818–4838：按窗口中心定位屏幕、尺寸与左上角夹取）；三个独立工作窗与新建 Mesh 窗口须复用同一规则（TASK-P2-I04-03 抽取公共助手）。

## 6. 中央工作区迁移项

| 当前对象 | 当前行为 | 目标行为 | 计划任务 |
|---|---|---|---|
| `center_tabs/Viewport` | 中央显示 3D/2D 舞台 | 已保留为唯一中央内容 | L-01/L-02 ✅ |
| `center_tabs/Plot` | 原先切换后替换舞台 | 已移入 Results Workspace | L-02 ✅ |
| `center_tabs/Table` | 原先切换后替换舞台 | 已移入 Results Workspace | L-02 ✅ |
| 底部 Console | 与主内容垂直分割，默认占用较高 | 默认紧凑，允许展开并持久化 | L-01 |

## 7. 入口与信号保护清单

- [x] `module_tabs_::currentChanged` 保留为内部兼容状态机；可见的工作上下文“模块”选择器切换正确页面并打开/激活对应工作窗。
- [x] 模型树单击只同步选择和上下文，不强制弹出编辑窗；显式模块入口可打开兼容工作窗。
- [x] `GmshPanel::mesh_written`、MoosePanel 运行/日志、Results 列表和 VTK 信号在换父容器后仍连接；I-02 新增树—舞台双向定位自动回归。
- [x] PropertyEditor 的 `set_item()`、`refresh_form_options()` 在兼容工作窗关闭时仍安全。
- [x] 草图进入/退出 2D 会话不依赖永久右栏可见性。
- [x] Job/Results 工作窗关闭不销毁页面，不停止作业、不卸载结果。
- [x] Module Workspace 与主窗口布局状态可恢复；多屏幕越界恢复仍由 L-05 完成。

## 8. I-04 拆分执行顺序与验收结论

- L-00：已完成 13 个旧页面、3 个中央页面和关键入口/信号的迁移基线清点。
- L-01 已完成“两栏主工作区 + 模块兼容工作窗”；L-02 前不得删除本表中尚未完成最终迁移的页面。
- I-04 执行顺序：`TASK-P2-I04-02`（栈 9 Mesh 迁入专用窗口，修正右键入口未切页缺陷）→ `TASK-P2-I04-03`（Job/Visualization/Results 合同断言）→ `TASK-P2-I04-04`（Results 对比窗口多实例）→ `TASK-P2-I04-05`（launcher/通用承载收口 + 人工验收清单 + 巡览扩展）。

---

*本清单随 `doc/UI重构开发任务清单.md` 同步维护；页面增删或入口变更时必须更新。*
