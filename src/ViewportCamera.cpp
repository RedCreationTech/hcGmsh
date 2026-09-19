#include "gmp/ViewportCamera.h"

#include <algorithm>
#include <cmath>

namespace gmp {

CameraPresetResult view_preset_camera(const double bounds[6], int preset) {
  CameraPresetResult result;
  const double cx = (bounds[0] + bounds[1]) * 0.5;
  const double cy = (bounds[2] + bounds[3]) * 0.5;
  const double cz = (bounds[4] + bounds[5]) * 0.5;
  const double dx = bounds[1] - bounds[0];
  const double dy = bounds[3] - bounds[2];
  const double dz = bounds[5] - bounds[4];
  const double max_extent = std::max({dx, dy, dz, 1.0});
  const double dist = max_extent * 2.5;
  result.focal[0] = cx;
  result.focal[1] = cy;
  result.focal[2] = cz;
  if (preset == 0) {
    result.fit = true;
    return result;
  }
  if (preset == 1) {  // Front (+X)
    result.position[0] = cx + dist;
    result.position[1] = cy;
    result.position[2] = cz;
    result.view_up[2] = 1.0;
  } else if (preset == 2) {  // Right (+Y)
    result.position[0] = cx;
    result.position[1] = cy + dist;
    result.position[2] = cz;
    result.view_up[2] = 1.0;
  } else if (preset == 3) {  // Top (+Z)
    result.position[0] = cx;
    result.position[1] = cy;
    result.position[2] = cz + dist;
    result.view_up[1] = 1.0;
    result.view_up[2] = 0.0;
  } else {  // Iso
    result.position[0] = cx + dist;
    result.position[1] = cy + dist;
    result.position[2] = cz + dist;
    result.view_up[2] = 1.0;
  }
  return result;
}

PreviewCameraResult preview_focus_camera(const double bounds[6],
                                         const double mesh_bounds[6],
                                         double view_x, double view_y,
                                         double view_z) {
  PreviewCameraResult result;
  const bool valid_bounds =
      std::isfinite(bounds[0]) && std::isfinite(bounds[1]) &&
      std::isfinite(bounds[2]) && std::isfinite(bounds[3]) &&
      std::isfinite(bounds[4]) && std::isfinite(bounds[5]) &&
      bounds[1] >= bounds[0] && bounds[3] >= bounds[2] &&
      bounds[5] >= bounds[4];
  const double norm =
      std::sqrt(view_x * view_x + view_y * view_y + view_z * view_z);
  if (!valid_bounds || norm <= 1e-12) {
    return result;
  }
  const double center[3] = {0.5 * (bounds[0] + bounds[1]),
                            0.5 * (bounds[2] + bounds[3]),
                            0.5 * (bounds[4] + bounds[5])};
  const double dx = mesh_bounds[1] - mesh_bounds[0];
  const double dy = mesh_bounds[3] - mesh_bounds[2];
  const double dz = mesh_bounds[5] - mesh_bounds[4];
  const double distance =
      std::max(1.0, 1.35 * std::sqrt(dx * dx + dy * dy + dz * dz));
  result.valid = true;
  result.focal[0] = center[0];
  result.focal[1] = center[1];
  result.focal[2] = center[2];
  result.position[0] = center[0] + distance * view_x / norm;
  result.position[1] = center[1] + distance * view_y / norm;
  result.position[2] = center[2] + distance * view_z / norm;
  if (std::abs(view_z / norm) > 0.95) {
    result.view_up[1] = 1.0;
    result.view_up[2] = 0.0;
  } else {
    result.view_up[2] = 1.0;
  }
  return result;
}

}  // namespace gmp
