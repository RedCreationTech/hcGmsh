#include "gmp/DependencyGraph.h"

#include <QQueue>
#include <QSet>

#include "gmp/ProjectSchema.h"

namespace gmp::core {

bool DependencyGraph::addDependency(const QString& upstream,
                                    const QString& downstream) {
  if (upstream.isEmpty() || downstream.isEmpty() || upstream == downstream) {
    return false;
  }
  QStringList& targets = edges_[upstream];
  if (targets.contains(downstream)) {
    return false;
  }
  targets.append(downstream);
  return true;
}

bool DependencyGraph::removeDependency(const QString& upstream,
                                       const QString& downstream) {
  auto it = edges_.find(upstream);
  if (it == edges_.end() || !it.value().contains(downstream)) {
    return false;
  }
  it.value().removeAll(downstream);
  if (it.value().isEmpty()) {
    edges_.erase(it);
  }
  return true;
}

bool DependencyGraph::hasDependency(const QString& upstream,
                                    const QString& downstream) const {
  return edges_.value(upstream).contains(downstream);
}

int DependencyGraph::edgeCount() const {
  int count = 0;
  for (auto it = edges_.cbegin(); it != edges_.cend(); ++it) {
    count += it.value().size();
  }
  return count;
}

QStringList DependencyGraph::downstream(const QString& upstream) const {
  return edges_.value(upstream);
}

QStringList DependencyGraph::upstream(const QString& downstream) const {
  QStringList sources;
  for (auto it = edges_.cbegin(); it != edges_.cend(); ++it) {
    if (it.value().contains(downstream)) {
      sources.append(it.key());
    }
  }
  return sources;
}

QStringList DependencyGraph::markStaleFrom(const QString& source) const {
  QStringList closure;
  QSet<QString> visited;
  QQueue<QString> queue;
  queue.enqueue(source);
  visited.insert(source);
  while (!queue.isEmpty()) {
    const QString current = queue.dequeue();
    for (const QString& next : edges_.value(current)) {
      if (visited.contains(next)) {
        continue;
      }
      visited.insert(next);
      closure.append(next);
      queue.enqueue(next);
    }
  }
  return closure;
}

QStringList DependencyGraph::topologicalOrder() const {
  QMap<QString, int> in_degree;
  for (auto it = edges_.cbegin(); it != edges_.cend(); ++it) {
    in_degree.insert(it.key(), in_degree.value(it.key(), 0));
    for (const QString& target : it.value()) {
      in_degree[target] = in_degree.value(target, 0) + 1;
    }
  }
  QQueue<QString> queue;
  for (auto it = in_degree.cbegin(); it != in_degree.cend(); ++it) {
    if (it.value() == 0) {
      queue.enqueue(it.key());
    }
  }
  QStringList order;
  QMap<QString, QStringList> edges = edges_;
  while (!queue.isEmpty()) {
    const QString node = queue.dequeue();
    order.append(node);
    for (const QString& next : edges.value(node)) {
      in_degree[next] -= 1;
      if (in_degree.value(next) == 0) {
        queue.enqueue(next);
      }
    }
  }
  if (order.size() != in_degree.size()) {
    return {};  // 有环
  }
  return order;
}

bool DependencyGraph::hasCycle() const {
  if (edges_.isEmpty()) {
    return false;
  }
  return topologicalOrder().isEmpty();
}

QList<QPair<QString, QString>> legacy_stale_rule_edges() {
  QList<QPair<QString, QString>> edges;
  auto add = [&edges](const QString& from, const QString& to) {
    edges.append(qMakePair(from, to));
  };
  const QStringList geometry_sources = {"Parts", "Features", "Sketches"};
  for (const QString& source : geometry_sources) {
    add(source, "Assembly");
  }
  add("Mesh", "Input Cases");
  add("Mesh", "Jobs");
  add("Input Cases", "Jobs");
  // 旧实现 else 分支：除 Mesh/Input Cases 外的一切源（含上面的几何源）
  // 都追加 Mesh/Input Cases/Jobs；Jobs/Results 不作为源。
  for (const QString& root : gmp::project_schema::model_root_nodes()) {
    if (root == "Jobs" || root == "Results" || root == "Mesh" ||
        root == "Input Cases") {
      continue;
    }
    add(root, "Mesh");
    add(root, "Input Cases");
    add(root, "Jobs");
  }
  return edges;
}

}  // namespace gmp::core
