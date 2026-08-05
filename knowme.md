# GMP-ISE — Gmsh / MOOSE / ParaView 集成仿真环境

> Know-me 文档：项目功能速览 + 开发依赖安装 + 开发模式启动方式。

## 1. 项目是什么

GMP-ISE (`gmp_ise`) 是一个基于 **Qt 6 (C++)** 的独立桌面仿真应用，把仿真全流程整合在统一界面下，目标体验对齐 **Abaqus** 的主线工作流：

```
Part -> Materials/Sections -> Assembly -> Step -> BC/Load -> Mesh -> Job -> Results
```

- **前处理**：直接调用 **Gmsh C++ API**（OCC 几何内核），支持 STEP/IGES/BREP 导入、几何基元/变换/布尔运算、物理组管理、网格尺寸场、2D/3D 网格算法与质量检查。
- **求解**：自动生成 **MOOSE** 输入文件（模型树 -> MOOSE block 联动），通过 `mpiexec` / QProcess 以**独立进程**异步运行，支持 local / WSL / remote(SSH) 三种 Runner。
- **后处理**：内嵌 **VTK** 视图加载 **ExodusII (.e)** 结果，支持标量/向量/变形/切片/探针/图表/表格等可视化；预留 ParaView SDK / Catalyst 原位分析接口。
- **数据打通**：文件级 `.msh -> MOOSE -> .e` 为主，内存 API 级实时网格预览为辅。

技术栈：Qt 6 Widgets + QVTKOpenGLNativeWidget、Gmsh API、VTK、yaml-cpp（项目文件 YAML 持久化），CMake 构建，MOOSE 作为外部子模块进程调度。

源码布局：

```
src/            Qt 应用源码（MainWindow / GmshPanel / MoosePanel / VtkViewer / Runner 等）
include/gmp/    头文件
external/       子模块：gmsh、moose、yaml-cpp
out/            运行时输出（网格/结果，不入版本库）
```

## 2. 开发依赖安装

### 2.0 本机开发环境定位（重要）

本机配置（2026-08 实测）：

- MacBookPro16,2（Intel i7-1068NG7，4 核 8 线程）/ **32GB 内存** / macOS 26.5.1
- 磁盘 466GB，**仅剩约 27GB 可用** —— 磁盘是主要瓶颈
- 已有：Xcode CLT（Apple clang 21）、cmake、Homebrew；缺：ninja、conda、MPI

**职责划分决策**：

- **本机**：只做 UI / 网格 / 可视化开发（Qt + Gmsh + VTK），**不安装 MOOSE 求解环境**。
- **求解**：MOOSE 部署在另一台机器上，本机通过 SSH（`remote` Runner）调度，或拷贝 `.e` 结果回本机做可视化调试。

因此依赖原则是：**重依赖一律用 Homebrew 二进制，源码只保留必需的 yaml-cpp**——既省磁盘（避免 MOOSE 工具链约 12-15GB 占用），也免去 4 核 CPU 上数小时的编译。

### 2.1 拉取子模块（只需 yaml-cpp）

```bash
git submodule update --init external/yaml-cpp
```

说明：`CMakeLists.txt` 无条件 `add_subdirectory(external/yaml-cpp)`，不初始化则 CMake 配置直接失败，**这是唯一必须拉的子模块**。

- `external/gmsh`：**跳过**。Gmsh 走 Homebrew 二进制提供 SDK；只有需要跟踪上游最新 libgmsh 时才初始化并从源码构建（额外占用约 3-4GB）。
- `external/moose`：**跳过**。本机不求解，见 2.4。

### 2.2 macOS（Homebrew，本机推荐）

```bash
# UI / 可视化 / 网格 / 构建工具（一次装齐）
brew install qt vtk gmsh cmake ninja

# MPI（可选，仅当 GMP_ENABLE_MPI=ON 时需要）
brew install open-mpi
```

安装后用以下方式让 CMake 找到依赖（配合 `dev.sh`）：

```bash
export CMAKE_PREFIX_PATH="$(brew --prefix qt);$(brew --prefix vtk);$(brew --prefix gmsh)"
```

### 2.3 Linux（Ubuntu 22.04+，备选开发平台）

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build qt6-base-dev libvtk9-dev libgmsh-dev

# MPI（可选）
sudo apt install -y libopenmpi-dev
```

### 2.4 MOOSE 求解器（本机不装，部署在求解机）

本机 UI 不依赖 MOOSE，跳过 conda/moose-dev 环境（可省 12-15GB 磁盘与数小时编译）。

在**求解机**上按官方流程安装（推荐 conda 工具链 + 源码构建）：

```bash
conda create -n moose -c https://conda.software.inl.gov/public -c conda-forge moose-dev
conda activate moose
git clone https://github.com/idaholab/moose && cd moose
./scripts/update_and_rebuild_petsc.sh && ./scripts/update_and_rebuild_libmesh.sh
cd test && make -j "$(nproc)"
```

本机与求解机的协作方式：

1. 本机 MoosePanel 生成 `.i` 输入 + `.msh` 网格；
2. 通过 SSH（`remote` Runner，预留）下发到求解机执行 `mpiexec ... moose_test-opt -i case.i`；
3. 求解机产出的 `.e`（ExodusII）结果拷回本机，在 Visualization/Results 中直接加载调试。

## 3. 开发模式启动

仓库根目录提供 `dev.sh`：一条命令完成「配置 -> 构建 -> 运行」。

```bash
./dev.sh
```

默认行为：`Debug` 构建、导出 `compile_commands.json`（供 clangd 使用）、启用 `GMP_ENABLE_GMSH_GUI=ON` 与 `GMP_ENABLE_VTK_VIEWER=ON`，构建目录 `build/`。

启用 Gmsh/VTK 时需要让 CMake 找到依赖，例如（Homebrew）：

```bash
export CMAKE_PREFIX_PATH="$(brew --prefix qt)"
./dev.sh \
  -Dgmsh_DIR="$(brew --prefix gmsh)/share/gmsh" \
  -DVTK_DIR="$(brew --prefix vtk)/lib/cmake/vtk-9.6"
```

> 注意：Homebrew 的 gmsh 配置文件在 `share/gmsh/`（不是 `lib/cmake/gmsh`）。
> 另外 brew 版 gmsh 的 `gmshTargets.cmake` 引用了 `MacOSX14.sdk` 的绝对路径，本机 CLT 只有 13.x SDK，
> 已将其替换为 `MacOSX.sdk`；若日后 `brew upgrade gmsh` 覆盖了该文件，需要重新执行：
>
> ```bash
> sed -i '' 's|CommandLineTools/SDKs/MacOSX14.sdk|CommandLineTools/SDKs/MacOSX.sdk|g' \
>   "$(brew --prefix gmsh)/share/gmsh/gmshTargets.cmake"
> ```

常用开关：

```bash
# 最小构建（不启用 Gmsh/VTK，仅需 Qt6）
GMP_ENABLE_GMSH_GUI=OFF GMP_ENABLE_VTK_VIEWER=OFF ./dev.sh

# Release 构建 / 追加任意 CMake 参数
./dev.sh -DCMAKE_BUILD_TYPE=Release
```

日常增量开发无需重复配置，直接：

```bash
cmake --build build -j4 && ./build/gmp_ise
```

## 4. Windows EXE 打包

**本机（macOS）无法产出 Windows EXE**（Qt6/VTK/Gmsh 全栈需 Windows 目标构建）。依赖走 **conda-forge 预编译包**（`qt6-main / vtk / gmsh`，零源码编译，CI 全程约 10-20 分钟），本机与 CI 共用同一脚本 `packaging/windows/build-windows.ps1`。

### 4.1 GitHub Actions（推荐）

`.github/workflows/windows-build.yml`：推送 `v*` 标签或在 Actions 页手动触发，`windows-latest` runner 上用 micromamba 安装 conda-forge 预编译依赖 → 构建 → `windeployqt` 打包 → 上传 `gmp_ise-windows-x64.zip` artifact。

> 历史说明：此前使用 vcpkg 源码编译路线，因 runner 磁盘上限反复失败（Debug+Release 双份 / host triplet 遗漏 / VTK 构建树堆积），已弃用；conda 版 vtk 官方启用了 `GUISupportQt/RenderingQt`，满足 `QVTKOpenGLNativeWidget` 需求。

### 4.2 Windows 本机打包

前置：VS 2022（C++ 桌面开发）+ CMake + [micromamba](https://mamba.readthedocs.io/)，然后：

```powershell
micromamba create -n gmp-win -c conda-forge qt6-main vtk gmsh
powershell -ExecutionPolicy Bypass -File packaging/windows/build-windows.ps1 `
  -CondaPrefix "$env:USERPROFILE\micromamba\envs\gmp-win"
```

产出 `dist-windows\`（免安装目录）与 `gmp_ise-windows-x64.zip`。

如坚持 vcpkg 源码路线（备选）：

```powershell
vcpkg install qtbase "vtk[qt]" gmsh yaml-cpp --triplet x64-windows-release --host-triplet x64-windows-release
powershell -ExecutionPolicy Bypass -File packaging/windows/build-windows.ps1 -VcpkgRoot C:\vcpkg
```

说明：

- Windows 端**不需要 MOOSE 求解环境**（求解在远程机器）；如需 WSL 求解后端，构建时追加 `-DGMP_ENABLE_WSL_RUNNER=ON`。

## 5. 参考文档

- `readme.org` — 架构设计（L1–L5 分层）、数据打通协议、平台矩阵与 Roadmap
- `progress.org` — 迭代计划与每次进度记录
