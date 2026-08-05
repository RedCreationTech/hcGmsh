# packaging/windows/build-windows.ps1
# Windows 打包脚本：构建 gmp_ise (Release) 并收集全部运行时 DLL，产出 zip 包。
# 本脚本与 .github/workflows/windows-build.yml 共用同一套构建逻辑。
#
# 依赖两种来源（二选一）:
#   A. conda-forge 预编译包 (CI 默认, 零编译):
#        micromamba create -n gmp-win -c conda-forge qt6-main vtk gmsh
#      然后:
#        build-windows.ps1 -CondaPrefix <env路径>   # 如 ~\micromamba\envs\gmp-win
#      说明: conda 版 vtk 已启用 GUISupportQt/RenderingQt, 满足 QVTKOpenGLNativeWidget.
#   B. vcpkg 源码编译 (本机备选, 首次需 2-4 小时):
#        vcpkg install qtbase "vtk[qt]" gmsh yaml-cpp --triplet x64-windows-release --host-triplet x64-windows-release
#      然后:
#        build-windows.ps1 -VcpkgRoot C:\vcpkg
#
# 前置条件: Visual Studio 2022+ (含"使用 C++ 的桌面开发"工作负载) + CMake
# 产出: dist-windows\ (免安装目录) + gmp_ise-windows-x64.zip

[CmdletBinding()]
param(
  [string]$CondaPrefix = "",               # conda 环境根目录 (含 Library 子目录)
  [string]$VcpkgRoot = $env:VCPKG_ROOT,    # vcpkg 根目录, 如 C:\vcpkg
  [string]$QtDir = "",                      # 可选, 外部 Qt6 目录; 缺省从依赖源探测
  [string]$Triplet = "x64-windows-release",
  [string]$BuildDir = "build-windows",
  [string]$DistDir = "dist-windows",
  [string]$BuildType = "Release"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
if (-not $CondaPrefix -and -not $VcpkgRoot) {
  throw "请指定依赖来源: -CondaPrefix <conda环境路径> 或 -VcpkgRoot <vcpkg根目录>"
}

$cmakeArgs = @(
  "-S", $RepoRoot, "-B", (Join-Path $RepoRoot $BuildDir),
  # 不写死生成器: GitHub runner 已预装更新的 VS (如 VS18), 让 CMake 自动探测
  "-DGMP_ENABLE_GMSH_GUI=ON",
  "-DGMP_ENABLE_VTK_VIEWER=ON"
)
# Windows 上如需 WSL 求解后端, 追加: "-DGMP_ENABLE_WSL_RUNNER=ON"

if ($CondaPrefix) {
  # conda-forge 布局: 库与工具在 <env>\Library 下
  $libPrefix = Join-Path $CondaPrefix "Library"
  $dllDir    = Join-Path $libPrefix "bin"
  # conda 版 qt6-main 的 windeployqt 可能叫 windeployqt6.exe 或在 lib\qt6\bin 下
  $windeployqtCandidates = @(
    (Join-Path $dllDir "windeployqt.exe"),
    (Join-Path $dllDir "windeployqt6.exe"),
    (Join-Path $libPrefix "lib\qt6\bin\windeployqt.exe")
  )
  $cmakeArgs += "-DCMAKE_PREFIX_PATH=$libPrefix"
  # gmsh 的 cmake 配置可能在 share/gmsh 下
  $gmshCmake = Join-Path $libPrefix "share\gmsh"
  if (Test-Path (Join-Path $gmshCmake "gmshConfig.cmake")) {
    $cmakeArgs += "-Dgmsh_DIR=$gmshCmake"
    # conda 包的 gmshTargets-release.cmake 把 DLL 记录在 lib/gmsh.dll,
    # 实际在 bin/gmsh.dll, 配置前修正, 否则 find_package 校验直接 FATAL
    $gmshTargetsRel = Join-Path $gmshCmake "gmshTargets-release.cmake"
    if (Test-Path $gmshTargetsRel) {
      (Get-Content $gmshTargetsRel -Raw) -replace '/lib/gmsh\.dll"', '/bin/gmsh.dll"' |
        Set-Content $gmshTargetsRel -NoNewline
      Write-Host "==> patched gmshTargets-release.cmake (dll path)"
    }
  }
} else {
  $installed = Join-Path $VcpkgRoot "installed\$Triplet"
  $dllDir    = Join-Path $installed "bin"
  $toolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
  $windeployqtCandidates = @((Join-Path $installed "tools\Qt6\bin\windeployqt.exe"))
  $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
  $cmakeArgs += "-DVCPKG_TARGET_TRIPLET=$Triplet"
}
if ($QtDir) {
  $cmakeArgs += "-DCMAKE_PREFIX_PATH=$QtDir"
  $windeployqtCandidates = @((Join-Path $QtDir "bin\windeployqt.exe")) + $windeployqtCandidates
}
$windeployqt = $windeployqtCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $windeployqt) {
  throw "未找到 windeployqt.exe, 候选路径: $($windeployqtCandidates -join '; ')"
}

# 依赖 DLL 目录加入 PATH: CMake AUTOMOC 会试运行 moc.exe,
# conda 环境的 Qt6Core.dll 等在 Library\bin, 不在 PATH 会报 0xc0000135
$env:PATH = "$dllDir;$env:PATH"

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

# 依赖 DLL (VTK / Gmsh / yaml-cpp / OCC / HDF5 等传递依赖)
Copy-Item (Join-Path $dllDir "*.dll") $dist

$zip = Join-Path $RepoRoot "gmp_ise-windows-x64.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $dist "*") -DestinationPath $zip
Write-Host "==> 打包完成: $zip"
