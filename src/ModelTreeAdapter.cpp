#include "gmp/ModelTreeAdapter.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QSignalBlocker>
#include <QSet>

#include <functional>
#include <utility>

#include "gmp/PropertyBag.h"
#include "gmp/PropertyEditor.h"

namespace gmp {

ModelTreeAdapter::ModelTreeAdapter(QTreeWidget* tree)
    : QObject(tree), tree_(tree) {
  for (int i = 0; tree_ && i < tree_->topLevelItemCount(); ++i) {
    QTreeWidgetItem* root = tree_->topLevelItem(i);
    if (!root) {
      continue;
    }
    root_names_.append(root->text(0));
    const core::ObjectId id = root_id(root->text(0));
    document_.addObject(std::make_unique<core::ProjectObject>(
        root->text(0), root->text(0), id));
    root->setData(0, PropertyEditor::kObjectIdRole, id.toString());
  }
}

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

core::ProjectDocument& ModelTreeAdapter::document() {
  return document_;
}

const core::ProjectDocument& ModelTreeAdapter::document() const {
  return document_;
}

void ModelTreeAdapter::replace_document(core::ProjectDocument document) {
  for (const QString& root_name : root_names_) {
    const core::ObjectId id = root_id(root_name);
    if (!document.contains(id)) {
      document.addObject(
          std::make_unique<core::ProjectObject>(root_name, root_name, id));
    }
  }
  document_ = std::move(document);
  project_document();
}

void ModelTreeAdapter::project_object(const core::ObjectId& id) {
  QTreeWidgetItem* item = item_for_id(id);
  const core::ProjectObject* object = document_.object(id);
  if (!item || !object) {
    return;
  }
  QSignalBlocker blocker(tree_);
  item->setText(0, object->name());
  item->setData(0, PropertyEditor::kKindRole, object->kind());
  item->setData(0, PropertyEditor::kParamsRole,
                object->properties().to_variant_map());
  item->setData(0, PropertyEditor::kStatusRole, object->statusText());
  item->setData(0, PropertyEditor::kObjectIdRole, id.toString());
}

void ModelTreeAdapter::project_document(const core::ObjectId& selected) {
  if (!tree_) {
    return;
  }
  const core::ObjectId selected_id =
      selected.isValid() ? selected : id_for_item(tree_->currentItem());
  QSignalBlocker blocker(tree_);

  QMap<QString, QIcon> root_icons;
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    QTreeWidgetItem* root = tree_->topLevelItem(i);
    if (root) {
      root_icons.insert(root->text(0), root->icon(0));
    }
  }
  auto update_item = [this](QTreeWidgetItem* item, const core::ObjectId& id,
                            const QIcon& icon) {
    const core::ProjectObject* object = document_.object(id);
    if (!item || !object) {
      return;
    }
    item->setIcon(0, icon);
    item->setText(0, object->name());
    item->setData(0, PropertyEditor::kKindRole, object->kind());
    item->setData(0, PropertyEditor::kParamsRole,
                  object->properties().to_variant_map());
    item->setData(0, PropertyEditor::kStatusRole, object->statusText());
    item->setData(0, PropertyEditor::kObjectIdRole, id.toString());
  };

  std::function<void(QTreeWidgetItem*, const core::ObjectId&, const QIcon&)>
      reconcile_children = [&](QTreeWidgetItem* parent,
                               const core::ObjectId& parent_id,
                               const QIcon& icon) {
        const QList<core::ObjectId> children = document_.children(parent_id);
        QSet<QString> expected;
        for (int row = 0; row < children.size(); ++row) {
          const core::ObjectId id = children.at(row);
          expected.insert(id.toString());
          QTreeWidgetItem* item = nullptr;
          for (int old_row = row; old_row < parent->childCount(); ++old_row) {
            if (id_for_item(parent->child(old_row)) == id) {
              item = parent->takeChild(old_row);
              parent->insertChild(row, item);
              break;
            }
          }
          if (!item) {
            item = new QTreeWidgetItem();
            parent->insertChild(row, item);
          }
          update_item(item, id, icon);
          reconcile_children(item, id, icon);
        }
        for (int row = parent->childCount() - 1; row >= 0; --row) {
          if (!expected.contains(id_for_item(parent->child(row)).toString())) {
            delete parent->takeChild(row);
          }
        }
      };

  const QList<core::ObjectId> roots = document_.roots();
  QSet<QString> expected_roots;
  for (int row = 0; row < roots.size(); ++row) {
    const core::ObjectId id = roots.at(row);
    const core::ProjectObject* object = document_.object(id);
    if (!object) {
      continue;
    }
    expected_roots.insert(id.toString());
    QTreeWidgetItem* root = nullptr;
    for (int old_row = row; old_row < tree_->topLevelItemCount(); ++old_row) {
      if (id_for_item(tree_->topLevelItem(old_row)) == id) {
        root = tree_->takeTopLevelItem(old_row);
        tree_->insertTopLevelItem(row, root);
        break;
      }
    }
    if (!root) {
      root = new QTreeWidgetItem();
      tree_->insertTopLevelItem(row, root);
    }
    const QIcon icon = root_icons.value(object->name(), root->icon(0));
    update_item(root, id, icon);
    reconcile_children(root, id, icon);
  }
  for (int row = tree_->topLevelItemCount() - 1; row >= 0; --row) {
    if (!expected_roots.contains(
            id_for_item(tree_->topLevelItem(row)).toString())) {
      delete tree_->takeTopLevelItem(row);
    }
  }
  tree_->expandToDepth(0);
  blocker.unblock();
  if (QTreeWidgetItem* item = item_for_id(selected_id)) {
    tree_->setCurrentItem(item);
  }
}

}  // namespace gmp
