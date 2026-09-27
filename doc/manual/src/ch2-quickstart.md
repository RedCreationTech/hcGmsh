# 快速上手

本章用软件内置的「V01 单轴拉伸」演示算例（混凝土立方体单轴受拉，CDP 损伤本构），带你十分钟完成第一次「载入模型 → 校验与同步 → 求解 → 查看云图/曲线」的完整流程。所有步骤都基于真实界面，界面布局请先阅读 [简介与界面总览](ch1-intro.html)。

## 十分钟完成 V01 单轴拉伸

1. 新建项目：选择**文件（File）→ 新建项目（New Project）**，获得一个空白工作区。

![新建项目后的主窗口：模型树、视口与消息区](images/quick-step-1.png)

2. 载入演示：选择**工具（Tools）→ 演示案例（Demos）→ 载入 V01 单轴拉伸（Load V01 Uniaxial Tension）**。软件会一次性导入演示网格（Exodus，体组 concrete_cube__concrete、面组 bottom/top），并在模型树中生成完整 V01 模型：CDP 材料 concrete（含四张损伤/硬化 CSV 曲线）、实体截面指派、Physics 固体力学准静态动作、bottom/top 位移边界、瞬态分析步与 cdp_uniaxial_z 历史输出套餐。若想一步载入并直接本地求解，改选**运行 V01 单轴拉伸（Run V01 Uniaxial Tension）**。

![演示案例菜单：载入/运行 V01 单轴拉伸](images/quick-step-2.png)

3. 校验并同步：选择**作业（Job）→ 校验工作流（Validate Workflow）**做无副作用预检（0 错误即工作流就绪）；再选择**模型（Model）→ 同步模型到 MOOSE 输入（Sync Model -> MOOSE Input）**（快捷键 Ctrl+Shift+R），模型树被转换为完整的 MOOSE 输入文件，可在作业工作窗的 MOOSE 设置 → Generated Input 页签中核对。

![同步后作业工作窗的 Generated Input 输入文件](images/quick-step-3.png)

4. 求解：本地求解选**作业（Job）→ 运行（Run）**（快捷键 F5）；若计算节点已配置 LIMS Facade，也可在作业工作窗点击**远程作业 (经 LIMS Facade 提交)（Remote Job (via LIMS Facade)）**提交到远程计算。若只想核对输入，改选**作业（Job）→ 检查输入（Check Input）**（快捷键 Ctrl+K）。

5. 查看结果：切换到**结果（Results）**模块工作窗，点击**导入任务目录...（Import Task Directory...）**选择任务目录，软件会登记场结果、历史数据（case.csv）、输入快照与日志报告；选中结果后点击**在视图中打开**查看云图，或切到**曲线**页签查看历史曲线（也可**导入 CSV...** 自己的曲线数据）。

![结果工作窗导入任务目录后的结果清单](images/quick-step-4.png)

![结果工作窗曲线页：历史曲线与曲线管理](images/quick-step-5.png)

完成以上五步即走完第一个算例。各步骤的详细含义见后续章节，遇到问题先查 [常见问题](ch3-faq.html)。

## 演示算例说明

「载入 V01 单轴拉伸」生成的模型树对象构成如下，可作为理解软件数据组织的样例：

- **网格（Mesh）**：导入的 Exodus 演示网格（体组 concrete_cube__concrete，面组 bottom/top；1331 节点 / 1000 HEX8 单元），清单同时供截面指派、边界选择与快照打包使用；
- **材料（Materials）**：concrete，AbaqusCDP 损伤本构（弹性 29.7915 GPa、ν 0.2、膨胀角 36°、粘滞正则 5e-4），四张硬化/损伤曲线 CSV 由软件内置文本提供；
- **截面（Sections）**：concrete_section 实体截面，把 concrete 材料指派到体组 concrete_cube__concrete；
- **物理（Physics）**：concrete 固体力学准静态动作（SolidMechanics/QuasiStatic），作用在 concrete_cube__concrete 上；
- **函数（Functions）**：top_displacement 解析函数 2.5e-05·t，驱动顶面位移加载；
- **边界条件（BC）**：bottom_z 固定底面 z 向；top_x_gauge/top_y_gauge 固定顶面 x/y 向（量规约束）；top_z 用 FunctionDirichletBC 施加顶面 z 向位移；
- **分析步（Steps）**：step_v01 瞬态步（默认参数）；
- **输出（Outputs）**：v01_outputs 输出套餐——场输出含 DamageC/DamageT/kappa_c/kappa_t 等 CDP 诊断量，历史输出 cdp_uniaxial_z，Times 输出启用，file_base = uniaxial_tension_single。

该算例与 damASR 冻结的 V01 单轴拉伸回归基线同源（网格逐字节一致），适合作为端到端验证入口。掌握了这条最小流程后，就可以按 [简介与界面总览](ch1-intro.html) 中的主线工作流，逐步搭建自己的模型。
