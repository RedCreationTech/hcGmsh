# UI 重构 Phase 1 开发记录

> 开始日期：2026-09-02
> 前置条件：Phase 0 自动测试及 TEST-P0-M01～TEST-P0-M04 人工验收通过
> 当前状态：进行中

## 1. 任务状态

| 任务 | 内容 | 状态 |
|---|---|---|
| L-00 | 旧页面迁移清单与入口回归基线 | ✅ 完成 |
| L-01 | 两栏主工作区与兼容承载窗口 | ✅ 完成 |
| L-02 | 逐页迁出并移除永久右栏 | ✅ 完成 |
| L-03 | 舞台左侧垂直工具栏 | 未开始 |
| L-04 | 顶部三层结构与工作上下文 | 未开始 |
| L-05 | 工具组浮动、磁吸和布局持久化 | 未开始 |

## 2. L-00 交付

- 页面迁移清单：`doc/ui-migration-map.md`
- 基线范围：`property_stack_` 13 个页面、中央 Viewport/Plot/Table、底部 Console、模块切换和树选择信号。
- 决策：Phase 1 使用非模态模块兼容工作窗保留旧功能；对象事务式编辑表单仍由 Phase 2 / I-01 实现。

## 3. L-01 实施记录

- 主水平分割器已改为“模型树 + 中央舞台”两栏；默认截图中舞台宽度约占主内容区 84%。
- 旧 `property_stack_` 的 13 个页面已整体迁入可关闭、可移动、非模态的 Module Workspace 兼容工作窗。
- 模型树单击仅更新选择和模块上下文，不自动打开工作窗；显式点击模块页签或选择 View → Module Workspace 才显示工作窗。
- 主窗口、左栏、底部 Console 和兼容工作窗的几何/可见状态使用带版本号的 `QSettings` 键保存与恢复。
- 自动验证：`cmake --build build -j2` 通过；`ctest --test-dir build --output-on-failure` 1/1 通过；GUI 截图巡览 16/16 成功。
- 人工验证：2026-09-02，用户确认 `doc/UI重构Phase1人工验收清单.md` 中 TEST-P1-L01-01～04 全部通过；L-01 准入完成，可以启动 L-02。

## 4. L-02 实施记录

- 中央 `center_tabs` 仅保留 Viewport；Plot/Table 已迁入 Results Workspace，任何预览入口都不再替换中央舞台。
- Job、Visualization、Results 已分别迁入可关闭、非模态、单实例工作窗；关闭窗口只隐藏界面，不销毁原页面对象。
- Results Workspace 使用 Results/Plot/Table 三页结构；打开结果到 Viewer 时只更新舞台数据并聚焦 Viewport。
- Module Workspace 在 Job、Visualization、Results 原索引处保留启动页，避免破坏 `property_stack_` 的既有索引映射和信号连接。
- 三个独立工作窗已加入 View 菜单，并保存/恢复各自的位置、尺寸和可见状态。
- GUI 巡览扩展为 19 个步骤，独立抓取 Job、Visualization、Results/Plot/Table 工作窗。
- 自动验证：构建及 CTest 1/1 通过；19 步 GUI 巡览全部成功。
- 人工验证：2026-09-02，用户确认 TEST-P1-L02-01～04 全部通过；L-02 准入完成，可以启动 L-03。
