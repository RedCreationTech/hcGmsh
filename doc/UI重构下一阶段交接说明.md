# GMP-ISE UI 重构下一阶段交接说明

> 交接日期：2026-09-06
> 代码基线：远端 `origin/main` 中包含“完成 Phase 2 交互状态重构”的提交
> 当前阶段：Phase 2 进行中；I-01、I-01A、I-02、I-03 已关闭，下一任务为 I-04

## 1. 接续目标

下一会话只处理 I-04“迁移 Mesh、Visualization、Job、Results 复合页面为独立工作窗”，完成自动回归和人工验收后再关闭 Phase 2。不要提前开始 Phase 3 的视觉精修，以免工作窗所有权尚未稳定时重复调整样式和布局。

I-04 的现状不是空白：Job、Visualization、Results 已有独立非模态单实例工作窗、基础尺寸记忆和越界校正；Mesh 以及 Part、Sketch 等页面仍通过 `property_stack_` 进入通用 Module Workspace。Results 默认工作窗已有页签，但尚未实现任务合同要求的“新建对比窗口”多实例。

## 2. 在另一台电脑恢复代码

```bash
git clone --recurse-submodules git@github.com:RedCreationTech/hcGmsh.git
cd hcGmsh
git checkout main
git pull --ff-only origin main
git submodule update --init --recursive
```

如果仓库已经存在，只需进入仓库后执行最后三行。开始开发前确认：

```bash
git status --short
git log -3 --oneline --decorate
```

工作区应为空，`HEAD` 应与 `origin/main` 一致。不要提交 `.env`、本机依赖路径、临时 BREP/MESH、截图或构建目录。

## 3. 构建与回归

工程使用 Qt 6/C++17；完整 UI 默认还需要 Gmsh、OpenCASCADE、Eigen3、Boost 和 VTK。`dev.sh` 默认启用 Gmsh API 与 VTK Viewer，并会完成配置、构建和启动：

```bash
./dev.sh
```

仅配置和构建时可执行：

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGMP_ENABLE_GMSH_GUI=ON \
  -DGMP_ENABLE_VTK_VIEWER=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

依赖不在系统默认搜索路径时，按 `dev.sh` 注释设置 `gmsh_DIR`、`VTK_DIR` 或 `CMAKE_PREFIX_PATH`。每次 I-04 代码变更至少运行构建和 CTest；工作窗入口、持久化或关闭语义变更后，还要运行真实点击 GUI 巡览：

```bash
mkdir -p /tmp/gmp-ui-tour
GMP_TOUR_REAL_CLICKS=1 \
GMP_SCREENSHOT_DIR=/tmp/gmp-ui-tour \
./build/gmp_ise
```

当前回归基线为 67 步全部通过、CTest `1/1` 通过。新增 I-04 用例时应在此基线上递增，不得删减既有断言来换取通过。

## 4. 先读文档

按以下顺序建立上下文：

1. `doc/UI重构需求记录.md`：整体需求基线。
2. `doc/UI重构开发任务清单.md`：阶段依赖、I-04 合同和 `TASK-P2-I04-01～05`。
3. `doc/UI重构Phase2开发记录.md`：已经完成的实现和 I-03 关闭状态。
4. `doc/UI重构Phase2人工验收清单.md`：I-01～I-03 已通过的不可回归行为。
5. `doc/用户使用手册.md`、`doc/缺陷汇总.md`：当前用户语义和历史问题。

## 5. I-04 首轮任务

### TASK-P2-I04-01 补齐迁移地图

- 新建 `doc/ui-migration-map.md`。
- 清点 `src/MainWindow.cpp` 中全部 `property_stack_->addWidget(...)` 页面。
- 对每一页记录：模型树/菜单/工具栏/模块选择器入口、当前承载、I-04 目标承载、单实例或多实例规则、关闭语义、人工回归步骤。
- 这是历史 L-00 的缺失交付物，也是安全拆分通用 Module Workspace 的前置条件。

### TASK-P2-I04-02 拆出 Mesh 专用工作窗

- 在 `MainWindow` 中增加专用 `mesh_work_window_`，不要继续用通用 `module_work_window_` 承载 Mesh。
- 所有 Mesh 入口重复打开时只激活同一窗口。
- 保留模型、几何、分组与网格场、网格、日志页签，以及生成参数、运行态、进度、质量摘要和中央舞台联动。
- 使用独立、版本化的几何与可见性配置键，并复用现有越界恢复逻辑。

### TASK-P2-I04-03 收紧三个既有工作窗合同

- Job：默认单实例；运行中关闭窗口不停止作业，重新打开可继续查看状态与日志。
- Visualization：默认单实例；详细参数继续驱动中央舞台。
- Results：默认单实例；关闭窗口不卸载舞台结果。
- 三者补齐一致的标题、尺寸记忆、重复打开激活、第二屏移除后的越界恢复自动断言。

### TASK-P2-I04-04 Results 对比窗口

- 在默认 Results 工作窗提供“新建对比窗口”。
- 对比窗口允许多实例，必须拥有可区分的标题和独立几何记忆；默认 Results 工作窗仍为单实例。
- 关闭任一对比窗口不得影响主 Results 工作窗、已加载舞台结果或其他对比窗口。

### TASK-P2-I04-05 兼容窗收口与验收

- 根据迁移地图清理通用 Module Workspace 的剩余职责，避免页面同时被多个容器拥有或入口指向不同实例。
- 新增 I-04 人工验收清单，覆盖任务文档现有 6 项人工验证。
- 扩展真实点击 GUI 巡览，完成构建、CTest、巡览和人工验收后，才能把 I-04 与 Phase 2 标记为完成。

## 6. 关键代码位置

- `include/gmp/MainWindow.h`：工作窗成员、全局 UI 状态和命令声明。
- `src/MainWindow.cpp`：工作窗创建、`property_stack_` 页面注册、模块/树/舞台同步、几何持久化、真实点击巡览。
- `src/GmshPanel.cpp`、`include/gmp/GmshPanel.h`：Mesh 参数、生成状态和结果信号。
- `src/MoosePanel.cpp`、`include/gmp/MoosePanel.h`：Job/MOOSE 页面和作业状态。
- `src/VtkViewer.cpp`、`include/gmp/VtkViewer.h`：中央舞台、结果加载与显示状态。

当前已知的结构性事实：

- `module_work_window_` 仍拥有通用 `property_stack_`。
- `job_work_window_`、`visualization_work_window_`、`results_work_window_` 已独立存在。
- `property_stack_` 仍注册 Property、Part、Material、Section、Assembly、Step、Interaction、Load、Sketch、Mesh 和三个独立工作窗的 launcher 页面。
- 工作窗几何使用 `QSettings` 版本化键；主窗已有屏幕变化和越界恢复逻辑，新增窗口应复用同一规则，避免另起一套实现。

## 7. 不可回归行为

- Part 特征写回当前 Part，只新增 Feature，不重复创建 Part。
- 草图选择、移动、缩放、绘制语义互斥，顶部/左侧/Sketch Editor 高亮同步。
- 矩形默认按完整逻辑图形选择；Option/Alt 下钻子边；Shift 多选并整体移动。
- 草图应用级撤销/重做快捷键有效，绘制工具保持高亮，编辑窗采用独立紧凑尺寸。
- 删除 Sketch/Part/Feature 时同步清理依赖舞台；删除 Part 级联清理所属 Feature。
- 多轮廓拉伸保存全部 Volume，Sketch/Part 往返后仍完整显示。
- 同一模型树根节点下名称唯一；冲突时命名弹窗保持打开供用户修改。
- Mesh/Job 长任务防重复提交，模块、模型树、工作窗和舞台保持同一活动对象。

## 8. 新 AI 会话可直接使用的提示词

```text
请继续 GMP-ISE 的 UI 重构 Phase 2。先阅读 doc/UI重构需求记录.md、doc/UI重构开发任务清单.md、doc/UI重构Phase2开发记录.md、doc/UI重构Phase2人工验收清单.md 和 doc/UI重构下一阶段交接说明.md。当前 I-01、I-01A、I-02、I-03 已验收关闭，不要回退其行为；从 TASK-P2-I04-01 开始，先补齐 ui-migration-map，再审计并实现 Mesh 独立工作窗、Job/Visualization/Results 工作窗合同、Results 对比多实例，扩展自动巡览和 I-04 人工验收清单。每个切片完成后构建并运行 CTest，涉及 UI 入口时运行真实点击巡览；I-04 验收前不要进入 Phase 3。
```
