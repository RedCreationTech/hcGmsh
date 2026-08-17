# Bug / Issue 记录

## 2026-08-04 — ComboPopupFix 事件过滤器无限递归导致启动后段错误（SIGSEGV）

**状态**：已修复（src/ComboPopupFix.cpp，方案 A+B）
**环境**：macOS 26.5.1 / Qt 6.11.1 (Homebrew) / x86_64

### 问题现象

应用启动后正常使用一段时间（约 15 分钟），随后主线程崩溃：

- 终端输出 `segmentation fault ./build/gmp_ise`
- macOS 崩溃报告：`EXC_BAD_ACCESS (SIGSEGV)`，`KERN_PROTECTION_FAILURE`，命中主线程 **Stack Guard** 页（栈溢出）
- 崩溃栈显示递归深度 **43,680 层**，循环体为：

  ```
  ComboPopupFixer::eventFilter (ComboPopupFix.cpp:47)
    -> QComboBox::view()
    -> QComboBoxPrivate::viewContainer()   // 创建弹窗容器
    -> QComboBoxPrivateContainer() -> setParent -> 投递事件
    -> ComboPopupFixer::eventFilter        // 再次进入，无限循环
  ```

- 崩溃栈底为 `~QComboBox -> deleteChildren -> setParent_helper`：即崩溃发生在某个 ComboBox 被析构（PropertyEditor 动态重建表单）的过程中。

### 根因分析

`QComboBox::view()` **不是纯 getter**：视图不存在时会**惰性创建**弹窗容器（`QComboBoxPrivateContainer` + 内部 view）。

`ComboPopupFixer::eventFilter`（原 `src/ComboPopupFix.cpp:47`）对**每一个**收到的事件都无条件调用 `combo_->view()`。当事件发生在 ComboBox 析构或容器创建的路径上时：

1. 析构子控件 → `setParent_helper` 投递事件 → 进入 eventFilter；
2. `combo_->view()` 发现容器不存在 → **当场重新创建一个弹窗容器**；
3. 创建过程又触发 `setParent` / `ChildAdded` 等事件 → 重入 eventFilter；
4. 此时私有容器指针尚未赋值，`view()` 再次创建 → 无限递归直至栈耗尽。

教训：**事件过滤器内绝不能调用任何可能惰性创建控件的 API**。

### 解决方案（A + B）

- **A. 缓存视图指针，杜绝过滤器内的惰性创建**：`ComboPopupFixer` 构造时（此时 `install_combo_popup_fix` 已完成 `setView(new QListView)`，视图必然存在）将视图缓存为 `QPointer<QAbstractItemView> view_`；`eventFilter` / `ensure_filter` / `apply_popup_geometry` 中所有 `combo_->view()` 一律改为读缓存，过滤器内再无任何"创建"动作，递归链从源头断开。
- **B. 可重入保护 + 析构期事件放行**：过滤器处理段加 `ReentryGuard`（`in_filter_` 标志）；`ChildRemoved` / `DeferredDelete` / `Destroy` 事件直接放行不做任何 view 访问；`combo_` / `view_` 均改为 `QPointer`，延时回调（`QTimer::singleShot`）中自动判活。

### 验证

- `cmake --build build -j8` 编译链接通过；
- 冒烟测试：启动后运行 40 秒无崩溃，正常终止（原崩溃需约 15 分钟交互触发，结构性根因已消除，后续交互使用中继续观察）。

### 遗留（与本 bug 无关的同期日志）

- `Gmsh has not been initialized` × 6：GmshPanel 在 `gmsh::initialize()` 之前执行查询，独立问题，待单独修复；
- `SFMono-Regular` 字体缺失提示：macOS 平台噪音，无害。

---

## 2026-08-04 — PropertyEditor 悬垂指针：切换模型树节点后 setPlainText 崩溃（SIGSEGV）

**状态**：已修复（src/PropertyEditor.cpp）
**环境**：macOS 26.5.1 / Qt 6.11.1 (Homebrew) / x86_64

### 问题现象

应用启动后操作数分钟（期间切换过模型树节点类型），主线程崩溃：

- `EXC_BAD_ACCESS (SIGSEGV)`，`KERN_INVALID_ADDRESS at 0x3fc`（近零地址解引用，典型的释放后使用）；
- 崩溃栈仅两帧：

  ```
  QPlainTextEdit::setPlainText(QString const&)
  gmp::PropertyEditor::build_form_for_kind (PropertyEditor.cpp:1260)
  ```

### 根因分析

`PropertyEditor::clear_form()` 在重建表单时，会把 `form_layout_` 中的行控件（含 `template_tabs_` 及其子控件 `template_preview_`）`deleteLater()` 销毁，但只把 `template_combo_` 置空，**没有同步置空 `template_tabs_` / `template_preview_`**。

下一次 `build_form_for_kind()` 执行时：

1. 第 1238 行 `if (!template_tabs_)` 判断失效——指针悬垂但非空，跳过重新创建；
2. 第 1260 行 `template_preview_->setPlainText(...)` 在已释放的对象上调用 → 崩溃。

触发路径：模型树切换不同类型节点 → PropertyEditor 重建表单 → 第二次及以后重建必踩。

### 解决方案

- `clear_form()` 中在 `deleteLater` 之前将 `template_tabs_`、`template_preview_` 一并置空（与 `template_combo_` 对齐），确保下次重建走 `new` 分支；
- 第 1260 行加 `if (template_preview_)` 防御性判空。

### 验证

- `cmake --build build -j8` 编译链接通过；
- 冒烟测试运行 30 秒无崩溃。说明：该崩溃需"切换节点类型触发表单二次重建"的交互路径，自动化冒烟无法完全复现，但根因经代码审查是确定性的，修复直接消除了悬垂窗口。

### 关联经验

与同日 `ComboPopupFix` 递归崩溃同源：**凡是被 `clear_form()` / 布局重建销毁的成员控件指针，必须在销毁点同步置空**（或统一改用 `QPointer`）。建议后续将 `PropertyEditor` 中所有缓存的控件裸指针逐步迁移为 `QPointer`，从机制上杜绝此类问题。

---

## 2026-08-17 — Exodus 可视化：部分块单元变量丢失、云图不随变量/时间刷新

**状态**：已修复（src/VtkViewer.cpp、include/gmp/VtkViewer.h）
**环境**：macOS / Qt 6 / VTK 9.6；测试数据 BlackBear 官方 `asr_confined_strip_out.e`（2 个 element block、6 时间步、`len_name=256`）

### 问题现象

1. 打开含多个 element block 的 Exodus 结果后，变量列表缺 `ASR_ex`、`ASR_strain_*`、`stress_xy` 等单元变量；
2. 切换任意标量并拖动时间到最后一步，云图仍是均匀色，"自动范围"显示 `0.000000/0.000000`，色标固定 0–1——而此时 `stress_yy` 实际约为 -2.0e7 Pa。

### 根因分析

- **变量缺项（真数据丢失）**：原管线 `reader → vtkCompositeDataGeometryFilter` 直接扁平化多块输出，该过滤器**只保留所有块共有的数组**。MOOSE 只在 block 1 定义 ASR 材料，`ASR_ex` 与 6 个 `ASR_strain_*` 仅存于 block 1，扁平化后整体消失（探针实证：block1 16 个单元数组 → 扁平化后 10 个）。
- **"stress_x"/"disp_" 并非截断**：`vtkExodusIIReader` 内部 `GlomArrayNames` 把仅尾部 `_x/_y/_z` 不同的标量合并为多组分向量（`stress_xx/xy` → 3 组分 `stress_x`，`disp_x/y` → `disp_`），是 reader 固有行为，数据未丢。
- **云图不刷新（三因叠加）**：
  1. `on_apply_range` 的 auto 分支为空操作，mapper 标量范围永远停在默认 0–1；
  2. `refresh_time_only` 设 `UPDATE_TIME_STEP` 后 `reader_->Update(); geom_->Update()`，reader 输出对象被原地复用、mtime 不变，geom 判定未过期不重新执行，拿不到新时间步数据；
  3. 时间切换后不重算当前数组范围，"自动范围"一直显示加载时 t=0 的全 0 数据。

### 解决方案

- 新增 `vtkPadPartialBlockArrays`（`vtkMultiBlockDataSetAlgorithm` 子类，VtkViewer.cpp 匿名命名空间）：收集所有叶子块点/单元数组并集，浅拷贝后为缺失块补 0 填充；管线改为 `reader → block_pad_ → geom_`；
- `update_pipeline` / `refresh_time_only`：`reader_->Update()` 后 `block_pad_->Modified()`，强制下游按新时间步重执行；
- `refresh_time_only`：geom 更新后回调 `on_array_changed(currentIndex())` 重算当前数组范围并刷新映射；
- `on_apply_range` auto 分支补 `mapper_->SetScalarRange(...)`。

### 验证

- `cmake --build build -j8` 编译链接通过（gmp_ise）；
- 无 GUI 探针（/tmp/gmp_probe/probe4，管线实现与应用逐行一致）：t=0 与 t=500000 均得 16 个单元数组（含 ASR_ex、ASR_strain_*）+ 5 个点数组；t=0 时 stress_yy 范围 [0,0]，t=500000 时 [-1.99973e+07, -0.216256]，与官方数据吻合；
- 未启动 GUI，交互路径（拖滑块 → 刷新 → 列表重建）为代码级+探针级验证，建议后续人工 GUI 复测一次。

### 遗留

- block 2 上 ASR_* 为 0 填充（该块本未定义），如需区分"未定义/真 0"需加掩码数组；
- VTK 9.6 下无法还原 17 个原始标量名（reader 向量合并），如 UI 必须展示原始分量名需另加组分拆分层；
- 变形（warp）开启时，时间切换后范围读取可能滞后一次渲染。

---

## 2026-08-17（第二轮）— Exodus 可视化交互路径：色标刻度滞后、时间重置、刻度裁剪

**状态**：已修复（src/VtkViewer.cpp），GUI 远程操控复测通过
**环境**：macOS / Qt 6 / VTK 9.6；测试数据 `asr_confined_out.e`（test_full，920 单元、2 block、6 时间步）

### 问题现象（第一轮修复后的 GUI 复测发现）

1. 拖到 t=500000 时 stress_yy 云图为均匀橙色（约量程 7% 处），与真值 -2e7（深蓝）不符；
2. 切换变量后色标标题更新但刻度保持上一个变量的量程；
3. 色标刻度文本被裁剪（"-2.00e+" 指数位不可读）；
4. 文件 watcher 重读或点击表格预览等操作后时间被重置回 t=0。

### 根因与方案

- **刻度滞后/橙色中间态**：`on_array_changed` 先 `apply_lookup_table()`（内部 Render 用旧范围布局刻度）后 `on_apply_range()`；且 LUT Range 依赖渲染期 painter 回写，时序错位。修复：对调两调用顺序，`on_apply_range` 在 `SetScalarRange` 后显式 `lut_->SetRange(...)`；合并 auto/手动重复分支。
- **刻度裁剪**：`style_scalar_bar` 条宽 0.08 + 默认 `%.2e` 格式超宽。修复：`SetLabelFormat("%.3g")`、宽度 0.08→0.10。
- **时间重置**：`update_time_steps_from_reader` 中 `setRange` 在 `blockSignals` 外，钳位时真实发射 `valueChanged(0)`；watcher 赶上文件重写读到空 steps 时走 `setRange(0,0)` 把滑块钳到 0 且不可恢复。修复：空 steps 容错（恢复原有 time_steps_ 直接返回），`setRange/setValue` 全部纳入信号屏蔽。
- **橙色根因补充**：离屏渲染探针 probe9 完整复刻交互序列后渲染结果正确（block1 深蓝、block2 红），橙色判定为修复前的时序中间态帧；上述时序修复消除了其来源。

### 验证

- `cmake --build build -j8` 通过；
- probe9：t=500000 时 mapper/LUT range 均为 [-1.99989e7, 1.37497]，渲染前即正确；
- **GUI 远程操控复测（辅助功能 + 截屏 + 像素比对）**：打开 test_full → 选 Cell: stress_yy → 拖到 t=500000，混凝土主体深蓝 (0,0,248)、钢套边缘红 (248,0,0)，色标刻度 1.37/-2e+07 完整可读；t=0 表格预览 920 元组全 0。与官方数据一致。

### 遗留

- 相机"斜视"在变量/时间切换代码路径上排查为阴性（无 ResetCamera 调用），疑为测试期合成交互触发 `apply_view_preset`/`set_2d_mode`，复测未再现；
- 若 orange 复现，需记录 auto_range 勾选状态与 range 读数（QSettings 手动范围残留是唯一未排除状态源）。
