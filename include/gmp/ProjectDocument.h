#pragma once

// v0.2 核心层（hc_core 过渡形态，Q5 批复：逻辑边界先行，不搬目录、不改
// 命名前缀；层次 = core/domain）。本头文件只依赖 Qt Core，禁止引入
// Qt Widgets / Gmsh / VTK / 网络。Stage 1 只建层不接线：现有代码不调用
// 本层，直至 TASK-V02-014 投影适配器落地。

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <map>
#include <memory>

namespace gmp::core {

// 稳定对象 ID。生成后不再变化；序列化（to/from_variant_list）原样保留，
// 项目重开语义不变。空值表示无效 ID（用作“无父对象”等哨兵）。
class ObjectId {
 public:
  ObjectId() = default;
  explicit ObjectId(const QString& value) : value_(value) {}

  static ObjectId generate();  // QUuid 随机 ID，冲突概率可忽略

  bool isValid() const { return !value_.isEmpty(); }
  QString toString() const { return value_; }

  friend bool operator==(const ObjectId& lhs, const ObjectId& rhs) {
    return lhs.value_ == rhs.value_;
  }
  friend bool operator!=(const ObjectId& lhs, const ObjectId& rhs) {
    return !(lhs == rhs);
  }

 private:
  QString value_;
};

// 对象类别语义键。与 YAML `kind` / 模型树根节点名一致（"Parts"、
// "Materials" 等）；强类型对象集合（SketchObject/PartObject…）推迟到
// doc/ref/03 §5.3 排期，本阶段保持字符串键以兼容全部既有节点类型。
using ObjectKind = QString;

// 对象状态机，取值与既有 status 字符串逐字对齐（doc/schema/project-v2.md
// §3.2）。空字符串与未知大小写按 Ready 解析（旧项目 status: "" 的语义）。
enum class ObjectStatus { Ready, Incomplete, Invalid, Stale, Disabled };

QString to_string(ObjectStatus status);
ObjectStatus object_status_from_string(const QString& text, bool* ok = nullptr);

class PropertyBag;

// 最小领域对象：稳定 ID + 名称 + 类别 + 状态 + 属性包。
class ProjectObject {
 public:
  ProjectObject(ObjectKind kind, const QString& name,
                ObjectId id = ObjectId());

  ObjectId id() const { return id_; }
  const QString& name() const { return name_; }
  const ObjectKind& kind() const { return kind_; }
  ObjectStatus status() const { return status_; }
  void setName(const QString& name) { name_ = name; }
  void setStatus(ObjectStatus status) { status_ = status; }

  PropertyBag& properties();
  const PropertyBag& properties() const;

 private:
  // ProjectDocument 在挂载时为无 ID 对象分配稳定 ID。
  friend class ProjectDocument;

  ObjectId id_;
  QString name_;
  ObjectKind kind_;
  ObjectStatus status_ = ObjectStatus::Ready;
  std::unique_ptr<PropertyBag> properties_;
};

// 项目文档：对象生命周期、父子层级与状态机的唯一真源。不负责 Qt 树、
// VTK、网络、文件对话框或 MOOSE 文本（doc/ref/03 §5.1 职责边界）。
// DependencyGraph / Transaction / Recompute 由 TASK-V02-012/013 引入。
class ProjectDocument {
 public:
  ProjectDocument();
  ~ProjectDocument();

  // 挂载对象。object 未携带 ID 时由文档分配；ID 重复或 parent 无效
  // （非空且不存在）时失败并返回无效 ObjectId。parent 为空 = 顶层对象。
  ObjectId addObject(std::unique_ptr<ProjectObject> object,
                     ObjectId parent = ObjectId());
  // 删除对象及其整个子树；对象不存在返回 false。
  bool removeObject(ObjectId id);
  void clear();

  ProjectObject* object(ObjectId id);
  const ProjectObject* object(ObjectId id) const;
  bool contains(ObjectId id) const;
  int count() const;

  ObjectId parentOf(ObjectId id) const;
  // 直接子对象，按挂载顺序。parent 为空时返回顶层对象（同 roots()）。
  QList<ObjectId> children(ObjectId parent = ObjectId()) const;
  QList<ObjectId> roots() const { return children(ObjectId()); }

  bool setStatus(ObjectId id, ObjectStatus status);

  // 确定性序列化（遍历顺序固定）：用于 ID 稳定性与 round-trip 合同测试。
  // 条目：{id, name, kind, status, parent, params}。
  QVariantList to_variant_list() const;
  bool from_variant_list(const QVariantList& list, QString* error = nullptr);

 private:
  // Qt 容器要求值可拷贝，unique_ptr 对象表使用 std::map（键序遍历保证
  // to_variant_list 输出确定性）。
  std::map<QString, std::unique_ptr<ProjectObject>> objects_;
  QMap<QString, ObjectId> parent_;
  QMap<QString, QList<ObjectId>> children_;  // parent id（空串=顶层）→ 子序
};

}  // namespace gmp::core
