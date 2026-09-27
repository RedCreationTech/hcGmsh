# 分析定义

本章介绍分析步、函数、边界条件与载荷、相互作用与选择集的定义方式。分析定义依赖 [网格](ch7-mesh.html) 提供的物理组清单，求解提交见 [作业](ch8-job.html)。

## 分析步：静力、瞬态与预设

切换到**分析步（Step）**模块，提供三个预设按钮：

- **静力步（Static Step）**：dt=0 的准静态步，适合单调加载；
- **瞬态步（Transient Step）**：默认 dt=0.1、end_time=1.0，可改时间积分方案、求解器类型与非线性/线性容差；
- **稳态步（Steady Step）**：稳态求解。

页底**分析步序列预览（Step Sequence Preview）**只读列出全部 Step 的类型与时间参数。注意：同步到 MOOSE 输入时 `[Executioner]` 只使用第一个 Step，多于一个时控制台会告警，请把真正求解的步放在第一位。

![分析步模块页：静力/瞬态/稳态预设与序列预览](images/ch6-step.png)

## 函数：解析与分段线性

**函数（Functions）**节点在模型树 Functions 根下创建，属性表单「类型」下拉提供两类：

- **解析函数（ParsedFunction）**：在 expression 中写解析表达式，支持 x/y/z/t 变量（如 V01 顶面加载 2.5e-05*t）；表达式为空时校验页标出缺失；
- **分段线性（PiecewiseLinear）**：在 x、y 两栏按空格分隔填写数据对，适合试验复载曲线；x/y 个数不一致会被校验拒绝。

函数本身不求解，供边界条件 FunctionDirichletBC、体载荷 BodyForce 等按名引用。

![属性表单常规页：类别、状态与名称编辑](images/ch6-function.png)

## 边界条件与载荷（BC / Load）

**载荷（Load）**模块集中承载边界与载荷入口（BC 根节点也可在模型树直接维护）：

- **狄利克雷边界（DirichletBC）**：variable 选变量、boundary 选面组（可多选）、value 给常数值；
- **函数狄利克雷（FunctionDirichletBC）**：用 function 引用 [函数](ch6-analysis.html#s2) 节点，按时间/空间函数施加位移或温度；
- **面压力（Pressure）**：variable 必须是位移变量（disp_x/disp_y/dz 系），作用在面上形成法向压力。

**载荷**一侧：体载荷 BodyForce（value 或 function 二选一）、扩散 MatDiffusion、固体力学核 TensorMechanics 等，同步时生成 `[Kernels]` 块。BC/Loads 的属性表单都提供**组（Groups）**多选框，从物理组清单勾选后点「应用所选」写入 boundary/block，避免手填拼写错误。

![载荷模块页：通用载荷、体载荷、热源与 BC 入口](images/ch6-bc-load.png)

## 相互作用、约束与选择集

**相互作用（Interaction）**模块可新建通用相互作用或 **Tie 约束**（主/从面参数）。当前 Interaction 仅为数据节点，尚未接入求解输入生成，主要用于在建模期记录接触/绑定意图。

**选择集（Selection）**提供从网格复用几何集合的快捷路径：在模型树右键菜单选择**从物理组新建选择集（New Selection from Physical Group）**，选定一个物理组即生成同名选择集节点，供 BC、截面等按名引用，避免重复圈选。

![相互作用模块页：新建相互作用与 Tie 约束](images/ch6-interaction.png)
