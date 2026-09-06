#pragma once

#include <QString>
#include <functional>

namespace gmp {

// 轻量操作日志：所有关键用户操作（项目、几何导入、网格生成、作业、结果
// 加载等）统一经过 log_operation 记录，带时间戳写入持久化日志文件，
// 并可通过钩子同步到 UI 控制台。目标是出现问题后能凭日志复现操作序列、
// 定位根因，而不是依赖记忆和猜测。

// 启动时调用一次：确定日志目录、打开当日日志文件、清理过期文件、
// 安装 Qt message handler（把 qWarning/qCritical 也收入同一文件）。
void init_operation_log();

// 当前日志文件路径；尚未初始化时返回空串。
QString operation_log_path();

// 记录一条操作日志。category 建议短分类名（project/geometry/mesh/job/
// results/ui 等），message 为具体操作与结果（含参数与错误详情）。
void log_operation(const QString& category, const QString& message);

// 挂载 UI 控制台钩子；每条操作日志格式化后同步回调。传 nullptr 解除。
void set_operation_log_console_hook(
    std::function<void(const QString&)> hook);

}  // namespace gmp
