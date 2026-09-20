#include "gmp/ModelTreeAdapter.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <functional>
#include <utility>

#include "gmp/PropertyBag.h"
#include "gmp/PropertyEditor.h"

namespace gmp {

ModelTreeAdapter::ModelTreeAdapter(QTreeWidget* tree)
    : QObject(tree), tree_(tree) {}

core::ObjectId ModelTreeAdapter::root_id(const QString& root_name) {
  return core::ObjectId::root(root_name);
}

core::ObjectId ModelTreeAdapter::id_for_item(const QTreeWidgetItem* item) const {
  if (!item) {
    return core::ObjectId();
  }
  const QString stored =
      item->data(0, PropertyEditor::kObjectIdRole).toString();
  return stored.isEmpty() && !item->parent() ? root_id(item->text(0))
                                             : core::ObjectId(stored);
}

QTreeWidgetItem* ModelTreeAdapter::item_for_id(const core::ObjectId& id) const {
  if (!tree_ || !id.isValid()) {
    return nullptr;
  }
  std::function<QTreeWidgetItem*(QTreeWidgetItem*)> find =
      [&](QTreeWidgetItem* item) -> QTreeWidgetItem* {
    if (!item) {
      return nullptr;
    }
    if (id_for_item(item) == id) {
      return item;
    }
    for (int row = 0; row < item->childCount(); ++row) {
      if (auto* match = find(item->child(row))) {
        return match;
      }
    }
    return nullptr;
  };
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    if (auto* match = find(tree_->topLevelItem(i))) {
      return match;
    }
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

void ModelTreeAdapter::replace_document(core::ProjectDocument document) {
  document_ = std::move(document);
  dirty_ = false;
}

void ModelTreeAdapter::rebuild_from_tree() {
  document_.clear();
  if (tree_) {
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
      QTreeWidgetItem* root = tree_->topLevelItem(i);
      if (!root) {
        continue;
      }
      const core::ObjectId root_object_id = root_id(root->text(0));
      if (!document_.addObject(std::make_unique<core::ProjectObject>(
                               root->text(0), root->text(0), root_object_id))
               .isValid()) {
        continue;
      }
      for (int row = 0; row < root->childCount(); ++row) {
        QTreeWidgetItem* child = root->child(row);
        if (!child) {
          continue;
        }
        // 旧会话中尚无专用 role 的节点在第一次投影时获得 ID。
        core::ObjectId object_id = id_for_item(child);
        if (!object_id.isValid()) {
          object_id = core::ObjectId::generate();
          child->setData(0, PropertyEditor::kObjectIdRole,
                         object_id.toString());
        }
        auto object = std::make_unique<core::ProjectObject>(
            child->data(0, PropertyEditor::kKindRole).toString(),
            child->text(0), object_id);
        object->setStatusText(
            child->data(0, PropertyEditor::kStatusRole).toString());
        object->properties() = core::PropertyBag::from_variant_map(
            child->data(0, PropertyEditor::kParamsRole).toMap());
        document_.addObject(std::move(object), root_object_id);
      }
    }
  }
  dirty_ = false;
}

}  // namespace gmp
