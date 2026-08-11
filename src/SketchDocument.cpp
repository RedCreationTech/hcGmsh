#include "gmp/SketchDocument.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <set>

namespace gmp {
namespace {

constexpr double kPi = 3.14159265358979323846;

double dist(const SketchPoint2d& a, const SketchPoint2d& b) {
  return std::hypot(a.x - b.x, a.y - b.y);
}

// 点到线段最短距离
double dist_to_segment(const SketchPoint2d& pt, const SketchPoint2d& a,
                       const SketchPoint2d& b) {
  const double dx = b.x - a.x;
  const double dy = b.y - a.y;
  const double len2 = dx * dx + dy * dy;
  if (len2 < 1e-18) {
    return dist(pt, a);
  }
  double t = ((pt.x - a.x) * dx + (pt.y - a.y) * dy) / len2;
  t = std::max(0.0, std::min(1.0, t));
  return dist(pt, {a.x + t * dx, a.y + t * dy});
}

// 把角度归一化到 [0, 2pi)
double norm_angle(double a) {
  a = std::fmod(a, 2.0 * kPi);
  if (a < 0.0) {
    a += 2.0 * kPi;
  }
  return a;
}

// 判断角度 a 是否落在从 start 逆时针到 end 的范围内
bool angle_in_arc(double a, double start, double end) {
  a = norm_angle(a);
  start = norm_angle(start);
  end = norm_angle(end);
  if (start <= end) {
    return a >= start && a <= end;
  }
  return a >= start || a <= end;  // 跨越 0 度
}

SketchPoint2d arc_point(const SketchEntity& e, double angle) {
  return {e.center.x + e.radius * std::cos(angle),
          e.center.y + e.radius * std::sin(angle)};
}

const char* entity_type_str(SketchEntityType t) {
  switch (t) {
    case SketchEntityType::Line: return "line";
    case SketchEntityType::Circle: return "circle";
    case SketchEntityType::Arc: return "arc";
  }
  return "line";
}

bool entity_type_from_str(const std::string& s, SketchEntityType* out) {
  if (s == "line") { *out = SketchEntityType::Line; return true; }
  if (s == "circle") { *out = SketchEntityType::Circle; return true; }
  if (s == "arc") { *out = SketchEntityType::Arc; return true; }
  return false;
}

const char* role_str(SketchPointRole r) {
  switch (r) {
    case SketchPointRole::None: return "none";
    case SketchPointRole::Start: return "start";
    case SketchPointRole::End: return "end";
    case SketchPointRole::Center: return "center";
  }
  return "none";
}

SketchPointRole role_from_str(const std::string& s) {
  if (s == "start") return SketchPointRole::Start;
  if (s == "end") return SketchPointRole::End;
  if (s == "center") return SketchPointRole::Center;
  return SketchPointRole::None;
}

const char* constraint_type_str(SketchConstraintType t) {
  switch (t) {
    case SketchConstraintType::Coincident: return "coincident";
    case SketchConstraintType::Horizontal: return "horizontal";
    case SketchConstraintType::Vertical: return "vertical";
    case SketchConstraintType::Parallel: return "parallel";
    case SketchConstraintType::Perpendicular: return "perpendicular";
    case SketchConstraintType::EqualLength: return "equal_length";
    case SketchConstraintType::EqualRadius: return "equal_radius";
    case SketchConstraintType::Fixed: return "fixed";
    case SketchConstraintType::Distance: return "distance";
    case SketchConstraintType::Radius: return "radius";
    case SketchConstraintType::Angle: return "angle";
  }
  return "coincident";
}

bool constraint_type_from_str(const std::string& s, SketchConstraintType* out) {
  static const std::map<std::string, SketchConstraintType> kMap = {
      {"coincident", SketchConstraintType::Coincident},
      {"horizontal", SketchConstraintType::Horizontal},
      {"vertical", SketchConstraintType::Vertical},
      {"parallel", SketchConstraintType::Parallel},
      {"perpendicular", SketchConstraintType::Perpendicular},
      {"equal_length", SketchConstraintType::EqualLength},
      {"equal_radius", SketchConstraintType::EqualRadius},
      {"fixed", SketchConstraintType::Fixed},
      {"distance", SketchConstraintType::Distance},
      {"radius", SketchConstraintType::Radius},
      {"angle", SketchConstraintType::Angle}};
  const auto it = kMap.find(s);
  if (it == kMap.end()) {
    return false;
  }
  *out = it->second;
  return true;
}

void write_point(YAML::Node& node, const char* key, const SketchPoint2d& p) {
  YAML::Node seq(YAML::NodeType::Sequence);
  seq.push_back(p.x);
  seq.push_back(p.y);
  node[key] = seq;
}

bool read_point(const YAML::Node& node, SketchPoint2d* out) {
  if (!node || !node.IsSequence() || node.size() != 2) {
    return false;
  }
  out->x = node[0].as<double>(0.0);
  out->y = node[1].as<double>(0.0);
  return true;
}

}  // namespace

bool operator==(const SketchPoint2d& a, const SketchPoint2d& b) {
  return a.x == b.x && a.y == b.y;
}

bool operator!=(const SketchPoint2d& a, const SketchPoint2d& b) {
  return !(a == b);
}

int SketchDocument::add_entity(const SketchEntity& e) {
  SketchEntity copy = e;
  copy.id = next_entity_id_++;
  entities_.push_back(copy);
  return copy.id;
}

bool SketchDocument::remove_entity(int id) {
  const auto it = std::find_if(entities_.begin(), entities_.end(),
                               [id](const SketchEntity& e) { return e.id == id; });
  if (it == entities_.end()) {
    return false;
  }
  entities_.erase(it);
  constraints_.erase(
      std::remove_if(constraints_.begin(), constraints_.end(),
                     [id](const SketchConstraint& c) {
                       return c.entity1 == id || c.entity2 == id;
                     }),
      constraints_.end());
  return true;
}

const SketchEntity* SketchDocument::entity(int id) const {
  for (const auto& e : entities_) {
    if (e.id == id) {
      return &e;
    }
  }
  return nullptr;
}

SketchEntity* SketchDocument::entity(int id) {
  for (auto& e : entities_) {
    if (e.id == id) {
      return &e;
    }
  }
  return nullptr;
}

int SketchDocument::add_constraint(const SketchConstraint& c) {
  SketchConstraint copy = c;
  copy.id = next_constraint_id_++;
  constraints_.push_back(copy);
  return copy.id;
}

bool SketchDocument::remove_constraint(int id) {
  const auto it =
      std::find_if(constraints_.begin(), constraints_.end(),
                   [id](const SketchConstraint& c) { return c.id == id; });
  if (it == constraints_.end()) {
    return false;
  }
  constraints_.erase(it);
  return true;
}

const SketchConstraint* SketchDocument::constraint(int id) const {
  for (const auto& c : constraints_) {
    if (c.id == id) {
      return &c;
    }
  }
  return nullptr;
}

SketchConstraint* SketchDocument::constraint(int id) {
  for (auto& c : constraints_) {
    if (c.id == id) {
      return &c;
    }
  }
  return nullptr;
}

void SketchDocument::clear() {
  entities_.clear();
  constraints_.clear();
  next_entity_id_ = 1;
  next_constraint_id_ = 1;
}

bool SketchDocument::point_at_role(const SketchEntity& e, SketchPointRole role,
                                   SketchPoint2d* out) {
  if (!out) {
    return false;
  }
  switch (e.type) {
    case SketchEntityType::Line:
      if (role == SketchPointRole::Start) { *out = e.p1; return true; }
      if (role == SketchPointRole::End) { *out = e.p2; return true; }
      if (role == SketchPointRole::Center) {
        *out = {(e.p1.x + e.p2.x) / 2.0, (e.p1.y + e.p2.y) / 2.0};
        return true;
      }
      return false;
    case SketchEntityType::Circle:
      if (role == SketchPointRole::Center) { *out = e.center; return true; }
      return false;
    case SketchEntityType::Arc:
      if (role == SketchPointRole::Start) {
        *out = arc_point(e, e.start_angle);
        return true;
      }
      if (role == SketchPointRole::End) {
        *out = arc_point(e, e.end_angle);
        return true;
      }
      if (role == SketchPointRole::Center) { *out = e.center; return true; }
      return false;
  }
  return false;
}

double SketchDocument::distance_to_entity(const SketchEntity& e,
                                          const SketchPoint2d& pt) {
  switch (e.type) {
    case SketchEntityType::Line:
      return dist_to_segment(pt, e.p1, e.p2);
    case SketchEntityType::Circle:
      return std::fabs(dist(pt, e.center) - e.radius);
    case SketchEntityType::Arc: {
      const double d = dist(pt, e.center);
      if (d < 1e-12) {
        return e.radius;
      }
      const double ang = std::atan2(pt.y - e.center.y, pt.x - e.center.x);
      if (angle_in_arc(ang, e.start_angle, e.end_angle)) {
        return std::fabs(d - e.radius);
      }
      // 角度范围外: 退化为到两端点的距离
      return std::min(dist(pt, arc_point(e, e.start_angle)),
                      dist(pt, arc_point(e, e.end_angle)));
    }
  }
  return std::numeric_limits<double>::max();
}

int SketchDocument::hit_test(const SketchPoint2d& pt, double tol) const {
  int best_id = -1;
  double best = tol;
  for (const auto& e : entities_) {
    const double d = distance_to_entity(e, pt);
    if (d <= best) {
      best = d;
      best_id = e.id;
    }
  }
  return best_id;
}

std::vector<std::vector<int>> SketchDocument::closed_loops(double tol) const {
  std::vector<std::vector<int>> loops;

  // 1) 圆自身成环
  for (const auto& e : entities_) {
    if (e.type == SketchEntityType::Circle) {
      loops.push_back({e.id});
    }
  }

  // 2) 线/弧端点聚类成图节点, 图元为边, 枚举简单环
  struct Edge {
    int entity_id;
    int node_a;
    int node_b;
  };
  std::vector<SketchPoint2d> nodes;
  auto find_or_add_node = [&](const SketchPoint2d& p) -> int {
    for (size_t i = 0; i < nodes.size(); ++i) {
      if (dist(nodes[i], p) <= tol) {
        return static_cast<int>(i);
      }
    }
    nodes.push_back(p);
    return static_cast<int>(nodes.size() - 1);
  };

  std::vector<Edge> edges;
  for (const auto& e : entities_) {
    SketchPoint2d a, b;
    bool ok = false;
    if (e.type == SketchEntityType::Line) {
      a = e.p1;
      b = e.p2;
      ok = true;
    } else if (e.type == SketchEntityType::Arc) {
      a = arc_point(e, e.start_angle);
      b = arc_point(e, e.end_angle);
      ok = true;
    }
    if (!ok) {
      continue;
    }
    const int na = find_or_add_node(a);
    const int nb = find_or_add_node(b);
    if (na == nb) {
      loops.push_back({e.id});  // 退化: 首尾同点 (如整圆弧)
      continue;
    }
    edges.push_back({e.id, na, nb});
  }

  // 邻接表
  std::map<int, std::vector<int>> adj;  // node -> edge indices
  for (size_t i = 0; i < edges.size(); ++i) {
    adj[edges[i].node_a].push_back(static_cast<int>(i));
    adj[edges[i].node_b].push_back(static_cast<int>(i));
  }

  // DFS 枚举简单环, 以"排序后的实体 id 列表"去重
  std::set<std::multiset<int>> seen;
  std::vector<int> path_edges;   // edge indices
  std::vector<int> path_nodes;   // node sequence, path_nodes[0] 为起点

  std::function<void(int, int)> dfs = [&](int start, int cur) {
    for (const int ei : adj[cur]) {
      const Edge& e = edges[ei];
      const int nxt = (e.node_a == cur) ? e.node_b : e.node_a;
      if (nxt == start && path_edges.size() >= 2) {
        // 成环 (>=3 条边时才算; 2 边环即往返, 排除)
        std::multiset<int> key;
        for (const int pe : path_edges) {
          key.insert(edges[pe].entity_id);
        }
        key.insert(e.entity_id);
        if (key.size() >= 3 && seen.insert(key).second) {
          std::vector<int> loop;
          for (const int pe : path_edges) {
            loop.push_back(edges[pe].entity_id);
          }
          loop.push_back(e.entity_id);
          loops.push_back(loop);
        }
        continue;
      }
      // 不重复经过节点 (除 start 外)
      if (std::find(path_nodes.begin(), path_nodes.end(), nxt) !=
          path_nodes.end()) {
        continue;
      }
      path_edges.push_back(ei);
      path_nodes.push_back(nxt);
      dfs(start, nxt);
      path_edges.pop_back();
      path_nodes.pop_back();
    }
  };

  for (size_t start = 0; start < nodes.size(); ++start) {
    path_edges.clear();
    path_nodes = {static_cast<int>(start)};
    dfs(static_cast<int>(start), static_cast<int>(start));
  }

  return loops;
}

QString SketchDocument::to_yaml_string() const {
  YAML::Node root;
  YAML::Node entities(YAML::NodeType::Sequence);
  for (const auto& e : entities_) {
    YAML::Node n;
    n["id"] = e.id;
    n["type"] = entity_type_str(e.type);
    if (e.type == SketchEntityType::Line) {
      write_point(n, "p1", e.p1);
      write_point(n, "p2", e.p2);
    } else {
      write_point(n, "center", e.center);
      n["radius"] = e.radius;
      if (e.type == SketchEntityType::Arc) {
        n["start_angle"] = e.start_angle;
        n["end_angle"] = e.end_angle;
      }
    }
    entities.push_back(n);
  }
  root["entities"] = entities;

  YAML::Node constraints(YAML::NodeType::Sequence);
  for (const auto& c : constraints_) {
    YAML::Node n;
    n["id"] = c.id;
    n["type"] = constraint_type_str(c.type);
    if (c.entity1 >= 0) {
      n["entity1"] = c.entity1;
      n["role1"] = role_str(c.role1);
    }
    if (c.entity2 >= 0) {
      n["entity2"] = c.entity2;
      n["role2"] = role_str(c.role2);
    }
    if (c.type == SketchConstraintType::Distance ||
        c.type == SketchConstraintType::Radius ||
        c.type == SketchConstraintType::Angle) {
      n["value"] = c.value;
      n["driving"] = c.driving;
    }
    constraints.push_back(n);
  }
  root["constraints"] = constraints;
  root["next_entity_id"] = next_entity_id_;
  root["next_constraint_id"] = next_constraint_id_;

  return QString::fromStdString(YAML::Dump(root));
}

bool SketchDocument::from_yaml_string(const QString& yaml, QString* error) {
  SketchDocument tmp;
  try {
    const YAML::Node root = YAML::Load(yaml.toStdString());
    if (!root || !root.IsMap()) {
      if (error) *error = "sketch yaml is not a map";
      return false;
    }
    const YAML::Node entities = root["entities"];
    if (entities && entities.IsSequence()) {
      for (const auto& n : entities) {
        SketchEntity e;
        e.id = n["id"].as<int>(0);
        if (e.id <= 0 ||
            !entity_type_from_str(n["type"].as<std::string>(""), &e.type)) {
          continue;
        }
        if (e.type == SketchEntityType::Line) {
          if (!read_point(n["p1"], &e.p1) || !read_point(n["p2"], &e.p2)) {
            continue;
          }
        } else {
          if (!read_point(n["center"], &e.center)) {
            continue;
          }
          e.radius = n["radius"].as<double>(0.0);
          if (e.type == SketchEntityType::Arc) {
            e.start_angle = n["start_angle"].as<double>(0.0);
            e.end_angle = n["end_angle"].as<double>(0.0);
          }
        }
        tmp.entities_.push_back(e);
        tmp.next_entity_id_ = std::max(tmp.next_entity_id_, e.id + 1);
      }
    }
    const YAML::Node constraints = root["constraints"];
    if (constraints && constraints.IsSequence()) {
      for (const auto& n : constraints) {
        SketchConstraint c;
        c.id = n["id"].as<int>(0);
        if (c.id <= 0 ||
            !constraint_type_from_str(n["type"].as<std::string>(""), &c.type)) {
          continue;
        }
        c.entity1 = n["entity1"].as<int>(-1);
        c.entity2 = n["entity2"].as<int>(-1);
        c.role1 = role_from_str(n["role1"].as<std::string>("none"));
        c.role2 = role_from_str(n["role2"].as<std::string>("none"));
        c.value = n["value"].as<double>(0.0);
        c.driving = n["driving"].as<bool>(true);
        tmp.constraints_.push_back(c);
        tmp.next_constraint_id_ = std::max(tmp.next_constraint_id_, c.id + 1);
      }
    }
    if (root["next_entity_id"]) {
      tmp.next_entity_id_ =
          std::max(tmp.next_entity_id_, root["next_entity_id"].as<int>(1));
    }
    if (root["next_constraint_id"]) {
      tmp.next_constraint_id_ = std::max(tmp.next_constraint_id_,
                                         root["next_constraint_id"].as<int>(1));
    }
  } catch (const std::exception& e) {
    if (error) *error = QString::fromUtf8(e.what());
    return false;
  }
  *this = std::move(tmp);
  return true;
}

}  // namespace gmp
