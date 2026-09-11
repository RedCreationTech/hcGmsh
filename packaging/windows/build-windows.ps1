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
  # conda 版 qt6-main: 必须用 lib\qt6\bin 下的 windeployqt.exe,
  # 其同目录有 qtpaths.exe 供其查询插件; bin\ 下的 windeployqt6.exe
  # 会因找不到 qtpaths 而部署失败并以非零退出
  $windeployqtCandidates = @(
    (Join-Path $libPrefix "lib\qt6\bin\windeployqt.exe"),
    (Join-Path $dllDir "windeployqt.exe"),
    (Join-Path $dllDir "windeployqt6.exe")
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

# 运行时数据：MOOSE 模板 / 应用档案 / 映射注册表。
# 应用按 appDir 候选路径解析 templates/moose（ApplicationProfile.cpp /
# MooseTemplates.cpp），免安装包必须把它放在 exe 旁的 templates\ 下，
# 否则档案选择器显示“未配置”、模板下拉为空。
$tplSrc = Join-Path $RepoRoot "templates\moose"
if (Test-Path $tplSrc) {
  New-Item -ItemType Directory -Path (Join-Path $dist "templates") -Force | Out-Null
  Copy-Item $tplSrc (Join-Path $dist "templates") -Recurse
  Write-Host "==> deployed runtime data: templates/moose"
}

if ($CondaPrefix) {
  # conda 版 Qt 布局与 windeployqt 的路径推导不兼容(前缀重复拼接),
  # 改为手动部署: Qt6 DLL 已由下方 *.dll 复制覆盖, 这里补插件与 MSVC 运行时
  $qtPlugins = Join-Path $libPrefix "lib\qt6\plugins"
  foreach ($dir in @("platforms", "styles", "imageformats")) {
    $src = Join-Path $qtPlugins $dir
    if (Test-Path $src) {
      Copy-Item $src $dist -Recurse
      Write-Host "==> deployed Qt plugin: $dir"
    }
  }
  if (-not (Test-Path (Join-Path $dist "platforms\qwindows.dll"))) {
    throw "Qt platforms 插件部署失败 (qwindows.dll 缺失), 打包中止"
  }
  # MSVC 运行时 (msvcp140/vcruntime140 系列, 取自 VS 安装目录的 Redist)
  $crt = Get-ChildItem "C:\Program Files\Microsoft Visual Studio\*\*\VC\Redist\MSVC\*\x64\*.CRT\*" `
    -Include msvcp140.dll, vcruntime140.dll, vcruntime140_1.dll -ErrorAction SilentlyContinue
  if ($crt) {
    Copy-Item $crt.FullName $dist
    Write-Host "==> deployed MSVC CRT ($($crt.Count) dlls)"
  } else {
    Write-Warning "未找到 MSVC CRT redist, 目标机器需自行安装 VC++ Redistributable"
  }
} else {
  & $windeployqt --release --compiler-runtime --no-translations (Join-Path $dist "gmp_ise.exe")
  if (-not (Test-Path (Join-Path $dist "platforms\qwindows.dll"))) {
    throw "windeployqt 未部署 platforms 插件 (qwindows.dll 缺失), 打包中止"
  }
}

# 依赖 DLL (VTK / Gmsh / yaml-cpp / OCC / HDF5 等传递依赖)
Copy-Item (Join-Path $dllDir "*.dll") $dist

# 校验应用及 Qt 动态插件的 PE 依赖闭包。Conda 的大部分 DLL 位于
# Library\bin，但 python3xx.dll 等运行时位于环境根目录；仅复制前者会导致
# 包在 CI 机器可运行、在干净客户机启动失败。这里会补齐能在构建环境找到的
# 传递依赖，并让任何遗漏在 Action 内直接失败。GMP-ISE 不嵌入 Python，因此
# Python 运行时依赖属于 VTK 链接范围回归，不能静默带入发布包。
function Find-Dumpbin {
  $command = Get-Command "dumpbin.exe" -ErrorAction SilentlyContinue
  if ($command) { return $command.Source }

  $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
  if (-not (Test-Path $vswhere)) {
    throw "dumpbin.exe and vswhere.exe are unavailable; cannot validate packaged DLL dependencies"
  }
  $vsInstall = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
  if (-not $vsInstall) {
    throw "Visual Studio C++ tools were not found; cannot validate packaged DLL dependencies"
  }
  $matches = Get-ChildItem `
    (Join-Path $vsInstall "VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe") `
    -File -ErrorAction SilentlyContinue | Sort-Object FullName -Descending
  if (-not $matches) {
    throw "dumpbin.exe was not found under Visual Studio: $vsInstall"
  }
  return $matches[0].FullName
}

function Get-ImportedDllNames([string]$Binary, [string]$Dumpbin) {
  $output = & $Dumpbin /nologo /dependents $Binary 2>&1
  if ($LASTEXITCODE -ne 0) {
    throw "dumpbin failed for '$Binary':`n$($output -join "`n")"
  }
  return @($output | ForEach-Object {
    if ($_ -match '^\s+([A-Za-z0-9_.+-]+\.dll)\s*$') { $Matches[1] }
  } | Sort-Object -Unique)
}

function Test-WindowsSystemDll([string]$Name) {
  if ($Name -match '^(api-ms-win-|ext-ms-win-)') { return $true }
  foreach ($systemDir in @(
    (Join-Path $env:SystemRoot "System32"),
    (Join-Path $env:SystemRoot "SysWOW64")
  )) {
    if (Test-Path (Join-Path $systemDir $Name)) { return $true }
  }
  return $false
}

function Find-PackagedDll([string]$Name, [string]$Root) {
  return Get-ChildItem $Root -Recurse -File -Filter $Name -ErrorAction SilentlyContinue |
    Select-Object -First 1
}

$dumpbin = Find-Dumpbin
$runtimeSearchDirs = @($dllDir)
if ($CondaPrefix) { $runtimeSearchDirs += $CondaPrefix }
if ($QtDir) { $runtimeSearchDirs += (Join-Path $QtDir "bin") }

$dependencyRoots = @((Join-Path $dist "gmp_ise.exe"))
foreach ($pluginDir in @("platforms", "styles", "imageformats")) {
  $path = Join-Path $dist $pluginDir
  if (Test-Path $path) {
    $dependencyRoots += @(Get-ChildItem $path -Filter "*.dll" -File | ForEach-Object FullName)
  }
}

$queue = [System.Collections.Generic.Queue[string]]::new()
$seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
$missingDependencies = [System.Collections.Generic.List[string]]::new()
$pythonDependencies = [System.Collections.Generic.List[string]]::new()
foreach ($root in $dependencyRoots) { $queue.Enqueue($root) }

while ($queue.Count -gt 0) {
  $binary = $queue.Dequeue()
  $binaryKey = [System.IO.Path]::GetFullPath($binary)
  if (-not $seen.Add($binaryKey)) { continue }

  foreach ($dependency in (Get-ImportedDllNames $binary $dumpbin)) {
    if ($dependency -match '^python(?:3|\d{2,})\.dll$') {
      $pythonDependencies.Add("$([System.IO.Path]::GetFileName($binary)) -> $dependency")
    }

    $bundled = Find-PackagedDll $dependency $dist
    if ($bundled) {
      $queue.Enqueue($bundled.FullName)
      continue
    }
    if (Test-WindowsSystemDll $dependency) { continue }

    $source = $null
    foreach ($searchDir in $runtimeSearchDirs) {
      $candidate = Join-Path $searchDir $dependency
      if (Test-Path $candidate) {
        $source = $candidate
        break
      }
    }
    if ($source) {
      $target = Join-Path $dist $dependency
      Copy-Item $source $target -Force
      Write-Host "==> 补齐传递运行时: $dependency"
      $queue.Enqueue($target)
      continue
    }

    $missingDependencies.Add("$([System.IO.Path]::GetFileName($binary)) -> $dependency")
  }
}

if ($pythonDependencies.Count -gt 0) {
  $details = $pythonDependencies | Sort-Object -Unique
  throw "Unexpected Python runtime dependency detected; check VTK component linkage:`n$($details -join "`n")"
}
if ($missingDependencies.Count -gt 0) {
  $details = $missingDependencies | Sort-Object -Unique
  throw "Packaged runtime dependency closure is incomplete:`n$($details -join "`n")"
}
Write-Host "==> DLL 依赖闭包校验通过 ($($seen.Count) 个二进制文件)"

$zip = Join-Path $RepoRoot "gmp_ise-windows-x64.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $dist "*") -DestinationPath $zip
Write-Host "==> 打包完成: $zip"
# 显式成功退出: 防止上游原生命令残留的非零 LASTEXITCODE 让 CI 误判失败
exit 0
