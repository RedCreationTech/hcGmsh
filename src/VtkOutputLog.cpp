#include "gmp/VtkOutputLog.h"

#ifdef GMP_ENABLE_VTK_VIEWER

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>

#include <vtkFileOutputWindow.h>
#include <vtkObjectFactory.h>
#include <vtkOutputWindow.h>
#include <vtkSmartPointer.h>

namespace {

// 转发器：每条 VTK 消息先落盘（vtkFileOutputWindow），再原样转给
// VTK 原生输出窗口（Windows 为 vtkWin32OutputWindow，即警告弹窗；
// 桌面平台为 stderr）。五个 Display* 出口全部覆盖，行为与原生一致。
class VtkOutputForwarder final : public vtkOutputWindow {
 public:
  static VtkOutputForwarder* New();
  vtkTypeMacro(VtkOutputForwarder, vtkOutputWindow);

  void SetLogger(vtkFileOutputWindow* logger) { this->Logger = logger; }
  void SetForwardTarget(vtkOutputWindow* target) {
    this->ForwardTarget = target;
  }

  void DisplayText(const char* text) override {
    if (this->Logger) this->Logger->DisplayText(text);
    if (this->ForwardTarget) this->ForwardTarget->DisplayText(text);
  }
  void DisplayErrorText(const char* text) override {
    if (this->Logger) this->Logger->DisplayErrorText(text);
    if (this->ForwardTarget) this->ForwardTarget->DisplayErrorText(text);
  }
  void DisplayWarningText(const char* text) override {
    if (this->Logger) this->Logger->DisplayWarningText(text);
    if (this->ForwardTarget) this->ForwardTarget->DisplayWarningText(text);
  }
  void DisplayGenericWarningText(const char* text) override {
    if (this->Logger) this->Logger->DisplayGenericWarningText(text);
    if (this->ForwardTarget)
      this->ForwardTarget->DisplayGenericWarningText(text);
  }
  void DisplayDebugText(const char* text) override {
    if (this->Logger) this->Logger->DisplayDebugText(text);
    if (this->ForwardTarget) this->ForwardTarget->DisplayDebugText(text);
  }

 private:
  vtkSmartPointer<vtkFileOutputWindow> Logger;
  vtkSmartPointer<vtkOutputWindow> ForwardTarget;
};
vtkStandardNewMacro(VtkOutputForwarder);

QString vtk_log_path() {
  const QDir log_dir(QCoreApplication::applicationDirPath() +
                     QStringLiteral("/logs"));
  log_dir.mkpath(QStringLiteral("."));
  return log_dir.filePath(QStringLiteral("vtk-output.log"));
}

} // namespace

namespace gmp {

void init_vtk_output_log() {
  static bool initialized = false;
  if (initialized) {
    return;
  }
  initialized = true;

  const QString path = vtk_log_path();

  // 预写文件头：确认目录可写并让日志文件始终存在（无 VTK 消息时为
  // 仅含文件头的空日志，便于现场直接定位路径）。
  if (QFile file(path); file.open(QIODevice::WriteOnly | QIODevice::Append)) {
    file.write(QStringLiteral("\n== GMP-ISE VTK output log started %1 ==\n")
                   .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
                   .toUtf8());
  }

  // 函数级静态持有，进程退出前不释放，避免与 VTK 静态析构顺序纠缠。
  static vtkSmartPointer<vtkFileOutputWindow> logger;
  static vtkSmartPointer<vtkOutputWindow> target;
  static vtkSmartPointer<VtkOutputForwarder> forwarder;
  logger = vtkSmartPointer<vtkFileOutputWindow>::New();
  logger->SetFileName(path.toUtf8().constData());
  logger->SetAppend(1); // 默认截断模式会冲掉启动时预写的文件头
  // 取当前平台默认实例（Windows 即弹窗实现）作为转发目标。
  target = vtkOutputWindow::GetInstance();
  forwarder = vtkSmartPointer<VtkOutputForwarder>::New();
  forwarder->SetLogger(logger);
  forwarder->SetForwardTarget(target);
  vtkOutputWindow::SetInstance(forwarder);
}

} // namespace gmp

#else // GMP_ENABLE_VTK_VIEWER

namespace gmp {
void init_vtk_output_log() {}
} // namespace gmp

#endif
