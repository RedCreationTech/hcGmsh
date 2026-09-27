# 作业

作业工作窗（Job Workspace）含**作业列表**与 **MOOSE 设置**两大页签；后者再分「算例设置 / 执行配置 / 输入文件」三子页，完成「配路径 → 配执行 → 配输入 → 提交监控」闭环。结果查看见 [结果](ch9-results.html)。

## 算例设置

**算例设置（Case Setup）**子页集中配置路径与网格：

- **路径（Paths）**：可执行文件（留空时按环境变量 → PATH → `external/moose` 逐级自动探测）、输入文件（`.i`）与工作目录，各行右侧**浏览...（Browse...）**按钮可选路径，历史可下拉复用；
- **网格（Mesh）**：网格文件下拉选择已生成的 `.msh`，点**插入网格块（Insert Mesh Block）**把输入文件中的 `[Mesh]` 块替换为 `type = FileMesh, file = <路径>`；
- **物理组（Physical Groups）**：同步网格后列出只读边界组清单，点**由物理组生成边界（Insert BCs From Groups）**按边界组批量生成 DirichletBC 的 `[BCs]` 块（组名中的非字母数字字符自动转为 `_`）；
- 页底常驻**校验工作流（Validate Workflow）**、**运行（Run，F5）**、**检查输入（Check Input，Ctrl+K）**与**停止（Stop，Shift+F5）**。

![算例设置子页：路径、网格块插入与由物理组生成边界](images/ch8-setup.png)

## 执行配置：MPI 与远程 LIMS

**执行配置（Execution）**子页配置运行方式：

- **MPI**：勾选**使用 mpiexec（Use mpiexec）**并设置进程数（1–4096），运行命令自动变为 `mpiexec -n <ranks> <exec> -i <input>`；
- **Runner**：本地（Local）/ WSL（仅 Windows）/ 远程（Remote）三种运行器；
- **额外参数（Extra Args）**：空格分词的附加命令行参数；
- **远程作业（Remote Job (via LIMS Facade)）**：填写 LIMS Facade 服务地址（可用环境变量 `GMP_LIMS_BASE_URL` 预设），点**提交（Submit）**把最新导出的输入快照提交到远程计算节点，不直连计算节点、不保存凭据。

以上设置均持久化，下次启动自动恢复。

![执行配置子页：MPI、运行器与远程 LIMS 提交](images/ch8-execution.png)

## 输入文件：生成、自定义与报告

**输入文件（Input File）**子页分三页管理同一份 `.i`：

- **生成（Generated Input）**：模型树同步（**模型（Model）→ 同步模型到 MOOSE 输入**，Ctrl+Shift+R）后的自动生成文本，只读核对用；
- **自定义（Custom）**：可直接编辑的输入文本，修改后**写入输入（Write Input）**落盘；自定义内容优先于生成内容用于运行；
- **报告（Report）**：输入检查与运行报告（日志摘要、产出文件清单）。

底部动作行提供运行、检查输入与停止；运行日志实时镜像到主窗口底部控制台，并自动识别产出的 `.e` 结果通知视口。

![输入文件子页生成页：自动生成的 MOOSE 输入文本](images/ch8-input.png)

## 作业监控、下载与取消

**作业列表（Jobs）**页签是 LIMS 式作业监控台，本地与远程作业并列：

- **作业表**：名称、状态、算例、进度、开始时间、时长、类型、可执行文件与结果共 9 列；操作行提供**运行 / 停止 / 重试（Retry）/ 打开日志（Open Log）/ 打开结果（Open Result）**；
- **状态筛选与刷新**：按 排队/运行/完成/失败/已取消 筛选，支持手动**刷新**与**自动（5 秒）**轮询；
- **详情面板**：选中行显示进度条、执行状态（输入/PID/并行数/CPU/内存/时间步/物理时间/收敛/心跳等）与**制品（Artifacts）**清单，远程作业可**刷新文件**并**下载选中制品（Download Selected）**；
- 远程作业支持**取消（Cancel）**；产出的 Exodus 结果自动登记到模型树 Results 根与 [结果](ch9-results.html) 工作窗。

![作业列表页：九列作业表、状态筛选与详情面板](images/ch8-monitor.png)
