#include <QApplication>
#include <QDebug>
#include <QString>
#include <QTimer>

#ifdef GMP_ENABLE_VTK_VIEWER
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>
#endif

#ifdef GMP_ENABLE_GMSH_GUI
#include "gmp/OccBridge.h"
#endif

#include "gmp/MainWindow.h"
#include "gmp/Env.h"
#include "gmp/ComboPopupFix.h"
#include "gmp/OperationLog.h"
#include "gmp/IconFactory.h"

int main(int argc, char** argv) {
#ifdef GMP_ENABLE_VTK_VIEWER
  QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
#endif
  QApplication app(argc, argv);

  // 全局统一样式须设在 QApplication 级：主窗口样式表不覆盖独立顶级
  // 工作窗/浮动表单（Sketch/Mesh/Job/表单均为无父对象工具窗），
  // 且控件带局部样式表时其 tooltip 只继承应用级规则。
  app.setStyleSheet(QStringLiteral(
      "QToolTip { background: #1f2937; color: #f9fafb; border: none; "
      "padding: 4px 8px; }"
      // 滚动条：带上下箭头的经典细条样式（用户指定），全应用统一。
      "QScrollBar:vertical { background: #f0f2f5; width: 14px; margin: 0; }"
      "QScrollBar::handle:vertical { background: #c8ced6; border: 1px solid "
      "#b8bfc8; min-height: 24px; }"
      "QScrollBar::handle:vertical:hover { background: #aeb6c0; }"
      "QScrollBar::add-line:vertical { height: 14px; background: #e4e8ee; "
      "subcontrol-position: bottom; subcontrol-origin: margin; "
      "border-top: 1px solid #d5dbe3; }"
      "QScrollBar::sub-line:vertical { height: 14px; background: #e4e8ee; "
      "subcontrol-position: top; subcontrol-origin: margin; "
      "border-bottom: 1px solid #d5dbe3; }"
      "QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical { "
      "width: 8px; height: 8px; }"
      "QScrollBar:horizontal { background: #f0f2f5; height: 14px; margin: 0; }"
      "QScrollBar::handle:horizontal { background: #c8ced6; border: 1px solid "
      "#b8bfc8; min-width: 24px; }"
      "QScrollBar::handle:horizontal:hover { background: #aeb6c0; }"
      "QScrollBar::add-line:horizontal { width: 14px; background: #e4e8ee; "
      "subcontrol-position: right; subcontrol-origin: margin; "
      "border-left: 1px solid #d5dbe3; }"
      "QScrollBar::sub-line:horizontal { width: 14px; background: #e4e8ee; "
      "subcontrol-position: left; subcontrol-origin: margin; "
      "border-right: 1px solid #d5dbe3; }"
      "QScrollBar::left-arrow:horizontal, QScrollBar::right-arrow:horizontal { "
      "width: 8px; height: 8px; }"
      "QScrollBar::add-page, QScrollBar::sub-page { background: none; }"
      // 勾选/单选：强调蓝选中态，全应用一致（消除原生风格差异）。
      "QCheckBox, QRadioButton { spacing: 6px; }"
      "QCheckBox::indicator, QRadioButton::indicator { width: 14px; "
      "height: 14px; border: 1px solid #aab4c0; border-radius: 3px; "
      "background: #ffffff; }"
      "QRadioButton::indicator { border-radius: 7px; }"
      "QCheckBox::indicator:checked { background: #2f6fed; "
      "border: 1px solid #2f6fed; image: url(\":/icons/check.png\"); }"
      "QRadioButton::indicator:checked { background: #2f6fed; "
      "border: 1px solid #2f6fed; image: url(\":/icons/dot.png\"); }"
      "QCheckBox::indicator:hover, QRadioButton::indicator:hover { "
      "border-color: #2f6fed; }"
      // 滑块：与强调蓝一致（播放进度条等）。
      "QSlider::groove:horizontal { height: 4px; background: #d5dbe3; "
      "border-radius: 2px; }"
      "QSlider::handle:horizontal { width: 14px; height: 14px; "
      "margin: -5px 0; border-radius: 7px; background: #2f6fed; }"
      "QSlider::handle:horizontal:hover { background: #1d4ed8; }"));

  // 全局下拉框滚轮拦截：选项只能通过展开下拉点选切换。
  gmp::install_combo_wheel_block(&app);

  // 操作日志必须先于一切业务初始化，保证启动期错误也留痕。
  gmp::init_operation_log();
  gmp::log_operation(
      "app", QString("GMP-ISE starting (pid %1)").arg(app.applicationPid()));

  const QString env_path = gmp::load_dotenv();
  if (!env_path.isEmpty()) {
    qInfo() << "Loaded local environment configuration:" << env_path;
    gmp::log_operation("app", "Loaded local environment configuration: " +
                                  env_path);
  }

#ifdef GMP_ENABLE_GMSH_GUI
  // WS0-A 冒烟测试: GMP_WS0_SMOKE=1 时在 GUI 启动前运行, 不影响正常启动
  if (qgetenv("GMP_WS0_SMOKE") == "1") {
    qInfo() << "[WS0] occ_direct_call_smoke:" << gmp::occ_direct_call_smoke();
    qInfo() << "[WS0] planegcs_smoke:" << gmp::planegcs_smoke();
  }
#endif

  gmp::MainWindow window;

  // 图标体系初始化（QtAwesome / FA7 Free，须在 QApplication 之后、UI 构造前）
  gmp::icons::init(&window);
  // 开发工具：GMP_ICON_AUDIT=1 时报告映射表中渲染为空白的字形。
  if (qEnvironmentVariableIsSet("GMP_ICON_AUDIT")) {
    const QStringList blank = gmp::icons::auditBlankGlyphs();
    qWarning("[icon-audit] total mapped glyphs checked, blank: %d",
             int(blank.size()));
    for (const QString& glyph : blank) {
      qWarning("[icon-audit] blank glyph: %s", qPrintable(glyph));
    }
  }
  window.show();

  // 文档截图巡览：GMP_SCREENSHOT_DIR=<目录> 时自动切换页面截图并退出
  const QByteArray shot_dir = qgetenv("GMP_SCREENSHOT_DIR");
  if (!shot_dir.isEmpty()) {
    QTimer::singleShot(1200, &window, [&window, shot_dir]() {
      window.run_screenshot_tour(QString::fromLocal8Bit(shot_dir));
    });
  }

  return app.exec();
}
