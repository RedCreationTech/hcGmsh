#pragma once

// v0.2 Stage 4 hc_mesh 过渡层（Q5 批复：逻辑边界先行）。
// PhysicalGroupService：自定义 Physical Group 的定义存储、JSON 持久化
// （gmsh.custom_physical_groups_json）、owner+bbox 几何签名恢复/重绑定、
// 组输入校验。只依赖 Qt Core + gmsh API（GMP_ENABLE_GMSH_GUI 守护），
// 不依赖 Qt Widgets；控件读取与日志/反馈呈现由 GmshPanel 负责。
// 实现逐字搬运自 GmshPanel 同名方法（019 缺陷修复点时序不得变化：
// restore() 在装配重建内的调用位置保持不变）。

#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QStringList>

#include <vector>

namespace gmp {

class PhysicalGroupService {
 public:
  // ---- 定义存储（替代 GmshPanel::custom_physical_groups_） ----
  void set_definitions(const QVariantList& defs) { definitions_ = defs; }
  const QVariantList& definitions() const { return definitions_; }
  void clear() { definitions_.clear(); }

  // JSON 持久化（紧凑单行，与既有 gmsh 设置读写器兼容）。非法 JSON
  // （非空且非 "[]"）返回 false 并写 error（既有提示文案）。
  QString to_json() const;
  bool load_json(const QString& json, QString* error);

  void forget(const QString& name);
  // 记忆当前 gmsh 模型上的组定义（owner+bbox 签名）。is_assembly_active
  // 替代面板 model_selector 状态读取；返回 false = 未持久化（原因入 log）。
  bool remember(const QString& name, int dim, const std::vector<int>& tags,
                bool is_assembly_active, QStringList* log);
  // 在当前 gmsh 模型上按 owner+bbox 重绑定全部已存定义（装配重建内
  // 调用点不变）；恢复/拒绝原因入 log（既有文案逐字保持）。
  void restore(QStringList* log);

  // 组输入校验。name/entities_text 由面板读取控件后传入；
  // has_existing_entities = 面板 resolve_entity_tags 结果非空；
  // chinese 替代 l10n 依赖。返回 false 并写 error（中英文案逐字保持）。
  bool validate_group_input(int dim, const QString& name,
                            const QString& entities_text,
                            bool has_existing_entities, int exclude_tag,
                            bool chinese, QString* error) const;

  // ---- 纯文本/纯 gmsh 工具（自 GmshPanel 下沉，各特征共用） ----
  struct DimTagToken {
    int dim = -1;
    int tag = 0;
    bool has_dim = false;
  };
  static std::vector<DimTagToken> parse_dim_tag_tokens(const QString& text);
  static QVariantList entity_bounding_box(int dim, int tag);
  static QString assembly_owner_for_entity(int dim, int tag);
  static std::vector<int> assembly_owner_entities(const QString& owner,
                                                  int dim);

 private:
  QVariantList definitions_;
};

}  // namespace gmp
