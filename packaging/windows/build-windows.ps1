# packaging/windows/build-windows.ps1
# Windows 打包脚本：构建 gmp_ise (Release) 并收集全部运行时 DLL，产出 zip 包。
# 本脚本与 .github/workflows/windows-build.yml 共用同一套构建逻辑。
#
# 前置条件:
#   1. Visual Studio 2022 (含"使用 C++ 的桌面开发"工作负载) + CMake
#   2. vcpkg 及依赖 (首次安装需 2-4 小时编译, 之后走 vcpkg 缓存):
#        vcpkg install qtbase "vtk[qt]" gmsh yaml-cpp --triplet x64-windows-release --host-triplet x64-windows-release
#      说明: vtk 必须带 [qt] 特性 (QVTKOpenGLNativeWidget 需要 vtkGUISupportQt);
#      Qt 也从 vcpkg 装, 避免与官方 Qt 安装混链; 若 gmsh 缺 OCC 内核, 改用 gmsh[occ].
#
# 用法:
#   powershell -ExecutionPolicy Bypass -File packaging/windows/build-windows.ps1 -VcpkgRoot C:\vcpkg
#
# 产出: dist-windows\ (免安装目录) + gmp_ise-windows-x64.zip

[CmdletBinding()]
param(
  [string]$VcpkgRoot = $env:VCPKG_ROOT,   # vcpkg 根目录, 如 C:\vcpkg
  [string]$QtDir = "",                     # 可选, 外部 Qt6 目录 (msvc2022_64); 缺省用 vcpkg 的 Qt
  [string]$Triplet = "x64-windows-release",
  [string]$BuildDir = "build-windows",
  [string]$DistDir = "dist-windows",
  [string]$BuildType = "Release"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
if (-not $VcpkgRoot) { throw "请通过 -VcpkgRoot 或环境变量 VCPKG_ROOT 指定 vcpkg 根目录" }

$installed = Join-Path $VcpkgRoot "installed\$Triplet"
$toolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"

# windeployqt: 优先外部 -QtDir, 否则用 vcpkg 安装的 Qt6 工具
$windeployqt = ""
if ($QtDir) { $windeployqt = Join-Path $QtDir "bin\windeployqt.exe" }
if (-not (Test-Path $windeployqt)) {
  $windeployqt = Join-Path $installed "tools\Qt6\bin\windeployqt.exe"
}
if (-not (Test-Path $windeployqt)) {
  throw "未找到 windeployqt.exe, 请安装 Qt6 或先执行 vcpkg install qtbase"
}

$cmakeArgs = @(
  "-S", $RepoRoot, "-B", (Join-Path $RepoRoot $BuildDir),
  "-G", "Visual Studio 17 2022", "-A", "x64",
  "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
  "-DVCPKG_TARGET_TRIPLET=$Triplet",
  "-DGMP_ENABLE_GMSH_GUI=ON",
  "-DGMP_ENABLE_VTK_VIEWER=ON"
)
if ($QtDir) { $cmakeArgs += "-DCMAKE_PREFIX_PATH=$QtDir" }
# Windows 上如需 WSL 求解后端, 追加: "-DGMP_ENABLE_WSL_RUNNER=ON"

Write-Host "==> CMake configure"
cmake @cmakeArgs

Write-Host "==> Build ($BuildType)"
cmake --build (Join-Path $RepoRoot $BuildDir) --config $BuildType -j $env:NUMBER_OF_PROCESSORS

$exe = Join-Path $RepoRoot "$BuildDir\$BuildType\gmp_ise.exe"
if (-not (Test-Path $exe)) { throw "未找到构建产物: $exe" }

Write-Host "==> 收集运行时到 $DistDir"
$dist = Join-Path $RepoRoot $DistDir
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Path $dist | Out-Null
Copy-Item $exe $dist

& $windeployqt --release --compiler-runtime --no-translations (Join-Path $dist "gmp_ise.exe")

# vcpkg 依赖 DLL (VTK / Gmsh / yaml-cpp / OCC / HDF5 等传递依赖)
Copy-Item (Join-Path $installed "bin\*.dll") $dist

$zip = Join-Path $RepoRoot "gmp_ise-windows-x64.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $dist "*") -DestinationPath $zip
Write-Host "==> 打包完成: $zip"
