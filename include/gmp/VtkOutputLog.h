// VTK 消息输出重定向：追加写入 <exe_dir>/logs/vtk-output.log，
// 同时保留 VTK 原生输出窗口（Windows 上即警告弹窗，不抑制显示）。
// 仅在 GMP_ENABLE_VTK_VIEWER 构建时生效，否则 init 为空操作。
// 须在 QCoreApplication 构造之后调用（依赖 applicationDirPath）。
#pragma once

namespace gmp {

void init_vtk_output_log();

} // namespace gmp
