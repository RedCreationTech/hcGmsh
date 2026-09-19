#pragma once

// v0.2 Stage 5 hc_ui 过渡层共享底座（Q5 批复：逻辑边界先行）。
// SelectionOverlay：舞台选择/过滤/实体预览的纯状态机（不依赖 VTK/
// Qt Widgets，可无 Widget 测试）。字段公开以便 VtkViewer 既有
// handle_pick 等细粒度写点逐字迁移；语义化迁移入口用下面的方法：
// set_group_filter 负值清除时组/实体/单元同清（不动预览）；
// set_entity_filter 仅清单元；预览独立设置/清除。

namespace gmp {

struct ViewportSelection {
  int group_dim_ = -1;
  int group_id_ = -1;
  int entity_dim_ = -1;
  int entity_tag_ = -1;
  int cell_id_ = -1;
  int preview_dim_ = -1;
  int preview_tag_ = -1;

  // 物理组筛选。dim/tag 负值 = 清除：组、实体、单元一并复位（预览不动）。
  void set_group_filter(int dim, int tag);
  // 实体筛选。仅设置实体并清单元。
  void set_entity_filter(int dim, int tag);
  // 实体预览（黄色叠加）。dim/tag 非负激活；负值清除。
  void set_preview(int dim, int tag);
  void clear_all();

  bool is_previewed(int dim, int tag) const {
    return preview_dim_ == dim && preview_tag_ == tag;
  }
  bool has_preview() const { return preview_dim_ >= 0 && preview_tag_ >= 0; }
};

}  // namespace gmp
