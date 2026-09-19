#include "gmp/ViewportSelection.h"

namespace gmp {

void ViewportSelection::set_group_filter(int dim, int tag) {
  group_dim_ = dim;
  group_id_ = tag;
  cell_id_ = -1;
  if (dim < 0 || tag < 0) {
    // 清除筛选时同时回到完整实体视图：组/实体/单元全清（预览不动）。
    group_dim_ = -1;
    group_id_ = -1;
    entity_dim_ = -1;
    entity_tag_ = -1;
    cell_id_ = -1;
  }
}

void ViewportSelection::set_entity_filter(int dim, int tag) {
  entity_dim_ = dim;
  entity_tag_ = tag;
  cell_id_ = -1;
}

void ViewportSelection::set_preview(int dim, int tag) {
  const bool activate = dim >= 0 && tag >= 0;
  preview_dim_ = activate ? dim : -1;
  preview_tag_ = activate ? tag : -1;
}

void ViewportSelection::clear_all() {
  group_dim_ = -1;
  group_id_ = -1;
  entity_dim_ = -1;
  entity_tag_ = -1;
  cell_id_ = -1;
  preview_dim_ = -1;
  preview_tag_ = -1;
}

}  // namespace gmp
