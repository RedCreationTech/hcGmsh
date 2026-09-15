# GMP-ISE UI 重构 Phase 5 人工测试说明

> 版本：2026-09-15 v7
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
| TEST-P5-G1-06～09 | 实现完成后执行 | 依赖 Load/Contact 真实映射和 M-01 结果闭环 |
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

> **开发状态（2026-09-15）**：尚未完成，暂不开始人工验收。当前物理组编辑器已经支持
> 维度选择、舞台拾取、名称/维度/非空校验以及写入 `.msh`；但项目保存重开只恢复
> `mesh_snapshot` 清单，重新构建 `assembly: active` 时只自动重建 `instance_*` 和
> `instance_*_surface`，尚不会恢复下列四个专用面组与具体 OCC 面实体的绑定。必须先补齐
> 专用面组定义的项目级持久化、Assembly 重建后的稳定重绑定及自动回归，再执行本节。

1. 在“分组与网格场 → 物理组”把维度设为 `2`。
2. 新建 `contact_concrete`，拾取混凝土块与钢板相邻的顶面。
3. 新建 `contact_plate`，拾取钢板与混凝土块相邻的底面。
4. 新建 `fixed_bottom`，拾取混凝土块最下方的面。
5. 新建 `load_top`，拾取钢板最上方的面。
6. 检查四个面组实体集合均非空。
7. 确认 `contact_concrete` 与 `contact_plate` 不是同一个 Gmsh 实体 tag。
8. 重新生成 3D 网格。
9. 在物理组摘要中确认四个名称仍存在且维度均为 2。
10. 保存并重开项目，再次检查组名称和实体集合。

通过标准：Contact、固定和加载均使用具体命名面组；不得用裸 tag，也不得用实例的整个
`_surface` 组代替单个作用面。

## 五、G1：Load 与 Interaction/Contact 真实映射

### 5.1 冻结活动应用支持矩阵（TEST-P5-G1-06）

本节只能在 `TASK-P5-02` 实现完成后执行。

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

1. 在 `BC` 下为 `fixed_bottom` 创建三个零位移约束：`disp_x`、`disp_y`、`disp_z`。
2. 新建 `loading_curve`，使用 `PiecewiseLinear`；填写单调加载的时间—位移数据。
3. 在钢板 `load_top` 上创建 M-01 所需的位移边界，并引用 `loading_curve`。
4. 确认位移边界使用活动档案支持的真实类型。
5. 为验证 Load 映射，另建一份测试副本，在 `Loads` 中选择活动档案支持的面力或压力类型。
6. 该 Load 的作用面选择 `load_top`，不得输入裸 Gmsh tag。
7. 同步后检查 Load 被映射到档案声明的真实 Kernel/BC/应用专属对象。
8. 临时把 Load 作用域改成三维体组，确认维度不匹配被阻断。
9. 恢复 M-01 的位移加载配置；不要在同一自由度同时保留互相冲突的位移和力加载。

通过标准：固定端、位移加载和 Load 都引用命名组；Load 不被统一硬编码为无关的
`BodyForce`；冲突或维度错误能定位到具体对象/字段。

## 六、G1：M-01 线弹性接触案例与结果闭环

### 6.1 材料、Section、Physics、Step 和 Outputs

1. 为混凝土和钢板分别创建线弹性材料，使用 M-01 冻结的测试参数和单位。
2. 创建两个 Section：
   - 混凝土 Section 指派给 `instance_concrete`；
   - 钢板 Section 指派给 `instance_plate`。
3. 确认两个体组不交叉，材料引用存在。
4. 创建活动档案支持的三维准静态 Physics，block 同时覆盖两个体组或按映射要求拆分。
5. 创建一个 Step。G1 只允许单 Step；如项目中已有第二个 Step，先删除或禁用，避免把
   Phase 4 的“只取第一个”过渡行为带入 M-01 准出。
6. 配置求解控制、时间步和预条件参数。
7. 配置 Outputs，至少包括位移、应力、应变、接触量、加载面平均位移和反力。
8. 启用 Exodus、CSV 和求解日志制品。
9. 保存项目。

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
| TEST-P5-G1-06 Contact 支持矩阵与真实映射 | 待实现后验证 |
| TEST-P5-G1-07 BC/Load 选择集和映射 | 待实现后验证 |
| TEST-P5-G1-08 确定性输入与 `--check-input` | 待实现后验证 |
| TEST-P5-G1-09 succeeded/Results/回放闭环 | 待实现后验证 |

G1 的所有行都通过、相关缺陷关闭、全量真实点击巡览与 CTest 通过后，才能进入 G2 准出。

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

当前自动巡览清单为 109 步，其中 `assembly_instance_contract`、
`mesh_property_summary_contract` 与 `project_save_feedback_contract` 已定向通过；
后续新增用例只能递增，不能删除旧断言换取通过。

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
| TEST-P5-G1-06 |  |  |  | 待实现后验证 |  |
| TEST-P5-G1-07 |  |  |  | 待实现后验证 |  |
| TEST-P5-G1-08 |  |  |  | 待实现后验证 |  |
| TEST-P5-G1-09 |  |  |  | 待实现后验证 |  |
| TEST-P5-G2-01～05 |  |  |  | 待实现后验证 |  |
| TEST-P5-G3-01～05 |  |  |  | 待实现后验证 |  |

发现问题时至少记录：现象、复现步骤、期望、实际结果、项目/快照/Job 路径、日志、截图、
影响闸门和缺陷状态。修复后应在原用例下追加回归记录，不覆盖首次失败证据。
