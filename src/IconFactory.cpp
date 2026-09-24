#include "gmp/IconFactory.h"

#include <QApplication>
#include <QDebug>
#include <QHash>
#include <QPalette>
#include <QSet>
#include <QWidget>

#include "QtAwesome.h"

namespace gmp::icons {

namespace {

fa::QtAwesome* g_awesome = nullptr;
QSet<QString> g_warned_keys;

// 键名（snake_case 英文动作名）→ Font Awesome 7 Free 字形名。
// 字形名逐一核对过 QtAwesomeStringGenerated.h（QtAwesome main @ 3d9064a）。
const QHash<QString, QString>& mapping() {
  static const QHash<QString, QString> kMap = {
      // ---- 项目 / 文件 ----
      {"open_root", "folder-tree"},
      {"open_parts_root", "folder-tree"},
      {"open_selected", "folder-open"},
      {"rename", "pen"},
      {"duplicate", "copy"},
      {"create_instance", "copy"},
      {"remove", "trash-can"},
      {"trash", "trash-can"},
      {"refresh", "arrows-rotate"},
      {"reload", "arrows-rotate"},
      {"rescan", "arrows-rotate"},
      // ---- 草图 ----
      {"new_sketch", "pen-to-square"},
      {"open_edit", "pen"},
      {"edit_selected", "pen"},
      {"sketch_select", "arrow-pointer"},
      {"sketch_move", "arrows-up-down-left-right"},
      {"sketch_line", "slash"},
      {"sketch_circle", "circle"},
      {"sketch_arc", "circle-half-stroke"},
      {"sketch_rectangle", "square"},
      {"sketch_delete", "eraser"},
      // ---- 约束 / 标注 ----
      {"constraint_horizontal", "arrows-left-right"},
      {"constraint_vertical", "arrows-up-down"},
      {"constraint_parallel", "grip-lines"},
      {"constraint_perpendicular", "arrow-turn-down"},
      {"constraint_coincident", "circle-dot"},
      {"dim_distance", "arrows-left-right-to-line"},
      {"dim_radius", "compass-drafting"},
      // ---- 通用编辑 ----
      {"undo", "rotate-left"},
      {"redo", "rotate-right"},
      {"retry", "rotate-right"},
      {"finish", "check"},
      {"apply", "check"},
      {"clear", "eraser"},
      {"clear_x", "xmark"},
      {"cancel", "xmark"},
      {"remove_selected", "xmark"},
      {"clear_selection", "xmark"},
      // ---- 部件 / 装配 ----
      {"new_part", "cube"},
      {"add_primitive", "cube"},
      {"generate_3d", "cube"},
      {"open_gmsh_panel", "table-cells"},
      {"insert_mesh_block", "table-cells"},
      {"generate_2d", "table-cells"},
      {"feat_extrude", "cubes"},
      {"feat_revolve", "arrows-rotate"},
      {"rotate", "arrows-rotate"},
      {"feat_loft", "layer-group"},
      {"slice", "layer-group"},
      {"feat_sweep", "route"},
      {"apply_groups", "object-group"},
      {"bool_fuse", "object-group"},
      {"add_param", "plus"},
      {"pg_add", "plus"},
      {"add_csv", "plus"},
      {"remove_param", "minus"},
      {"bool_cut", "minus"},
      {"go_to_node", "arrow-turn-up"},
      // ---- 材料 / 属性 / 截面 ----
      {"new_material", "flask"},
      {"new_cdp_material", "atom"},
      {"open_property_editor", "sliders"},
      {"new_section", "crop-simple"},
      {"build_assembly", "puzzle-piece"},
      // ---- 分析步 / 相互作用 / 载荷 / BC ----
      {"add_static_step", "square-plus"},
      {"add_transient_step", "clock"},
      {"add_step_preset", "wand-magic-sparkles"},
      {"add_interaction", "link"},
      {"add_surface_contact", "handshake"},
      {"add_generic_load", "weight-hanging"},
      {"add_surface_pressure", "compress"},
      {"bool_intersect", "compress"},
      {"open_bc_root", "vector-square"},
      {"insert_bcs_from_groups", "vector-square"},
      {"add_thermal_source", "fire"},
      // ---- 几何工具 ----
      {"open_geometry", "folder-open"},
      {"result_folder", "folder-open"},
      {"clear_model", "broom"},
      {"translate", "up-down-left-right"},
      {"scale", "up-right-and-down-left-from-center"},
      // ---- 物理组 ----
      {"pg_update", "file-pen"},
      {"pg_delete", "trash-can"},
      {"clear_stage_filter", "filter-circle-xmark"},
      {"reset_filters", "filter-circle-xmark"},
      {"clear_filters", "filter-circle-xmark"},
      // ---- 网格 ----
      {"generate_mesh", "gears"},
      {"export_geometry", "file-export"},
      {"write_input", "file-export"},
      // ---- 结果 / 输出 ----
      {"pick_output", "file-arrow-up"},
      {"load_selected", "file-arrow-up"},
      {"run", "play"},
      {"play", "play"},
      {"stop", "stop"},
      {"open_log", "file-lines"},
      {"open_as_text", "file-lines"},
      {"open_result", "chart-simple"},
      {"refresh_files", "arrows-rotate"},
      {"download", "file-arrow-down"},
      // ---- 远程作业 ----
      {"submit_job", "paper-plane"},
      {"remote_artifact", "box-open"},
      // ---- 模板 / 快照 ----
      {"apply_template", "clone"},
      {"preview_merge", "code-merge"},
      {"export_snapshot", "camera"},
      {"screenshot", "camera"},
      {"validate_workflow", "clipboard-check"},
      {"check_input", "file-circle-check"},
      {"verify_package", "badge-check"},
      // ---- 结果面板 ----
      {"apply_view", "eye"},
      {"open_in_viewer", "eye"},
      {"preview_mesh", "eye"},
      {"apply_deform", "wave-square"},
      {"import_result_file", "file-import"},
      {"import_exodus", "file-import"},
      {"import_task_dir", "boxes-stacked"},
      {"relocate", "map-location-dot"},
      {"preview", "magnifying-glass"},
      {"new_compare", "window-restore"},
      {"pin_preview", "thumbtack"},
      {"import_csv", "file-csv"},
      {"export_csv", "file-csv"},
      {"compare_curves", "code-compare"},
      {"export_png", "image"},
      {"export_svg", "file-code"},
      {"copy_image", "copy"},
      {"latest", "forward-step"},
      {"step_next", "forward-step"},
      {"prev_page", "chevron-left"},
      {"next_page", "chevron-right"},
      // ---- 项目 ----
      {"new_project", "file-circle-plus"},
      {"open_project", "folder-open"},
      {"save", "floppy-disk"},
      {"save_as", "pen-to-square"},
      {"sync_model", "arrows-spin"},
      {"cycle_display", "images"},
      {"pick", "crosshairs"},
      // ---- 回放 ----
      {"pause", "pause"},
      {"step_prev", "backward-step"},
      {"focus_viewport", "bullseye"},
      {"show_plot_preview", "chart-line"},
      {"show_table_preview", "table"},
  };
  return kMap;
}

} // namespace

void init(QWidget* paletteAnchor) {
  if (g_awesome) {
    return;
  }
  g_awesome = new fa::QtAwesome(qApp);
  g_awesome->initFontAwesome();

  const QPalette palette =
      paletteAnchor ? paletteAnchor->palette() : QApplication::palette();
  // 浅色主题下纯黑图标视觉重量过重，用深灰中和；深色主题保持文字色。
  const QColor text =
      palette.color(QPalette::Normal, QPalette::Text);
  const QColor iconColor =
      text.lightness() > 128 ? QColor(0x44, 0x44, 0x44) : text;
  const QColor highlight =
      palette.color(QPalette::Normal, QPalette::Highlight);
  g_awesome->setDefaultOption("color", iconColor);
  g_awesome->setDefaultOption("color-disabled",
                              palette.color(QPalette::Disabled, QPalette::Text));
  g_awesome->setDefaultOption("color-active", highlight);
  g_awesome->setDefaultOption("color-selected", highlight);
  // 可勾选按钮的选中态（On）同样用主题色，如草图工具、固定预览曲线。
  g_awesome->setDefaultOption("color-on", highlight);
  g_awesome->setDefaultOption("scale-factor", 0.8);
}

QIcon get(const QString& key, Size size) {
  Q_UNUSED(size); // 字体图标可缩放，目标尺寸由渲染方按 int(size) 取值
  if (!g_awesome) {
    init();
  }
  const auto it = mapping().constFind(key);
  if (it == mapping().constEnd()) {
    if (!g_warned_keys.contains(key)) {
      g_warned_keys.insert(key);
      qWarning() << "[gmp::icons] unknown icon key:" << key;
    }
    return QIcon();
  }
  return g_awesome->icon(it.value());
}

QIcon toolbar(const QString& key) { return get(key, Size::Toolbar); }

} // namespace gmp::icons
