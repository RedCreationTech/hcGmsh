# GMP-ISE 视觉与交互对标审计（Abaqus 对照）

> 版本：2026-09-09 v1
> 审计材料：`doc/UI重构需求记录.md`（REQ-001～017、8 项关键决策）、`doc/images/abaqus-workflow/` 44 张 Abaqus/CAE 2020 操作截图、最新全量巡览截图 95 张（`/tmp/gmp-ui-tour/`）、`src/` 代码全量盘点（菜单/工具组/工作窗）。
> 用途：回答“我们的弹窗和功能交互，相对 Abaqus 交互范式，视觉现状如何、下一步优化什么”。作为弹窗美化与 UI 功能手册的前置审计。

---

## 1. 信息架构对标结论（已达成部分）

REQ-001～REQ-010 的布局/交互重构已在 Phase 1–3 落地，主窗口信息架构与 Abaqus 目标层级一致：

| Abaqus 分区 | GMP-ISE 现状 | 结论 |
|---|---|---|
| 顶部菜单栏（完整命令发现） | 文件/模型/视图/网格/作业/工具/设置/帮助 8 个菜单，复用同一 QAction | ✅ 一致 |
| 紧凑图标工具组 | 项目/编辑/模型/网格/作业/回放 6 组 + 显示组（预置舞台右上角浮动） | ✅ 一致 |
| 工作上下文条 Module/Model/Part | 模块选择器（13 模块）/项目只读/当前对象选择器 + 模块快捷按钮 | ✅ 一致 |
| 左侧导航 Model/Results 页签 + 树 | 模型/结果两页签、过滤框、状态图标列、右键菜单 | ✅ 一致 |
| 舞台左侧垂直工具条 | 42px 窄条，选择/导航/视图/显示常驻，Sketch/Mesh/Viz 上下文组 | ✅ 一致 |
| 舞台上方模态编辑弹窗（Edit Material 范式） | FloatingPropertyForm：树克隆缓冲、确定/取消、标题含对象名 | ✅ 一致 |
| 底部消息区（紧凑可展开） | 底部 Console 紧凑高度 + 状态栏常驻 | ✅ 一致 |

主窗口层级不是当前痛点。**当前差距集中在各工作窗/弹窗的内部排版密度与信息层级**（REQ-006「弹窗内容必须针对工作窗重新排版」与 REQ-009「紧凑密度、克制的表单层级」在执行层面打了折扣）。

## 2. Abaqus 弹窗的视觉范式（从 44 张参考图提炼）

以图 A-08（编辑材料）、A-18（编辑分析步）、A-39（固定底面 BC）为典型：

1. **零冗余标题**：对话框标题栏即唯一标题（“编辑材料”），窗内不再重复大标题、不再放描述段落。
2. **标签—字段同行左对齐**：标签窄列、字段紧随其后，字段宽度按内容取值（数值框不拉满全宽）。
3. **页签承载分类**：材料行为按 通用/力学/热学 分页签；分析步按 基本信息/增量/其它 分页签；页签内是紧凑表单而非滚动长页。
4. **按钮只在两处**：相关小按钮紧贴其作用的控件（同行右侧），提交按钮固定底部右侧（确定/取消），不让按钮独占整行居中。
5. **表格/列表占剩余空间**：参数数据用网格表格（杨氏模量/泊松比行），吃掉弹窗剩余高度，文本标签不与其争夺纵向空间。
6. **空状态克制**：内容少时弹窗本身不高（Abaqus 弹窗也存在底部留白，但留白在表单区内部，而不是控件被拉伸摊满全窗）。

## 3. 各工作窗/弹窗视觉现状诊断

### 3.1 跨窗口共性问题（按出现频率排序）

| # | 问题 | 典型位置 | 与范式的差距 |
|---|---|---|---|
| C1 | 双/三标题：dock 标题 + 页内加粗大标题 + 描述段落 | 几乎每个工作窗（GmshPanel.cpp:74、MoosePanel.cpp:72、make_module_page、PartFeaturePanel、SketchPanel、Results 页） | 违反范式 1；每个窗口白丢 2~3 行高度 |
| C2 | 按钮独占整行且居中 | GmshPanel 各组（Add/Update/Delete、Apply/Clear/Refresh、Fuse/Cut/Intersect）、可视化窗“加载所选” | 违反范式 4；3 个按钮吃掉 3 行 |
| C3 | 单列长表单、字段拉满全宽 | Job 详情 14 行（MainWindow.cpp:1257）、GmshPanel Transform 20+ 行、Mesh 页 20 行 | 违反范式 2；窗口越宽越空旷 |
| C4 | 标签与控件上下两行排列 | 可视化 Scalar 页 4 项设置占 8 行（VtkViewer.cpp:745-802）、“输出/标量/色标”等 | 违反范式 2；密度只有范式的一半 |
| C5 | 占位 stretch 形成大面积空白 | 启动器页（MainWindow.cpp:3032-3054）、Slice/Time/Vector/Probe 控制页、GmshPanel Model 页底部 | 窗口大部分区域无信息 |
| C6 | 功能重复堆放 | Plot/Table 预览三处（Results 页签、VtkViewer 控制页、模块页按钮）；Open Log 与详情 Log；Jobs 页顶/详情/MOOSE 底部三处 Run/Stop | 用户难以判断哪个是“正主” |
| C7 | 默认尺寸与内容不匹配 | FloatingPropertyForm 默认 760×700 对 2~3 参数节点过大；Results 窗 640 宽塞 7 按钮行；可视化窗 10 个控制页每页只有几个控件 | 不是过大就是挤压 |

### 3.2 逐窗口诊断

**可视化工作窗**（用户反馈最直接的窗口，巡览图 `workspace_Visualization.png`）
- 页头“可视化”标题 + 描述 + 3 按钮 + 分隔线，与 dock 标题、回放工具组重复（C1/C6）。
- “标量”页签下拉 + Scalar 页内“标量”标签，同词两连出现，视觉上是 bug 观感。
- 4 项设置 8 行（C4）；Slice/Time/Vector/Deformation/Probe 5 个控制页各只有 1~4 行控件 + stretch（C5）；10 个控制页可合并为 6 个。
- Plot/Table 控制页与 Results 工作窗页签完全重复（C6），可视化窗内应只留跳转。

**网格工作窗（GmshPanel）**（巡览图 `workspace_Mesh.png`）
- “Gmsh 面板”页内标题与 dock 标题重复（C1）。
- Physical Groups 的 Add/Update/Delete、Mesh Fields 的 Apply/Clear/Refresh、实体尺寸的 Apply/Clear 全部按钮独占行（C2）；Pick 与 IDs 输入框分两行（C2）。
- Transform 页 Rotate/Scale 12 个 spin 各占一行（C3）；Mesh 页 20 行需滚动，高级项（Element Order/High-Order/MSH Version/算法）应收进可折叠组。
- Model 页与 Geometry→Primitives 都管几何来源，分区职责重叠。

**作业工作窗（Jobs 页 + MoosePanel）**
- “MOOSE Panel”页内标题重复（C1）；“  State:” 用前导空格凑间距（代码异味）。
- 顶部 5 按钮 + 筛选 + 刷新 + Auto 挤一行，窗宽 820 已满（C7）。
- “Execution Details” 14 行单列（C3），改 2 列 7 行省一半高度。
- Run/Stop 三处重复、Open Log 两处重复（C6）；“Local Job”组对远程作业疑似始终占位（未确认显隐逻辑）。

**结果工作窗**
- 7 按钮 + 2 处 stretch + 过滤全在一行，640 宽必然挤压（C7）；应拆“文件操作行 / 视图对比过滤行”两行。
- Plot/Table 页签内标题 + 提示 + 状态 3 行纯文字（C1）。
- 结果对比窗顶部说明标签占 3 行、预览区始终展开（与主窗 Preview 默认折叠不一致）。

**通用模块工作窗（Module Workspace / 属性表单）**
- make_module_page 边距 18/16 是全套最大留白（C3）；“Current entries:”标签冗余；启动器页近乎空白（C5）。
- FloatingPropertyForm 内嵌完整 4 页签 PropertyEditor，Validation/Preview 与属性模块页重复（C6）；760×700 默认尺寸对小节点过大（C7）。

**部件/草图**
- Part 模块页标题+描述与 PartFeaturePanel 标题+描述双份（C1）；Revolve 页签复用 Extrude 的草图下拉，用户在 Revolve 页看不到作用草图（可用性问题，不只是视觉）。
- 草图编辑器已是各窗口中最紧凑的；剩余：“Current tool:” 标签与按钮选中态重复、提示标签占 2 行可改 tooltip。

**Abaqus 有而我们缺失的交互**（非弹窗排版问题，记录在案）
- 草图会话的“提示行”（Abaqus 视口底部“为实体拉伸绘制截面草图 / 完成”）：我们的完成编辑在弹窗按钮里，视口底部无提示行。
- 管理器式对话框（BC 管理器、场输出管理器：列表 + 创建/编辑/复制/删除/关闭右侧竖排按钮，图 A-42/A-20）：我们的等价物是“树 + 工作窗列表”，语义已覆盖，不另建。

## 4. 下一步优化点清单

按“投入小、视觉收益大”排序。P0 为弹窗美化首轮范围；P1 次之；P2 记录待议。

### P0（首轮美化，全是排版调整，不改功能语义）

1. **统一删除页内标题/描述/HLine**：GmshPanel、MoosePanel、make_module_page、PartFeaturePanel、SketchPanel、Results 页、可视化页头（C1）。dock 标题保留；必要说明并入 tooltip/状态栏。
2. **按钮同行化**：GmshPanel 各组 2~3 按钮合并一行、IDs+Pick 同行（C2）；Results 7 按钮拆两行（文件操作 / 视图对比过滤）（C7）。
3. **标签—控件同行化**：可视化 Scalar 页 8 行→4 行表单；“输出”下拉+“加载所选”一行（C4）。
4. **控制页合并**：可视化 10 页→6 页（Slice 并入 View、Vector 并入 Deformation、Probe 并入 Mesh、Plot/Table 改跳转按钮）（C5/C6）。
5. **Job 详情两列化**：Execution Details 14 行→2 列 7 行；修 “  State:” 前导空格（C3）。
6. **去重**：可视化窗 Plot/Table 页只留跳转；Jobs 页 Open Log 与详情 Log 合并（C6）。
7. **尺寸矫正**：Results 默认宽 640→720；FloatingPropertyForm 默认高 700→按页签内容 560，小节点不再大面积留白（C7）。

### P1（结构改进）

8. GmshPanel Mesh 页高级项折叠组；Transform 页 xyz 三格同行（C3）。
9. FloatingPropertyForm 模态表单只保留 General+Parameters 两页签（Validation/Preview 留给属性模块页）（C6）。
10. 模块启动器页改单行提示条或切换模块时直接隐藏 Module Workspace（C5）。
11. Revolve/Loft/Sweep 页签各自放草图选择，或草图选择提到页签外公共行（可用性）。
12. Jobs 页筛选行与按钮行分行；“Local Job”组按本地/远程显隐。

### P2（待议）

13. 草图会话视口底部提示行（对齐 Abaqus 草图器）。
14. Playback 工具组文案进 L10n 字典（当前切中文仍为英文）。
15. Model 页与 Geometry→Primitives 几何来源职责合并。

## 5. 验证与衔接

- 每个 P0/P1 项实施后按 `AGENTS.md` 策略跑对应巡览步骤（工作窗改动跑 `i04_work_window_contracts` 及相关 workspace 步骤），提交前全量 85 步。
- 视觉验收：P0 完成后重新截取各工作窗巡览图，与本文件 §2 六条范式逐条对照。
- 本审计完成后，UI 功能手册（菜单/工具栏/各模块弹窗功能说明）按美化后的最终状态编写，避免文档写完即过时。
