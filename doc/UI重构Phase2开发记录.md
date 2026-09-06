# UI 重构 Phase 2 开发记录

> 开始日期：2026-09-02
> 前置条件：Phase 1 / L-01～L-05 自动与人工验收全部通过
> 当前状态：已完成（I-01 / I-01A / I-02 / I-03 / I-04 全部关闭，Phase 2 于 2026-09-06 准出）

## 1. 任务状态

| 任务 | 内容 | 状态 |
|---|---|---|
| I-01 | 浮动属性表单与编辑缓冲 | 已完成（自动与人工验收通过） |
| I-01A | 弹窗内容布局与嵌套滚动治理 | 已完成（自动与人工验收通过） |
| I-02 | 保留并优化左侧模型树 | 已完成（自动与人工验收通过） |
| I-03 | 统一树、表单、舞台与模块状态 | 已完成（2026-09-06 自动与人工验收通过） |
| I-04 | 旧右栏功能最终迁移与兼容窗收口 | 已完成（2026-09-06 自动与人工验收通过，TEST-P2-I04-01～09） |

## 2. I-01 实施记录

- 新增 `FloatingPropertyForm`，以模态、可移动的独立窗口承载属性编辑；默认放在中央舞台偏左且保证中心位于舞台可视区。
- 表单创建时复制完整模型树到隐藏缓冲树；既保留材料、变量、物理组等跨节点引用上下文，又阻断表单字段对真实项目的即时写入。
- “确定”先检查名称非空、同类节点名称唯一性和原 `PropertyEditor` 类型参数规则；失败时保持弹窗并聚焦首个错误字段，通过后一次性写回名称和参数。
- “取消”、标题栏关闭和 `Esc` 统一丢弃缓冲；未确定输入不改变模型树，不改变项目脏状态。
- 模型树单击仍只选择和同步上下文；普通对象可通过双击、`Model → Edit Properties...`、`Enter` 或右键菜单打开。Sketch/Part 继续进入已有的专用编辑器，Mesh/Job/Results 继续使用独立工作窗。
- 表单尺寸按节点类型保存在版本化 `QSettings` 键中；恢复时根据当前舞台尺寸夹取，避免旧尺寸在小窗口或屏幕变化后不可用。

## 3. I-01A 实施记录

- 根据用户截图审计确认：Module Workspace 同时存在外层与页面内层 `QScrollArea`，模块页仍沿用旧右侧窄栏的全纵向布局，导致双竖向滚动条、主要按钮被裁切及大面积无效留白。
- Module Workspace 改为直接承载模块页面；模块主操作改为横排，对象列表占用可伸缩区域，对象操作固定在底部。自动检查禁止任意页面形成嵌套滚动。
- `PropertyEditor` 改为“常规/参数/校验/预览”四页签，表单恢复标签—字段同排；`FloatingPropertyForm` 使用新的宽窗默认尺寸和尺寸配置版本。
- `PartFeaturePanel` 将拉伸、旋转、放样、扫掠四套表单拆为页签，默认窗口内可访问全部部件编辑入口。
- Job 工作窗拆为“作业列表/MOOSE 设置”；MOOSE 设置进一步拆为“算例设置/执行配置/输入文件/运行日志”，运行、检查、停止操作固定在底部，避免复合页面把窗口撑出屏幕。
- Mesh/Gmsh 工作窗拆为“模型/几何/分组与网格场/网格/日志”；几何与分组内部再按具体任务切换，彻底移除原先贯穿全部 Gmsh 功能的超长旧侧栏。
- Visualization 与 Results 工作窗去除无必要的外层滚动，保留各自既有功能页签和可伸缩内容区。
- Module、Property、Job 的旧窄窗/超高窗几何键升级，避免历史尺寸覆盖本轮合理默认值。

## 4. 自动验证

- `cmake --build build -j2`：通过。
- `ctest --test-dir build --output-on-failure`：`1/1` 通过。
- 标准 GUI 截图巡览：19 步全部通过；Job、Visualization、Results 和模块工作窗均在可用屏幕内完成截图。
- 真实点击 GUI 巡览：I-01 阶段 52 步全部通过；I-02 阶段扩展为 56 步，全部通过。新增 Model/Results 双导航、模型与结果名称筛选、状态图标/文本、Input Cases、树—舞台双向定位、Results—Model 选择同步和上游修改触发下游失效断言。
- `git diff --check`：通过。

## 5. 人工验收结论

- 2026-09-02 用户确认 `doc/UI重构Phase2人工验收清单.md` 中 TEST-P2-I01-01～09 全部通过。
- I-01 / I-01A 已关闭；2026-09-03 用户确认 I-02 人工验收通过，下一任务为 I-03“统一树、表单、舞台与模块状态”。

## 6. I-02 实施记录

- 左侧导航调整为 Model/Results 两个页签，不增加 Material Library 占位；切换页签只切换导航视图，项目模型和当前选择保持不变。
- Model 树保留既有节点，并补齐 Assembly、Physics、Constraints、Selections 和 Input Cases；Input Cases 作为持久化节点随项目 YAML 保存/加载。
- Model 和 Results 导航均提供按名称过滤及一键清除；过滤只改变可见性，不移动、删除或改写节点。
- 根据 I-02 人工预验收反馈，移除过滤框右侧独立的大号“扫帚”按钮，改用输入框内部随文本出现的原生小号清除按钮；Model/Results 两棵树的“对象/状态”列均改为 Interactive 模式，用户可直接拖动表头分隔线调整列宽。
- 树增加独立状态列，以图标和文本共同表达未配置、缺失/不完整、就绪、失效、已生成、运行中、成功和失败；Mesh/Results 引用文件缺失时显示失效。
- 根节点右键菜单提供新增、全部展开/折叠以及 Mesh/Job/Results 工作窗入口；子节点菜单提供类型匹配的打开/编辑命令及复制、重命名、删除，根节点和子节点菜单不再相同。
- Part/Feature 的 `gmsh_volume_tag` 与 Selections 的维度/tag 可驱动舞台实体或 Physical Group 过滤；舞台拾取实体/组后按相同标识反向定位 Part/Feature/Selection 节点，无绑定对象时定位相应根节点并给出状态提示。
- 同步模型到 MOOSE 输入时创建或更新 Input Cases 中的 `.i` 算例记录；上游对象变化自动把关联 Mesh、Input Cases 和未运行中的 Job 标为 Stale，历史 Results 保持不变。重新生成网格或输入后相应节点恢复“已生成”。
- Results 导航镜像 Jobs/Results 节点与状态；选择结果项会同步 Model 树，双击则加载结果并打开对应独立工作窗。

## 7. I-02 自动验证结果

- `cmake --build build -j4`：通过。
- `ctest --test-dir build --output-on-failure`：`1/1` 通过；项目 schema 回归新增 Input Cases 根节点断言。
- 标准 GUI 巡览：19 步全部通过。
- 真实点击 GUI 巡览：56 步全部通过；另有属性表单内部截图 2 张，因此输出目录共 58 张 PNG。
- 加载 `out/box.msh` 后再次执行 56 步真实点击巡览全部通过（含额外 VTK 视口截图共 59 张 PNG）；Part/Selection 选择、舞台过滤、模块/工作窗回归过程中未出现异常退出。
- 2026-09-03 用户确认 `doc/UI重构Phase2人工验收清单.md` 的 TEST-P2-I02-01～07 全部通过；I-02 已关闭。

## 8. I-03 实施记录

- 在 `MainWindow` 中建立统一活动 UI 上下文，集中记录唯一活动模块、主树对象、舞台选择集合及 Mesh/Job 运行态；模块切换不再依赖各控件各自猜测当前对象。
- 为各模块记录最后一次合法子对象；通过模块选择器往返切换时，模型树、顶部“当前对象”、底部“上下文”和工作窗标题同步恢复同一对象。没有历史对象时定位到对应根节点，避免沿用其他模块的旧选择。
- Sketch 编辑会话改为受保护的特殊状态：编辑期间锁定模块选择器、对象选择器和左侧导航；其他入口发起的模块切换会被回退并显示原因。完成/关闭编辑后统一保存、转为只读预览并恢复控件，离开 Sketch 时恢复 3D 场景。
- 集中计算菜单、共享工具栏、模块快捷按钮及 Job 管理按钮的可用状态；不支持普通属性表单的根节点、Mesh/Job/Result 节点会禁用编辑命令，Tooltip 给出原因；无运行任务时停止命令保持禁用。
- 舞台实体/Physical Group 拾取写入统一选择集合并反向定位模型树；清除后同步清空集合并禁用“清除选择”。
- Gmsh 面板新增网格任务开始/结束信号和内部重入保护；生成期间三个网格按钮、共享“生成网格”及冲突的 Job 命令显示运行态并禁用，结束后按成功/失败恢复和反馈。Job 开始/结束信号同样驱动共享命令与管理按钮。

## 9. I-03 自动验证结果

- `cmake --build build -j4`：通过。
- `ctest --test-dir build --output-on-failure`：`1/1` 通过。
- 真实点击 GUI 巡览扩展为 67 步并全部通过；新增模块对象往返恢复、上下文标签一致、禁用命令原因、舞台选择清除、Mesh 运行态恢复、Sketch 切换保护、“特征写回当前 Part、不重复创建 Part”、草图二维导航/撤销重做、工具多入口同步、“删除模型对象同步清空 3D 舞台”、“双轮廓 Part 跨模块保持两个 Volume”及“全树对象重名阻止与原位重试”断言。
- GUI 巡览完成 I-01/I-02 既有路径回归，草图新建/双击/完成、部件新建/双击、模块工作窗、Job/Visualization/Results 独立工作窗和全部舞台按钮均通过。
- `git diff --check`：通过。

## 10. I-03 人工验收状态

- 2026-09-06 用户确认 `doc/UI重构Phase2人工验收清单.md` 中 TEST-P2-I03-01～14 全部通过。
- I-03 正式关闭；进入 I-04“迁移 Mesh、Visualization、Job、Results 复合页面为独立工作窗”的剩余收口工作。

## 11. I-03 人工预验收缺陷修复

- 修复 Part 特征结果无条件新建部件的问题：Feature 现在关联并写回当前 Part；未选中 Part 时拒绝执行。对应 `TEST-P2-I03-06`。
- 修复 macOS 后台执行巡览时把系统焦点限制误判为表单失败的问题；后台模式检查字段可见性和可编辑性，前台模式继续检查真实焦点。
- 巡览断言失败改为捕获异常后以退出码 2 正常结束并输出失败步骤，不再通过 `qFatal()` 触发 `SIGABRT` 和系统崩溃报告。
- 修复草图二维舞台工具误走 3D/网格路径的问题：草图选择支持编辑态与只读预览态，左键平移/缩放交给二维相机，适配窗口和清除选择对草图生效；绘制和删除在预览态保持禁用。
- Sketch Editor 浮动窗口下的撤销/重做 QAction 改为应用级快捷键作用域，保留平台标准组合键。
- 明确拆分草图“选择 / 移动图形 / 移动视图”语义：四向箭头在 Sketch 上下文中移动命中的草图图形，整体视图平移使用中键；移动、缩放和草图工具跨按钮组互斥，并与 Sketch Editor 同步。
- 草图实体增加持久化 `shape_id`：矩形四边作为同一逻辑图形，默认整形选择/移动；`Option/Alt` 下钻单条边，配合 `Shift` 支持多图形或多子图元选择。旧草图通过 Coincident 连接关系补建逻辑分组。
- 草图工具状态改由 `VtkViewer::sketch_tool_changed` 单向回写到顶部拾取、舞台左栏和 Sketch Editor；预览态点击修改工具自动打开当前草图编辑器，悬浮工具组的 3D 拾取保护不再覆盖草图工具。
- 草图绘制按钮增加持久 checked 视觉样式和独立“当前工具”提示；编辑区改为“工具 / 约束与尺寸”双栏，撤销/重做与定宽“完成编辑”合并到底部操作行。Module Workspace 的堆叠容器改为只采用当前页尺寸提示，Sketch 使用独立 `680×350` 默认 profile 与版本化几何记忆，不再被隐藏的 Mesh/Part 页面撑高，也不再和其他模块共用窗口尺寸。
- 删除 Sketch/Part/Feature 时根据引用关系同步失效当前 3D 网格；统一清空网格 Actor、节点/轮廓、选择高亮、标尺、管线和文件监听。删除 Part 同时移除其 Feature 历史，避免树节点消失但舞台和 Feature 仍残留。对应 `TEST-P2-I03-12`。
- Feature 结果从只记录首个 `gmsh_volume_tag` 扩展为保存完整 `gmsh_volume_tags`；Part/Feature 选择优先恢复其自有 MESH 并显示全部实体，避免多轮廓拉伸在 Sketch/Part 往返后被单实体过滤。对应 `TEST-P2-I03-13`。
- 模型树所有根节点共用名称唯一性服务：用户新增/重命名冲突时在原弹窗内提示并继续修改；复制和内部自动创建统一生成唯一后缀；底层插入再做一次唯一化兜底。对应 `TEST-P2-I03-14`。
- 2026-09-04 完整 67 步真实点击 GUI 巡览通过，包含 Part 生命周期、草图二维交互、多入口工具同步、模型删除清屏、双轮廓多 Volume 往返和树对象重名拦截回归；CTest `1/1` 通过。

## 12. I-03 关闭与 I-04 交接（2026-09-06）

- 用户已完成 I-03 人工验收并确认通过；TEST-P2-I03-01～14 与本轮缺陷均关闭。
- I-04 并非从零开始：Job、Visualization、Results 已迁入独立非模态工作窗，重复打开会激活现有实例，且已具备基础尺寸记忆与越界校正。
- 当前明确差距：Mesh 仍由通用 Module Workspace 承载；Results 尚无“新建对比窗口”多实例；`doc/ui-migration-map.md` 尚未落盘；关闭不停止作业/不卸载舞台结果以及多屏恢复仍需按 I-04 用例集中验收。
- 下一会话按 `doc/UI重构开发任务清单.md` 的 `TASK-P2-I04-01～05` 执行；跨电脑恢复和验证命令见 `doc/UI重构下一阶段交接说明.md`。

## 13. I-04 实施记录（2026-09-06）

- `TASK-P2-I04-01`：复核并扩充 `doc/ui-migration-map.md`，逐页记录 13 个栈页面的入口、当前承载、I-04 目标承载、实例规则、关闭语义和回归步骤，并登记全部几何/可见性持久化键。
- `TASK-P2-I04-02`：Mesh 从通用 Module Workspace 迁入专用 `mesh_work_window_`（`meshWorkspaceWindow`，960×720）。`GmshPanel` 通过 `setWidget` 唯一归属新窗口；栈 9 改为与 Job/Visualization/Results 一致的 launcher 页，栈索引与模块映射不变。模块页签、舞台左栏、树右键（根/子节点，同步修复右键未切页的缺陷）四类入口统一激活同一窗口；几何与可见性使用版本化新键 `ui/layout/v1/mesh_workspace_*`。
- `TASK-P2-I04-03`：从 `apply_module_workspace_profile` 抽取公共越界恢复 `clamp_window_to_screen()`，四个独立工作窗在每次入口激活（`show_workspace`）和启动恢复时统一夹取到当前屏幕可用区；Mesh/Visualization 标题纳入 `sync_active_ui_context` 的活动对象后缀同步；`apply_toolbar_actions` 不再为已有独立窗口的模块回写通用窗口标题。新增巡览断言 `i04_work_window_contracts`：同名工作窗唯一（单实例）、隐藏 Job 窗口不改变作业表与日志、隐藏 Results 窗口不改变舞台已加载数据、四个窗口移出屏幕后经真实模块入口激活必须回到可视区。
- `TASK-P2-I04-04`：Results 页新增“New Comparison Window”（`resultsNewCompareButton`）。对比窗口为多实例 QDockWidget：标题 `Results Compare #N` 并随选中结果名更新，内容含结果列表、详情预览和“Focus Viewport”；几何记忆按实例编号写入 `ui/layout/v1/results_compare_N_geometry`，关闭（隐藏）时保存并在同编号实例下次创建时恢复；主 Results 工作窗保持单实例。新增巡览断言 `i04_results_compare_windows`：真实按钮连点创建两个实例、标题可区分、主窗口单实例、两实例位置独立、关闭一个不影响另一个/主窗口/舞台、关闭后独立几何键落盘。
- `TASK-P2-I04-05`：通用 Module Workspace 剩余职责收口——Mesh/Job/Visualization/Results 在栈中均为 launcher 页，真实页面唯一归属各自独立窗口；新增 `doc/UI重构Phase2人工验收清单.md` 的 TEST-P2-I04-01～07；巡览扩展为 70 步（新增 `workspace_Mesh`、`i04_work_window_contracts`、`i04_results_compare_windows`）。

## 14. I-04 自动验证结果

- `cmake --build build -j4`：通过。
- `ctest --test-dir build --output-on-failure`：`1/1` 通过。
- 真实点击 GUI 巡览：70 步全部通过（基线 67 步 + 3 个 I-04 新步骤），未删减任何既有断言。
- `git diff --check`：通过。
- I-04 待用户按 TEST-P2-I04-01～07 完成人工验收后方可关闭并准出 Phase 2。

## 15. I-04 人工预验收缺陷修复（2026-09-06）

- 用户在 TEST-P2-I04-02 预验收中发现：打开含脚本错误的 `.geo` 后无任何错误提示、中央舞台不更新，且之后合法 `.geo` 也无法导入。详见 `doc/缺陷汇总.md` 2026-09-06-013。
- 根因两层：Gmsh API 默认 `AbortOnError=2` 使脚本错误抛异常且不释放 busy 标志，导致会话级 `gmsh::open` 全部静默失效；`import_geometry` 失败只写日志页签，无可见反馈、不刷新面板状态，失败后在空模型上生成网格还会覆盖输出文件（本地 `out/box.msh` 夹具被空网格覆盖，已按样例盒参数重新生成）。
- 修复：导入期间临时 `AbortOnError=0` 并扫描 logger 错误行统一按失败处理（丢弃残缺模型、恢复选项原值、清空路径与摘要、`model_loaded_=false`）；新增 `last_import_error()` 与 `on_open_geometry` 失败告警弹窗。
- 巡览新增第 71 步 `i04_geo_import_feedback`：坏 `.geo` 断言失败且错误非空、随后合法 `.geo` 断言成功，直接回归 busy 卡死问题；71 步全部通过，CTest `1/1` 通过。

## 16. 操作日志基建与空网格保护（2026-09-06）

- 用户提出“所有操作都应有操作日志，便于复现与定位根因”；本次 .geo 事件也证明缺少持久化日志时只能猜测坏状态成因。
- 新增 `src/OperationLog.cpp` / `include/gmp/OperationLog.h`：按天写入 `~/Library/Application Support/gmp_ise/logs/operations-YYYY-MM-DD.log`（保留最近 10 个），格式为 `时间戳 [级别] 分类 | 消息`；安装 Qt message handler 把 qWarning/qCritical/qFatal 同步入文件（注意 QtInfoMsg 数值大于 QtWarningMsg，不能用 >= 过滤）；MainWindow 挂载钩子把操作日志镜像到作业/消息控制台。
- 埋点覆盖：应用启动、项目新建/打开/保存、几何导入（开始/成功含实体数/失败含错误详情）、网格生成（开始含输出路径与参数、成功/失败、写文件并载入舞台）、作业开始/结束（状态、耗时、结果文件）、模型同步、演示案例、截图、调试包导出。File 菜单新增 “Open Operation Log Folder”；Export Debug Bundle 自动附带操作日志。
- 顺藤摸出第二个洞：`on_generate` 在生成结果为空时仍写文件并报告成功——空网格覆盖有效输出并造成“已生成但舞台为空”。已加保护：生成后先查节点数，零节点直接失败、不写文件、不发 `mesh_written`。
- 巡览扩展为 72 步：`i04_geo_import_feedback` 改为合法几何 + 自动剖分并断言舞台真实载入（覆盖“导入→生成→舞台”全链路）；新增 `operation_log_smoke` 断言日志文件落盘且包含标记行。72 步全部通过，CTest `1/1` 通过。

## 17. 带孔圆环网格失败根因与退化几何拦截（2026-09-06）

- 用户用 `demo_fixed.geo` 生成网格报 `Invalid boundary mesh (overlapping facets)`。独立复现确认两层原因：脚本把矩形建在 xy 平面导致旋转体零厚度退化（体积质量为 0）；正确的圆环再做圆柱布尔又触发本机 gmsh/OCC 对四次空间交线无法闭合的限制。详见 `doc/缺陷汇总.md` 2026-09-06-014。
- `import_geometry` 新增零体积退化实体检查，把这类错误从生成阶段提前到导入阶段并给出可读原因；`demo_fixed.geo` 重写为“xz 截面旋转成环 + 方盒切扇区”的已验证版本。
- 巡览第 70 步新增退化几何断言；72 步全部通过，CTest `1/1` 通过。

## 18. 远程（LIMS）作业登记到作业列表（2026-09-06）

- 用户反馈：通过 LIMS Facade 提交的作业在 LIMS 监控页可见，但 Job 工作窗“作业列表”为空。根因：远程提交路径（`on_submit_job`/`on_sim_submit_finished`/`on_sim_job_fetched`）只更新 MOOSE 设置页的状态标签和面板日志，从未像本地作业那样发出 `job_started` 并登记到 Jobs 树/作业表。
- 修复：`MoosePanel` 新增 `remote_job_event` 信号（event=submitted/status，携带 job_id、state、server、snapshot、submit_time、progress）；提交成功与“刷新状态”时发出。`MainWindow` 统一处理：按 job_id 在 Jobs 树查找或新建节点（同名只更新不重复），参数映射到列表列（status、start_time、exec=remote:服务器、mesh=快照目录、duration=进度文本），随后刷新作业表、树状态和 Results 导航；树状态映射已原生支持 queued/running/success/failed 图标。远程提交与状态刷新同时写入操作日志。
- 巡览新增第 73 步 `remote_job_registration`：模拟提交事件断言树/表各增一条，模拟状态刷新断言不重复建节点且参数正确合并。73 步全部通过，CTest `1/1` 通过。真实 LIMS 链路的显示效果需用户复验。

## 19. 作业列表与 LIMS 任务列表全量同步（2026-09-06）

- 用户复验发现作业列表仍为空。操作日志确认：当次会话没有任何远程作业事件——作业是上一会话提交的，而旧实现只跟踪本次会话的 `last_job_id_`，重启后无从得知历史作业。
- 修复：`SimClient` 新增 `fetch_jobs(project_id, limit)`（GET `/api/sim/jobs`）与 `jobs_fetched` 信号；“刷新状态”按钮改为同时拉取本会话作业详情和 LIMS 任务摘要列表，列表中每个作业经 `remote_job_event` 登记/更新（同名只合并不重复）。LIMS 状态词汇映射到列表状态列（queued/preparing/running→Queued/Running，succeeded→Completed，failed→Failed，canceled→Canceled），耗时列依次取进度文本、percent、起止时间差。打开 Job 工作窗时自动同步一次（巡览模式跳过，避免依赖外部服务）。
- 用真实 LIMS（127.0.0.1:8200）`curl /api/sim/jobs?project_id=gmp-ise` 验证响应结构与解析一致（含 `job_20260906_180241_8x4m1i` running 50.2%）。巡览第 73 步扩展状态映射与列表合并断言，73 步全部通过，CTest `1/1` 通过。

## 20. “作业”模块 UI 重构为 LIMS 式作业监控（2026-09-06）

- 用户要求参照 structlab-lims「远程实时仿真」的任务监控/制品库交互重构 Job 模块。方案经计划评审后实施（单方案，无备选）。
- 接口层：`SimClient` 新增 `fetch_execution_status`、`fetch_job_files`、`fetch_job_log(tail)`、`cancel_job`、`download_job_file`（实时快照单文件下载，不做 sha 校验），复用既有错误描述与日志语义；字段结构已对照 `api/routes/sim.py` 与 `services/simApi.ts` 并用真实 LIMS curl 核验。
- 转发层：`MoosePanel` 新增 `refresh_job_execution/refresh_job_files/request_job_log/request_cancel_job/download_remote_file` 公共方法与 `remote_execution_status/remote_files/remote_log/remote_cancel_done/remote_file_downloaded` 转发信号（QVariantMap 化，便于巡览注入）；日志/下载请求用独立成员记录归属 job_id，取消成功后自动重刷列表。
- 监控页（Job 工作窗页签 1）：顶部状态筛选 + 刷新 + Auto(5s) 自动刷新；作业表扩展为 名称/状态/算例/进度/开始/耗时/类型/程序/结果 9 列（本地=Local，远程=Remote）；右侧详情面板含标题行、操作按钮（刷新/取消[二次确认]/task.md/日志/结果）、进度条与步数/百分比/物理时间、Execution Details 键值网格（输入文件/PID/MPI ranks/CPU/内存/当前步/dt/物理时间/已收敛/平均步耗时/整体时间/ETA/心跳/health）、Artifacts 文件表（kind/名称/大小/时间/快照标记 + 刷新文件/下载选中）；Exodus 下载完成自动注册 Results 节点（追溯 job_id）并询问是否载入舞台。选中远程作业即拉取执行状态与文件清单；自动刷新仅在窗口可见且选中远程运行中作业时触发；巡览模式跳过全部网络请求。
- 巡览新增第 74 步 `remote_job_monitor`：注册运行中远程作业并选中，注入合成执行状态/制品清单，断言进度条、详情字段、制品表、取消按钮可用性和三档状态筛选。74 步全部通过，CTest `1/1` 通过。
- 范围外（后续）：/curves 曲线接入、快照 ZIP 轮询打包、本地作业资源监控、徽标视觉精修（Phase 3）。

## 21. Results 页新增“导入结果文件”入口（2026-09-06）

- 用户询问结果页如何导入 `.e` 文件。盘点发现结果列表只有三条间接来源（本地作业完成自动登记、远程 Exodus 制品下载登记、空节点手动建），缺少直接导入外部文件入口。
- Results 页操作行新增 “Import Result File...”：文件对话框选择 `.e/.exo/.exodus/.msh/.csv/.txt/.log`，经 `import_result_file()` 注册为 Results 节点（写入操作日志），Exodus/网格同时载入中央舞台；重复导入同一路径只更新不重复。
- 巡览新增第 75 步 `results_import_file`：断言导入后树节点、结果列表条目和舞台数据三者齐备。75 步全部通过，CTest `1/1` 通过。

## 22. 结果导航树右键菜单（2026-09-06）

- 用户反馈：左侧“结果”导航分页下的 Jobs/Results 节点不支持右键操作。确认 `results_navigation_tree_` 只有选择/双击处理，没有上下文菜单。
- 新增 `build_results_navigation_menu()`：根节点提供工作窗入口（Open Job/Results Workspace）、根级操作（Jobs→Refresh Remote Jobs，Results→Import Result File...）和展开/折叠；子节点按类型分流（Jobs→打开工作窗并选中该作业、远程作业 Refresh Status、有结果时 Open Result；Results→Open in Viewer、Copy Path）以及映射回模型树条目的 Rename/Remove（复用 `model_item_for_navigation` 路径/名称匹配，`select_model_item_from_results_navigation` 同步重构复用）。
- 巡览新增第 76 步 `results_navigation_context_menu`：断言根/子节点四类菜单的动作构成。76 步全部通过，CTest `1/1` 通过。

## 23. I-04 关闭与 Phase 2 准出（2026-09-06）

- 用户确认 `doc/UI重构Phase2人工验收清单.md` 的 TEST-P2-I04-01～09 全部通过；I-04 正式关闭，Phase 2 全部完成。
- 最终回归基线：构建通过；CTest `1/1` 通过；真实点击 GUI 巡览 76 步全部通过（I-04 期间累计新增 `workspace_Mesh`、`i04_work_window_contracts`、`i04_results_compare_windows`、`i04_geo_import_feedback`、`operation_log_smoke`、`remote_job_registration`、`remote_job_monitor`、`results_import_file`、`results_navigation_context_menu`）。
- 下一阶段：Phase 3（V-01 视觉层级、V-02 国际化与键盘可访问性、V-03 布局恢复与回归保障）。
