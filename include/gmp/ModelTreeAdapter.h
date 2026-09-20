#pragma once

// v0.2 Stage 1 app/adapter 层（TASK-V02-014，Q5 批复：逻辑边界先行）。
// 桥接 gmp::core::ProjectDocument 与现有 QTreeWidget 模型树：Tree 仍是
// 唯一操作入口并继续承载 Data Role 数据；Document 作为投影经本适配器
// 懒同步——树变更只标脏，任何 document() 读取先全量重建，保证业务读写
// 路径上 Document 与 Tree 一致（约束 5 的懒同步选项）。依赖 Qt Widgets，
// 不属于 core 层。

#include <QObject>

#include "gmp/ProjectDocument.h"

class QTreeWidget;
class QTreeWidgetItem;

namespace gmp {

class ModelTreeAdapter : public QObject {
 public:
  explicit ModelTreeAdapter(QTreeWidget* tree);

  // 懒同步入口：脏时先从 Tree 全量重建再返回。
  core::ProjectDocument& document();
  bool is_dirty() const { return dirty_; }
  void mark_dirty();
  void rebuild_from_tree();

  // 根节点使用确定性保留 ID；领域对象 ID 只从专用 Data Role 读取。
  static core::ObjectId root_id(const QString& root_name);
  core::ObjectId id_for_item(const QTreeWidgetItem* item) const;
  QTreeWidgetItem* item_for_id(const core::ObjectId& id) const;

 private:
  QTreeWidget* tree_;
  core::ProjectDocument document_;
  bool dirty_ = true;
};

}  // namespace gmp
