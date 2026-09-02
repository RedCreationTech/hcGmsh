#include "gmp/SketchPanel.h"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QDoubleSpinBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "gmp/SketchDocument.h"
#include "gmp/VtkViewer.h"

namespace gmp {

SketchPanel::SketchPanel(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(10, 10, 10, 10);
  layout->setSpacing(6);

  management_box_ = new QWidget(this);
  auto* management_layout = new QVBoxLayout(management_box_);
  management_layout->setContentsMargins(0, 0, 0, 0);
  management_layout->setSpacing(6);

  auto* heading = new QLabel("Sketch", management_box_);
  QFont hfont = heading->font();
  hfont.setPointSize(hfont.pointSize() + 3);
  hfont.setBold(true);
  heading->setFont(hfont);
  management_layout->addWidget(heading);

  auto* desc = new QLabel(
      "Create and manage 2D sketches on the XY plane. Parts reference sketches "
      "as the basis for feature operations (extrude, revolve, ...).",
      management_box_);
  desc->setWordWrap(true);
  management_layout->addWidget(desc);

  auto* new_btn = new QPushButton("New Sketch", management_box_);
  new_btn->setObjectName("newSketchButton");
  auto* open_edit_btn = new QPushButton("Open Edit", management_box_);
  open_edit_btn->setObjectName("openSketchEditButton");
  open_edit_btn->setToolTip(
      "Open the selected sketch for editing (2D view; drawing tools land in WS1).");
  management_layout->addWidget(new_btn);
  management_layout->addWidget(open_edit_btn);

  management_layout->addWidget(
      new QLabel("Current entries:", management_box_));
  list_ = new QListWidget(management_box_);
  list_->setSelectionMode(QAbstractItemView::SingleSelection);
  list_->setMinimumHeight(120);
  list_->setAlternatingRowColors(true);
  list_->setToolTip("Double click a sketch to open the 2D editor.");
  management_layout->addWidget(list_);

  auto* rename_btn = new QPushButton("Rename", management_box_);
  auto* duplicate_btn = new QPushButton("Duplicate", management_box_);
  auto* remove_btn = new QPushButton("Remove", management_box_);
  auto* refresh_btn = new QPushButton("Refresh", management_box_);
  auto* row = new QHBoxLayout();
  row->setContentsMargins(0, 0, 0, 0);
  row->addWidget(rename_btn);
  row->addWidget(duplicate_btn);
  row->addWidget(remove_btn);
  row->addStretch(1);
  row->addWidget(refresh_btn);
  management_layout->addLayout(row);
  layout->addWidget(management_box_);

  // ---- WS1 编辑工具区 (默认隐藏, set_editing(true) 时显示) ----
  edit_box_ = new QWidget(this);
  auto* edit_layout = new QVBoxLayout(edit_box_);
  edit_layout->setContentsMargins(0, 6, 0, 0);
  edit_layout->setSpacing(6);

  edit_name_ = new QLabel(edit_box_);
  QFont nfont = edit_name_->font();
  nfont.setBold(true);
  edit_name_->setFont(nfont);
  edit_layout->addWidget(edit_name_);

  // 工具按钮组: int id 与 VtkViewer::SketchTool 一致
  edit_layout->addWidget(new QLabel("Tools:", edit_box_));
  auto* tool_group = new QButtonGroup(edit_box_);
  tool_group->setExclusive(true);
  auto* tool_row1 = new QHBoxLayout();
  tool_row1->setContentsMargins(0, 0, 0, 0);
  auto* tool_row2 = new QHBoxLayout();
  tool_row2->setContentsMargins(0, 0, 0, 0);
  auto make_tool = [&](const QString& text, int id, const QString& tip,
                       QHBoxLayout* target) {
    auto* btn = new QPushButton(text, edit_box_);
    btn->setCheckable(true);
    btn->setToolTip(tip);
    tool_group->addButton(btn, id);
    target->addWidget(btn);
    return btn;
  };
  auto* select_btn = make_tool("Select", SketchToolSelect,
                               "Click to select an entity; Shift+click toggles "
                               "multi-selection.",
                               tool_row1);
  make_tool("Line", SketchToolDrawLine,
            "Draw a line: click start point, then end point. Endpoints snap "
            "to existing points.",
            tool_row1);
  make_tool("Circle", SketchToolDrawCircle,
            "Draw a circle: click center, then a point on the radius.",
            tool_row1);
  make_tool("Arc", SketchToolDrawArc,
            "Draw an arc: click center, then start point (sets radius), then "
            "end angle.",
            tool_row2);
  make_tool("Delete", SketchToolDelete,
            "Click an entity to delete it. The Delete key removes the current "
            "selection.",
            tool_row2);
  make_tool("Rectangle", SketchToolDrawRectangle,
            "Draw an axis-aligned rectangle: click one corner, then the "
            "opposite corner. Creates 4 lines with coincident corners.",
            tool_row2);
  tool_row1->addStretch(1);
  tool_row2->addStretch(1);
  edit_layout->addLayout(tool_row1);
  edit_layout->addLayout(tool_row2);
  select_btn->setChecked(true);

  // 几何约束按钮: int id 与 SketchConstraintType 一致
  edit_layout->addWidget(new QLabel("Constraints:", edit_box_));
  auto* c_row1 = new QHBoxLayout();
  c_row1->setContentsMargins(0, 0, 0, 0);
  auto* c_row2 = new QHBoxLayout();
  c_row2->setContentsMargins(0, 0, 0, 0);
  auto make_constraint = [&](const QString& text, SketchConstraintType type,
                             const QString& tip, QHBoxLayout* target) {
    auto* btn = new QPushButton(text, edit_box_);
    btn->setToolTip(tip);
    target->addWidget(btn);
    connect(btn, &QPushButton::clicked, this,
            [this, type]() { emit constraint_requested(static_cast<int>(type)); });
  };
  make_constraint("Horizontal", SketchConstraintType::Horizontal,
                  "Make the selected line horizontal.", c_row1);
  make_constraint("Vertical", SketchConstraintType::Vertical,
                  "Make the selected line vertical.", c_row1);
  make_constraint("Parallel", SketchConstraintType::Parallel,
                  "Make two selected lines parallel.", c_row1);
  make_constraint("Perpendicular", SketchConstraintType::Perpendicular,
                  "Make two selected lines perpendicular.", c_row2);
  make_constraint("Coincident", SketchConstraintType::Coincident,
                  "Snap the nearest endpoints of two selected entities "
                  "together.", c_row2);
  c_row1->addStretch(1);
  c_row2->addStretch(1);
  edit_layout->addLayout(c_row1);
  edit_layout->addLayout(c_row2);

  // driving 尺寸: 选中线 -> Distance; 选中圆/弧 -> Radius
  edit_layout->addWidget(new QLabel("Dimension:", edit_box_));
  auto* d_row = new QHBoxLayout();
  d_row->setContentsMargins(0, 0, 0, 0);
  dim_value_ = new QDoubleSpinBox(edit_box_);
  dim_value_->setRange(0.0001, 1e7);
  dim_value_->setDecimals(4);
  dim_value_->setValue(10.0);
  dim_value_->setToolTip("Target value for the driving dimension (mm).");
  auto* dim_dist_btn = new QPushButton("Add Distance", edit_box_);
  dim_dist_btn->setToolTip("Add a driving distance (length) dimension to the "
                           "selected line.");
  auto* dim_radius_btn = new QPushButton("Add Radius", edit_box_);
  dim_radius_btn->setToolTip("Add a driving radius dimension to the selected "
                             "circle or arc.");
  d_row->addWidget(dim_value_);
  d_row->addWidget(dim_dist_btn);
  d_row->addWidget(dim_radius_btn);
  d_row->addStretch(1);
  edit_layout->addLayout(d_row);

  // 光标世界坐标 + 状态提示
  cursor_label_ = new QLabel("Cursor: --", edit_box_);
  edit_layout->addWidget(cursor_label_);
  status_label_ = new QLabel(edit_box_);
  status_label_->setWordWrap(true);
  edit_layout->addWidget(status_label_);

  // 撤销/重做 (快照栈由 MainWindow 维护)
  auto* ur_row = new QHBoxLayout();
  ur_row->setContentsMargins(0, 0, 0, 0);
  undo_btn_ = new QPushButton("Undo", edit_box_);
  undo_btn_->setToolTip("Undo the last sketch change (Cmd/Ctrl+Z).");
  undo_btn_->setEnabled(false);
  redo_btn_ = new QPushButton("Redo", edit_box_);
  redo_btn_->setToolTip("Redo the last undone change (Cmd/Ctrl+Shift+Z).");
  redo_btn_->setEnabled(false);
  ur_row->addWidget(undo_btn_);
  ur_row->addWidget(redo_btn_);
  ur_row->addStretch(1);
  edit_layout->addLayout(ur_row);

  auto* finish_btn = new QPushButton("Finish Edit", edit_box_);
  finish_btn->setToolTip("Close the sketch editor and return to the 3D view.");
  edit_layout->addWidget(finish_btn);

  edit_box_->setVisible(false);
  layout->addWidget(edit_box_);

  layout->addStretch(1);

  connect(new_btn, &QPushButton::clicked, this,
          &SketchPanel::new_sketch_requested);
  connect(open_edit_btn, &QPushButton::clicked, this,
          &SketchPanel::open_edit_requested);
  connect(rename_btn, &QPushButton::clicked, this,
          &SketchPanel::rename_requested);
  connect(duplicate_btn, &QPushButton::clicked, this,
          &SketchPanel::duplicate_requested);
  connect(remove_btn, &QPushButton::clicked, this,
          &SketchPanel::remove_requested);
  connect(refresh_btn, &QPushButton::clicked, this,
          &SketchPanel::refresh_requested);

  connect(tool_group, &QButtonGroup::idClicked, this,
          &SketchPanel::tool_selected);
  connect(dim_dist_btn, &QPushButton::clicked, this, [this]() {
    emit dimension_requested(static_cast<int>(SketchConstraintType::Distance),
                             dim_value_->value());
  });
  connect(dim_radius_btn, &QPushButton::clicked, this, [this]() {
    emit dimension_requested(static_cast<int>(SketchConstraintType::Radius),
                             dim_value_->value());
  });
  connect(finish_btn, &QPushButton::clicked, this,
          &SketchPanel::finish_edit_requested);
  connect(undo_btn_, &QPushButton::clicked, this,
          &SketchPanel::undo_requested);
  connect(redo_btn_, &QPushButton::clicked, this,
          &SketchPanel::redo_requested);
}

void SketchPanel::set_editing(bool editing, const QString& sketch_name) {
  editing_ = editing;
  if (edit_box_) {
    edit_box_->setVisible(editing);
  }
  if (management_box_) {
    management_box_->setVisible(!editing);
  }
  if (editing) {
    if (edit_name_) {
      edit_name_->setText(QString("Editing: %1").arg(sketch_name));
    }
    if (status_label_) {
      status_label_->clear();
    }
    if (cursor_label_) {
      cursor_label_->setText("Cursor: --");
    }
    // 进入编辑默认回到 Select 工具, 并通知外部同步视口工具
    emit tool_selected(SketchToolSelect);
    set_undo_redo_state(false, false);  // 栈由 MainWindow 在新会话清空
  }
}

void SketchPanel::set_undo_redo_state(bool can_undo, bool can_redo) {
  if (undo_btn_) {
    undo_btn_->setEnabled(can_undo);
  }
  if (redo_btn_) {
    redo_btn_->setEnabled(can_redo);
  }
}

void SketchPanel::set_cursor_pos(double x, double y) {
  if (cursor_label_) {
    cursor_label_->setText(QString("Cursor: %1, %2 mm")
                               .arg(x, 0, 'f', 3)
                               .arg(y, 0, 'f', 3));
  }
}

void SketchPanel::set_status_text(const QString& text) {
  if (status_label_) {
    status_label_->setText(text);
  }
}

}  // namespace gmp
