#include "gmp/StageLeftToolbar.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHash>
#include <QIcon>
#include <QPair>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include "gmp/IconFactory.h"
#include "gmp/VtkViewer.h"

namespace gmp {

namespace {

// 舞台工具条键（kebab-case）→ gmp::icons 映射键（snake_case）。
// 映射值表见 src/IconFactory.cpp 的 mapping()。
const QHash<QString, QString>& icon_key_map() {
  static const QHash<QString, QString> kMap = {
      {"collapse", "collapse"},
      {"expand", "expand"},
      {"rotate", "rotate"},
      {"pan", "sketch_move"},
      {"zoom", "zoom"},
      {"pick", "pick"},
      {"clear", "clear_x"},
      {"fit", "fit"},
      {"front", "front"},
      {"right", "right"},
      {"top", "top"},
      {"iso", "iso"},
      {"display", "cycle_display"},
      {"slice", "slice"},
      {"sketch-select", "sketch_select"},
      {"sketch-line", "sketch_line"},
      {"sketch-circle", "sketch_circle"},
      {"sketch-arc", "sketch_arc"},
      {"sketch-rect", "sketch_rectangle"},
      {"sketch-delete", "sketch_delete"},
      {"mesh", "open_gmsh_panel"},
      {"mesh-generate", "generate_mesh"},
      {"visualization", "open_in_viewer"},
      {"results", "open_result"},
  };
  return kMap;
}

QWidget* make_group(QWidget* parent) {
  auto* group = new QWidget(parent);
  auto* layout = new QVBoxLayout(group);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(2);
  return group;
}

void add_separator(QVBoxLayout* layout, QWidget* parent) {
  auto* line = new QFrame(parent);
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  line->setFixedHeight(5);
  layout->addWidget(line);
}

}  // namespace

StageLeftToolbar::StageLeftToolbar(QWidget* parent) : QWidget(parent) {
  setObjectName("stageLeftToolbar");
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  setFixedWidth(42);

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(2, 2, 2, 2);
  root->setSpacing(2);

  collapse_button_ = add_button(this, "collapse",
                                "折叠舞台工具栏", false);
  root->addWidget(collapse_button_);
  connect(collapse_button_, &QToolButton::clicked, this,
          [this]() { set_collapsed(!collapsed_); });

  common_group_ = make_group(this);
  auto* common = qobject_cast<QVBoxLayout*>(common_group_->layout());
  interaction_button_group_ = new QButtonGroup(this);
  interaction_button_group_->setExclusive(true);
  rotate_button_ = add_button(common_group_, "rotate", "旋转：在视口中左键拖动", true);
  pan_button_ = add_button(common_group_, "pan", "平移视图：在视口中左键拖动", true);
  zoom_button_ = add_button(common_group_, "zoom", "缩放：在视口中左键上下拖动", true);
  interaction_button_group_->addButton(rotate_button_, 0);
  interaction_button_group_->addButton(pan_button_, 1);
  interaction_button_group_->addButton(zoom_button_, 2);
  rotate_button_->setChecked(true);
  connect(interaction_button_group_, &QButtonGroup::idClicked, this,
          [this](int mode) {
            if (current_context_ == "Sketch") {
              if (sketch_button_group_) {
                sketch_button_group_->setExclusive(false);
                for (auto* button : sketch_button_group_->buttons()) {
                  button->setChecked(false);
                }
                sketch_button_group_->setExclusive(true);
              }
              if (mode == 1) {
                emit sketch_tool_requested(SketchToolMove);
                return;
              }
            }
            emit interaction_mode_requested(mode);
          });
  common->addWidget(rotate_button_);
  common->addWidget(pan_button_);
  common->addWidget(zoom_button_);
  add_separator(common, common_group_);

  pick_button_ = add_button(common_group_, "pick", "选择/拾取", true);
  auto* clear = add_button(common_group_, "clear", "清除舞台选择");
  connect(pick_button_, &QToolButton::toggled, this,
          &StageLeftToolbar::picking_toggled);
  connect(clear, &QToolButton::clicked, this,
          &StageLeftToolbar::clear_selection_requested);
  common->addWidget(pick_button_);
  common->addWidget(clear);
  add_separator(common, common_group_);

  const QList<QPair<QString, int>> views = {{"fit", 0},   {"front", 1},
                                            {"right", 2}, {"top", 3},
                                            {"iso", 4}};
  const QStringList tips = {"适配窗口", "前视图", "右视图", "顶视图", "轴测图"};
  for (int i = 0; i < views.size(); ++i) {
    auto* button = add_button(common_group_, views.at(i).first, tips.at(i));
    connect(button, &QToolButton::clicked, this,
            [this, preset = views.at(i).second]() {
              emit view_preset_requested(preset);
            });
    common->addWidget(button);
  }
  add_separator(common, common_group_);
  auto* display = add_button(common_group_, "display", "切换显示方式");
  slice_button_ = add_button(common_group_, "slice", "启用/关闭剖切", true);
  connect(display, &QToolButton::clicked, this,
          &StageLeftToolbar::representation_cycle_requested);
  connect(slice_button_, &QToolButton::toggled, this,
          &StageLeftToolbar::slice_toggled);
  common->addWidget(display);
  common->addWidget(slice_button_);
  root->addWidget(common_group_);

  sketch_group_ = make_group(this);
  auto* sketch = qobject_cast<QVBoxLayout*>(sketch_group_->layout());
  sketch_button_group_ = new QButtonGroup(this);
  sketch_button_group_->setExclusive(true);
  const QList<QPair<QString, int>> sketch_tools = {
      {"sketch-select", SketchToolSelect},
      {"sketch-line", SketchToolDrawLine},
      {"sketch-circle", SketchToolDrawCircle},
      {"sketch-arc", SketchToolDrawArc},
      {"sketch-rect", SketchToolDrawRectangle},
      {"sketch-delete", SketchToolDelete}};
  const QStringList sketch_tips = {"草图选择", "绘制直线", "绘制圆",
                                   "绘制圆弧", "绘制矩形", "删除图元"};
  for (int i = 0; i < sketch_tools.size(); ++i) {
    auto* button = add_button(sketch_group_, sketch_tools.at(i).first,
                              sketch_tips.at(i), true);
    sketch_button_group_->addButton(button, sketch_tools.at(i).second);
    sketch->addWidget(button);
    if (sketch_tools.at(i).second == SketchToolSelect) {
      sketch_select_button_ = button;
      button->setChecked(true);
    }
  }
  connect(sketch_button_group_, &QButtonGroup::idClicked, this,
          [this](int tool) {
            if (current_context_ == "Sketch" && interaction_button_group_) {
              interaction_button_group_->setExclusive(false);
              for (auto* button : interaction_button_group_->buttons()) {
                button->setChecked(false);
              }
              interaction_button_group_->setExclusive(true);
            }
            emit sketch_tool_requested(tool);
          });
  root->addWidget(sketch_group_);

  mesh_group_ = make_group(this);
  auto* mesh = qobject_cast<QVBoxLayout*>(mesh_group_->layout());
  auto* mesh_open = add_button(mesh_group_, "mesh", "打开 Mesh Workspace");
  auto* mesh_generate =
      add_button(mesh_group_, "mesh-generate", "生成网格");
  connect(mesh_open, &QToolButton::clicked, this,
          &StageLeftToolbar::mesh_workspace_requested);
  connect(mesh_generate, &QToolButton::clicked, this,
          &StageLeftToolbar::mesh_generate_requested);
  mesh->addWidget(mesh_open);
  mesh->addWidget(mesh_generate);
  root->addWidget(mesh_group_);

  visualization_group_ = make_group(this);
  auto* visualization =
      qobject_cast<QVBoxLayout*>(visualization_group_->layout());
  auto* viz = add_button(visualization_group_, "visualization",
                         "打开 Visualization Workspace");
  auto* results =
      add_button(visualization_group_, "results", "打开 Results Workspace");
  connect(viz, &QToolButton::clicked, this,
          &StageLeftToolbar::visualization_workspace_requested);
  connect(results, &QToolButton::clicked, this,
          &StageLeftToolbar::results_workspace_requested);
  visualization->addWidget(viz);
  visualization->addWidget(results);
  root->addWidget(visualization_group_);
  root->addStretch(1);

  set_context(QString());
}

QToolButton* StageLeftToolbar::add_button(QWidget* host, const QString& icon_key,
                                          const QString& tooltip,
                                          bool checkable) {
  auto* button = new QToolButton(host);
  button->setObjectName("stageTool_" + icon_key);
  button->setIcon(
      icons::toolbar(icon_key_map().value(icon_key, icon_key)));
  button->setIconSize(QSize(20, 20));
  button->setAccessibleName(tooltip);
  button->setToolTip(tooltip.contains("键") ? tooltip
                                           : tooltip + "（无快捷键）");
  button->setCheckable(checkable);
  button->setAutoRaise(true);
  button->setFixedSize(36, 34);
  return button;
}

void StageLeftToolbar::set_context(const QString& module) {
  current_context_ = module;
  const bool sketch_context = module == "Sketch";
  if (rotate_button_) {
    rotate_button_->setVisible(!sketch_context);
  }
  if (pick_button_) {
    const QSignalBlocker blocker(pick_button_);
    pick_button_->setChecked(false);
    pick_button_->setVisible(!sketch_context);
  }
  if (pan_button_) {
    const QString tip = sketch_context
                            ? "移动图形：左键拖动完整图形；Option/Alt 拖动子图元"
                            : "平移视图：在视口中左键拖动";
    pan_button_->setAccessibleName(tip);
    pan_button_->setToolTip(tip + "（无快捷键）");
  }
  if (sketch_context) {
    if (interaction_button_group_) {
      interaction_button_group_->setExclusive(false);
      for (auto* button : interaction_button_group_->buttons()) {
        button->setChecked(false);
      }
      interaction_button_group_->setExclusive(true);
    }
    if (sketch_select_button_) {
      sketch_select_button_->setChecked(true);
    }
  } else if (interaction_button_group_ &&
             !interaction_button_group_->checkedButton() && rotate_button_) {
    rotate_button_->setChecked(true);
  }
  if (sketch_group_) {
    sketch_group_->setVisible(!collapsed_ && module == "Sketch");
  }
  if (mesh_group_) {
    mesh_group_->setVisible(!collapsed_ && module == "Mesh");
  }
  if (visualization_group_) {
    visualization_group_->setVisible(
        !collapsed_ && (module == "Visualization" || module == "Results"));
  }
}

void StageLeftToolbar::set_picking_checked(bool checked) {
  if (current_context_ == "Sketch") {
    if (checked) {
      set_sketch_tool_checked(SketchToolSelect);
    }
    if (pick_button_) {
      const QSignalBlocker blocker(pick_button_);
      pick_button_->setChecked(false);
    }
    return;
  }
  if (pick_button_ && pick_button_->isChecked() != checked) {
    pick_button_->setChecked(checked);
  }
}

void StageLeftToolbar::set_slice_checked(bool checked) {
  if (slice_button_ && slice_button_->isChecked() != checked) {
    slice_button_->setChecked(checked);
  }
}

void StageLeftToolbar::set_sketch_tool_checked(int tool) {
  if (!sketch_button_group_ || !interaction_button_group_) {
    return;
  }
  interaction_button_group_->setExclusive(false);
  for (auto* button : interaction_button_group_->buttons()) {
    button->setChecked(false);
  }
  interaction_button_group_->setExclusive(true);
  if (tool == SketchToolMove) {
    sketch_button_group_->setExclusive(false);
    for (auto* button : sketch_button_group_->buttons()) {
      button->setChecked(false);
    }
    sketch_button_group_->setExclusive(true);
    if (pan_button_) {
      pan_button_->setChecked(true);
    }
  } else if (auto* button = sketch_button_group_->button(tool)) {
    button->setChecked(true);
  }
}

void StageLeftToolbar::reset_temporary_modes() {
  if (current_context_ == "Sketch") {
    set_sketch_tool_checked(SketchToolSelect);
    emit sketch_tool_requested(SketchToolSelect);
    return;
  }
  if (rotate_button_) {
    rotate_button_->setChecked(true);
  }
  if (pick_button_) {
    pick_button_->setChecked(false);
  }
  if (slice_button_) {
    slice_button_->setChecked(false);
  }
  if (sketch_select_button_ && sketch_group_ && sketch_group_->isVisible()) {
    sketch_select_button_->setChecked(true);
    emit sketch_tool_requested(SketchToolSelect);
  }
  emit interaction_mode_requested(0);
}

void StageLeftToolbar::set_collapsed(bool collapsed) {
  collapsed_ = collapsed;
  if (common_group_) {
    common_group_->setVisible(!collapsed);
  }
  if (sketch_group_) {
    sketch_group_->setVisible(false);
  }
  if (mesh_group_) {
    mesh_group_->setVisible(false);
  }
  if (visualization_group_) {
    visualization_group_->setVisible(false);
  }
  if (collapse_button_) {
    const QString key = collapsed ? "expand" : "collapse";
    collapse_button_->setIcon(icons::toolbar(key));
    collapse_button_->setToolTip(collapsed ? "展开舞台工具栏"
                                           : "折叠舞台工具栏");
  }
  if (!collapsed) {
    set_context(current_context_);
  }
}

}  // namespace gmp
