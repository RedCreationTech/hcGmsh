#pragma once

// HARD-050 app/adapter 层：ProjectDocument 是唯一领域真源，QTreeWidget
// 只保存显示缓存与选中/展开等 UI 状态。依赖 Qt Widgets，不属于 core 层。

#include <QObject>
#include <QStringList>

#include "gmp/ProjectDocument.h"

class QTreeWidget;
class QTreeWidgetItem;

namespace gmp {

class ModelTreeAdapter : public QObject {
 public:
  explicit ModelTreeAdapter(QTreeWidget* tree);

  core::ProjectDocument& document();
  const core::ProjectDocument& document() const;
  void replace_document(core::ProjectDocument document);
  void project_document(const core::ObjectId& selected = core::ObjectId());
  void project_object(const core::ObjectId& id);

  // 根节点使用确定性保留 ID；领域对象 ID 只从专用 Data Role 读取。
  static core::ObjectId root_id(const QString& root_name);
  core::ObjectId id_for_item(const QTreeWidgetItem* item) const;
  QTreeWidgetItem* item_for_id(const core::ObjectId& id) const;

 private:
  QTreeWidget* tree_;
  QStringList root_names_;
  core::ProjectDocument document_;
};

}  // namespace gmp
