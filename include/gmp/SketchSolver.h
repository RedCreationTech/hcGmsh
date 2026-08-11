#pragma once

// WS2: planegcs 约束求解封装 (契约头, 实现由 WS2 完成)。
// 把 SketchDocument 的图元+约束映射到 GCS::System, 求解后回写几何。
//
// 注意: planegcs 的 System::solve() 不写回调用内存,
// 必须再调 applySolution() 才能拿到解 (WS0-A 已验证, 见 src/OccBridge.cpp)。

#include <QString>

namespace gmp {

class SketchDocument;

class SketchSolver {
 public:
  SketchSolver();
  ~SketchSolver();

  SketchSolver(const SketchSolver&) = delete;
  SketchSolver& operator=(const SketchSolver&) = delete;

  // 求解 doc 当前全部约束; 收敛时把解回写到 doc 的图元几何。
  // 欠约束(仍可求解时按最小改动收敛)不视为失败; 矛盾约束返回 false。
  bool solve(SketchDocument& doc, QString* error = nullptr);

  // 尺寸驱动入口: 修改驱动尺寸约束的目标值并重求解。
  // constraint_id 必须是 Distance/Radius/Angle 且 driving=true 的约束。
  bool set_driving_value(SketchDocument& doc, int constraint_id, double value,
                         QString* error = nullptr);

  // 诊断: 返回 (欠约束自由度估计, 冲突约束数)。不要求精确, 供 UI 提示用。
  int last_dof() const { return last_dof_; }
  int last_conflict_count() const { return last_conflicts_; }

 private:
  int last_dof_ = -1;
  int last_conflicts_ = 0;
};

}  // namespace gmp
