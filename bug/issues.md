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
