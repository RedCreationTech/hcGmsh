#pragma once

#include <QString>
#include <QWidget>

class QToolButton;
class QWidget;
class QButtonGroup;

namespace gmp {

class StageLeftToolbar : public QWidget {
  Q_OBJECT

 public:
  explicit StageLeftToolbar(QWidget* parent = nullptr);

  void set_context(const QString& module);
  void set_picking_checked(bool checked);
  void set_slice_checked(bool checked);
  void set_sketch_tool_checked(int tool);
  void reset_temporary_modes();

 signals:
  void interaction_mode_requested(int mode);
  void view_preset_requested(int preset);
  void picking_toggled(bool enabled);
  void clear_selection_requested();
  void slice_toggled(bool enabled);
  void representation_cycle_requested();
  void sketch_tool_requested(int tool);
  void mesh_workspace_requested();
  void mesh_generate_requested();
  void visualization_workspace_requested();
  void results_workspace_requested();

 private:
  QToolButton* add_button(QWidget* host, const QString& icon_key,
                          const QString& tooltip, bool checkable = false);
  void set_collapsed(bool collapsed);

  QWidget* common_group_ = nullptr;
  QWidget* sketch_group_ = nullptr;
  QWidget* mesh_group_ = nullptr;
  QWidget* visualization_group_ = nullptr;
  QToolButton* collapse_button_ = nullptr;
  QToolButton* rotate_button_ = nullptr;
  QToolButton* pick_button_ = nullptr;
  QToolButton* slice_button_ = nullptr;
  QToolButton* sketch_select_button_ = nullptr;
  QButtonGroup* sketch_button_group_ = nullptr;
  QString current_context_;
  bool collapsed_ = false;
};

}  // namespace gmp
