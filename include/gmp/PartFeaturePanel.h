#pragma once

// WS3: 部件页签"特征操作"区面板 (契约头, 实现由 WS3 完成)。
// 承载 拉伸/旋转/放样/扫掠 的入口按钮与参数输入,
// 信号由 MainWindow 接线到 OccBridge 特征函数与模型树 Features 根。

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;

namespace gmp {

class PartFeaturePanel : public QWidget {
  Q_OBJECT
 public:
  explicit PartFeaturePanel(QWidget* parent = nullptr);

  // 由 MainWindow 喂入当前可选草图名列表 (模型树 Sketches 根子节点名)
  void set_sketch_names(const QStringList& names);
  QString selected_sketch() const;
  // 放样第二截面的 Z 抬升 (mm); loft_requested 信号只带截面名,
  // 接线方在收到信号后经此接口取抬升值 (WS3 新增, 契约补充)
  double loft_second_z() const;

 signals:
  void extrude_requested(const QString& sketch, double distance);
  void revolve_requested(const QString& sketch, double angle_deg);
  void loft_requested(const QStringList& sketches);   // 按顺序的截面名
  void sweep_requested(const QString& profile, const QString& path);

 private:
  QComboBox* sketch_combo_ = nullptr;        // 拉伸/旋转共用主草图选择
  QDoubleSpinBox* extrude_distance_ = nullptr;
  QDoubleSpinBox* revolve_angle_ = nullptr;
  QComboBox* loft_first_ = nullptr;          // 放样第一截面
  QComboBox* loft_second_ = nullptr;         // 放样第二截面
  QDoubleSpinBox* loft_z_ = nullptr;         // 第二截面 Z 抬升 (mm)
  QComboBox* sweep_profile_ = nullptr;       // 扫掠轮廓草图
  QComboBox* sweep_path_ = nullptr;          // 扫掠路径草图
};

}  // namespace gmp
