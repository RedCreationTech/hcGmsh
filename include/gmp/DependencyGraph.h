#pragma once

// v0.2 核心层（hc_core 过渡形态，Q5 批复：逻辑边界先行）。依赖图
// （doc/ref/03 §7.2）：登记上游→下游边，提供下游查询、拓扑排序与 stale
// 传播闭包。只依赖 Qt Core；Stage 1 与旧程序化传播并存（旧逻辑仍唯一
// 生效），TASK-V02-060 才切换为图驱动。

#include <QList>
#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>

namespace gmp::core {

// 键为对象 ID 字符串或 kind 名（本阶段的 legacy stale 规则是 kind 级）。
class DependencyGraph {
 public:
  // 自环与重复边拒绝（返回 false）。
  bool addDependency(const QString& upstream, const QString& downstream);
  bool removeDependency(const QString& upstream, const QString& downstream);
  bool hasDependency(const QString& upstream, const QString& downstream) const;
  int edgeCount() const;

  // 直接下游/上游，按登记顺序。
  QStringList downstream(const QString& upstream) const;
  QStringList upstream(const QString& downstream) const;

  // stale 传播闭包：从 source 出发的全部传递下游，不含 source 自身；
  // BFS 发现序，确定性输出。
  QStringList markStaleFrom(const QString& source) const;

  // Kahn 拓扑排序（上游先于下游）；有环时返回空表且 hasCycle() 为 true。
  QStringList topologicalOrder() const;
  bool hasCycle() const;

 private:
  // 邻接表：上游 → 下游列表（登记顺序）。
  QMap<QString, QStringList> edges_;
};

// MainWindow::invalidate_downstream_from() 既有程序化 stale 规则的单一
// 真源登记（kind 级边集）。细节与旧实现逐条对应：
//   - Jobs/Results 不作为源（无出边）；
//   - Parts/Features/Sketches → Assembly，且与其他源一样 → Mesh/Input
//     Cases/Jobs（旧代码 else 分支对非 Mesh/Input Cases 源追加三者）；
//   - Mesh → Input Cases/Jobs；Input Cases → Jobs。
// 旧规则中“跳过 run/queue/submit 状态对象”是应用侧过滤，不是边规则，
// 不登记在此。对照测试（test_dependency_graph_contract）把本边集的传播
// 闭包与手工转录的旧规则矩阵逐条比对，任一侧改动都会变红。
QList<QPair<QString, QString>> legacy_stale_rule_edges();

}  // namespace gmp::core
