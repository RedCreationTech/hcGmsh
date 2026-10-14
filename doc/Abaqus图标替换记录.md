# Abaqus 图标替换记录

> 记录日期：2026-10-14
> 目的：将项目中与 Abaqus/CAE 功能对应的 Font Awesome 图标替换为 Abaqus 原版图标，形成"本项目功能 → Abaqus 图标"的对照档案，方便后续查阅、增补或回退。

## 1. 来源与机制

- **图标来源**：`/Users/a123/Desktop/test/moose/abaqus_resources/icons_png/`（Abaqus/CAE 2021 安装目录原始 PNG，共 446 个；含义对照见同目录 `Abaqus图标对照表.md`）。
- **入库位置**：`resources/icons/abaqus/`（共 59 个 PNG，保持 Abaqus 原始文件名 `icoR_*.png` 便于溯源），已注册进 `resources/icons.qrc`（前缀 `/`，资源路径 `:/icons/abaqus/<文件名>`）。
- **替换机制**：`src/IconFactory.cpp` 新增 `pngOverrides()` 键→PNG 映射表；`gmp::icons::get(key)` **优先**命中覆盖层返回 `QIcon(":/icons/abaqus/...")`，未命中的键回退到原 Font Awesome 字形映射（原 `mapping()` 保持不变，作为对照与回退基线）。
- **未改动**：`gmp::icons` 的对外接口（`get` / `toolbar` / `Size`）完全不变，所有调用方（MainWindow、SketchPanel、MoosePanel、GmshPanel、StageLeftToolbar 等 12 个文件）零改动。

### 如何回退单个图标

从 `pngOverrides()` 中删除对应行即可，该键自动回退到原 FA 字形（下表"原 FA 字形"列即为回退后的外观）。

### 如何新增替换

1. 从 `abaqus_resources/icons_png/` 拷贝目标 PNG 到 `resources/icons/abaqus/`；
2. 在 `resources/icons.qrc` 增加一行 `<file>icons/abaqus/xxx.png</file>`；
3. 在 `src/IconFactory.cpp` 的 `pngOverrides()` 增加键→文件名条目；
4. 在本文件第 2 节表格中补一行记录。

## 2. 替换对照总表

"原 FA 字形"是替换前 `mapping()` 中的 Font Awesome 7 Free 字形名（也是删除覆盖条目后的回退外观）。

### 2.1 项目 / 文件

| 项目键 | 原 FA 字形 | 新 Abaqus 文件 | CAE 含义 |
|---|---|---|---|
| `new_project` | file-circle-plus | `icoR_fileNew.png` | 新建模型数据库 |
| `open_project` | folder-open | `icoR_fileOpen.png` | 打开模型数据库 |
| `save` | floppy-disk | `icoR_fileSave.png` | 保存模型数据库 |
| `rename` | pen | `icoR_edit.png` | 编辑所选对象 |
| `remove` / `trash` | trash-can | `icoR_delete.png` | 删除 |
| `refresh` / `reload` / `rescan` | arrows-rotate | `icoR_fileReload.png` | 重新载入 |
| `open_root` | folder-tree | `icoR_modelSmall.png` | 模型数据库（树根） |

### 2.2 草图

| 项目键 | 原 FA 字形 | 新 Abaqus 文件 | CAE 含义 |
|---|---|---|---|
| `sketch_select` | arrow-pointer | `icoR_select.png` | 选择（箭头指针） |
| `sketch_move` | arrows-up-down-left-right | `icoR_viewPan.png` | 平移视图（移动语义） |
| `sketch_line` | slash | `icoR_partWirePoint.png` | 创建线：点到点 |
| `sketch_circle` | circle | `icoR_dtmAxisThreePointCircle.png` | 三点圆 |
| `sketch_rectangle` | square | `icoR_dragRectangle.png` | 矩形拖拽框 |
| `sketch_delete` | eraser | `icoR_delete.png` | 删除 |

### 2.3 通用编辑

| 项目键 | 原 FA 字形 | 新 Abaqus 文件 | CAE 含义 |
|---|---|---|---|
| `undo` | rotate-left | `icoR_undo.png` | 撤销 |
| `redo` / `retry` | rotate-right | `icoR_redo.png` | 重做 |
| `select_all` | check-double | `icoR_addAll.png` | 全部添加 |
| `cancel` / `clear_x` | xmark | `icoR_closeX.png` | 关闭/取消（×） |

### 2.4 部件 / 特征 / 装配

| 项目键 | 原 FA 字形 | 新 Abaqus 文件 | CAE 含义 |
|---|---|---|---|
| `new_part` | cube | `icoR_part.png` | 创建部件 |
| `feat_extrude` | cubes | `icoR_partSolidExtrude.png` | 创建实体：拉伸 |
| `feat_revolve` | arrows-rotate | `icoR_partSolidRevolve.png` | 创建实体：旋转 |
| `feat_sweep` | route | `icoR_partSolidSweep.png` | 创建实体：扫掠 |
| `feat_loft` | layer-group | `icoR_partSolidLoft.png` | 创建实体：放样 |
| `create_instance` | copy | `icoR_partInstance.png` | 创建实例 |
| `build_assembly` | puzzle-piece | `icoR_partInstance.png` | 装配（实例） |
| `bool_fuse` | object-group | `icoR_instanceMergeCut.png` | 实例合并/切削（合并语义） |
| `translate` | up-down-left-right | `icoR_instanceTranslate.png` | 实例平移 |
| `scale` | up-right-and-down-left-from-center | `icoR_animScaleFactor.png` | 比例缩放 |

### 2.5 网格

| 项目键 | 原 FA 字形 | 新 Abaqus 文件 | CAE 含义 |
|---|---|---|---|
| `generate_mesh` | gears | `icoR_meshControls.png` | 网格控制（划分） |
| `open_gmsh_panel` / `insert_mesh_block` / `generate_2d` | table-cells | `icoR_meshControls.png` | 同上（网格入口组） |

### 2.6 材料 / 属性 / 截面

| 项目键 | 原 FA 字形 | 新 Abaqus 文件 | CAE 含义 |
|---|---|---|---|
| `new_material` | flask | `icoR_material.png` | 材料容器 |
| `new_cdp_material` | atom | `icoR_materialAdd.png` | 材料编辑器添加（混凝土损伤材料） |
| `new_section` | crop-simple | `icoR_section.png` | 截面容器 |

### 2.7 分析步 / 相互作用 / 载荷 / BC

| 项目键 | 原 FA 字形 | 新 Abaqus 文件 | CAE 含义 |
|---|---|---|---|
| `add_static_step` / `add_transient_step` | square-plus / clock | `icoR_step.png` | 分析步 |
| `add_interaction` | link | `icoR_interaction.png` | 相互作用容器 |
| `add_surface_contact` | handshake | `icoR_contactPairFind.png` | 查找接触对 |
| `add_generic_load` | weight-hanging | `icoR_load.png` | 载荷容器 |
| `open_bc_root` / `insert_bcs_from_groups` | flag | `icoR_boundaryCondition.png` | 边界条件容器 |
| `add_thermal_source` | fire | `icoR_predefinedField.png` | 预定义场（含温度场） |

### 2.8 结果 / 作业 / 回放

| 项目键 | 原 FA 字形 | 新 Abaqus 文件 | CAE 含义 |
|---|---|---|---|
| `run` / `play` | play | `icoR_framePlay.png` | 帧播放 |
| `run_job` | person-running | `icoR_job.png` | 分析作业 |
| `submit_job` | paper-plane | `icoR_job.png` | 提交作业 |
| `stop` | stop | `icoR_stop.png` | 停止 |
| `pause` | pause | `icoR_framePause.png` | 帧暂停 |
| `step_prev` | backward-step | `icoR_framePrevious.png` | 上一帧 |
| `step_next` / `latest` | forward-step | `icoR_frameNext.png` | 下一帧 |
| `import_result_file` / `import_exodus` | file-import | `icoR_fileOpenBlue.png` | 打开（蓝色变体，结果导入） |
| `preview_merge` | code-merge | `icoR_swap.png` | 交换两项（模板合并预览） |

### 2.9 舞台左侧工具条（视图）

| 项目键 | 原 FA 字形 | 新 Abaqus 文件 | CAE 含义 |
|---|---|---|---|
| `zoom` | magnifying-glass-plus | `icoR_viewZoom.png` | 缩放视图 |
| `fit` | expand | `icoR_viewFit.png` | 适应窗口 |
| `front` | view_front（自定义绘制器） | `icoR_viewFront.png` | 前视图 |
| `right` | view_right（自定义绘制器） | `icoR_viewRight.png` | 右视图 |
| `top` | view_top（自定义绘制器） | `icoR_viewTop.png` | 俯视图 |
| `iso` | cube | `icoR_viewIso.png` | 等轴测视图 |

> 注：`front`/`right`/`top` 原来用 `CubeFacePainter` 自定义绘制器（`init()` 中 `g_awesome->give(...)` 仍保留，删除覆盖条目后自动恢复该绘制器）。

### 2.10 模型树根节点

| 项目键 | 原 FA 字形 | 新 Abaqus 文件 | CAE 含义 |
|---|---|---|---|
| `tree_parts` | cube | `icoR_partSmall.png` | 部件容器 |
| `tree_features` | cubes | `icoR_featureSmall.png` | 特征节点 |
| `tree_datums` | anchor | `icoR_createCsys.png` | 创建坐标系（基准） |
| `tree_materials` | flask | `icoR_material.png` | 材料容器 |
| `tree_sections` | crop-simple | `icoR_section.png` | 截面容器 |
| `tree_assembly` | puzzle-piece | `icoR_partInstance.png` | 装配容器 |
| `tree_physics` | atom | `icoR_analyticalField.png` | 场（物理/分析场）容器 |
| `tree_steps` | list-ol | `icoR_step.png` | 分析步容器 |
| `tree_bc` | flag | `icoR_boundaryCondition.png` | 边界条件容器 |
| `tree_loads` | weight-hanging | `icoR_load.png` | 载荷容器 |
| `tree_outputs` / `tree_input_cases` | file-export | `icoR_fieldOutput.png` | 场输出请求 |
| `tree_mesh` | table-cells | `icoR_meshSmall.png` | 网格部件 |
| `tree_jobs` | briefcase | `icoR_jobSmall.png` | 作业容器 |
| `tree_interactions` | link | `icoR_interaction.png` | 相互作用容器 |
| `tree_constraints` | lock | `icoR_constraint.png` | 约束容器 |
| `tree_selections` | crosshairs | `icoR_selectInsideCrossing.png` | 内部/交叉选择（选择集） |

### 2.11 模型树状态列

| 项目键 | 原 FA 字形 | 新 Abaqus 文件 | CAE 含义 |
|---|---|---|---|
| `status_failed` | stop | `icoR_error.png` | 错误状态 |
| `status_running` | play | `icoR_framePlay.png` | 运行（播放）状态 |
| `status_invalid` | arrows-rotate | `icoR_warning.png` | 警告/无效状态 |

### 2.12 树操作行 / 物理组

| 项目键 | 原 FA 字形 | 新 Abaqus 文件 | CAE 含义 |
|---|---|---|---|
| `tree_add` | plus | `icoR_add.png` | 添加 |
| `tree_rename` | pen | `icoR_edit.png` | 编辑 |
| `tree_remove` | trash-can | `icoR_delete.png` | 删除 |
| `pg_update` | file-pen | `icoR_edit.png` | 编辑（物理组更新） |
| `pg_delete` | trash-can | `icoR_delete.png` | 删除（物理组） |

## 3. 保留 Font Awesome 未替换的键

以下键在 Abaqus 图标库中没有语义对应（或对应图标未包含在已提取的 446 个 PNG 中），保留原 FA 字形：

| 分组 | 保留的键 |
|---|---|
| 项目/文件 | `save_as`（自定义 SaveAsPainter）、`duplicate`、`open_parts_root`、`open_selected`、`manual` |
| 草图 | `new_sketch`、`open_edit`、`edit_selected`、`sketch_arc` |
| 约束/标注 | `constraint_horizontal`、`constraint_vertical`、`constraint_parallel`、`constraint_perpendicular`、`constraint_coincident`、`dim_distance`、`dim_radius` |
| 通用编辑 | `finish`、`apply`、`clear`、`remove_selected`、`clear_selection` |
| 部件/装配 | `add_primitive`、`generate_3d`、`rotate`、`slice`、`apply_groups`、`add_param`、`pg_add`、`add_csv`、`remove_param`、`bool_cut`、`go_to_node`、`clear_model`、`open_geometry`、`result_folder` |
| 材料 | `open_property_editor` |
| 步/载荷 | `add_step_preset`、`add_surface_pressure`、`bool_intersect` |
| 物理组 | `clear_stage_filter`、`reset_filters`、`clear_filters` |
| 结果/输出 | `pick_output`、`load_selected`、`open_log`、`open_as_text`、`open_result`、`refresh_files`、`download`、`remote_artifact` |
| 模板/快照 | `apply_template`、`export_snapshot`、`screenshot`、`validate_workflow`、`check_input`、`verify_package` |
| 结果面板 | `apply_view`、`open_in_viewer`、`preview_mesh`、`apply_deform`、`import_task_dir`、`relocate`、`preview`、`new_compare`、`pin_preview`、`import_csv`、`export_csv`、`compare_curves`、`export_png`、`export_svg`、`copy_image`、`prev_page`、`next_page` |
| 项目 | `sync_model`、`cycle_display`、`pick` |
| 回放 | `focus_viewport`、`show_plot_preview`、`show_table_preview` |
| 舞台工具条 | `collapse`、`expand` |
| 模型树 | `tree_sketches`、`tree_functions`、`tree_variables`、`tree_results` |
| 树状态 | `status_success`、`status_generated`、`status_unconfigured` |
| 树操作 | `tree_duplicate` |

## 4. 资源文件清单（59 个）

入库 `resources/icons/abaqus/` 的文件（来源 `abaqus_resources/icons_png/`）：

`icoR_add.png`、`icoR_addAll.png`、`icoR_analyticalField.png`、`icoR_animScaleFactor.png`、`icoR_boundaryCondition.png`、`icoR_closeX.png`、`icoR_constraint.png`、`icoR_contactPairFind.png`、`icoR_createCsys.png`、`icoR_delete.png`、`icoR_dragRectangle.png`、`icoR_dtmAxisThreePointCircle.png`、`icoR_edit.png`、`icoR_error.png`、`icoR_featureSmall.png`、`icoR_fieldOutput.png`、`icoR_fileNew.png`、`icoR_fileOpen.png`、`icoR_fileOpenBlue.png`、`icoR_fileReload.png`、`icoR_fileSave.png`、`icoR_frameNext.png`、`icoR_framePause.png`、`icoR_framePlay.png`、`icoR_framePrevious.png`、`icoR_instanceMergeCut.png`、`icoR_instanceTranslate.png`、`icoR_interaction.png`、`icoR_job.png`、`icoR_jobSmall.png`、`icoR_load.png`、`icoR_material.png`、`icoR_materialAdd.png`、`icoR_meshControls.png`、`icoR_meshSmall.png`、`icoR_modelSmall.png`、`icoR_part.png`、`icoR_partInstance.png`、`icoR_partSmall.png`、`icoR_partSolidExtrude.png`、`icoR_partSolidLoft.png`、`icoR_partSolidRevolve.png`、`icoR_partSolidSweep.png`、`icoR_partWirePoint.png`、`icoR_predefinedField.png`、`icoR_redo.png`、`icoR_section.png`、`icoR_select.png`、`icoR_selectInsideCrossing.png`、`icoR_step.png`、`icoR_stop.png`、`icoR_swap.png`、`icoR_undo.png`、`icoR_viewFit.png`、`icoR_viewFront.png`、`icoR_viewIso.png`、`icoR_viewPan.png`、`icoR_viewRight.png`、`icoR_viewTop.png`、`icoR_viewZoom.png`、`icoR_warning.png`

> 尺寸说明：绝大多数为 128px 母版（Qt 按目标尺寸平滑缩放，工具栏/树节点渲染 16–20px）；树节点专用 `*Small.png` 为 16px；`icoR_stop.png` 24px；`icoR_closeX.png` 12px。

## 5. 变更文件与验证

**变更文件**：
- 新增 `resources/icons/abaqus/`（59 个 PNG）
- 修改 `resources/icons.qrc`（注册 59 个资源）
- 修改 `src/IconFactory.cpp`（新增 `pngOverrides()` + `get()` 优先命中逻辑）

**验证**：
- `cmake --build build -j4` 通过（AUTORCC 重新生成 `qrc_icons.cpp`，RCC 编译无缺失文件）。
- 定向巡览 `GMP_TOUR_STEP_FILTER=s1`（真实点击工具栏场景，覆盖新工具栏图标渲染路径）通过。
