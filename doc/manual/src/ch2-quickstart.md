# 快速上手

本章用软件内置的瞬态扩散演示项目，带你十分钟完成第一次「建模仿真 → 提交求解 → 查看输入/结果」的完整流程。所有步骤都基于真实界面，界面布局请先阅读 [简介与界面总览](ch1-intro.html)。

## 十分钟完成第一个算例

1. 新建项目：选择**文件（File）→ 新建项目（New Project）**，获得一个空白工作区。

![图](images/quick-step-1.png)

2. 载入演示：选择**工具（Tools）→ 演示案例（Demos）→ 载入瞬态扩散（Setup Transient Diffusion）**，模型树随即出现材料、分析步、边界条件等预置对象。

![图](images/quick-step-2.png)

3. 生成网格：选择**网格（Mesh）→ 生成网格（Generate Mesh）**（快捷键 Ctrl+M）；演示项目没有几何，软件会自动以示例盒兜底生成网格。

![图](images/quick-step-3.png)

4. 同步：选择**模型（Model）→ 同步模型到 MOOSE 输入（Sync Model -> MOOSE Input）**（快捷键 Ctrl+Shift+R），模型树被转换为 MOOSE 输入文件。

![图](images/quick-step-4.png)

5. 运行或仅查看输入：选择**作业（Job）→ 运行（Run）**（快捷键 F5）在本地启动求解；若只想核对输入，改选**作业（Job）→ 检查输入（Check Input）**（快捷键 Ctrl+K），或在作业工作窗的 MOOSE 设置 → 输入文件页签中查看生成的内容。

![图](images/quick-step-5.png)

完成以上五步即走完第一个算例。运行结束后，可在**结果（Results）**模块中导入输出的 Exodus 文件查看云图与曲线；各步骤的详细含义见下一节，遇到问题先查 [常见问题](ch3-faq.html)。

## 演示项目说明

载入瞬态扩散后，模型树中的预置对象构成如下，可作为理解软件数据组织的样例：

- **材料（Materials）**：名为 diffusion 的常值材料（GenericConstantMaterial），提供两个扩散系数属性 diff_u = 1.0、diff_v = 0.25；
- **变量（Variables）**：u、v 两个一阶拉格朗日（LAGRANGE）场变量；
- **分析步（Steps）**：transient 瞬态步，NEWTON 求解、bdf2 时间格式、时间步长 dt = 0.01、结束时间 0.2；
- **边界条件（BC）**：u 的左边界由函数 bc_left = 1.0 + 0.1·sin(2πt) 驱动、右边界固定 0；v 的左右边界均固定 0；
- **载荷（Loads）**：每个变量由三项组成——时间导数（TimeDerivative）、扩散（MatDiffusion，取用材料属性 diff_u/diff_v）与体积力（BodyForce，由源项函数驱动）；
- **函数（Functions）**：初始场 ic_u/ic_v、源项 source_u/source_v 与边界 bc_left/bc_right，均为解析函数（ParsedFunction）；
- **输出（Outputs）**：Exodus 结果文件与 CSV 数据同时输出；
- **网格**：输入文件内嵌生成网格（GeneratedMesh），因此演示项目无需几何即可直接运行。

掌握了这条最小流程后，就可以按 [简介与界面总览](ch1-intro.html) 中的主线工作流，逐步搭建自己的模型。
