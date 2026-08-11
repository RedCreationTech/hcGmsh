#pragma once

#include <QString>
#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;

namespace gmp {

// 草图模块 v1 骨架面板: 草图列表 + 新建/打开编辑/重命名/复制/删除/刷新。
// 面板本身不碰模型树与视口, 具体行为由 MainWindow 连接信号实现,
// 交互模式参照 make_module_node_page 生成的模块页。
//
// WS1: 追加"编辑工具区"(默认隐藏, set_editing(true) 进入编辑时显示):
// 工具按钮组 (Select/Line/Circle/Arc/Delete/Rectangle, 与 VtkViewer 的
// SketchTool 枚举 int 值一一对应)、几何约束按钮、driving 尺寸输入、
// 撤销/重做、光标坐标与状态提示、"完成编辑"按钮。
class SketchPanel : public QWidget {
  Q_OBJECT
 public:
  explicit SketchPanel(QWidget* parent = nullptr);

  // 供 MainWindow 用 refresh_module_node_list 刷新列表内容
  QListWidget* sketch_list() const { return list_; }

  // 进入/退出草图编辑态: 显示/隐藏编辑工具区; 进入时重置为 Select 工具
  // 并发出 tool_selected(SketchToolSelect), name 为当前编辑的草图名
  void set_editing(bool editing, const QString& sketch_name = QString());
  bool is_editing() const { return editing_; }

  // 编辑态状态显示 (由 MainWindow 连接 VtkViewer 信号驱动)
  void set_cursor_pos(double x, double y);         // 光标世界坐标 (毫米)
  void set_status_text(const QString& text);       // 选中/提示信息
  // 撤销/重做按钮可用状态 (由 MainWindow 按快照栈驱动)
  void set_undo_redo_state(bool can_undo, bool can_redo);

 signals:
  void new_sketch_requested();
  void open_edit_requested();
  void rename_requested();
  void duplicate_requested();
  void remove_requested();
  void refresh_requested();

  // ---- WS1 编辑态信号 ----
  void tool_selected(int tool);          // 工具按钮组, 值 = VtkViewer::SketchTool
  void finish_edit_requested();          // "完成编辑"按钮
  void constraint_requested(int type);   // 几何约束, 值 = SketchConstraintType
  void dimension_requested(int type, double value);  // Distance/Radius + 目标值
  void undo_requested();                 // 撤销一步 (快照栈)
  void redo_requested();                 // 重做一步

 private:
  QListWidget* list_ = nullptr;

  bool editing_ = false;
  QWidget* edit_box_ = nullptr;      // 编辑工具区容器 (默认隐藏)
  QLabel* edit_name_ = nullptr;      // 当前编辑草图名
  QLabel* cursor_label_ = nullptr;   // 光标世界坐标
  QLabel* status_label_ = nullptr;   // 状态/选中提示
  QDoubleSpinBox* dim_value_ = nullptr;  // 尺寸目标值输入
  QPushButton* undo_btn_ = nullptr;
  QPushButton* redo_btn_ = nullptr;
};

}  // namespace gmp
