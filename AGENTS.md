# GMP-ISE 工程约定（AGENTS.md）

> 适用于本仓库的协作约定。随流程演进同步维护。

## 构建与测试命令

```bash
# 构建（Qt 6/C++17，需 Gmsh、OpenCASCADE、Eigen3、Boost、VTK）
cmake --build build -j4

# 单元测试
ctest --test-dir build --output-on-failure

# 全量真实点击 GUI 巡览（当前基线 85 步）
mkdir -p /tmp/gmp-ui-tour
GMP_TOUR_REAL_CLICKS=1 GMP_SCREENSHOT_DIR=/tmp/gmp-ui-tour ./build/gmp_ise

# 定向巡览：只跑名称包含过滤词的步骤（1~2 个用例）
GMP_TOUR_REAL_CLICKS=1 GMP_TOUR_STEP_FILTER=<步骤名片段> \
  GMP_SCREENSHOT_DIR=/tmp/gmp-ui-tour ./build/gmp_ise
```

## 测试策略（2026-09-09 用户确认）

1. **日常修改**：每次代码修改后，只运行与本次修改直接相关的 1~2 个测试用例：
   - 优先用 `GMP_TOUR_STEP_FILTER` 定向运行对应巡览步骤；
   - 涉及数据合同/schema 时运行 `ctest`。
2. **全量巡检**（85 步真实点击巡览 + CTest）只在以下时机执行：
   - `git commit` 之前；
   - 用户明确要求全量验证时。
3. 全量巡览基线只增不减：新增用例在既有基线上递增，不得删减既有断言换取通过。

## 巡览步骤过滤速查

| 场景 | GMP_TOUR_STEP_FILTER 值 |
|---|---|
| 工具栏场景（S1~S6） | `GMP_TOUR_TOOLBAR_SCENARIOS=1` + `s1`…`s6` |
| 最大化展开 | `GMP_TOUR_MAXIMIZE_ONLY=1` 或 `maximize` |
| 工作窗合同 | `i04_work_window_contracts` |
| 作业监控 | `remote_job_monitor` |
| 拉伸成孔 | `sketch_nested` |
| 回放工具组 | `playback_controls` |
