# 网格

网格工作窗（Mesh Workspace）按「模型 / 几何 / 物理组与网格场 / 网格」四个页签组织，覆盖几何导入、几何编辑、分组与剖分输出全链路。网格结果供 [作业](ch8-job.html) 生成 FileMesh 输入，物理组供 [分析定义](ch6-analysis.html) 引用。

## 导入几何与示例盒

**模型（Model）**页管理几何来源：

- **打开几何（Open Geometry）**：导入 `.geo / .geo_unrolled / .step / .stp / .iges / .igs / .brep`（STEP/IGES/BREP 走 OpenCASCADE 内核）；打开项目时若开启「项目加载时自动重载几何」会自动恢复上次几何；
- **示例盒（Sample Box）**：空模型时自动生成带 solid 体组与 boundary 面组的示例盒，Size X/Y/Z 可调，用于快速验证流程；
- **统计（Summary）**：显示实体数量（点/线/面/体），**清空模型**一键归零；
- 勾选「导入后自动剖分」可在导入后立即生成网格。

![网格工作窗模型页：几何路径、示例盒与实体统计](images/ch7-import.png)

## 几何基元、变换与布尔

**几何（Geometry）**页提供三类编辑能力：

- **基元（Primitives）**：盒体、圆柱、球，指定原点、边长/轴向与半径后点**添加基元**；
- **变换（Transform）**：在 Selection 行用维度 + ID（支持 `1,2` 或 `2:5`，留空为全部）或拾取按钮选定实体后，执行平移、绕任意轴旋转或缩放；非 OCC 实体会被自动过滤并记入日志；
- **布尔（Boolean）**：选择 Obj/Tool 两组实体，勾选是否保留原实体，执行**融合（Fuse）/ 剪切（Cut）/ 交集（Intersect）**。

![几何页：基元、变换与布尔运算](images/ch7-geometry.png)

## 物理组与网格场

**物理组与网格场（Groups & Fields）**页：

- **物理组（Physical Groups）**：Dim + 名称 + 实体清单新建分组，支持更新与删除选中；统计表列出各组的维度、Tag、名称、实体数与单元数，选中行会在视口高亮。组名会同步为 MOOSE 的边界/块名称（BC 的 boundary、载荷的 block 均按此引用）；
- **网格场（Mesh Fields）**：设置 Distance 场的 DistMin/DistMax 与 Threshold 场的 SizeMin/SizeMax，点**应用场**对目标实体做局部加密，已创建的场自动设为背景场，**清除场**一键还原全局尺寸控制。

![物理组页：分组清单、统计表与视口高亮](images/ch7-groups.png)

## 生成网格与输出

**网格（Mesh）**页控制剖分与输出：

- **网格尺寸（Mesh Size）**：全局尺寸（默认 0.2）；**生成维度**可选自动/1D/2D/3D；**单元阶数**支持 1~4 阶及高阶网格光顺策略；**MSH 版本** 2.2/4.1；**2D/3D 算法**、四/六面体重组、光顺与优化开关按需调整；
- **网格输出（Mesh Output）**：默认 `<工作目录>/out/box.msh`，可**选择输出**改路径；**导出几何**支持 `.brep / .geo_unrolled`；
- 点**生成网格**（或菜单**网格（Mesh）→ 生成网格**，Ctrl+M）开始剖分；生成后日志输出节点/单元数、单元质量 minSICN  min/mean/max 与各物理组单元计数。

生成的网格文件节点自动登记到模型树 Mesh 根，供快照打包与作业提交。

![网格页：尺寸、阶数、算法与生成网格](images/ch7-mesh.png)
