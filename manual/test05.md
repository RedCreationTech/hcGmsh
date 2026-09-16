# GMP-ISE UI 重构 Phase 5 人工测试说明

> 版本：2026-09-16 v17
> 执行范围：Phase 5 / G1～G3
> 任务依据：`doc/UI重构下一阶段任务清单.md` 的 `TASK-P5-01`～`TASK-P5-05`
> 需求依据：`REQ-011`～`REQ-017`
> 前置结论：Phase 4 / G0 已完成人工准出

本文沿用 `manual/test04.md` 的逐条操作方式。建议每个闸门都使用独立的验收项目，
不要直接修改生产项目或 Phase 4 的验收证据项目。

## 一、执行顺序与当前状态

Phase 5 必须按以下顺序执行，不应在前一闸门未通过时提前关闭后一闸门：

1. **G1**：Assembly 实例与变换 → Load/Contact 真实映射 → M-01 线弹性接触算例及结果闭环。
2. **G2**：Reference Point 与 Coupling Constraint 真实映射及传力验证。
3. **G3**：CDP 材料来源/单位/快照追溯及认可基准对比。

当前实施状态：

| 用例 | 当前可执行性 | 说明 |
|---|---|---|
| TEST-P5-G1-01 | 可执行 | 两个独立 Part 已作为 G1-02 前置实际使用；单项结论按执行记录回填 |
| TEST-P5-G1-02 | 已人工通过 | 2026-09-15：两个 Part 独立实例化、平移/旋转及恢复、装配预览均通过 |
| TEST-P5-G1-03 | 已人工通过 | 2026-09-15：可见性、顺序持久化、非法缩放与悬空 Part 引用校验均通过 |
| TEST-P5-G1-04 | 已人工通过 | 2026-09-15：保存重开、实例属性恢复及上游修改后的过期传播均通过 |
| TEST-P5-G1-05 | 已人工通过 | 2026-09-15：装配网格、四个实例 Physical Groups、摘要和 SHA-256 均通过 |
| TEST-P5-G1-06～07 | 待人工验收回填 | Pressure/Contact 真实映射已生成，负向验证与接触量仍需按用例留证 |
| TEST-P5-G1-08 | 部分完成／环境待测 | 生成报告和内部工作流校验已有证据；目标应用 `--check-input` 未执行 |
| TEST-P5-G1-09 | 未测试／延期 | 当前无法连接远端计算环境，成功终态和结果制品待后续远端任务验证 |
| TEST-P5-G2-01～05 | 实现完成后执行 | 依赖 Reference Point/Coupling 能力 |
| TEST-P5-G3-01～05 | 实现完成后执行 | 依赖认可基准、比较指标和容差冻结 |

“实现完成后执行”不等于失败或通过。对应 UI、应用档案或映射尚未提供时，应记录为
“阻塞：功能未交付”，不得用手工修改 `.i`、普通 `DirichletBC` 或占位对象绕过。

## 二、环境、项目与证据准备

### 2.1 构建与启动

1. 在仓库根目录构建：

   ```bash
   cmake --build build -j4
   ```

2. 启动应用：

   ```bash
   ./build/gmp_ise
   ```

3. 新建项目并另存为 `phase5-g1-assembly-contact.gmp.yaml`。
4. 确认活动应用为 `DamSafetyApp-opt`，状态栏没有 prototype 提示。
5. 打开“作业工作窗 → MOOSE 设置 → 算例设置”，确认新项目没有继承其他项目的
   输入文件、工作目录或网格路径。
6. 保存后确认项目自己的工作目录位于项目目录下的：

   ```text
   .work/case/phase5-g1-assembly-contact/
   ```

   点击“保存”后，主工作区右上角应出现“项目保存成功”和当前文件名；点击“项目另存为”
   后应出现“项目另存为成功”。两种提示约 3.5 秒后自动消失，不要求额外确认；两个工具栏
   图标也应可区分（另存为图标带蓝色加号徽标）。

7. 不要选择仓库共享 `out/`、历史 `case-<时间戳>` 目录或 `/tmp/gmp-ui-tour...` 作为
   当前 Mesh 输出。

### 2.2 建议的稳定命名

后续检查会直接比对名称。建议整轮保持以下命名：

| 对象 | 建议名称 |
|---|---|
| 混凝土草图 | `sketch_concrete` |
| 钢板草图 | `sketch_plate` |
| 混凝土 Part | `part_concrete` |
| 钢板 Part | `part_plate` |
| 混凝土实例 | `instance_concrete` |
| 钢板实例 | `instance_plate` |
| 混凝土体组 | `instance_concrete` |
| 钢板体组 | `instance_plate` |
| 混凝土接触面 | `contact_concrete` |
| 钢板接触面 | `contact_plate` |
| 混凝土固定面 | `fixed_bottom` |
| 钢板加载面 | `load_top` |
| 接触对象 | `contact_plate_concrete` |
| 位移函数 | `loading_curve` |
| G1 Job | `job_phase5_g1` |

Assembly 自动生成的 `<实例名>_surface` 表示该实例的全部边界面。Contact、固定和加载
不能直接把整个边界当成单个作用面；应在装配完成后另外建立上表中的具体面组。

### 2.3 每个用例的证据

每个测试至少保留：

- 项目路径和 Git 提交号；
- 关键 UI 截图；
- 生成 `.msh`、`.i` 和生成报告的路径；
- “校验工作流”结果及 `--check-input` 日志；
- 快照 `manifest.json`；
- 远程 Job ID、终态、制品路径；
- 失败时的完整操作日志和缺陷编号。

## 三、G1：Assembly 实例、变换与装配网格

### 3.1 创建混凝土块与钢板 Part（TEST-P5-G1-01）

关联：`TASK-P5-01`、`REQ-011`、`REQ-012`。

#### 创建混凝土草图和 Part

1. 顶部“模块”切换到“草图”。
2. 点击“新建草图”。
3. 在 Sketch Editor 中选择“矩形”。
4. 在 XY 平面绘制一个闭合矩形。记录矩形的实际宽度和高度；本轮只要求它明显小于
   钢板轮廓，不强制使用某个工程尺寸。
5. 点击“完成编辑”。
6. 在模型树把草图重命名为 `sketch_concrete`。
7. 顶部“模块”切换到“部件”。
8. 点击“新建部件”，在草图选择框中选择 `sketch_concrete`。
9. 把新 Part 重命名为 `part_concrete`。
10. 在“部件特征”中选择“拉伸”。
11. 草图选择 `sketch_concrete`，填写正的拉伸距离；记录该值为 `H_concrete`。
12. 点击“沿 +Z 拉伸”。
13. 检查中央舞台出现一个三维实体，而不是空模型或仅有二维轮廓。
14. 检查 `part_concrete` 参数中存在可读取的 BREP 路径，文件实际存在。

#### 创建钢板草图和 Part

1. 回到“草图”，再次点击“新建草图”。
2. 绘制第二个闭合矩形，使其在 XY 方向明显大于混凝土矩形。
3. 完成编辑并重命名为 `sketch_plate`。
4. 回到“部件”，点击“新建部件”，选择 `sketch_plate`。
5. 把新 Part 重命名为 `part_plate`。
6. 在“部件特征 → 拉伸”中选择 `sketch_plate`。
7. 填写比 `H_concrete` 明显小的正拉伸距离，记录为 `H_plate`。
8. 点击“沿 +Z 拉伸”。
9. 检查舞台显示钢板三维实体，`part_plate` 也有独立 BREP 路径。
10. 在 Part 列表中往返选择两个 Part，确认形状和 BREP 来源不会互相覆盖。

通过标准：存在两个独立 Part；两者都是可重建的三维实体；混凝土块和钢板的 BREP
路径不同且文件可读；切换 Part 时不会丢失另一 Part。

#### Part、Feature 与 Assembly 的引用关系

当前实现中，Part 与 Feature 不要求一一对应。每次成功执行拉伸、旋转、放样或扫掠，都会
在 `Features` 根节点下追加一条建模历史，而不会覆盖或自动删除该 Part 的旧 Feature；因此
修改草图来源后对同一 Part 再次拉伸，出现“2 个 Part、3 个 Feature”符合设计预期。

引用链如下：

1. Feature 的 `part` 字段指向所属 Part，并保存本次操作的 `type`、来源草图、参数以及产出的
   BREP/网格路径。
2. Part 的 `feature` 字段只指向最近一次成功生成、当前生效的 Feature；Part 同时保存该
   Feature 最新产出的 `sketch`、`brep`、`mesh` 和 Gmsh 体标签。
3. Assembly 实例的 `part` 字段只引用 Part 名称。构建装配时，系统通过 Part 读取当前
   `brep` 作为实例几何来源，不会直接读取或叠加该 Part 的全部历史 Feature。
4. 后续装配网格及 Physical Groups 基于当前 Assembly 几何生成，因此使用的是各 Part 当前
   BREP，而不是 Feature 数量或旧 Feature 的产物。

本验收项目保存后的预期关系为：

```text
instance_concrete -> part_concrete -> feature_1 -> sketch_concrete / 当前 BREP
instance_plate    -> part_plate    -> feature_3 -> sketch_plate    / 当前 BREP
                                      feature_2 -> sketch_concrete / 旧历史，不参与当前装配
```

其中 `feature_2.part` 仍可记录为 `part_plate`，用于保留该 Part 的生成历史；只要
`part_plate.feature` 指向 `feature_3`，且 `part_plate.brep` 与 `feature_3.brep` 一致，旧的
`feature_2` 就不会改变装配结果。当前 Feature 模型属于“每次操作保留结果快照”的历史，
不是把所有 Feature 按顺序叠加重放的参数化特征链。

执行 G1-05 前检查：

- 不以 `Parts` 与 `Features` 数量相等作为通过条件；
- 分别打开两个 Part，确认摘要中的当前 Feature 和来源草图正确；
- 确认两个 Part 的当前 BREP 路径不同且文件存在；
- 确认 `instance_concrete.part = part_concrete`、`instance_plate.part = part_plate`；
- 若当前 Part 指向旧 Feature、旧草图或旧 BREP，应先重新生成并保存，不得继续生成装配网格。

删除关系说明：删除 Part 会一并移除所有 `feature.part` 指向它的历史 Feature，并使引用该
Part 的 Assembly 实例失效；删除当前生效的 Feature 会清除 Part 的当前 BREP/网格派生引用。
人工验收期间如无清理历史的明确需要，不应删除当前生效 Feature。

### 3.2 创建并定位两个 Assembly 实例（TEST-P5-G1-02，已通过）

1. 顶部“模块”切换到“装配”。
2. 点击“打开装配根节点”，确认模型树定位到 `Assembly`，而不是 `Parts`。
3. 点击“创建实例”。
4. 把新节点重命名为 `instance_concrete`。
5. 双击该节点，进入“参数”页。
6. 设置：
   - 部件：`part_concrete`
   - 平移 X/Y/Z：`0 / 0 / 0`
   - 旋转 X/Y/Z：`0 / 0 / 0`
   - 缩放 X/Y/Z：`1 / 1 / 1`
   - 可见：`true`
   - 顺序：`1`
7. 点击“确定”。
8. 再次点击“创建实例”，重命名为 `instance_plate`。
9. 打开参数并设置：
   - 部件：`part_plate`
   - 平移 X/Y：先填 `0 / 0`
   - 平移 Z：填写第 3.1 节记录的 `H_concrete`
   - 旋转 X/Y/Z：`0 / 0 / 0`
   - 缩放 X/Y/Z：`1 / 1 / 1`
   - 可见：`true`
   - 顺序：`2`
10. 点击“确定”。
11. 点击“构建装配”。
12. 检查 Mesh Workspace 自动置于前面，模型来源下拉显示 `assembly: active`。
13. 在舞台选择合适的轴测视图并适配窗口。
14. 确认钢板位于混凝土块顶部；如 XY 中心未对齐，只调整 `instance_plate` 的 X/Y
    平移，再次点击“构建装配”。
15. 至少临时把 `instance_plate` 的 Z 旋转改为一个容易识别的角度，例如 `30`，重建后
    确认舞台中的钢板方向发生变化；验证后恢复为 M-01 需要的角度并再次重建。

在点击“构建装配”前，再次双击两个实例核对“部件”字段：

- `instance_concrete` 必须引用 `part_concrete`；
- `instance_plate` 必须引用 `part_plate`。

连续创建实例时，系统会优先把尚未被引用的 Part 作为默认值；仍允许用户主动选择同一
Part 创建多个实例。切换 Part 后，部件特征区的主草图选择也必须自动跟随该 Part 保存的
来源草图，避免上一 Part 的草图选择被误用于当前 Part。

回归说明（2026-09-15）：曾出现 Gmsh 已显示 `2V`、日志显示
`Assembly built: 2 visible instance(s)`，但中央舞台仍停留在最后一个 Part 预览的问题。
修复后点击“构建装配”会生成项目工作目录内的轻量表面预览
`assembly_preview.msh`，立即替换旧 Part 画面并切换到轴测视图；若预览失败，会清空旧画面
并明确报错，不再用单 Part 画面伪装成装配结果。

通过标准：两个实例同时存在；平移和旋转实际改变 Gmsh 几何位置，不只是修改表单文本；
两实例可独立定位，模型来源为 `assembly: active`。

人工验证记录（2026-09-15）：用户确认 `instance_concrete → part_concrete`、
`instance_plate → part_plate`，两 Part 使用独立 BREP，长宽与 20/5 厚度在装配舞台中保持；
`instance_plate` 的 Z 平移和临时 30° 旋转实际生效，随后已恢复 M-01 所需角度并重建。
构建日志报告 `Assembly built: 2 visible instance(s)`，本用例关闭。验收过程中发现并修复
`2026-09-15-001`（装配预览未替换旧 Part）和 `2026-09-15-002`（选择器沿用首个
草图/Part）两个缺陷。

### 3.3 可见性、顺序与非法参数（TEST-P5-G1-03，已通过）

1. 打开 `instance_plate` 参数，将“可见”改为 `false`。
2. 点击“构建装配”。
3. 确认钢板从装配模型消失，混凝土块仍存在。
4. 将“可见”恢复为 `true`，重建后确认钢板只恢复一次，不出现重复实体。
5. 把两个实例的“顺序”分别改为 `2` 和 `1`，重建后确认装配仍成功且对象名称不变。
6. 保存并重开属性，确认顺序值保持。
7. 临时把任一缩放值改为 `0` 或负数并点击“确定”。
8. 确认表单拒绝提交，提示缩放必须大于 0；不得构建退化实体。
9. 恢复缩放为 `1`。
10. 临时把 Part 引用切换为空或不存在对象。
11. 确认校验提示 `part`/`part reference`，无法把实例标记为就绪。
12. 恢复正确 Part 引用和顺序 `1 / 2`，重新构建装配。

通过标准：隐藏实例不参与装配；恢复可见不会复制实体；顺序可保存；非法缩放和悬空 Part
引用被明确拒绝或标记失效。

人工验证记录（2026-09-15）：用户确认可见性切换与恢复无重复实体、实例顺序调整及保存
保持、非法缩放拒绝、空或不存在 Part 引用校验均符合预期；恢复正确 Part 引用、缩放与顺序
后装配可正常构建，本用例关闭。

### 3.4 保存重开与过期传播（TEST-P5-G1-04，已通过）

1. 保存 `phase5-g1-assembly-contact.gmp.yaml`。
2. 关闭项目并重新打开。
3. 进入“装配”，确认默认仍有 `instance_concrete` 和 `instance_plate`。
4. 分别打开两个实例，核对 Part 引用、平移、旋转、缩放、可见性和顺序。
5. 打开 Mesh Workspace，确认可恢复 `assembly: active`，两个实体位置与保存前一致。
6. 双击 `part_plate`，对其参数做一个可恢复的小改动并确定。
7. 检查 Assembly 实例、Mesh、输入案例和 Job 的既有派生结果被标记为“过期”或等价状态。
8. 点击“构建装配”，重新生成网格并同步输入。
9. 确认过期状态只在重新产生产物后恢复，不会继续使用修改前的网格或输入。
10. 恢复 `part_plate` 原参数并再次构建、保存。

通过标准：实例数据与装配模型保存重开一致；上游 Part 修改会使 Assembly 及下游产物
失效；重建后使用的是新几何。

人工验证记录（2026-09-15）：用户确认保存并重开后两个实例及其 Part 引用、变换、
可见性和顺序均正确恢复，`assembly: active` 装配位置与保存前一致；修改上游 Part 后，
Assembly 及下游派生产物正确进入过期状态，重建后使用新几何，恢复原参数并再次构建、
保存成功。本用例关闭。

## 四、G1：装配网格与 Physical Groups

### 4.1 生成装配网格（TEST-P5-G1-05，已通过）

1. 在模型树展开 `Mesh`，新建一个 Mesh 子节点并重命名为 `mesh_assembly_g1`。
2. 双击该节点打开 Mesh Workspace。
3. 在“模型”页确认几何来源为 `assembly: active`。
4. 在“网格”页确认输出文件位于项目自己的工作目录，例如：

   ```text
   <项目目录>/.work/case/phase5-g1-assembly-contact/mesh_assembly_g1.msh
   ```

5. 选择适合当前装配的全局尺寸和单元拓扑策略。
6. 点击“生成 3D 网格”。
7. 等待生成完成，确认舞台同时包含混凝土块和钢板网格。
8. 打开“分组与网格场 → 物理组”，确认至少存在：
   - `instance_concrete`，维度 3；
   - `instance_plate`，维度 3；
   - `instance_concrete_surface`，维度 2；
   - `instance_plate_surface`，维度 2。
9. 确认四个组都不是空组，两个体组对应不同实体。
10. 在主模型树单击 `mesh_assembly_g1`，再把顶部“模块”从“网格”切换到“属性”；进入
    “常规”页，确认“摘要”直接显示维度、节点数、单元数、四个 Physical Group 名称和
    非空 SHA-256。双击 Mesh 节点只会打开 Mesh Workspace，不是摘要入口。

通过标准：网格包含两个位于预期位置的实体；`.msh` 是项目自有文件；实例体组和边界组
完整写入网格清单。

人工验证记录（2026-09-15）：用户确认几何来源为 `assembly: active`，项目自有路径下的
3D 装配网格同时包含两个实例；`instance_concrete`、`instance_plate`、
`instance_concrete_surface`、`instance_plate_surface` 四个组均存在且非空。属性常规页显示
节点 175086、单元 196408（Hexahedron 8）、物理组 4、质量 `minSICN 1~1` 及完整
SHA-256，本用例关闭。

### 4.2 建立接触、固定和加载专用面组

> **开发状态（2026-09-16）**：开发与人工验证均已通过。物理组编辑器会把 Assembly
> 自定义组随项目保存；每个面同时记录所属实例、几何包围盒签名和原始 tag。项目重开并
> 重建 `assembly: active` 后，程序先限制到原所属实例，再按几何签名重绑定；原始 tag 只作
> 匹配提示，不作为持久身份。若实例几何已经变化且无法唯一匹配，程序拒绝恢复该组并写入
> 操作日志，不会把同号 tag 静默绑定到错误面。`assembly_instance_contract` 已覆盖四组非空
> 保存重开回归。

人工验证记录（2026-09-16）：项目打开时成功恢复四个自定义面组；人工把
`contact_concrete` 更新为成员 `2:12`、`contact_plate` 更新为成员 `2:5`，随后重新生成 3D
网格。日志与项目清单一致显示 `fixed_bottom`、`load_top`、`contact_concrete`、
`contact_plate` 均为二维、各含 1 个成员实体，网格单元数分别为 8544、27004、8544、
27004；两个接触组成员不同。网格生成成功（175086 节点，顶维 Hexahedron 8 为 152950，
`minSICN` 最小值 0.999962），最终项目保存成功，自定义组几何签名已写入项目文件，本节关闭。

1. 在“分组与网格场 → 物理组”把维度设为 `2`。
2. 点击实体“拾取”，确认选择器显示“实体 / 所属 Assembly 实例 / 几何范围”三列，且新建
   物理组时所有候选默认均未勾选；单击任意候选行，确认程序依据 Gmsh 曲面真实法向（包括
   旋转实例的斜面和侧面）把主舞台转到从该面外侧约 45° 观察的视角，完整装配降为灰色
   线框；点应显示为突出于背景的亮黄色球形标记，边应显示为有明显宽度的亮黄色管线，面和体应显示为不透明
   亮黄色表面。四种维度都必须能从灰色装配背景中清晰辨认。若进入拾取前舞台正过滤显示某个 Physical Group，
   点击“拾取”应自动取消该舞台过滤并恢复完整装配背景，同时保留当前组作为编辑目标。
3. 保持选择窗口打开，把鼠标移到它未遮挡的主舞台区域，依次拖动旋转、平移并滚轮缩放；
   确认舞台仍可交互，黄色候选面跟随视图变化且不会丢失。自动视角仅是初始辅助视角，用户
   可随时手工调整；再单击另一候选行时，程序才会按新面的外法向重新定位。关闭选择器后
   黄色临时高亮应自动清除，舞台恢复最高维的完整实体及正常实例着色，不得残留黄色高亮，
   也不得因二维 `contact/fixed/load` 边界组叠加而变成红绿拼色；编辑已有物理组时允许预勾选
   该组现有实体。也可在打开拾取前点击“取消舞台筛选”手工恢复同样的整体显示；该按钮不得
   清空当前编辑组。以上恢复要求对“确定”“取消”和窗口关闭三条退出路径均适用。
4. 编辑或删除已有组时，可直接单击下方统计表对应行；确认顶部“分组”从 `New` 自动切换到
   该组，名称和实体字段同步回显。统计表必须分别显示“组标识”和“成员实体”：例如物理组
   `2:9 contact_plate` 可以包含面实体 `2:5`，两者属于不同命名空间，不要求编号相等。修改
   实体后点击“更新所选”，确认“成员实体”列和绿色成功提示立即显示新值；组标识保持不变。
   新增、更新或删除后舞台应自动恢复完整模型，但当前组仍作为编辑目标保留。单元数只有在
   重新生成网格后才刷新；点击保存后变更才写入项目文件。点击“删除所选”应立即移除该行。
   若顶部仍显示“新建”，不得继续执行更新或删除。主动切换到“新建”时，名称和实体字段应
   初始化，统计表必须清除当前行，不得残留失焦后的灰色选择；选中已有组时，表格在有焦点
   和失焦状态下统一使用同一选择强调色。点击“刷新”后“新建”仍按当前语言显示。
5. 新建 `contact_concrete`：在选择器先点“清除”，选中“所属 Assembly 实例”为
   `instance_concrete`、Z 范围为接触高度（本例 `Z[20,20]`）的混凝土顶面。
6. 新建 `contact_plate`：选中所属实例为 `instance_plate`、Z 范围为同一接触高度（本例
   `Z[20,20]`）的钢板底面；通过所属实例列和黄色舞台高亮确认它不是上一步的同一面。
7. 新建 `fixed_bottom`：选中所属实例为 `instance_concrete`、Z 为混凝土最小值（本例约
   `Z[0,0]`）的底面。
8. 新建 `load_top`：选中所属实例为 `instance_plate`、Z 为钢板最大值（本例
   `Z[25,25]`）的顶面。
9. 检查四个面组实体集合均非空。
10. 确认 `contact_concrete` 与 `contact_plate` 不是同一个 Gmsh 实体 tag。
11. 重新生成 3D 网格。
12. 在物理组摘要中确认四个名称仍存在且维度均为 2、单元数均非零；点击“刷新”后单元数
    不得归零。
13. 保存并重开项目，再次检查组名称、实体集合和单元数；统计表应从项目保存的网格清单恢复
    各组单元数，不要求在 OCC 几何模型中重新生成网格。
14. 点击“刷新”，确认当前编辑组仍保留，但舞台不重新套用该组筛选；完整装配继续按最高维
    实例着色，不得从红蓝实例区分退化为单色。

通过标准：Contact、固定和加载均使用具体命名面组；不得用裸 tag，也不得用实例的整个
`_surface` 组代替单个作用面。

## 五、G1：Load 与 Interaction/Contact 真实映射

### 5.1 冻结活动应用支持矩阵（TEST-P5-G1-06）

> **开发状态（2026-09-16）**：`TASK-P5-02` 已实现。活动档案与 mapping v1 共同提供
> `Pressure` 和 `Contact`；新增定向合同 `contact_load_mapping_contract` 已验证表单候选、
> 参数校验、命名面组生成、来源报告和重复同步幂等性。以下步骤现可执行。

本轮冻结矩阵（DamSafetyApp-opt / mapping `1.0.0`）：

| 能力 | UI / MOOSE 类型 | 关键参数 | 生成位置 |
|---|---|---|---|
| 固定位移 | `DirichletBC` | `variable`、`boundary`、`value` | `[BCs]` |
| 函数位移 | `FunctionDirichletBC` | `variable`、`boundary`、`function` | `[BCs]` |
| 面压力 | `Pressure` | `variable`、`boundary`、`factor/function`、可选 `component` | `[BCs]` |
| 面—面接触 | `Contact` | `primary`、`secondary`、`model`、`formulation`；Coulomb 使用 `friction_coefficient >= 0` | `[Contact]` |

Contact 模型候选为 `frictionless / coulomb / glued`；算法候选为
`kinematic / penalty / augmented_lagrange / tangential_penalty / mortar`。本轮 M-01 使用
`model=coulomb`、`formulation=kinematic`、`friction_coefficient=0.15`。

1. 打开活动应用档案或应用兼容性说明。
2. 记录 DamSafetyApp 当前实际支持的：
   - 位移边界类型；
   - 面力/压力类型；
   - 面—面 Contact 类型；
   - 主面/从面参数名；
   - 摩擦参数名和取值范围；
   - 对应 MOOSE block 路径及 mapping 版本。
3. 切换到“相互作用”模块，点击“添加相互作用”。
4. 打开属性表单，确认类型下拉只列出活动档案声明支持的类型。
5. 若档案没有 Contact 能力，确认对象被标记为“不支持生成”，同步和提交被阻断。
6. 不得为了继续测试而选择 `Tie`、普通 BC 或手工 Custom Blocks 冒充 Contact。

通过标准：UI、校验器和生成器共用同一支持矩阵；无映射时明确阻断，有映射时可定位到
真实 MOOSE 类型。

### 5.2 创建面—面 Contact

> **人工回归记录（2026-09-16）**：主/从面下拉仅列二维 Physical Group、主从面相同的
> 冲突校验均已通过；通过高级参数把 `primary` 改为三维组 `instance_plate` 时曾可绕过
> 参数页、校验页和“确定”阻断，登记为 `2026-09-16-013`。现已在公共参数校验入口补齐
> 二维 Physical Group 成员校验，并纳入 `contact_load_mapping_contract`，等待本节第 7 步
> 人工复验后关闭。

1. 新建相互作用并重命名为 `contact_plate_concrete`。
2. 类型选择活动档案中已冻结的面—面 Contact 类型。
3. 主面选择 `contact_plate`，从面选择 `contact_concrete`；若映射合同规定相反顺序，
   按合同设置并在证据中注明。
4. 摩擦系数填写 `0.15`，其余参数使用已冻结的 M-01 测试基线。
5. 点击“确定”。
6. 打开“校验”页，确认两个面组存在、维度为 2、主从面不同。
7. 临时把主面改成 `instance_plate` 体组，确认维度错误被定位并阻断。
8. 临时把主从面设为同一个组，确认重复/冲突错误被定位。
9. 恢复正确配置。
10. 同步到 MOOSE 输入，确认出现档案声明的真实 Contact/Constraint/Interface 对象。
11. 确认没有把 Contact 静默写成 `DirichletBC`、`BodyForce` 或无映射的伪 block。

通过标准：Contact 引用稳定命名面组；类型和参数来自活动档案；错误配置被预检拦截；
生成对象能由来源报告追溯到 `contact_plate_concrete`。

### 5.3 固定、位移加载及 Load 映射（TEST-P5-G1-07）

1. 在模型树 `BC` 根节点下为 `fixed_bottom` 创建三个零位移约束。具体操作如下：
   1. 展开模型树中的 `BC`，右键 `BC` 根节点并选择 `Add BC`；在名称对话框中依次创建
      `disp_x`、`disp_y`、`disp_z` 三个对象。对象名用于生成 `[BCs]` 子块，本用例按这三个
      固定名称记录证据。
   2. 双击 `disp_x`，进入“参数”页。若准备使用“Fixed (Dirichlet 0)”模板，应先应用模板，
      再执行下面的边界指派；应用模板会恢复模板默认参数，可能覆盖此前的选择。
   3. 将“类型”设为 `DirichletBC`，“变量”设为 `disp_x`，“值”设为 `0`。
   4. 在页面顶部“可选边界分组”列表中单击 `fixed_bottom`，确认“待应用选择”只包含
      `fixed_bottom`，再点击“应用所选边界”。只有选中列表行还不算完成指派，必须点击该按钮。
   5. 确认只读字段“已指派边界”从默认占位值 `left` 变为 `fixed_bottom`。`left` 不是本项目
      Physical Group，不能保留。打开“校验”页确认当前对象无问题，然后点击“确定”。
   6. 对 `disp_y`、`disp_z` 重复上述操作，只把“变量”分别改为 `disp_y`、`disp_z`；类型、
      已指派边界和值保持相同。

   三个对象最终参数应为：

   | BC 对象名 | 类型 | 变量 | 已指派边界 | 值 |
   |---|---|---|---|---|
   | `disp_x` | `DirichletBC` | `disp_x` | `fixed_bottom` | `0` |
   | `disp_y` | `DirichletBC` | `disp_y` | `fixed_bottom` | `0` |
   | `disp_z` | `DirichletBC` | `disp_z` | `fixed_bottom` | `0` |

   完成后模型树应显示 `BC · 就绪 (3)`。此时整个 Workflow 仍可能因 Material、Section、
   Physics、Step、Load 或 Outputs 尚未配置而保持 blocked，这是分步建模期间的正常状态，
   不代表这三个 BC 失败。`disp_x/disp_y/disp_z` 是固体力学位移变量候选，本步骤无需再在
   `Variables` 根节点手工创建同名变量；后续 Physics 应启用 `add_variables=true`。

   同步 MOOSE 输入后，`[BCs]` 中应出现三个独立子块，逐项检查变量、边界和值，不要求参数
   行顺序完全一致：

   ```text
   [BCs]
     [disp_x]
       type = DirichletBC
       variable = disp_x
       boundary = fixed_bottom
       value = 0
     []
     [disp_y]
       type = DirichletBC
       variable = disp_y
       boundary = fixed_bottom
       value = 0
     []
     [disp_z]
       type = DirichletBC
       variable = disp_z
       boundary = fixed_bottom
       value = 0
     []
   []
   ```

   若生成文本中仍出现 `variable = u` 或 `boundary = left`，说明对应对象仍保留默认值，应返回
   属性弹窗修正后重新同步。
2. 在 `Functions` 下创建位移时程 `loading_curve`。具体操作如下：
   1. 展开模型树中的 `Functions`，右键根节点并选择“添加 Functions”，
      将新对象命名为 `loading_curve`。
   2. 双击 `loading_curve` 打开属性弹窗，进入“参数”页，将“类型”选为
      `PiecewiseLinear`。切换类型后应显示 `X Values` 和 `Y Values`，不再显示
      `Expression`。
   3. 本轮 M-01 先使用两个点的单调压缩位移基线：

      | 字段 | 填写值 | 含义 |
      |---|---|---|
      | `X Values` | `0 1` | 0～1 s |
      | `Y Values` | `0 -2.5e-5` | Z 向从 0 单调加载到 -0.025 mm |

      当前装配中钢板位于混凝土上方，因此使用负 Z 压向接触面。若后续重建模型
      导致局部 Z 方向反转，应把符号改成“朝向接触面”的方向，并在证据中记录；
      不得同时修改时间点数或未记录地改变幅值。
   4. 打开“校验”页并点击“刷新”，确认没有 `x/y count mismatch`。`X Values`
      与 `Y Values` 必须一一对应，本例均为 2 个数。然后点击“确定”。
   5. 同步 MOOSE 输入后，`[Functions]` 中应出现：

      ```text
      [loading_curve]
        type = PiecewiseLinear
        x = '0 1'
        y = '0 -2.5e-5'
      []
      ```

      不应在该子块内残留 `expression = ...`。生成器可能把 `-2.5e-5` 规范化为
      等价的科学计数法，只要数值与符号不变即可。

3. 在 `BC` 下为钢板加载面创建函数位移边界。具体操作如下：
   1. 右键 `BC` 根节点，选择 `Add BC`，将新对象命名为 `load_disp_z`。
   2. 打开 `load_disp_z` 的“参数”页。可以在“模板”中选择
      `Prescribed Function` 并点击“应用模板”；若不使用模板，则直接填写后续字段。
      应先应用模板再选边界，避免模板恢复默认值。
   3. 将字段设置为：

      | 字段 | 填写/选择值 |
      |---|---|
      | 类型 | `FunctionDirichletBC` |
      | 变量 | `disp_z` |
      | 函数 | `loading_curve` |

      选中 `FunctionDirichletBC` 后应显示“函数”下拉，并隐藏常量“值”字段。
   4. 在页面顶部“可选边界分组”中单击 `load_top`，确认“待应用选择”中只有
      `load_top`，然后点击“应用所选边界”。
   5. 确认只读字段“已指派边界”为 `load_top`，不是 `left`、`contact_plate`、
      `instance_plate_surface` 或数字 tag。打开“校验”页确认当前对象无问题，
      再点击“确定”。

4. 确认位移加载的真实映射。
   1. 点击“同步 MOOSE 输入”，打开生成的 `.i`。在 `[BCs]` 中应出现：

      ```text
      [load_disp_z]
        type = FunctionDirichletBC
        variable = disp_z
        boundary = load_top
        function = loading_curve
      []
      ```

   2. 该子块不得出现 `value = ...`；`FunctionDirichletBC` 已由函数给出整个时程，
      同时保留常量值会造成语义冲突。
   3. 在属性弹窗“校验”页及全局“校验工作流”中，不应出现
      `load_disp_z` 的 `function`、`boundary` 或 `variable` 引用错误。此时整个
      Workflow 仍可能因 Material、Section、Physics、Step 或 Outputs 未完成而 blocked；
      只要错误不指向 `loading_curve`/`load_disp_z`，不判定本步失败。

5. 使用独立测试副本验证 `Loads → Pressure` 映射，不要直接污染 M-01 主项目。
   > 若浮动属性窗的模板列表中没有“面压力”（英文界面为 `Surface Pressure`），且“类型”下拉也没有
   > `Pressure`，先停止本步、重新构建并重启应用；不要用 `BodyForce`、手输不可选的类型
   > 或编辑 `.i` 代替。此现象曾由浮动窗未继承活动档案的类型候选造成。修复后可继续编辑
   > 已创建的 `pressure_mapping_check`，无需再建一个同名对象。

   1. 先保存当前主项目 `phase5-g1-assembly-contact.gmp.yaml`。然后使用
      “项目另存为”创建 `phase5-g1-pressure-mapping-check.gmp.yaml`，确认窗口标题已切换到
      副本后再继续。
   2. 在副本中删除 `BC/load_disp_z`。`load_top + disp_z` 在主项目中已由位移边界
      约束，若不先删除就再加 Pressure，应被判定为同面同自由度冲突，无法单独
      验证 Pressure 映射。三个 `fixed_bottom` 零位移 BC 保留。
   3. 在 `Functions` 下新建 `pressure_curve`，类型选择 `PiecewiseLinear`，填写：
      - `X Values = 0 1`
      - `Y Values = 0 1`

      这是从 0 到 1 的无量纲幅值函数，实际压力由下面的 `factor` 给出。
   4. 若 `Loads` 下尚无 `pressure_mapping_check`，右键 `Loads` 根节点并选择“添加 Loads”，
      将新对象命名为 `pressure_mapping_check`；若该对象已存在且仍为默认 `BodyForce`，
      直接双击它继续，不要重复创建。打开“参数”页，先确认“类型”下拉中有 `Pressure`，
      再从“模板”中选择“面压力”（英文界面为 `Surface Pressure`）并点击“应用模板”。应用后确认“类型”变为
      `Pressure`，页面顶部可选列表从体组 `instance_plate`、`instance_concrete` 切换为
      包含 `load_top` 的二维边界组；直接把类型从 `BodyForce` 改为 `Pressure` 时也应
      自动将原先的 `u` 改为 `disp_z`。随后填写如下字段：

      | 字段 | 填写/选择值 | 说明 |
      |---|---|---|
      | 类型 | `Pressure` | DamSafetyApp-opt / mapping 1.0.0 的真实映射类型 |
      | 变量 | `disp_z` | 下拉应列出 `disp_x/disp_y/disp_z`，选 `disp_z`；若只看到 `u`，本项不通过 |
      | 压力系数 | `-250000` | Pa；本步只验证映射，不用此副本做 M-01 准出求解 |
      | 函数 | `pressure_curve` | 0～1 幅值 |
      | 分量 | `2` | Z 分量；`0/1/2` 分别为 X/Y/Z |
      | 使用变形后网格 | `true` | 保持自动合同基线 |

      应先应用模板再指派边界，避免模板覆盖后续选择。
      此时 `Variables` 根节点为空属正常：位移变量由后续 Physics 的
      `add_variables=true` 在生成输入时创建，不能因此把 Pressure 变量留作 `u`。
      对已保存为 `variable=u` 的测试对象，重新应用“面压力”模板并核对变量已改为
      `disp_z`；未修正时参数页校验应阻止确认。

6. 把 `pressure_mapping_check` 的作用面指派为命名二维组 `load_top`。
   1. 在页面顶部“可选边界分组”中选中 `load_top`，确认“待应用选择”只显示
      `load_top`，再点击“应用所选边界”。
   2. 确认只读的“已指派边界”为 `load_top`。列表行的选中高亮不等于完成指派，
      必须点击“应用所选边界”后再检查该只读字段。
   3. 不得填写 `2:6`、单独数字 tag，也不得使用整个钢板边界组
      `instance_plate_surface`。本用例要验证的是“命名 Physical Group → MOOSE boundary”
      映射。
   4. 打开“校验”页，确认当前 Load 没有缺少 `variable`、`boundary` 或
      `factor or function` 等问题，再点击“确定”。

7. 同步 MOOSE 输入，检查 Pressure 的生成位置和来源追溯。
   1. 点击“同步 MOOSE 输入”，在 `.i` 的 `[BCs]` 中查找
      `[pressure_mapping_check]`，预期子块为：

      ```text
      [pressure_mapping_check]
        type = Pressure
        variable = disp_z
        boundary = load_top
        factor = -250000
        function = pressure_curve
        component = 2
        use_displaced_mesh = true
      []
      ```

      参数行顺序可不同，但名称与值必须一致。
   2. 确认 `[pressure_mapping_check]` 出现在 `[BCs]`，而不是 `[Kernels]`。当前
      DamSafetyApp-opt / mapping `1.0.0` 把 `Loads/Pressure` 真实映射到 MOOSE
      `BCs/Pressure`，不应将其统一硬编码为 `BodyForce`。
   3. 打开生成报告，确认能找到来自模型树 `Loads/pressure_mapping_check` 的记录，
      并包含 `Physical Group=load_top`。若报告显示为 Kernel、`BodyForce` 或无来源占位块，
      本步失败。

8. 在 Pressure 测试副本中验证三维体组不能冒充二维作用面。
   1. 重新打开 `pressure_mapping_check`，勾选“高级参数”。在参数表中找到
      `boundary`，临时把值从 `load_top` 改为三维体组 `instance_plate`。
      这是负向验证，不是正常配置方式。
   2. “参数”页应立即提示 `boundary must reference an existing 2D Physical Group`；
      打开“校验”页并刷新，应看到同一错误。点击“确定”应保持弹窗打开，
      不得把 `boundary = instance_plate` 写回模型树。点击“取消”退出后重新打开，
      “已指派边界”仍须为 `load_top`。
   3. 由于第 2 项已阻止非法编辑写回，正常 UI 路径下此时同步的是原本有效的
      `load_top`，**不能**以同步成功判定非法 Pressure 未被拦截。对旧项目中已持久化
      非法边界的情况，主界面“校验工作流”应定位到
      `Loads/pressure_mapping_check/boundary`，并明确说明 `instance_plate`
      不存在于网格的二维 Physical Group 清单，例如：

      ```text
      Physical Group 'instance_plate' (dimension 2) is not present in the mesh.
      ```

   4. 对这类旧项目非法状态，同步/提交预检还必须阻断，不得生成
      `boundary = instance_plate` 的可提交输入；定向自动巡览覆盖这条绕过弹窗的路径。
      人工测试无需手改项目 YAML 来制造非法持久状态。

9. 恢复 M-01 主项目的位移加载，并检查不存在同面同自由度冲突。
   1. 关闭 Pressure 测试副本；如需保留负向验证现场，可以另存副本，但不得覆盖
      `phase5-g1-assembly-contact.gmp.yaml`。
   2. 重新打开主项目 `phase5-g1-assembly-contact.gmp.yaml`，确认：
      - `BC` 下仍有 `disp_x`、`disp_y`、`disp_z` 和 `load_disp_z`；
      - `load_disp_z` 仍为 `FunctionDirichletBC + disp_z + load_top + loading_curve`；
      - `Loads` 下没有 `pressure_mapping_check`；
      - `Functions` 下没有副本专用的 `pressure_curve`。
   3. 点击“校验工作流”，确认没有
      `Pressure conflicts with a prescribed BC on the same boundary and variable.`
      冲突。同一个 `load_top + disp_z` 最终只保留位移加载。
   4. 再次同步 MOOSE 输入，确认 `.i` 恢复为 `loading_curve + load_disp_z`，
      不再包含 `pressure_mapping_check`、`pressure_curve` 或测试性 Pressure。

通过标准：固定端、位移加载和 Load 都引用命名组；Load 不被统一硬编码为无关的
`BodyForce`；冲突或维度错误能定位到具体对象/字段。

## 六、G1：M-01 线弹性接触案例与结果闭环

### 6.1 材料、Section、Physics、Step 和 Outputs

以下操作都在主项目 `phase5-g1-assembly-contact.gmp.yaml` 中进行，不要打开
§5.3 的 Pressure 测试副本。先确认活动应用为 `DamSafetyApp-opt`，装配网格仍含
`instance_concrete`、`instance_plate` 两个 **三维体组**，以及 `load_top` 等二维面组；
`BC/load_disp_z`、`Functions/loading_curve` 和 Contact 均保留。材料、Section 等节点
创建前工作流报告有缺项是正常的，逐项完成后再用 §6.2 做全局预检。

1. **为混凝土和钢板建立线弹性本构。**
   1. Abaqus 参考截图中的混凝土为 `E=29791.45978 MPa、ν=0.2`，钢板为
      `E=206000 MPa、ν=0.3`（见
      [混凝土截图](../doc/images/abaqus-workflow/08-material-concrete-elastic.png) 和
      [钢板截图](../doc/images/abaqus-workflow/10-material-steel-elastic.png)）。
      `DamSafetyApp-opt` 的应力单位为 **Pa**，本项目输入须使用：

      | 区域 | 杨氏模量 `youngs_modulus`（Pa） | 泊松比 `poissons_ratio` |
      |---|---:|---:|
      | 混凝土 | `2.979145978e10` | `0.2` |
      | 钢板 | `2.06e11` | `0.3` |

      这是参考图的 MPa→Pa 换算，不要把 `29791.45978` 或 `206000` 原样填入 Pa 字段。
      网格坐标本身没有自动 mm→m 换算；在 §6.3 真正求解、解释力和位移前，还必须核对
      几何、位移与材料使用同一长度单位，不能仅凭输入预检通过认定物理单位正确。
   2. 模型树右键 `Materials` 根节点，依次选择“添加 Materials”，创建并重命名为
      `concrete_elasticity`、`concrete_stress`、`steel_elasticity`、`steel_stress`。
      这里的“两种线弹性材料”各由一个弹性张量对象和一个应力计算对象组成，不能
      只创建两个弹性张量而遗漏应力计算。MOOSE 的[线弹性入门输入](https://mooseframework.inl.gov/releases/moose/2024-11-11/modules/solid_mechanics/tutorials/introduction/step01.html)
      也采用这两个对象配对。
   3. 如果“类型”下拉尚无 `ComputeIsotropicElasticityTensor`，先重新构建并启动包含
      本节修复的应用；旧进程不会自动更新。分别双击 `concrete_elasticity`、
      `steel_elasticity`，在“参数 → 快捷参数”直接选择这个类型，并逐项填写：

      | 节点 | 类型 | Young's Modulus (MPa) | Poisson's Ratio | Block/作用区域 |
      |---|---|---:|---:|---|
      | `concrete_elasticity` | `ComputeIsotropicElasticityTensor` | `29791.45978` | `0.2` | `instance_concrete` |
      | `steel_elasticity` | `ComputeIsotropicElasticityTensor` | `206000` | `0.3` | `instance_plate` |

      `Block/作用区域` 是只读回显，不再手工输入：在参数页上方“可选物理体”列表中
      选中本行的 `instance_*`，点击“应用所选物理体”，确认回显为该组名。
      列表仅显示当前网格中的物理体组；若为空，先检查装配/网格物理组是否已恢复。
      同一材料需要覆盖多个物理体时可多选，`block` 以空格分隔保存。
      “Young's Modulus (MPa)”按 MPa 输入，系统存储为 Pa。若勾选“高级参数”复核，
      两节点应分别为以下精确键值；**不要**在快捷字段输入 Pa 数值：

      | 节点 | `type` | `youngs_modulus`（Pa） | `poissons_ratio` | `block` |
      |---|---|---:|---:|---|
      | `concrete_elasticity` | `ComputeIsotropicElasticityTensor` | `29791459780` | `0.2` | `instance_concrete` |
      | `steel_elasticity` | `ComputeIsotropicElasticityTensor` | `206000000000` | `0.3` | `instance_plate` |

      从默认 `GenericConstantMaterial` 切换类型后，`prop_names`、`prop_values` 应自动
      消失；若在高级参数表仍有这两行，逐行选中并点“删除参数”。当前
      “Linear Elastic (isotropic)”模板仍是示例 `C_ijkl` 路径，**不要**套用
      `2.1e5 0.8e5`，也不要改用 CDP 模板。
   4. 分别打开两个 `*_stress` 节点，在“参数 → 快捷参数”填写：

      | 节点 | 类型 | Block/作用区域 |
      |---|---|---|
      | `concrete_stress` | `ComputeLinearElasticStress` | `instance_concrete` |
      | `steel_stress` | `ComputeLinearElasticStress` | `instance_plate` |

      `*_stress` 也使用上方同一物理体列表选择并点击“应用所选物理体”，
      不要在只读“作用区域”中输入。高级参数或旧项目中若存在不存在的 `block` 名称，
      参数页和校验页应报错，点击“确定”应被阻断；恢复正确体组后才可确认。
      这两个节点不需要 `youngs_modulus` 或 `poissons_ratio`；若默认
      `prop_names`、`prop_values` 仍在高级参数表中，删除它们。逐个打开“校验”页，
      无当前材料字段错误后点击“确定”；重开核对所有值仍在。若类型下拉或这些快捷
      字段仍不可用，记录为材料编辑阻塞，不要手改 `.i` 绕过。
2. **创建两个 Section 并指派体组。**顶部“模块”切到“截面”，点击“新建实体截面”
   两次，或在 `Sections` 根节点上右键添加两次，命名为 `section_concrete` 和
   `section_plate`。逐个打开“参数”页：
   - `section_concrete`：类型保持只读的 `SolidSection`；“材料”下拉选
     `concrete_elasticity`；在上方“可选物理体”选 `instance_concrete`，点击
     “应用所选物理体”，确认只读“已指派物理体”为 `instance_concrete`。
   - `section_plate`：材料选 `steel_elasticity`；只选 `instance_plate` 并点击
     “应用所选物理体”，确认只读字段为 `instance_plate`。

   列表高亮不等于已指派，必须点击“应用所选物理体”再点“确定”。若可选体组列表为空，
   返回 Mesh 工作窗“分组与网格场 → 物理组”确认两组三维体组，刷新/重建装配网格后重试。
3. **检查材料—区域关系。**重开两个 Section，确认它们分别只含自己的体组、材料
   引用仍存在；重开四个 Material，确认各自的 `block` 与 Section 体组一致，且
   `instance_concrete`、`instance_plate` 是两个不同的三维组。Section 在生成报告中
   记录指派关系；本轮普通线弹性 Material 的 `block` 还需按第 1 步明确填写，
   不要只依赖 Section 名称自动推断。
4. **创建三维准静态 Physics。**在模型树 `Physics` 根节点右键“添加 Physics”，
   将子节点命名为 `physics_g1` 并打开“参数”。`Action` 选 `QuasiStatic`，
   `Strain=SMALL`、`add_variables=true`、`incremental=true`；其余默认
   `volumetric_locking_correction=true`、`save_in_resid=true` 保持。顶部“可选体组”
   同时选中 `instance_concrete` 和 `instance_plate`（macOS 按 Command 多选），
   点击“应用所选分组”，确认 `Block` 为两个名称，以空格分隔。`generate_output`
   至少保留默认的 `stress_xx … stress_zz` 和 `strain_xx … strain_zz`；`disp_x/y/z`
   由 `add_variables=true` 的 Physics action 建立，`Variables` 树根为空不等于缺少
   位移变量。点击“确定”后只应有一个 `physics_g1`。
5. **创建且只保留一个 Step。**在 `Steps` 根节点右键“添加 Steps”，命名为
   `step_g1`，打开“参数”。若主项目已经有 Step，优先编辑原节点，不要再添加第二个。
   若确有多余 Step，先另存项目备份再删除多余节点；当前 G1 不能用“同步只取第一个”
   的过渡行为冒充多 Step 执行。类型保持 `Transient`：这里通过 0～1 的伪时间推进
   位移，是准静态加载，不等于切换为动力学问题。
6. **填写 Step 的求解控制、时间步和预条件。**在 `step_g1` 的“参数”页按下表
   核对默认值；若值不同，明确改成表中值后点击“确定”：

   | 分组 | 字段和值 |
   |---|---|
   | 基本 | `start_time=0`，`end_time=1`，`num_steps=100000` |
   | 求解控制 | `solve_type=NEWTON`，`line_search=bt`，`automatic_scaling=true`，`nl_rel_tol=1e-9`，`nl_abs_tol=1e-8`，`nl_max_its=50` |
   | PETSc | `petsc_options_iname=-pc_type -pc_factor_mat_solver_type`，`petsc_options_value=lu mumps` |
   | 时间步 | `timestepper_type=IterationAdaptiveDT`，`dt=0.01`，`dtmin=1e-15`，`dtmax=1` |
   | 自适应 | `optimal_iterations=8`，`iteration_window=3`，`growth_factor=1.15`，`cutback_factor=0.5` |
   | 预条件 | `preconditioning_type=SMP`，`preconditioning_full=true` |

   这些是当前界面 G1/Phase 4 的起始验收配置，不保证所有接触计算都收敛；如
   §6.2 的目标应用输入检查或 §6.3 运行要求调整，记录修改值及理由，不静默改基线。
7. **配置场量与历史量 Outputs。**在 `Outputs` 根节点右键“添加 Outputs”，命名
   `outputs_g1` 并打开“参数”。位移由 Physics 的 `add_variables=true` 输出；应力、
   应变由第 4 步的 `generate_output` 指定。当前“Field Variables”复选框是 CDP
   专用的 `DamageC/DamageT/kappa_*` 等诊断量，**本线弹性用例不要勾选它们来冒充
   接触量**。接着在“历史输出”中：
   1. 在顶部“可选边界分组”只选择二维组 `load_top`，点击“应用所选边界”，确认
      “History Boundary/历史输出面组”为 `load_top`。
   2. 将 `Reaction Force=true`、`Displacement Avg=true`、
      `Disp Variable=disp_z`；`Extremum=false` 可保持默认。
   3. 将 `Enable Times=true`、`Times Name=field_output_times`、
      `start_time=0`、`end_time=1`、`time_interval=0.01`。这样历史量和文件
      输出都按 0～1 的时间序列采样。

   当前 Outputs 快捷表单**没有独立的接触量选择/映射**；不能把普通应力场或 CDP
   诊断量算作“接触量已配置”。MOOSE `ContactAction` 可能自动建立 `contact_pressure`
   等辅助量，因此生成 `.i` 未显式列出接触 AuxKernel 也不能单独判定无输出。
   检查活动档案的实际行为，并在 §6.3 的 Exodus 结果中核实接触压力、间隙或接触力；
   如目标应用未生成可用接触量，记录“6.1 接触量输出阻塞”，暂停 G1 准出，
   不要手改 `.i` 或虚构结果。§6.2 的输入检查不能替代此项结果验证。
8. **启用结果文件并明确日志来源。**同一 `outputs_g1` 的“Output Files”中确认
   `Exodus=true`、`CSV=true`，按需填写只含文件名前缀的 `file_base`，然后点击
   “确定”。`Outputs` 表单没有单独的“求解日志”开关；日志由 §6.3 的作业执行和
   远程制品流程获取。此处只验证 Exodus/CSV 配置，不能提前宣称日志制品已生成。
9. **保存并留证。**点击“保存项目”，记下主项目路径；关闭并重开上述各节点，确认
   四个 Material、两个 Section、一个 Physics、一个 Step、一个 Outputs 的名称和
   参数保持。保存材料、Section、Physics、Step 与 Outputs 的参数截图。下一步按
   §6.2 执行“校验工作流 → 同步模型到 MOOSE 输入 → 检查输入”，核对实际生成的
   `[Materials]`、`[Physics/SolidMechanics/QuasiStatic]`、`[Executioner]`、
   `[Preconditioning]`、`[Postprocessors]`、`[Outputs]`；仅属性页显示“就绪”
   不构成目标应用可运行证据。

本节通过条件：所有已列对象可从 UI 保存并重开、材料和体组对应正确、只有一个
Step，位移/应力/应变与加载面位移均值/反力以及 Exodus/CSV 有真实生成映射。
接触量输出若仍无映射，应按第 7 步记录阻塞；求解日志须在 §6.3 取得，不能在此
提前判定 G1 整体验收通过。

### 6.2 工作流校验、确定性生成与输入预检（TEST-P5-G1-08）

1. 点击“校验工作流”。
2. 确认结果为 `0 errors`；警告必须逐条记录并确认不会改变物理语义。
3. 如果 Contact 类型不受支持、面组维度错误、Mesh 过期或材料未指派，必须先修复，
   不得绕过校验。
4. 执行“同步模型到 MOOSE 输入”。
5. 在“作业工作窗 → MOOSE 设置 → 输入文件”保存第一次完整文本为证据 A。
6. 不修改任何对象，再同步一次并保存文本为证据 B。
7. 对 A/B 执行文本比较，确认完全一致。
8. 检查输入至少包含：
   - 项目自有 `.msh` 的 `[Mesh/file]`；
   - 两个材料及其正确 block；
   - Physics/变量；
   - 固定与位移加载；
   - 真实 Contact 对象；
   - 单一 Executioner/Preconditioning；
   - 接触量、位移、反力及 Exodus/CSV 输出。
9. 打开“生成报告”，逐项确认 Assembly、两个实例、体/面组、材料、Section、Contact、
   BC/Load、Step 和 Outputs 均能追溯到模型树对象。
10. 点击“检查输入”，执行目标应用 `--check-input`。
11. 确认预检退出成功，无未知类型、未知参数、空 boundary/block 或文件缺失。

通过标准：无需手工编辑 `.i`；两次生成完全一致；内部预检和目标应用 `--check-input`
均通过；报告来源完整。

### 6.3 快照、远程成功终态与 Results（TEST-P5-G1-09）

1. 在“执行配置”中确认远程服务器、项目 ID、MPI 进程数和运行器正确。
2. 导出 contract v2 作业快照。
3. 检查快照包含 `.i`、装配 `.msh`、必要附件和 `manifest.json`。
4. 检查 manifest 中的 application profile、mapping、input mode、所有文件 SHA-256、
   Physical Groups 和推荐命令。
5. 确认快照内 `.i` 使用包内相对网格文件名，而不是本机绝对路径。
6. 点击“提交作业”。
7. 记录 Job ID，观察 queued → preparing/running → succeeded。
8. 仅进入 running 不能关闭 G1；必须等待本用例得到 `succeeded`。
9. 刷新状态，确认远程日志没有输入预检错误、Contact 初始化错误或网格组缺失。
10. 打开远程制品，确认至少有 Exodus、CSV 和日志。
11. 检查模型树 `Results` 自动登记这些结果，且 `job`、`snapshot`、输入哈希可追溯。
12. 打开 Exodus，在 Visualization 中切换位移/应力/接触量并播放时间步。
13. 打开 CSV，确认加载面位移和反力随时间有数据，且时间范围与 Step 一致。
14. 保存并关闭项目，重开后确认 Job、Results 和快照追溯仍存在。

通过标准：远程作业成功完成；制品自动登记、可下载、可加载、可回放；结果可追溯到
同一输入快照。至此才能关闭 G1。

### 6.4 G1 准出清单

| 检查项 | 结果 |
|---|---|
| TEST-P5-G1-01 两个 Part 独立可重建 | 待验证 |
| TEST-P5-G1-02 实例平移/旋转真实生效 | 已人工通过（2026-09-15） |
| TEST-P5-G1-03 可见性/顺序/非法参数 | 已人工通过（2026-09-15） |
| TEST-P5-G1-04 保存重开与过期传播 | 已人工通过（2026-09-15） |
| TEST-P5-G1-05 装配网格与 Physical Groups | 已人工通过（2026-09-15） |
| TEST-P5-G1-06 Contact 支持矩阵与真实映射 | 待人工验证 |
| TEST-P5-G1-07 BC/Load 选择集和映射 | 待人工验证 |
| TEST-P5-G1-08 确定性输入与 `--check-input` | 部分完成／环境待测（2026-09-17） |
| TEST-P5-G1-09 succeeded/Results/回放闭环 | 未测试／延期（2026-09-17） |

G1 的所有行都通过、相关缺陷关闭、全量真实点击巡览与 CTest 通过后，才能进入 G2 准出。

### 6.5 阶段性执行记录（2026-09-17，非 G1 准出）

- **项目与静态证据**：`phase5-g1-assembly-contact.gmp.yaml` 的生成报告可追溯
  Mesh、四个 Material、两个 Section 的材料/三维体组指派、Physics、BC、Contact、
  Step 与 Outputs；“Workflow ready: no blocking issue found”仅证明应用内部工作流校验通过。
- **TEST-P5-G1-08：部分完成／环境待测**。操作日志显示多次生成 `.i`，但未提交 A/B
  文本比较证据；点击“检查输入”时记录 `moose | Executable is empty.`，目标应用
  `--check-input` 未启动，因此不能将本用例标为通过。用户仅在远端执行；当前计算节点
  不在可用局域网内，此项按环境待测记录，不据此登记产品缺陷。
- **TEST-P5-G1-09：未测试／延期**。尚无本轮 M-01 快照对应的远端 `succeeded`、
  求解日志、Exodus/CSV 制品、Results 回放和接触量数值证据；不得提前标为通过。
- **本地自动回归**：2026-09-17 提交前构建、CTest `1/1` 和 111 步全量真实点击巡览
  均通过；这些证据不替代目标应用输入预检或远端求解结果。
- **恢复条件**：远端环境可用后，保存两次同步输入 A/B 并逐字比较，在实际目标应用上
  完成 `--check-input`；随后按 §6.3 提交快照并等待 `succeeded`，核对结果文件中的
  `contact_pressure` 等真实接触量、位移和反力。补齐证据后再分别回填 6.1 第 7 步、
  TEST-P5-G1-08/09 与 G1 准出清单。
- **后续边界**：可先分析 G2 的档案支持矩阵、Reference Point/Coupling 映射和验证方案；
  不将该分析视为 G2 开工或 G1 已准出，G2 的项目复制、实施与准出仍按 §7 和闸门规则执行。

## 七、G2：Reference Point 与 Coupling Constraint

### 7.1 能力声明与参考点创建（TEST-P5-G2-01）

1. 复制 G1 已通过项目为 `phase5-g2-coupling.gmp.yaml`。
2. 确认活动档案明确声明 Reference Point/Coupling 的支持状态和 mapping 版本。
3. 若不支持，创建对象后应显示“不支持生成”，同步和提交被阻断；记录为功能阻塞，
   不得继续假装通过。
4. 若支持，进入 Constraints/参考对象入口，新建参考点 `rp_load`。
5. 通过 UI 把参考点放到钢板加载面中心或合同规定的位置。
6. 保存并重开，确认参考点坐标、名称和状态恢复。

通过标准：参考点是项目中的独立、可持久化对象；不是仅存在于一次拾取会话中的临时点。

### 7.2 创建 Coupling（TEST-P5-G2-02）

1. 新建 Coupling Constraint，命名为 `coupling_load_top`。
2. 控制点选择 `rp_load`。
3. 耦合面选择 `load_top`。
4. 配置活动档案支持的运动学/分布式耦合类型和自由度。
5. 点击“确定”，执行“校验工作流”。
6. 临时删除 `rp_load`，确认 Coupling 立即失效并提示悬空引用。
7. 恢复同名参考点，确认引用按合同恢复或提示用户重新确认，不得静默绑定到其他点。
8. 临时把 `instance_plate` 三维体组作为耦合面，确认维度错误。
9. 恢复正确配置并保存重开。

通过标准：Coupling 明确引用参考点、面组和自由度；引用/维度错误可定位；保存重开一致。

### 7.3 输入映射不得退化为整面位移（TEST-P5-G2-03）

1. 把原来直接施加到 `load_top` 的位移改为施加到 `rp_load`。
2. 同步输入两次，确认文本一致。
3. 检查生成输入中存在活动档案声明的真实 Reference Point/Coupling 对象。
4. 检查生成报告能定位回 `rp_load` 和 `coupling_load_top`。
5. 确认输入中没有用 `load_top` 上的统一 `DirichletBC` 静默替代 Coupling。
6. 执行 `--check-input` 并确认成功。

通过标准：参考点和 Coupling 均形成真实求解对象；不是 UI 有节点、输入仍是旧整面位移。

### 7.4 远程传力验证（TEST-P5-G2-04）

1. 导出新快照，确认 manifest 记录参考点、Coupling 来源和 mapping 版本。
2. 提交远程作业并等待 `succeeded`。
3. 在 CSV/历史结果中读取参考点位移、耦合面平均位移和参考点/加载端合力。
4. 比较参考点位移与耦合面目标运动，误差应满足执行前冻结的数值容差。
5. 比较参考点合力与耦合面反力，方向约定一致，量值误差满足容差。
6. 在 Visualization 中确认钢板加载面按耦合方式运动，没有非预期刚体自由度。
7. 保存重开并确认结果追溯完整。

### 7.5 G2 负向回归（TEST-P5-G2-05）

依次破坏并恢复以下条件，每次都执行“校验工作流”：

1. 删除参考点；
2. 删除 `load_top`；
3. 设置空自由度；
4. 同一自由度同时施加冲突 BC 和 Coupling；
5. 切换到不支持 Coupling 的应用档案。

通过标准：五种错误都在生成/提交前被阻断并可定位；恢复后重新同步、预检和提交能力恢复。

G2 只有在真实 Coupling 映射、远程成功以及位移/合力传递均通过后才能关闭。

## 八、G3：CDP 基准与追溯

### 8.1 冻结基准、指标和容差（TEST-P5-G3-01）

执行 G3 前必须填写并评审下表；任何“待确认”都阻止 G3 最终准出：

| 项目 | 决策值 |
|---|---|
| 认可的 Abaqus/MOOSE 参考输入路径和版本 | 待确认 |
| 四个 CDP CSV 的版本与 SHA-256 | 待确认 |
| 单位合同 | 待确认 |
| 力—位移比较采样/插值规则 | 待确认 |
| 峰值力相对误差容差 | 待确认 |
| 初始刚度相对误差容差 | 待确认 |
| 损伤起始点/损伤场比较规则 | 待确认 |
| 非收敛、缺失点和异常值处理规则 | 待确认 |

建议从 `tests/fixtures/cdp-v01/` 和对应的已认可基线开始，但必须记录其 prototype 适用范围，
不得把测试数据描述为正式工程材料参数。

### 8.2 将混凝土材料切换为 CDP（TEST-P5-G3-02）

1. 复制 G2 已通过项目为 `phase5-g3-cdp.gmp.yaml`。
2. 保留钢板线弹性材料。
3. 新建或编辑混凝土材料为活动应用认可的 CDP 类型。
4. 填写弹性参数、CDP 标量参数和四个材料 CSV。
5. 在 UI 中按显示单位录入，记录显示值与生成输入求解单位值。
6. 将混凝土 Section 指向 CDP 材料，block 保持 `instance_concrete`。
7. 执行工作流校验，确认四个 CSV 存在、列数/单调性等合同通过。
8. 临时删除一个 CSV，确认快照和提交被阻断；恢复后错误消失。
9. 同步输入，确认真实 CDP 材料三件套及所有必要参数存在，无占位值。
10. 执行 `--check-input`。

通过标准：CDP 类型、参数、单位换算和文件引用正确；缺文件会阻断；目标应用预检通过。

### 8.3 生成报告与 manifest 追溯（TEST-P5-G3-03）

1. 打开“生成报告”，确认 CDP 材料块能追溯到混凝土材料节点和 Section 体组。
2. 确认报告记录活动应用、mapping 版本、input mode 和单位合同。
3. 导出 contract v2 快照。
4. 确认 manifest 包含四个 CSV，文件名、role、大小和 SHA-256 与冻结基线一致。
5. 确认 `.i` 和 `.msh` 的哈希已记录，路径均为包内安全相对路径。
6. 保存、关闭并重开项目，再次确认生成报告、自定义专家块和材料文件引用保留。
7. 不修改项目再次导出快照，对比受管输入和输入附件哈希；除 case/time 等允许变化字段外
   不应发生漂移。

### 8.4 运行 CDP 案例（TEST-P5-G3-04）

1. 提交快照并记录 Job ID。
2. 观察远程预检、初始化、非线性迭代和时间步推进。
3. 等待作业进入 `succeeded`；取消或只进入 running 不足以通过。
4. 确认结果至少包含位移、反力、应力/应变、压缩/拉伸损伤和需要的内部变量。
5. 检查输出时间范围、采样数和冻结的比较规则一致。
6. 在 Visualization 中查看损伤场随加载演化，确认没有全 NaN、全零或明显越界值。
7. 从 CSV 提取力—位移曲线所需数据。

### 8.5 基准比较与独立报告（TEST-P5-G3-05）

1. 使用第 8.1 节冻结的规则对齐 Abaqus 与 MOOSE 的时间/位移采样点。
2. 统一坐标方向、力符号和单位。
3. 计算并记录：
   - 峰值力及相对误差；
   - 初始刚度及相对误差；
   - 指定位移点反力误差；
   - 损伤起始位置/时刻；
   - 主要损伤区空间分布；
   - 收敛步、cutback 和异常点。
4. 为本 Job 生成一份独立的 Abaqus–MOOSE 对比文档，不得只在总验收表中写一句“趋势一致”。
5. 报告必须包含 Job ID、Git 提交、项目/快照路径、输入和材料哈希、参数表、曲线、误差表、
   异常说明和最终结论。
6. 所有指标都在冻结容差内才可判定通过；若超差，登记缺陷并保留原始结果。

G3 通过标准：真实 CDP 作业成功；材料和单位可追溯；独立比较报告完整；全部冻结指标满足
容差。至此 Phase 5 / G1～G3 才能整体关闭。

## 九、通用保存重开与负向检查

每个闸门至少执行一次以下回归：

1. 保存并关闭项目。
2. 重新打开，确认活动应用、Assembly 实例、变换、Physical Groups、材料/Section、
   Contact/Coupling、Step、Outputs 和作业配置保持。
3. 重复同步两次，确认输入不新增重复块。
4. 重命名一个被引用对象，确认引用同步更新或明确失效，不得静默指向其他对象。
5. 删除一个被引用对象，确认校验错误可定位。
6. 恢复同名对象或修复引用，确认工作流可恢复。
7. 修改 Part/Assembly/Mesh 后，确认旧预检、快照和 Job 不再显示为当前有效产物。
8. 切换到不支持当前 Contact/Coupling/CDP 的应用档案，确认不兼容对象保留但阻断生成。
9. 切回原档案，确认对象和参数未丢失。

## 十、自动回归与闸门准出

日常修改只运行与本次变更直接相关的 1～2 个定向巡览。Assembly 当前定向命令为：

```bash
tour_dir=$(mktemp -d /tmp/gmp-assembly-tour.XXXXXX)
GMP_TOUR_REAL_CLICKS=1 \
GMP_TOUR_STEP_FILTER=assembly_instance_contract \
GMP_SCREENSHOT_DIR="$tour_dir" \
./build/gmp_ise
```

每个 G1/G2/G3 闸门或 Git 提交前运行：

```bash
ctest --test-dir build --output-on-failure

tour_dir=$(mktemp -d /tmp/gmp-ui-tour-phase5.XXXXXX)
GMP_TOUR_REAL_CLICKS=1 \
GMP_SCREENSHOT_DIR="$tour_dir" \
./build/gmp_ise
```

Mesh 常规摘要的定向回归命令为：

```bash
tour_dir=$(mktemp -d /tmp/gmp-mesh-summary-tour.XXXXXX)
GMP_TOUR_REAL_CLICKS=1 \
GMP_TOUR_STEP_FILTER=mesh_property_summary_contract \
GMP_SCREENSHOT_DIR="$tour_dir" \
./build/gmp_ise
```

保存反馈与保存/另存为图标区分的定向回归命令为：

```bash
tour_dir=$(mktemp -d /tmp/gmp-save-feedback-tour.XXXXXX)
GMP_TOUR_REAL_CLICKS=1 \
GMP_TOUR_STEP_FILTER=project_save_feedback_contract \
GMP_SCREENSHOT_DIR="$tour_dir" \
./build/gmp_ise
```

当前自动巡览清单为 111 步，其中 `assembly_instance_contract`、
`entity_picker_guidance_contract`、`mesh_property_summary_contract` 与
`project_save_feedback_contract` 已定向通过；本次 G1 还覆盖
`g1_isotropic_material_form_contract` 与 `contact_load_mapping_contract`；
后续新增用例只能递增，不能删除旧断言换取通过。

`assembly_instance_contract` 同时验证 Assembly 自定义 Physical Groups 的项目级持久化：
`contact_concrete`、`contact_plate`、`fixed_bottom`、`load_top` 保存重开后必须全部恢复，且
每个组仍绑定至少一个面实体。
该合同还验证选择器同时列出两个 Assembly 实例、新建组默认无勾选，并在切换候选面时让
主舞台进入灰色线框 + 亮黄色候选面预览、自动切换观察方向，关闭对话框后清除预览；
`entity_picker_guidance_contract` 验证三列表头、坐标范围、默认勾选状态和选择回写。

闸门准出规则：

- **G1**：TEST-P5-G1-01～09 全部通过，M-01 远程终态必须为 `succeeded`。
- **G2**：TEST-P5-G2-01～05 全部通过，真实 Coupling 及传力验证成立。
- **G3**：TEST-P5-G3-01～05 全部通过，CDP 独立比较报告满足冻结容差。
- 每个闸门均要求无未关闭 P0 缺陷、全量真实点击巡览通过、CTest 通过及文档结果回填。

## 十一、执行记录模板

| 用例 | 日期 | Git 提交 | 项目/快照/Job | 结果 | 缺陷或证据 |
|---|---|---|---|---|---|
| TEST-P5-G1-01 |  |  |  | 待验证 |  |
| TEST-P5-G1-02 | 2026-09-15 |  | `phase5-g1-assembly-contact.gmp.yaml` | 人工通过 | 独立实例、定位、旋转与恢复通过 |
| TEST-P5-G1-03 | 2026-09-15 |  | `phase5-g1-assembly-contact.gmp.yaml` | 人工通过 | 可见性、顺序与非法参数校验通过 |
| TEST-P5-G1-04 | 2026-09-15 |  | `phase5-g1-assembly-contact.gmp.yaml` | 人工通过 | 保存重开与过期传播通过 |
| TEST-P5-G1-05 | 2026-09-15 |  | `phase5-g1-assembly-contact.gmp.yaml` | 人工通过 | 项目自有装配网格、四组、网格摘要与 SHA-256 通过 |
| TEST-P5-G1-06 |  |  |  | 待人工验证 |  |
| TEST-P5-G1-07 |  |  |  | 待人工验证 |  |
| TEST-P5-G1-08 | 2026-09-17 |  | `phase5-g1-assembly-contact.gmp.yaml` | 部分完成／环境待测 | 生成报告、内部工作流校验通过；A/B 比较未留证；`Executable is empty.`，目标应用 `--check-input` 未启动 |
| TEST-P5-G1-09 | 2026-09-17 |  | `phase5-g1-assembly-contact.gmp.yaml` | 未测试／延期 | 远端计算环境当前不可用；无 `succeeded`、日志、Exodus/CSV、Results 与接触量数值证据 |
| TEST-P5-G2-01～05 |  |  |  | 待实现后验证 |  |
| TEST-P5-G3-01～05 |  |  |  | 待实现后验证 |  |

发现问题时至少记录：现象、复现步骤、期望、实际结果、项目/快照/Job 路径、日志、截图、
影响闸门和缺陷状态。修复后应在原用例下追加回归记录，不覆盖首次失败证据。
