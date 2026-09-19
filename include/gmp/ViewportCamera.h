#pragma once

// v0.2 Stage 5 hc_ui 过渡层共享底座（Q5 批复：逻辑边界先行）。
// CameraController：视口相机预设与实体预览对焦的纯数学（不依赖 VTK/
// Qt Widgets，可无 Widget 测试）。实现逐字提取自 VtkViewer::
// apply_view_preset/preview_mesh_entity 的相机计算段，行为冻结。

namespace gmp {

// 视角预设结果。fit=true 表示“适配窗口”（调用方执行 ResetCamera 语义）。
struct CameraPresetResult {
  bool fit = false;
  double position[3] = {0.0, 0.0, 0.0};
  double focal[3] = {0.0, 0.0, 0.0};
  double view_up[3] = {0.0, 0.0, 1.0};
};

// bounds = [xmin,xmax,ymin,ymax,zmin,zmax]；preset: 0=适配 1=前(+X)
// 2=右(+Y) 3=顶(+Z) 其他=轴测。dist = max_extent*2.5，max_extent 下限 1。
CameraPresetResult view_preset_camera(const double bounds[6], int preset);

// 实体预览对焦。view_dir 为观察方向（未归一化）；distance 取
// max(1, 1.35*|mesh_bounds 对角线|)。bounds 无效或方向近零时 valid=false。
struct PreviewCameraResult {
  bool valid = false;
  double position[3] = {0.0, 0.0, 0.0};
  double focal[3] = {0.0, 0.0, 0.0};
  double view_up[3] = {0.0, 0.0, 1.0};
};
PreviewCameraResult preview_focus_camera(const double bounds[6],
                                         const double mesh_bounds[6],
                                         double view_x, double view_y,
                                         double view_z);

}  // namespace gmp
