#!/usr/bin/env bash
# 开发模式启动脚本：配置 -> 构建 -> 运行 gmp_ise
#
# 用法:
#   ./dev.sh                          # 默认启用 Gmsh API + VTK Viewer
#   GMP_ENABLE_VTK_VIEWER=OFF ./dev.sh
#   ./dev.sh -DCMAKE_BUILD_TYPE=Release   # 额外参数透传给 cmake 配置
#
# 常用环境变量:
#   BUILD_DIR           构建目录 (默认 build)
#   JOBS                并行编译数 (默认 CPU 核数)
#   gmsh_DIR            Gmsh CMake 配置目录 (启用 Gmsh API 时需要)
#   VTK_DIR             VTK CMake 配置目录 (启用 VTK Viewer 时需要)
#   CMAKE_PREFIX_PATH   依赖查找前缀 (如 brew 安装的 qt/vtk/gmsh)

set -euo pipefail
cd "$(dirname "$0")"

BUILD_DIR="${BUILD_DIR:-build}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || nproc)}"

ENABLE_GMSH="${GMP_ENABLE_GMSH_GUI:-ON}"
ENABLE_VTK="${GMP_ENABLE_VTK_VIEWER:-ON}"

ARGS=(
  "-DCMAKE_BUILD_TYPE=Debug"
  "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
  "-DGMP_ENABLE_GMSH_GUI=${ENABLE_GMSH}"
  "-DGMP_ENABLE_VTK_VIEWER=${ENABLE_VTK}"
)
[ -n "${gmsh_DIR:-}" ] && ARGS+=("-Dgmsh_DIR=${gmsh_DIR}")
[ -n "${VTK_DIR:-}" ] && ARGS+=("-DVTK_DIR=${VTK_DIR}")
[ -n "${CMAKE_PREFIX_PATH:-}" ] && ARGS+=("-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")

cmake -S . -B "${BUILD_DIR}" "${ARGS[@]}" "$@"
cmake --build "${BUILD_DIR}" -j "${JOBS}"

echo "==> starting ${BUILD_DIR}/gmp_ise"
exec "${BUILD_DIR}/gmp_ise"
