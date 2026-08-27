# Gmsh–MOOSE–ParaView 集成仿真环境

本项目是 Qt 6/C++ 实现的 CAE 工作台，提供几何、Gmsh 网格、MOOSE 输入与作业、
Exodus/VTK 后处理能力。完整设计与构建说明见 [readme.org](readme.org)。

## 远程作业地址配置

CAE 的“作业”功能只调用 LIMS Facade，不直接连接 C06 Agent，也不保存 C06 地址、
共享令牌或 SSH 凭据。调用链固定为：

```text
Gmsh-moose-parview → LIMS Facade → C06 Agent → DamSafetyApp/MOOSE
```

首次配置：

```bash
test -f .env || cp .env.example .env
```

默认内容：

```dotenv
GMP_LIMS_BASE_URL=http://127.0.0.1:8200
```

应用启动时按以下顺序查找 `.env`：

1. `GMP_ENV_FILE` 指定的文件；
2. 当前工作目录的 `.env`；
3. 可执行文件同级及其上一级目录的 `.env`。

操作系统已经设置的环境变量优先于 `.env`。如果 LIMS 与 CAE 都运行在当前电脑，
计算笔记本从办公室 A 的 `192.168.0.138` 切换到办公室 B 的
`192.168.0.121` 时，CAE 的 `.env` 无需修改；只需修改 LIMS 的
`api/.env` 中 `C06_AGENT_BASE_URL`。121 与 138 是同一台物理计算机。

如 LIMS 部署在另一台电脑，只需把 `GMP_LIMS_BASE_URL` 改为该 LIMS API 地址。
界面 Job 页的 `Server` 输入框仍可临时覆盖该值；当 `.env` 明确设置
`GMP_LIMS_BASE_URL` 时，它优先于历史 QSettings 配置。

`.env` 已被 Git 忽略，不应提交本地地址或凭据。

## 构建运行

```bash
./dev.sh
```

也可以直接通过进程环境覆盖：

```bash
GMP_LIMS_BASE_URL=http://127.0.0.1:8200 ./build/gmp_ise
```
