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
