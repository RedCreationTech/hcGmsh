# UI 右栏页面迁移清单

> 阶段：Phase 1 / L-00
> 基线提交：`285699a`
> 目标：移除永久右栏前，保证 `property_stack_` 中每个页面都有临时承载、最终承载和回归入口。

## 1. 迁移原则

- 中央 3D 舞台始终存在；模块、Job、Results、Plot、Table 不得替换舞台。
- Phase 1 先将旧页面整体迁入非模态“模块工作窗”，保持对象、信号和现有功能可用。
- Phase 2 再将对象编辑页替换为带确定/取消语义的 `FloatingPropertyForm`，不得在 Phase 1 临时实现字段即时写回的新表单。
- Job 和 Results 最终使用各自的非模态单实例工作窗；Visualization、Mesh 使用独立工作窗加舞台高频工具。
- 页面完成最终迁移并通过入口回归前，不删除旧对象、不断开信号、不复用其索引。

## 2. `property_stack_` 页面清单

| 栈索引 | 模块页 | 当前入口/用途 | Phase 1 临时承载 | 最终承载 | 回归重点 | 状态 |
|---:|---|---|---|---|---|---|
| 0 | PropertyEditor | Property 页签；树节点参数查看/编辑 | 模块工作窗 | Phase 2 模态浮动属性表单 | 树选择、参数表、校验、预览、引用选项 | 已临时迁移 |
| 1 | Part | Part 页签；部件清单与 PartFeaturePanel | 模块工作窗 | 顶部/舞台命令 + 创建向导/编辑表单 | 草图转部件、拉伸/旋转/扫掠/放样、节点 CRUD | 已临时迁移 |
| 2 | Material | Material 页签；材料清单与快捷动作 | 模块工作窗 | 模态材料编辑表单 | 新建、复制、删除、参数模板、材料引用 | 已临时迁移 |
| 3 | Section | Section 页签；截面清单与指派 | 模块工作窗 | 模态截面/指派表单 | 材料引用、Physical Volume 指派 | 已临时迁移 |
| 4 | Assembly | Assembly 页签；当前以 Parts 中兼容节点承载 | 模块工作窗 | 装配工作窗 + 实例变换向导 | 实例创建、平移、持久化 | 已临时迁移 |
| 5 | Step | Step 页签；分析步清单和序列预览 | 模块工作窗 | 模态 Step 表单 + 序列工作窗 | 新建静力/瞬态步、顺序、参数预览 | 已临时迁移 |
| 6 | Interaction | Interaction 页签；接触/相互作用清单 | 模块工作窗 | 模态 Interaction/Contact 表单 | 主从面、摩擦属性、支持级别 | 已临时迁移 |
| 7 | Load | Load 页签；Load/BC 快捷动作 | 模块工作窗 | 模态 Load/BC 表单 | 载荷、边界条件、Step 关联 | 已临时迁移 |
| 8 | SketchPanel | Sketch 页签；草图列表和 2D 编辑命令 | 模块工作窗，舞台保持 2D 编辑模式 | 舞台左侧 Sketch 工具 + 草图工作窗 | 新建/打开/完成草图、撤销重做、2D/3D 切换 | 已临时迁移 |
| 9 | GmshPanel | Mesh 页签；导入、生成、预览网格 | 模块工作窗 | 舞台左侧 Mesh 工具 + Mesh 工作窗 | `.geo`、`.msh`、Physical Groups、预览 | 已临时迁移 |
| 10 | Job 容器/MoosePanel | Job 页签；生成、校验、运行、表格、日志 | 模块工作窗 | 独立非模态单实例 Job 工作窗 | 运行/停止/重试、日志、作业状态；关闭窗不停止作业 | ✅ 已迁移并回归 |
| 11 | Visualization | Visualization 页签；显示控制、Plot/Table | 模块工作窗 | 舞台高频工具 + Visualization 工作窗 | 数据加载、显示参数、相机与结果不卸载 | 详细控制已迁入独立窗，舞台工具待 L-03 |
| 12 | Results | Results 页签；结果筛选、详情、打开方式 | 模块工作窗 | 独立非模态单实例 Results 工作窗 | 筛选、Plot/Table、文本、追溯；关闭窗不卸载结果 | ✅ 已迁移并回归 |

## 3. 中央工作区迁移项

| 当前对象 | 当前行为 | 目标行为 | 计划任务 |
|---|---|---|---|
| `center_tabs/Viewport` | 中央显示 3D/2D 舞台 | 已保留为唯一中央内容 | L-01/L-02 ✅ |
| `center_tabs/Plot` | 原先切换后替换舞台 | 已移入 Results Workspace | L-02 ✅ |
| `center_tabs/Table` | 原先切换后替换舞台 | 已移入 Results Workspace | L-02 ✅ |
| 底部 Console | 与主内容垂直分割，默认占用较高 | 默认紧凑，允许展开并持久化 | L-01 |

## 4. 入口与信号保护清单

- [x] `module_tabs_::currentChanged` 切换正确的兼容页面；用户显式点击模块页签时打开/激活模块工作窗。
- [x] 模型树单击只同步选择和上下文，不强制弹出编辑窗；显式模块入口可打开兼容工作窗。
- [ ] `GmshPanel::mesh_written`、MoosePanel 运行/日志、Results 列表和 VTK 信号在换父容器后仍连接。
- [ ] PropertyEditor 的 `set_item()`、`refresh_form_options()` 在兼容工作窗关闭时仍安全。
- [ ] 草图进入/退出 2D 会话不依赖永久右栏可见性。
- [x] Job/Results 工作窗关闭不销毁页面，不停止作业、不卸载结果。
- [x] Module Workspace 与主窗口布局状态可恢复；多屏幕越界恢复仍由 L-05 完成。

## 5. L-00 验收结论

- 已完成 13 个旧页面、3 个中央页面和关键入口/信号的迁移基线清点。
- L-01 已完成“两栏主工作区 + 模块兼容工作窗”；L-02 前不得删除本表中尚未完成最终迁移的页面。
