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

int main(int argc, char** argv) {
#ifdef GMP_ENABLE_VTK_VIEWER
  QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
#endif
  QApplication app(argc, argv);

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
