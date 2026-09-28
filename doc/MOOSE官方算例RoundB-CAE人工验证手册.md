# MOOSE 官方算例 Round B CAE 人工验证手册

> 版本：2026-09-28 v1
> 验证范围：TASK-MOOSECASE-006 求解器路由与身份握手
> 计算节点：`192.168.0.138`
> 说明：本手册不验收 MC01～MC05 的结构化 CAE 建模能力，也不以手改 `.i` 代替后续案例文档。

## 1. 验收目标

本轮只需确认以下事实：

1. CAE 可选择 `HC MOOSE CAE [prototype]`，且选择可随项目保存。
2. CAE 导出的快照会显式提交 `solver_id = hc_moose-opt`。
3. LIMS/C06 实际执行 HcMooseApp，而不是 DamSafetyApp 或 BlackBear。
4. Job 和归档记录能核对 requested/resolved id、commit 与二进制 SHA-256。
5. 运行结果可从 CAE 下载并导入 Results 工作窗。
6. 未注册的 `combined-opt` 被明确拒绝，不静默跑到默认求解器。

## 2. 验收前准备

### 2.1 本地程序

1. 拉取 `RedCreationTech/hcGmsh` 的 `main` 最新代码。
2. 在仓库根目录编译：

   ```bash
   cmake --build build -j4
   ```

3. 确认 LIMS API 已启动，CAE 使用的地址为 `http://127.0.0.1:8200`。
4. 启动 CAE：

   ```bash
   ./build/gmp_ise
   ```

### 2.2 验收项目

推荐复制一份以前已经通过的 G1 各向同性项目作为本轮专用项目。该项目应已有：

- 可用网格及 Physical Groups 清单；
- 有效的项目网格 SHA-256；
- “校验工作流”可达到 0 error。

不要使用包含 `AbaqusCDPStressUpdate` 的 CDP 项目；HcMooseApp 本轮没有复制 DamSafetyApp 的自定义 CDP Object。如无可复用项目，先按 `manual/test05.md` 第 2～4 节建立一份含网格和 Physical Groups 的测试项目。

## 3. RB-01 HC 档案显示与持久化

1. 在 CAE 中新建项目。
2. 打开顶部“应用”选择器。
3. 确认列表中存在 `HC MOOSE CAE [prototype]`。
4. 选中该档案，确认界面显示 prototype 提示，而不是 production。
5. 将项目另存为 `roundb-hc-routing.gmp.yaml`。
6. 关闭后重新打开该项目。
7. 确认“应用”仍为 `HC MOOSE CAE [prototype]`。

通过标准：

- [ ] 下拉项存在且可选。
- [ ] prototype 标识清晰可见。
- [ ] 保存重开后档案不丢失。

## 4. RB-02 从 CAE 提交 HcMooseApp 短算例

### 4.1 准备输入

1. 打开第 2.2 节的验收项目副本。
2. 把“应用”切换为 `HC MOOSE CAE [prototype]`。
3. 进入“作业工作窗 -> MOOSE 设置 -> 输入文件”。
4. 在 Template 中选择 `GeneratedMesh (Transient Diffusion)`，点击“应用模板”。
5. 确认 Generated Input 中包含 `[Mesh]`、`[Variables]`、`[Kernels]`、`[Executioner]` 和 `[Outputs]`。
6. 本用例不执行“同步模型到 MOOSE 输入”，避免已选短模板被项目模型树内容覆盖。
7. 点击“校验工作流”，确认为 0 error。若提示缺 Physical Groups 或网格 fingerprint，说明验收项目未满足第 2.2 节，应先恢复网格清单，不得绕过校验。

### 4.2 导出和提交

1. 点击“导出任务快照”，选择一个新的验收目录。
2. 记录新生成的 `case-<timestamp>` 目录。
3. 在“执行配置 -> Remote Job”中确认：
   - Server：`http://127.0.0.1:8200`
   - Project ID：`roundb-cae-validation`
4. 点击“提交作业”。
5. 记录 CAE 显示的 `job_id`。
6. 点击“刷新状态”，直到作业进入 `succeeded`。

通过标准：

- [ ] 提交返回新 `job_id`，不是 400/422。
- [ ] 状态依次进入 queued/running/succeeded，或在短作业过快时直接见到 succeeded。
- [ ] 作业命令中的真实可执行文件为 `/home/kevin/hcMooseApp/hc_moose-opt`。

## 5. RB-03 求解器身份和结果导入

1. 在该 Job 的实时文件或制品列表中打开 `task.md`。
2. 核对以下值：

   | 字段 | 预期值 |
   |---|---|
   | 请求 Solver ID | `hc_moose-opt` |
   | 实际 Solver ID | `hc_moose-opt` |
   | 选择方式 | `explicit` |
   | 实际应用 | `HcMooseApp` |
   | HcMooseApp commit | `2a51c4ece428a2ff74cc2d32e18b1d94f506298c` |
   | MOOSE commit | `4bce02d91b56c7ed845a5747df4d24f415592504` |
   | 二进制 SHA-256 | `7e14a4e8206dc3ac0848a3bb2df32f863f0b6f5ec53415d28951924780cc71d6` |

3. 在制品列表中下载主 Exodus 结果 `.e`。
4. 进入 Results 工作窗，选择下载的 `.e`。
5. 确认视口中出现网格/场结果，时间回放控件不是空状态。

通过标准：

- [ ] `task.md` 中的 id、应用、commit 和二进制哈希全部一致。
- [ ] 制品包可下载。
- [ ] Exodus 可在 CAE Results 中打开。

## 6. RB-04 未注册求解器不得回退

这是负向验收，预期为“提交被拒绝”。

1. 保存当前项目副本。
2. 将“应用”切换为 `MOOSE Combined（优化版） [prototype]`。
3. 重新点击“导出任务快照”，不得复用 RB-02 的 HC 快照。
4. 提交新快照。
5. 预期 CAE 显示 HTTP 422 / `agent_error`，消息含 `未知 solver_id: combined-opt`。
6. 确认不会生成一条实际执行 DamSafetyApp 的成功作业。

通过标准：

- [ ] 明确收到 422，且提示 `combined-opt`。
- [ ] 没有静默回退到 `dam-safety-app`。

## 7. 验收回填

| 用例 | 结果 | Job ID / 证据 | 备注 |
|---|---|---|---|
| RB-01 档案显示与持久化 | 待验证 |  |  |
| RB-02 HC 远端提交 | 待验证 |  |  |
| RB-03 身份与 Results 导入 | 待验证 |  |  |
| RB-04 Combined 负向拒绝 | 待验证 |  |  |

反馈时请按 `RB-01 PASS`、`RB-02 FAIL：<现象>` 的格式逐项回复。失败项请保留当时的项目副本、Job ID 和完整操作日志，不要为继续后续步骤而手改快照 manifest 或 `.i`。
