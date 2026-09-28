# MOOSE 官方算例 Round A 产出物说明与人工验证

> 编制日期：2026-09-28
> 关联任务：`TASK-MOOSECASE-005A～005D`
> hcMooseApp commit：`2a51c4ece428a2ff74cc2d32e18b1d94f506298c`
> MOOSE commit：`4bce02d91b56c7ed845a5747df4d24f415592504`

## 1. Round A 解决什么问题

Round A 只回答一个前置问题：独立构建的 `hc_moose-opt` 是否真实具备运行五个冻结 MOOSE 官方算例的能力。

本轮使用官方 `.i` 直接调用计算节点上的 `hc_moose-opt`，完成输入解析、有限元求解和官方结果比较。它证明的是求解器基线，不代表 GMP-ISE 已能通过 GUI 创建、提交和后处理这些案例。

### 1.1 已交付

1. private 仓库 `RedCreationTech/hcMooseApp` 及标准 MOOSE Application 源码。
2. 固定到指定 commit 的 `moose/` submodule。
3. 独立 Conda 环境 `/home/kevin/hcMooseApp/.build/env`。
4. 启用 Heat Transfer、Solid Mechanics、Contact 的 `hc_moose-opt`。
5. 应用自带最小回归测试。
6. 五个官方案例的输入检查、真实求解、结果文件和官方比较证据。
7. 可重复执行的 `scripts/qualify.sh`。

### 1.2 尚未交付

- C06 多求解器路由和 LIMS `solver_id` 透传。
- GMP-ISE 中的 `hc_moose-opt` Application Profile 与 mapping。
- 通过 CAE GUI 建模并生成五个 `.i` 的能力。
- 通过 GMP-ISE 提交、监控、下载和导入结果的闭环。
- 五份面向用户的 CAE 人工操作文档。

以上内容从 Round B 和 MC01～MC05 产品化阶段开始，不属于 Round A。

## 2. 两条验证命令分别做什么

### TEST-MOOSE-A-001 应用最小回归

```bash
/home/kevin/miniforge3/bin/conda run --no-capture-output \
  -p .build/env ./run_tests -j4
```

- `conda run -p .build/env`：在 hcMooseApp 的独立构建环境中运行，避免依赖登录 shell 的 PATH。
- `./run_tests`：启动 MOOSE TestHarness，并调用当前目录的 `hc_moose-opt`。
- `-j4`：最多并行四个测试进程；当前只有一个测试。
- 当前测试会真实求解一个二维稳态扩散问题，生成 Exodus 结果，并与仓库中的 gold 结果执行 Exodiff。
- 预期结果：`1 passed, 0 failed`。

这条命令主要验证 Application 注册、框架链接、基础求解和输出链是否正常，不负责覆盖五个目标物理案例。

### TEST-MOOSE-A-002 五例资格验证

```bash
./scripts/qualify.sh
```

脚本对每个案例执行以下步骤：

```text
核对 MOOSE commit
  -> 把官方输入复制到隔离目录
  -> hc_moose-opt --check-input -i <input>
  -> hc_moose-opt -i <input>                # 真实有限元求解
  -> 生成 Exodus/CSV 结果
  -> 使用 MOOSE 官方 CSVDiff/Exodiff 或收敛判据验收
```

真实求解命令位于脚本 `run_case()` 中：

```bash
"$bin" -i "$input" > solve.log 2>&1
```

由于求解过程被保存到各案例的 `solve.log`，终端只显示汇总结论，因此表面上容易误认为脚本只做文件对照。文件对照发生在真实求解完成之后，用于判断新结果是否与官方 gold 数值一致。

## 3. 五个官方案例是什么

| ID | 物理问题 | 模型与载荷 | 时间设置 | 主要结果 | Round A 判据 |
|---|---|---|---|---|---|
| MC01 | 二维瞬态热传导 | `2 x 1` 矩形、`10 x 10` 网格；初温 300；左边恒温 300；右边温度 `300 + 5t` | `0～5`，`dt=1` | 温度场、中心线温度 CSV | 真实求解；CSV 与官方 gold 比较 |
| MC02 | 二维热-结构耦合 | MC01 热边界基础上增加体热源 `5e4`；线弹性 `E=1e9`、`ν=0.3`；热膨胀系数 `0.001`；有限应变 | `0～5`，`dt=1` | 温度、位移、von Mises 应力 | 真实求解；Exodus 与官方 gold 比较 |
| MC03 | 二维无摩擦接触 | 两根相邻立柱；底部固定；外侧压力 `1e4*t²` 将立柱压向彼此；penalty 接触 `1e9` | `0～5`，`dt=0.5` | 位移、应力和接触后的结构响应 | 真实求解；退出码成功且所有步收敛 |
| MC04 | 三维 J2 各向同性塑性 | 单立方体单元；顶部按 `0.0625t` 拉伸；`E=2.1e5`、`ν=0.3`；屈服应力 50；分段线性硬化 | `0～0.075`，`dt=0.00125`，共 60 步 | `stress_yy`、三个方向塑性应变 | 真实求解；Exodus 与官方 gold 比较 |
| MC05 | 三维 Newmark 瞬态动力学 | 细长杆；顶端固定、底端施加渐增压力；密度 7750；Newmark `β=0.25`、`γ=0.5` | `0～2`，`dt=0.1`，共 20 步 | 位移、速度、加速度、应力、应变 | 真实求解；Exodus 与官方 gold 比较 |

五个案例使用同一 `hc_moose-opt` 和同一 MOOSE commit，不允许逐例切换版本。

## 4. 计算产物在哪里

运行 `./scripts/qualify.sh` 后，结果位于：

```text
/home/kevin/hcMooseApp/.build/qualification/
├── MC01/
│   ├── check.log
│   ├── solve.log
│   ├── therm_step03_out.e
│   ├── therm_step03_out_t_sampler_0006.csv
│   └── compare.log
├── MC02/
│   ├── check.log
│   ├── solve.log
│   ├── thermomech_step01_out.e
│   └── compare.log
├── MC03/
│   ├── check.log
│   ├── solve.log
│   └── step01_out.e
├── MC04/
│   ├── check.log
│   ├── solve.log
│   ├── isotropic_plasticity_finite_strain_out.e
│   └── compare.log
├── MC05/
│   ├── check.log
│   ├── solve.log
│   ├── newmark_out.e
│   └── compare.log
└── solver.sha256
```

`check.log` 是输入解析证据，`solve.log` 是有限元迭代与收敛证据，`.e`/`.csv` 是计算结果，`compare.log` 是与官方基准比较的证据。

`.build/qualification/` 是可重复生成的本机产物，不提交 Git。官方输入和 gold 只保留在锁定的 `moose/` submodule 中。

## 5. 人工验证步骤

### TEST-MOOSE-A-003 身份核对

```bash
cd /home/kevin/hcMooseApp
git status --short
git rev-parse HEAD
git -C moose rev-parse HEAD
sha256sum hc_moose-opt
```

预期：工作树无修改；应用和 MOOSE commit 与本文开头一致；二进制 SHA-256 为 `7e14a4e8206dc3ac0848a3bb2df32f863f0b6f5ec53415d28951924780cc71d6`。

### TEST-MOOSE-A-004 查看真实求解证据

先运行资格脚本，再检查五份日志中的时间步和收敛记录：

```bash
./scripts/qualify.sh

for id in MC01 MC02 MC03 MC04 MC05; do
  echo "===== $id ====="
  grep -E "Time Step|Solve Converged|Solve Did NOT Converge" \
    ".build/qualification/$id/solve.log" | tail -10
done
```

预期：每个案例都有实际时间步和 `Solve Converged!`，不得出现 `Solve Did NOT Converge`。

### TEST-MOOSE-A-005 查看有限元结果

可用 ParaView 直接打开任意 `.e` 文件，例如：

```text
/home/kevin/hcMooseApp/.build/qualification/MC02/thermomech_step01_out.e
```

本步骤只用于人工查看官方案例求解结果。通过 GMP-ISE 导入结果目录并后处理属于后续产品闭环，不是 Round A 的准出条件。

## 6. Round A 准出结论

- `hc_moose-opt` 能够独立启动并执行有限元求解。
- 五个冻结官方输入均通过解析并完成真实计算。
- MC01、MC02、MC04、MC05 的结果通过官方数值基准比较。
- MC03 的所有时间步完成并收敛。
- 以上结论只证明求解器能力基线；下一阶段仍需证明 C06 路由和 GMP-ISE GUI 闭环。
