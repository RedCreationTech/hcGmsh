#include "gmp/StageLeftToolbar.h"

#include <QButtonGroup>
#include <QFrame>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QToolButton>
#include <QVBoxLayout>

#include "gmp/VtkViewer.h"

namespace gmp {

namespace {

QIcon stage_icon(const QString& key) {
  QPixmap pixmap(40, 40);
  pixmap.setDevicePixelRatio(2.0);
  pixmap.fill(Qt::transparent);

  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing, true);
  QPen pen(QColor("#17263a"), 1.7, Qt::SolidLine, Qt::RoundCap,
           Qt::RoundJoin);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);

  const auto arrow_head = [&p](const QPointF& tip, const QPointF& a,
                                const QPointF& b) {
    QPainterPath path(tip);
    path.lineTo(a);
    path.moveTo(tip);
    path.lineTo(b);
    p.drawPath(path);
  };
  const auto cube = [&p]() {
    const QPolygonF front{{6, 7}, {13, 7}, {13, 14}, {6, 14}};
    const QPolygonF back{{9, 4}, {16, 4}, {16, 11}, {13, 14}, {13, 7},
                          {6, 7}, {9, 4}};
    p.drawPolygon(front);
    p.drawPolyline(back);
    p.drawLine(QPointF(13, 7), QPointF(16, 4));
  };

  if (key == "collapse" || key == "expand") {
    const qreal x = key == "collapse" ? 12 : 8;
    const qreal dir = key == "collapse" ? -1 : 1;
    p.drawPolyline(QPolygonF{{x - dir * 3, 5}, {x + dir * 2, 10},
                             {x - dir * 3, 15}});
  } else if (key == "rotate") {
    p.drawArc(QRectF(4, 4, 12, 12), 35 * 16, 285 * 16);
    arrow_head({15.5, 6.2}, {12.1, 5.7}, {14.6, 9.0});
    p.drawEllipse(QRectF(8.5, 8.5, 3, 3));
  } else if (key == "pan") {
    p.drawLine(10, 3, 10, 17);
    p.drawLine(3, 10, 17, 10);
    arrow_head({10, 3}, {8, 6}, {12, 6});
    arrow_head({10, 17}, {8, 14}, {12, 14});
    arrow_head({3, 10}, {6, 8}, {6, 12});
    arrow_head({17, 10}, {14, 8}, {14, 12});
  } else if (key == "zoom") {
    p.drawEllipse(QRectF(3.5, 3.5, 10, 10));
    p.drawLine(QLineF(12.2, 12.2, 17, 17));
    p.drawLine(QLineF(6, 8.5, 11, 8.5));
    p.drawLine(QLineF(8.5, 6, 8.5, 11));
  } else if (key == "pick" || key == "sketch-select") {
    QPainterPath cursor;
    cursor.moveTo(4, 3);
    cursor.lineTo(5.2, 16);
    cursor.lineTo(8.2, 12.5);
    cursor.lineTo(11, 17);
    cursor.lineTo(13.2, 15.7);
    cursor.lineTo(10.5, 11.3);
    cursor.lineTo(15, 10.8);
    cursor.closeSubpath();
    p.drawPath(cursor);
  } else if (key == "clear" || key == "sketch-delete") {
    p.drawLine(5, 5, 15, 15);
    p.drawLine(15, 5, 5, 15);
  } else if (key == "fit") {
    p.drawLine(4, 8, 4, 4);
    p.drawLine(4, 4, 8, 4);
    p.drawLine(12, 4, 16, 4);
    p.drawLine(16, 4, 16, 8);
    p.drawLine(16, 12, 16, 16);
    p.drawLine(16, 16, 12, 16);
    p.drawLine(8, 16, 4, 16);
    p.drawLine(4, 16, 4, 12);
  } else if (key == "front" || key == "right" || key == "top") {
    p.drawRect(QRectF(4, 4, 12, 12));
    QPen accent(QColor("#2f6fed"), 2.4, Qt::SolidLine, Qt::RoundCap);
    p.setPen(accent);
    if (key == "front") {
      p.drawRect(QRectF(7, 7, 6, 6));
    } else if (key == "right") {
      p.drawLine(15, 5, 15, 15);
    } else {
      p.drawLine(5, 5, 15, 5);
    }
  } else if (key == "iso") {
    cube();
  } else if (key == "display") {
    cube();
    p.setBrush(QColor(47, 111, 237, 60));
    p.drawPolygon(QPolygonF{{6, 7}, {13, 7}, {13, 14}, {6, 14}});
  } else if (key == "slice") {
    cube();
    QPen accent(QColor("#2f6fed"), 2.0, Qt::SolidLine, Qt::RoundCap);
    p.setPen(accent);
    p.drawLine(3, 10, 17, 10);
  } else if (key == "sketch-line") {
    p.drawLine(4, 15, 16, 5);
    p.drawEllipse(QRectF(2.8, 13.8, 2.4, 2.4));
    p.drawEllipse(QRectF(14.8, 3.8, 2.4, 2.4));
  } else if (key == "sketch-circle") {
    p.drawEllipse(QRectF(4, 4, 12, 12));
    p.drawEllipse(QRectF(9, 9, 2, 2));
  } else if (key == "sketch-arc") {
    p.drawArc(QRectF(3, 5, 14, 12), 15 * 16, 150 * 16);
    p.drawEllipse(QRectF(15, 8, 2, 2));
    p.drawEllipse(QRectF(3, 8, 2, 2));
  } else if (key == "sketch-rect") {
    p.drawRect(QRectF(4, 5, 12, 10));
  } else if (key == "mesh" || key == "mesh-generate") {
    for (int v = 5; v <= 15; v += 5) {
      p.drawLine(v, 4, v, 16);
      p.drawLine(4, v, 16, v);
    }
    if (key == "mesh-generate") {
      p.setPen(QPen(QColor("#2f6fed"), 2.0, Qt::SolidLine,
                    Qt::RoundCap, Qt::RoundJoin));
      p.drawPolyline(QPolygonF{{12, 2}, {9, 9}, {13, 9}, {10, 18}});
    }
  } else if (key == "visualization") {
    p.setPen(QPen(QColor("#2f6fed"), 2.2));
    p.drawLine(5, 15, 5, 10);
    p.setPen(QPen(QColor("#31a36b"), 2.2));
    p.drawLine(10, 15, 10, 6);
    p.setPen(QPen(QColor("#d9782d"), 2.2));
    p.drawLine(15, 15, 15, 3);
  } else if (key == "results") {
    p.drawRect(QRectF(4, 4, 12, 12));
    p.setPen(QPen(QColor("#2f6fed"), 1.8, Qt::SolidLine,
                  Qt::RoundCap, Qt::RoundJoin));
    p.drawPolyline(QPolygonF{{5, 13}, {8, 10}, {11, 12}, {15, 6}});
  }
  p.end();
  return QIcon(pixmap);
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
  auto* interaction_group = new QButtonGroup(this);
  interaction_group->setExclusive(true);
  rotate_button_ = add_button(common_group_, "rotate", "旋转：在视口中左键拖动", true);
  auto* pan = add_button(common_group_, "pan", "平移：在视口中左键拖动", true);
  auto* zoom = add_button(common_group_, "zoom", "缩放：在视口中左键上下拖动", true);
  interaction_group->addButton(rotate_button_, 0);
  interaction_group->addButton(pan, 1);
  interaction_group->addButton(zoom, 2);
  rotate_button_->setChecked(true);
  connect(interaction_group, &QButtonGroup::idClicked, this,
          &StageLeftToolbar::interaction_mode_requested);
  common->addWidget(rotate_button_);
  common->addWidget(pan);
  common->addWidget(zoom);
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
          &StageLeftToolbar::sketch_tool_requested);
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
  button->setIcon(stage_icon(icon_key));
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
  if (sketch_button_group_) {
    if (auto* button = sketch_button_group_->button(tool)) {
      button->setChecked(true);
    }
  }
}

void StageLeftToolbar::reset_temporary_modes() {
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
    collapse_button_->setIcon(stage_icon(collapsed ? "expand" : "collapse"));
    collapse_button_->setToolTip(collapsed ? "展开舞台工具栏"
                                           : "折叠舞台工具栏");
  }
  if (!collapsed) {
    set_context(current_context_);
  }
}

}  // namespace gmp
