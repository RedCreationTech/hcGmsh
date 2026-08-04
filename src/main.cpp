#include <QApplication>
#include <QString>
#include <QTimer>

#ifdef GMP_ENABLE_VTK_VIEWER
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>
#endif

#include "gmp/MainWindow.h"

int main(int argc, char** argv) {
#ifdef GMP_ENABLE_VTK_VIEWER
  QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
#endif
  QApplication app(argc, argv);

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
