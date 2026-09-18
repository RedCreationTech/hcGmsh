#include "gmp/ModelTreeAdapter.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "gmp/PropertyBag.h"
#include "gmp/PropertyEditor.h"

namespace gmp {

ModelTreeAdapter::ModelTreeAdapter(QTreeWidget* tree)
    : QObject(tree), tree_(tree) {}

core::ObjectId ModelTreeAdapter::id_for_path(const QString& root_name,
                                             const QString& child_name) {
  if (root_name.trimmed().isEmpty()) {
    return core::ObjectId();
  }
  return core::ObjectId(child_name.isEmpty()
                            ? root_name
                            : root_name + QStringLiteral("/") + child_name);
}

core::ObjectId ModelTreeAdapter::id_for_item(const QTreeWidgetItem* item) const {
  if (!item) {
    return core::ObjectId();
  }
  const QTreeWidgetItem* parent = item->parent();
  return id_for_path(parent ? parent->text(0) : item->text(0),
                     parent ? item->text(0) : QString());
}

QTreeWidgetItem* ModelTreeAdapter::item_for_id(const core::ObjectId& id) const {
  if (!tree_ || !id.isValid()) {
    return nullptr;
  }
  const QString path = id.toString();
  const int slash = path.indexOf('/');
  const QString root_name =
      slash < 0 ? path : path.left(slash);
  const QString child_name = slash < 0 ? QString() : path.mid(slash + 1);
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    QTreeWidgetItem* root = tree_->topLevelItem(i);
    if (!root || root->text(0) != root_name) {
      continue;
    }
    if (child_name.isEmpty()) {
      return root;
    }
    for (int row = 0; row < root->childCount(); ++row) {
      if (root->child(row) && root->child(row)->text(0) == child_name) {
        return root->child(row);
      }
    }
    return nullptr;
  }
  return nullptr;
}

void ModelTreeAdapter::mark_dirty() {
  dirty_ = true;
}

core::ProjectDocument& ModelTreeAdapter::document() {
  if (dirty_) {
    rebuild_from_tree();
  }
  return document_;
}

void ModelTreeAdapter::rebuild_from_tree() {
  document_.clear();
  if (tree_) {
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
      const QTreeWidgetItem* root = tree_->topLevelItem(i);
      if (!root) {
        continue;
      }
      const core::ObjectId root_id = id_for_path(root->text(0), QString());
      if (!document_.addObject(std::make_unique<core::ProjectObject>(
                               root->text(0), root->text(0), root_id))
               .isValid()) {
        continue;
      }
      for (int row = 0; row < root->childCount(); ++row) {
        const QTreeWidgetItem* child = root->child(row);
        if (!child) {
          continue;
        }
        // 同名子节点会被 unique_child_name 拒绝；若仍出现重复路径，
        // addObject 拒绝并保持先挂载者，一致性合同会暴露数量差异。
        auto object = std::make_unique<core::ProjectObject>(
            child->data(0, PropertyEditor::kKindRole).toString(),
            child->text(0), id_for_item(child));
        object->setStatus(core::object_status_from_string(
            child->data(0, PropertyEditor::kStatusRole).toString()));
        object->properties() = core::PropertyBag::from_variant_map(
            child->data(0, PropertyEditor::kParamsRole).toMap());
        document_.addObject(std::move(object), root_id);
      }
    }
  }
  dirty_ = false;
}

}  // namespace gmp
