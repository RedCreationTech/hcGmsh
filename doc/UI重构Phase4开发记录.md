# GMP-ISE UI 重构 Phase 4 开发记录

> 起始：2026-09-10。方案与任务口径：`doc/UI重构Phase4方案设计.md`、`doc/UI重构开发任务清单.md`（W-00～W-07）。
> 自动回归基线：89 步真实点击巡览 + CTest `1/1`（本阶段起始时 85 步，经 W-00（+2）、W-03a/W-02b（+2）递增）。

## 1. W-00 接线 Phase 0 合同层（2026-09-10）

- **W-00a 活动应用档案 UI**：工作上下文条新增“应用”选择器（`workContextAppProfile`），接入既有的 `ApplicationProfileRegistry`；prototype/unsupported 标注；选中写入 `application_profile_`（id/version/status/support_level/mapping_version/solver_program）并同步 `unit_contract_`；状态栏永久标签“应用：\<id\>”；新建项目默认选中生产档案（不标脏）；旧项目无字段保持“未选择”可补选、不阻断。生成入口本切片仅轻提示未硬阻断。`DamSafetyApp-opt.json` 补充 CDP 物理声明（extra.physics_action / stress_update_materials / aux_property_naming）。
- **W-00b PG 清单生产者**：GmshPanel 新增 `mesh_manifest(QVariantMap)` 信号（物理组 名称/维度/tag/实体数/单元数 + 网格摘要 + 质量 minSICN + SHA-256）；MainWindow 消费填充 `mesh_snapshot_`，Mesh 节点写入双语 summary、physical_group_names、sha256；无物理组不阻断生成但节点标“不完整”。持久化复用 ProjectSchema 既有序列化。
- **W-00c 快照 v2 与提交接线**：`on_export_snapshot` 切到 `export_job_snapshot_v2`（缺档案/缺 PG 清单/引用文件缺失均可读拒绝，不写半成品）；导出前把 `.i` 绝对引用归一化为包内 basename；`case-<时间戳>` 版本子目录。SimClient solver 名改读快照 manifest 的 `application_profile.solver_program`（v1 旧快照回落缺省 + 日志），提交清单新增 solver_program/profile_id/profile_version/mapping_version。MainWindow 经 `push_context_to_moose_panel()` 在创建/新建/打开/保存/档案切换/网格清单更新六处注入档案、单位合同、PG 清单、项目路径。
- **W-00d mapping registry 消费**：`MainWindow::mapping_registry_` 按活动档案声明加载 mapping-v1；启动/切换档案/打开项目三处重载；失败状态栏可读报错不崩溃。供 W-03/W-04 消费。
- 巡览 +2：`app_profile_selector_contract`、`mesh_manifest_summary`（依赖 i04_geo fixture，单独跑会缺 fixture）。CTest 新增 `test_submission_manifest`。
- 验证：87 步全量巡览零失败，CTest `1/1`。

## 2. W-03a CDP 材料表单 + W-02b .e 显式导入（2026-09-10）

- **W-03a**：材料模块新增 "New CDP Material"（v01 默认值预设）；PropertyEditor 为 type=AbaqusCDP 提供 14 个专用字段（MPa 显示的弹性模量实时换算 SI 存储，比例因子取 unit_contract 并记入 params.unit_factor_stress；4 个 CSV 文件选择行）。生成扩展 `build_materials_block`：CDP 子项产出 v01 式三对象（elasticity/stress/cdp_stress_update），inelastic_models 自动连线，CSV 写 basename，未指派时 `block = ''` + 警告；非 CDP 材料走原路径不受影响。CSV 来源通道：`collect_material_file_sources()` 经 `MoosePanel::set_extra_file_sources` 注入，快照导出并入 file_sources。mapping-v1.json 补充两个材料对象 schema。
- **W-02b**：网格菜单/工具组/快捷按钮新增 "Import Exodus Mesh..."（`importExodusMeshAction`，巡览可用 `GMP_TOUR_EXODUS_IMPORT` 覆盖路径）；`import_exodus_mesh()` 登记 Mesh 节点（role=input_mesh、source=exodus_import、Generated）→ 舞台载入 → set_mesh_path 联动；`VtkViewer::read_exodus_side_set_names` 独立 reader 提取侧/节点集名（v01 网格 top/bottom 以 node set 表达），写入 params.boundary_names 并喂入 BC 表单组 chips；失败不阻断仅警告。结果导入路径未动，输入/结果角色隔离。
- 新增 fixture：`tests/fixtures/cdp-v01/`（v01 网格 .e + 4 张 CSV，复制自已验证基线）。
- 巡览 +2：`cdp_material_form_contract`（14 字段存在性、SI 存储、MPa 回显、三件套生成、无绝对路径泄漏）、`exodus_import_contract`（role/status/舞台/boundary 名）。
- 验证：定向巡览两步骤通过，CTest `1/1`。

## 3. 待办与已知边界

- W-03a 的 `block = ''` 待 W-01b Section 指派落地后闭合；`mesh_snapshot_.mesh_path` 绝对路径与 validate() 相对路径约束留给 W-06。
- W-02c（FileMeshGenerator 升级）未做，导入 .e 后 set_mesh_path 仍注入旧 FileMesh 写法。
- 档案切换的兼容性报告（只读提示级）建议并入 W-03/W-05。
- 人工验收：`doc/UI重构Phase4人工验收清单.md` v1（TEST-P4-W00-01～06、W03-01/02、W02-01/02、GEN-01/02）。

## 4. 新建项目对话框（2026-09-10，用户反馈）

- 用户指出：工具栏“新建项目”无弹窗、不要求输入项目名称、也不体现/允许修改项目目录，不符合工程软件习惯。
- 修复：`action_new_` 处理器重构——交互模式下：(1) 当前项目有未保存修改时先提示 保存/不保存/取消；(2) 弹出“新建项目”对话框（项目名称 + 存储目录 + 浏览，默认目录取上次项目目录 QSettings `ui/last_project_dir`）；(3) 确认后按 `<目录>/<名称>.gmp.yaml` 落盘保存（文件名特殊字符安全化、同名覆盖二次确认、目录不存在告警），标题栏/状态栏/最近项目同步显示真实路径。
- 巡览兼容：`GMP_SCREENSHOT_DIR` 模式下保持原静默直建行为（清空逻辑提取为 `create_fresh_project` lambda，两条路径共用），89 步基线不受影响（无步骤触发 action_new_）。
- 验收清单新增 TEST-P4-GEN-03；定向回归 app_profile_selector_contract、l04_top_context_1280 通过。

## 5. 快照导出被绝对路径误拒修复（2026-09-10，用户验收发现）

- 现象：TEST-P4-W00-05 点“导出任务快照”无目录对话框，日志报 “physical group manifest is missing or invalid”。
- 根因：W-00b 生成的清单按合同本应相对路径，但生产端记录的是绝对 mesh_path（§3 已列为 W-06 待办）；而 W-00c 在 MoosePanel 前置检查与 export_job_snapshot_v2 内部（MooseSnapshot.cpp:341）都用 `is_valid()` 全量校验，绝对路径必然误判，导出 100% 被拒。
- 修复：导出前置检查改为“就绪子集”（物理组非空 + SHA-256 格式 + 网格维度 1~3）；组装 cfg 时把清单 mesh_path 重写为包内 basename（网格以 basename 复制入快照，满足 v2 相对路径约束；项目内记录的清单仍保留绝对路径）。
- UX：拒绝原因此前只写操作日志（用户未察觉）；现交互模式下同步弹 QMessageBox 明示，巡览模式（GMP_SCREENSHOT_DIR）不弹窗避免阻塞自动化。
- 验证：构建通过，CTest `1/1`，remote_job_registration 定向巡览通过。

## 6. 提交清单与服务端严格 schema 冲突修复（2026-09-10，用户验收发现）

- 现象：远程提交失败 `invalid_manifest: Additional properties are not allowed ('mapping_version','profile_id','profile_version','solver_program')`。
- 根因：W-00c 给提交清单新增 solver_program/profile_* 四个键时假设“服务端可忽略未知字段”，但 LIMS Facade 按严格 schema 校验（additionalProperties=false），直接拒绝。
- 修复：提交清单恢复为服务端合同允许的 7 键（project_id/case_name/input_file/input_sha256/mesh_files/extra_files/command）；solver 程序名本就走 command 字段（W-00c 从活动档案读取的改进保留），档案溯源信息留在快照自带 manifest.json。test_submission_manifest 改为断言“仅 7 键 + command 含档案 solver”。
- 另注：用户把服务器指向 192.168.0.138 连接超时，实为另一台设备；本机 LIMS 在 127.0.0.1:8200。
- 验证：构建、CTest `1/1`、remote_job_registration 定向巡览通过。

## 7. 提交清单文件条目 role 字段裁剪（2026-09-10，用户验收发现）

- 现象：修复 7 键后再次提交仍失败 `Additional properties are not allowed ('role' was unexpected)`。
- 根因：服务端对 mesh_files/extra_files 的条目也按严格 schema 校验（只允许 name/sha256）；v2 快照条目带 role 溯源字段，build_submission_manifest 原样透传导致被拒。
- 修复：提交时文件条目裁剪为 {name, sha256}；role 留在快照 manifest.json 不进提交报文。单测同步断言条目仅两键且无 role。
- 验证：构建、CTest `1/1` 通过。

## 8. 服务端预检发现内置模板 hit 语法错误（2026-09-10，用户验收发现）

- 现象：提交失败 `failure check_input_failed: --check-input 预检未通过`。链路排查：客户端 → 本地 Facade（127.0.0.1:8200，structlab-lims api/app.py）→ C06 计算代理（192.168.0.138:8357，经 C06_AGENT_BASE_URL 配置）；代理上真实执行 `DamSafetyApp-opt --check-input`，报 `diffusion.i:16.2-16.3: syntax error, unexpected invalid token`。
- 根因：两个内置演示模板（template_generated_mesh/template_file_mesh，MoosePanel.cpp）的 `[./v]` 变量子块后用 `[]` 关闭，导致 `[Variables]` 多出一个闭合符——模板级语法错误，此前从未被真实求解器验证过。服务端预检忠实暴露了它。
- 修复：两处模板改为 `[../]` 正确闭合；用户已导出的快照含旧错误 .i，需重新应用模板生成 .i 并重新导出快照后再提交。
- 拓扑备注：远程提交的正确链路是 客户端 → 本地 Facade:8200 → C06 代理（.138:8357）；客户端永远只填本地 Facade 地址。
- 验证：构建、operation_log_smoke 定向巡览通过。

## 9. 首轮人工验收通过（2026-09-10）

- `doc/UI重构Phase4人工验收清单.md` v2：TEST-P4-W00-01～06、W03-01/02、W02-01/02、GEN-01～03 全部通过。
- 端到端证据：快照 case-20260910-171821 经本地 Facade → C06 代理（192.168.0.138:8357）提交成功；job_20260910_171821_hsrg0x 计算成功（21 时间步 u/v 场 + CSV 历史量，solve.log 与 CSV 数值互证）；diffusion_out.e 在应用内载入着色验证（M-P4-4 场景）。
- 验收中修复的缺陷见 §5～§8（导出误拒、提交 schema ×2、模板语法）。
- 任务清单状态回填：W-00 完成；W-02b/W-03a 完成，其余切片待实施。
- 已知边界：本轮用 GeneratedMesh 演示模板打通链路；“几何 → 命名物理组 → 真实 BC/材料绑定 → 装配提交”的真实前处理流是后续 W-01/W-02/W-03 切片的目标。

## 10. 第二轮：W-02a/W-02c/W-01b/W-01c/W-03b～W-03e（2026-09-10）

- **W-02a 组名校验**（GmshPanel）：Add/Update 前置校验——空名、无实体、维度不一致、同维度重名全部可读拒绝且先校验后执行无半状态；自更新豁免。
- **W-02c FileMeshGenerator**：inject_mesh_block 生成 `[Mesh/file] type = FileMeshGenerator` 子块；旧式顶层 [Mesh] 自动升级替换不并存；normalize_snapshot_refs 兼容。
- **W-01b Section 指派**：Sections 表单 Material 下拉 + Physical Volumes 体组 chips（复用 volume_groups 通道）；resolve_assigned_block(material) 供 CDP 材料/W-03b 复用，block 不再为 ''；material 引用悬空标失效（显示层自愈）。
- **W-01c Selections 生产路径**：右键“New Selection from Physical Group...”（维度+组名对话框），tag 未知时按名称从 mesh_snapshot_ 解析过滤视口；round-trip 通用序列化覆盖。
- **W-03c BC/函数**：Functions 支持 PiecewiseLinear（x/y 数据对 + 个数校验）；BC 支持 FunctionDirichletBC（function 下拉引用 Functions 节点，互斥键过滤）；variable 可编辑下拉。
- **W-03e Step 映射**：Steps 表单四组 20+ 字段对齐 v01；build_executioner_block 重写（[TimeStepper] 子块、[Preconditioning/smp]、petsc 引号、多 Step“仅取第一个”警告、参数未变重复 sync 幂等）。
- **W-03b Physics action**：Physics 根真实创建（Add Physics），block 自动取 Section 指派；生成 [GlobalParams] displacements + [Physics/SolidMechanics/<action>/<名>]（action 候选由档案 extra 驱动）。
- **W-03d 输出套餐**：Outputs 表单勾选式——场输出 8 变量（AuxVariables+MaterialRealAux，cdp_* 命名约定）、历史套餐（NodalSum/AverageNodalVariableValue/ElementExtremeValue）、Times（TimeIntervalTimes+sync_only）；未勾套餐保持旧行为。
- 巡览 91→95：section_assignment/selection_from_group/bc_function_dirichlet/step_executioner/physics_action/outputs_package 六个自包含合同（2A 两个 + 2B 四个）。修复两处步骤级问题（DeferredDelete 冲刷）与一个真实 bug（[Preconditioning/ 幂等锚串）。
- 已知遗留：MoosePanel::upsert_block 旧通道在块已存在时残留游离 []（7 块老通道，demo 首次 sync 即产生）；参数变更后的跨块去重——均归 W-04 装配器统一处理。
- 验收清单更新：TEST-P4-W02-03/04、W01-01/02、W03-03～06、E2E-01（端到端真实几何全流程，第二轮准出）。

## 11. 窗体高度变化统一处理：单层滚动区模式（2026-09-10，用户反馈）

- 起因：属性表单勾选“高级参数”后快捷参数区被压扁重叠。排查确认根因是“内容最低高度之和超过窗口高度时 Qt 布局放弃最小值约束、整体压扁”，尺寸策略无法解决总缺口；首轮“自动撑高窗口”方案被用户否定（窗口忽大忽小），确立统一模式：**窗口保持记忆尺寸，内容包单层 QScrollArea（widgetResizable + NoFrame），页内滚动，禁止外层整窗滚动与双层嵌套滚动**。
- 属性表单：参数页包 paramsTabScroll；“模板说明”（只读模板描述预览，自动内容）限高 72px 消除空间黑洞；i01 断言从“禁任何 QScrollArea”调整为“禁外层整窗滚动、允许页内单层滚动”。
- 统一推广：模块工作窗 7 个节点页（栈页外层包裹，兼容 p2 合同）、Jobs 页（jobManagerPageScroll，job_table_ 最小高 120）、MoosePanel 算例设置/执行配置页、VtkViewer 五个控制页（make_tab 一处覆盖）、Results 页与对比窗；输入文件页/Plot/Table 页（主体自带滚动吃拉伸）与 GmshPanel（既有）跳过不重复包。
- 验证：p2_workspace_content_layout、i04_work_window_contracts、workspace_Job、i04_results_compare_windows、workspace_Visualization、i01_property_form 定向通过；截图目检无压扁/滚动异常。
- 已知边界：Jobs 页页级滚动内存在既有 detail_scroll（QScrollArea 套 QScrollArea，既有设计保留）；低窗口下组合页会比以前更早出现页级滚动条（预期行为）。

## 12. 草图→部件→网格通道衔接修复（2026-09-11，用户提问发现）

- 用户问：几何能否不经导入、由“草图→部件→装配”生成并在网格模块使用。查证：部件特征（拉伸/旋转/放样/扫掠）经 OCC 构建几何后确实导入 Gmsh 模型（feature 日志 volume(s)），拾取/物理组可读；但 GmshPanel 的 on_generate 守卫（`!model_loaded_ || 示例盒勾选`）对面板外的模型来源无感知——用户在网格工作窗点“生成网格”会 gmsh::clear 清空部件几何、改画示例盒。
- 修复：GmshPanel 新增 `note_external_model_loaded(label)`（model_loaded_=true、取消示例盒勾选、几何路径标签更新、刷新实体/物理组/场列表）；MainWindow handle_feature_result 在特征成功后调用（标签 "part: <名> (<特征>)"）。
- 装配（多部件定位）属 W-01d 待实施，单部件流程无障碍。
- 验证：sketch_nested_loop_hole_extrude、part_feature_updates_selected_part 定向巡览通过。

## 13. 网格生成守卫加固：按模型实际空态判定（2026-09-11，用户复现发现）

- 现象：§12 修复后用户复现仍失败——拉伸后直接“生成 3D 网格”仍产出 12 节点退化网格（geometry= 空、2D/3D 微秒级空转、Mesh 节点不完整）。
- 分析：on_generate 的原条件 `!model_loaded_ || 示例盒勾选` 本质是“面板状态驱动清空”——示例盒勾选状态可被设置持久化/用户操作重新武装，`model_loaded_` 也只认面板自己的导入通道；任何一条为真都会 `gmsh::clear()` 吞掉部件几何。探针（草图→拉伸→generate_mesh）证明 §12 的同步在受控路径有效（1343 节点），但防线本身不可靠。
- 修复：守卫改为**只看当前模型的实际空态**（`getEntities(-1)` 为空才允许示例盒兜底；空模型且未勾选示例盒则抛出“没有几何”可读错误）；模型非空时一律 `mesh::clear()` 后按当前模型剖分——部件几何在任何面板状态下都不会再被清空。
- 验证：探针路径 1343 节点（部件模型剖分正常）；sketch_nested_loop_hole_extrude、i04_geo_import_feedback 定向巡览通过；探针已清理。

## 14. 2026-09-11-001 生成 3D 网格长时间无响应与舞台空白修复（2026-09-11）

- **现象**：约 `108.74 × 69.29 × 10` 的拉伸部件使用默认网格尺寸 `0.2` 生成 3D 网格时，界面长时间无响应；此前一次生成还出现 12 节点/12 单元、中央舞台近似空白、质量统计报 `Unknown element type ...: 1`。
- **根因**：按 `6 × 体积 / lc³` 粗估，该参数约需 5651 万个四面体，远超交互式生成规模；旧实现又在 GUI 线程同步调用 `gmsh::model::mesh::generate()`，因此窗口不能重绘/取消。另有两个叠加问题：VtkViewer 打开 `.msh` 会破坏共享的 current OCC model；质量统计把点/线低维单元也送入 `minSICN`。
- **修复方案**：
  - 生成前按最高维实体的 OCC 真实长度/面积/体积（不可用时回退包围盒）估算单元数；默认交互上限 200 万，超限时中英文提示预计规模、最低建议尺寸并立即恢复按钮，不启动剖分。测试/高级部署可用 `GMP_MESH_MAX_ESTIMATED_ELEMENTS` 调整阈值。
  - 当前模型先序列化为 `.geo_unrolled`，由独立 Gmsh CLI 子进程执行剖分；进度窗实时显示 1D/2D/3D/优化阶段、Gmsh 百分比和已用时，支持取消，主窗口保持重绘响应。
  - 子进程成功后先验证非空，再通过 `QSaveFile` 原子提交到用户输出路径；失败或取消不覆盖既有有效网格。
  - 前序流程留有物理组、而新 Part 体尚未入组时，仅在子进程 `.geo_unrolled` 中为未覆盖的最高维实体补充 `unassigned_<dim>d` 物理组，避免有效体被误写为零节点网格；不修改应用内 OCC 模型，也不用 `Mesh.SaveAll` 损失 MSH 2.2 物理组合同。
  - 文件网格使用进程内唯一文件基名回读；回读阶段重建干净的 Gmsh 会话，结束后再次重建会话并从未附加 worker 指令的 `.geo_unrolled` 快照恢复原 OCC 几何，隔离长时间 GUI 会话积累的文件模型与解析状态。部件特征模型名采用“时间戳 + 单调序号”保证进程内唯一，并通过 `FeatureResult` 回传；网格面板同时登记外部模型身份，在正式生成前和结果回读后均显式恢复。已知当前模型的即时预览直接读取内存，不再重复打开文件。
  - 质量统计只查询最高维单元，消除低维 element type 1 的误报。
- **验证结果**：构建通过；`sketch_nested_loop_hole_extrude` 定向真实点击回归通过（真实部件生成 3D 网格、舞台可见、OCC 模型可二次生成、超限拒绝不落盘且控件恢复）；`i04_geo_import_feedback` 定向真实点击回归通过。
- **状态**：已修复，待人工确认正常模式下的进度窗与取消手感。

## 15. W-02d 优先结构化四边形/砖形网格（2026-09-11，用户反馈）

- **目标**：正式网格在可用时优先输出四边形（2D）/六面体（3D），同时保留四面体路径和明确的能力边界。
- **分析**：第一版使用 `Mesh.SubdivisionAlgorithm=2`把四面体逐个细分为六面体。文件拓扑虽是 Hexahedron，单元却继承四面体方向，呈蜂窝状且扭曲，不等于用户所指的沿几何方向排列的砖形/立方体网格。带孔拉伸体也不能未经分区就严格映射成全结构化六面体。
- **实现**：
  - 开关更名为“优先结构化四边形/砖形立方体网格”，tooltip 明示适用条件、立方体成立条件和回退行为；保持原 `recombine` 持久化键兼容旧项目。
  - 禁用 `SubdivisionAlgorithm`伪全六面体路径。2D 仅对四边界面、3D 仅对所有体均为六个四边界面且对边分段数兼容的块体生成 `Transfinite Curve/Surface/Volume + Recombine Surface` 约束。
  - 带孔、多于六个面或面边界非四边的模型自动回退为三角形/四面体，日志给出不可映射的具体拓扑原因和“先几何分区”建议。
  - 规模预检分别按结构化砖形和单纯形回退路径估算，不再计入已删除的 3/4 倍细分放大。
  - 生成日志记录最高维单元组成，例如 `Hexahedron 8=16`；manifest 的 `element_type` 从最高维单元选主类型，避免被边/面单元干扰。
- **显示修正**：舞台打开网格时默认筛选最高维单元，再抽取其外表面，避免 0D/1D/2D 边界单元与 3D 体单元重叠造成“中空/剖开”的视觉误解；用户仍可在 Dim 中切回 All 或单独查看面/边。
- **验证结果**：带孔凹轮廓拉伸体回退为 Tetrahedron；六面长方体进入 Transfinite 路径并输出 Hexahedron。两条路径均保留舞台、质量统计、OCC 模型恢复与超限拒绝合同。
- **边界**：“结构化砖形”表示八节点、方向对齐的映射单元；只有正交长方体且各向分段与尺寸匹配时才能保证数学意义上的“正立方体”。曲面、圆孔附近若要边界共形，必须先做多块几何分区，单元不可能同时保持完全正方体。

### 15.1 W-02e 单元拓扑策略显式化与扫掠边界（2026-09-11，用户反馈）

- **问题**：复选框“优先”允许结构化条件不满足时回退，用户选择与最终单元类型之间不是强合同；带孔草图沿 Z 拉伸后通常形成 7 个或更多边界面，最终 BREP 被当作普通体处理，不能走简单六面体映射。
- **实现**：
  - 复选框改为“单元拓扑策略”下拉框：`自动（条件满足时结构化）`、`三角形/四面体（通用）`、`结构化四边形/六面体（严格）`。
  - 自动模式保留可观察的回退；通用模式不尝试结构化；严格模式不允许回退，拓扑或对边分段不兼容时在剖分前失败，不写出或覆盖网格文件，并给出分区/扫掠建议。
  - 严格模式下禁用 2D/3D Delaunay 等算法选择，因为实际算法由 Transfinite 映射约束决定。
  - 项目新增 `mesh_topology_mode` 持久化字段；读取旧项目时将 `recombine=true/false` 分别迁移为自动/通用模式，并继续写出旧字段用于向后兼容。
- **拉伸与扫掠关系**：沿 Z 拉伸是采用扫掠六面体的有利前提，但还不充分。要对带孔拉伸体生成全六面体，需要保留“源草图面 → 全四边形源面网格 → 拉伸方向与层数”的特征链，再沿 Z 复制源面网格。当前正式网格入口只接收到最终 BREP，特征来源未进入网格合同，因此本切片仅提供映射严格模式；专用带孔扫掠需后续把草图/拉伸元数据传入网格模块并增加源面四边形分区。
- **正立方体约束**：扫掠只能保证柱状六面体拓扑。圆孔等曲边附近为保持边界共形，单元必然变形；全域严格正立方体只能用于边界与笛卡尔网格对齐的正交实体，或采用非共形体素/切割单元方法。
