# 材料 · 截面 · 物理

本章介绍模型树的材料定义、截面指派与 Physics 动作三个环节，三者共同决定「用什么本构、算在哪个区域、输出哪些变量」。

## 新建材料与 CDP 损伤本构

切换到**材料（Material）**模块，模块工作窗提供：

- **新建材料（New Material）**：创建通用材料节点（默认 GenericConstantMaterial），通过属性表单改选类型（如各向同性线弹性、热膨胀等模板）；
- **CDP 材料（New CDP Material）**：一键创建混凝土损伤塑性（AbaqusCDP）材料，预置 V01 验收基线默认值，双击节点打开属性表单逐项核对；
- **打开根节点 / 打开属性编辑器**：在模型树定位 Materials 根，或直接唤起浮动属性表单。

CDP 材料的关键参数包括：弹性模量 youngs_modulus（表单按 MPa 显示、内部以 Pa 存储求解）、泊松比 poissons_ratio、膨胀角 dilation_angle、偏心率 eccentricity、双轴/单轴抗压比、拉压子午比、粘滞正则 viscosity、拉压恢复系数与最大子步数 maximum_substeps。

![材料模块页：新建材料、CDP 材料与条目列表](images/ch5-material.png)

## CDP 四张 CSV 曲线与属性表单

CDP 材料依赖四张硬化/损伤曲线 CSV，在属性表单**参数（Parameters）**页中通过文件框右侧的浏览按钮选择：

- 受压硬化 compression_hardening_file、受压损伤 compression_damage_file；
- 受拉硬化（刚度恢复）tension_stiffening_file、受拉损伤 tension_damage_file。

四张 CSV 均为「非弹性应变/开裂应变 — 应力/损伤因子」两列数据；缺失任一张都会在**校验（Validation）**页标红。属性表单共四个页签：**常规（General）**查看类别与状态、重命名；**参数（Parameters）**编辑快捷参数（字段随类型联动）、套用模板、展开高级参数表；**校验（Validation）**汇总全树问题并可跳转到问题节点；**预览（Preview）**显示参数与将生成的 MOOSE 输入块。点**确定**一次性写回，**取消**丢弃全部修改。

![浮动属性表单参数页：模板、快捷参数与高级参数](images/ch5-cdp-form.png)

## 截面指派（Section Assignment）

切换到**截面（Section）**模块，点击**新建实体截面（New Solid Section）**创建 SolidSection 节点，在属性表单中：

- **material**：选择要指派的材料名称；若材料被删除或重命名导致引用悬空，校验页会标出「material reference」问题（W-01b 引用语义）；
- **block**：选择要指派到的体物理组（多个用空格连接），体组清单在同步网格后可用；BC/Loads 则用面组清单写入 boundary。

截面把材料与几何区域绑定，是 Physics 动作与网格块之间的桥梁。配合上一节建好材料后，即可用 [分析定义](ch6-analysis.html) 的 Physics 节点声明求解动作。

![截面模块页：新建实体截面与条目列表](images/ch5-section.png)

## Physics 与输出变量

**Physics** 节点声明固体力学求解动作（SolidMechanics/QuasiStatic）：block 指向体物理组，generate_output 列出需要生成的应变/应力分量（默认 16 项）。**Outputs（输出套餐）**节点配置两类输出：

- **场输出（Field Output）**：多选候选变量（含 DamageC、DamageT、kappa_c、kappa_t 等 8 个 CDP 诊断量），随 Exodus 文件按时间步写出；
- **历史输出（History Output）**：hist_boundary 选择面物理组（如 V01 的 cdp_uniaxial_z），配合 Times 输出驱动时间历程曲线。

两类节点都在**预览（Preview）**页实时显示将生成的 MOOSE 块，同步模型到输入后可在 [作业](ch8-job.html) 工作窗的 Generated Input 页签核对。

![属性表单预览页：节点参数与 MOOSE 输入块预览](images/ch5-physics.png)
