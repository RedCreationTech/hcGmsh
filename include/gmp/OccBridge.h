#ifndef GMP_OCCBRIDGE_H
#define GMP_OCCBRIDGE_H

// WS0-A: OpenCASCADE 直调通道 + planegcs 约束求解器的最小验证接口。
// WS3: 草图 -> OCC 特征 (拉伸/旋转/扫掠/放样) 接口 (契约, 实现由 WS3 完成)。
// 仅在 GMP_ENABLE_GMSH_GUI=ON 时参与构建。

#include <QString>
#include <vector>

namespace gmp {

class SketchDocument;

// OCC 直调冒烟测试: 用 BRepOffsetAPI_ThruSections 建一个两圆截面 loft,
// BRepTools::Write 到字符串, 再经 gmsh::model::occ::importShapes 回注 gmsh
// 模型, 返回是否成功 (体积数 >= 1)。函数内自行保证 gmsh 已初始化。
bool occ_direct_call_smoke();

// planegcs 冒烟测试: 两点 + 一条线段 + 一个 P2P 距离约束, 调用 GCS::System
// 求解, 返回求解是否收敛且结果满足约束。
bool planegcs_smoke();

// ---- WS3: 草图特征操作 ----
// 通用流程: SketchDocument 闭环 -> OCC wire/face -> 特征建模 ->
// importShapes 回注 gmsh (供网格模块直接划分) + 可选落盘 brep。
// 草图坐标为 XY 平面毫米; loft 的 z_offset 表示该截面相对草图平面的抬升。
struct FeatureResult {
  bool ok = false;
  QString error;             // 失败原因 (面向用户)
  int gmsh_volume_tag = 0;   // importShapes 后主体积 tag, 0 表示未知/未导入
  QString brep_path;         // brep_out_path 非空且写盘成功时回显
};

// 拉伸: 沿 +Z 拉 distance (负值则反向)
FeatureResult extrude_sketch(const SketchDocument& doc, double distance,
                             const QString& brep_out_path = QString());
// 旋转: 绕 Y 轴 (草图平面内过原点) 转 angle_deg 度
FeatureResult revolve_sketch(const SketchDocument& doc, double angle_deg,
                             const QString& brep_out_path = QString());
// 放样: 截面序列 (草图, z 抬升), solid=true 生成实体
FeatureResult loft_sketches(
    const std::vector<std::pair<const SketchDocument*, double>>& sections,
    bool solid, const QString& brep_out_path = QString());
// 扫掠: profile 沿 path (另一草图的开环折线/弧线) 扫掠
FeatureResult sweep_sketch(const SketchDocument& profile,
                           const SketchDocument& path,
                           const QString& brep_out_path = QString());

// 对当前 gmsh 模型 (最近一次特征导入) 生成网格并写出 msh, 供视口即时显示。
// 优先生成 3D 网格, 失败回退 2D 面网格。msh_out_path 非空。
bool mesh_current_model(const QString& msh_out_path, QString* error = nullptr);

} // namespace gmp

#endif // GMP_OCCBRIDGE_H
