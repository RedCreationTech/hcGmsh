#pragma once

#include <QString>

namespace gmp {

// 加载 GMP_ENV_FILE 或项目/可执行文件附近的 .env。已有进程环境变量优先，
// 返回实际加载的文件路径；未找到时返回空字符串。
QString load_dotenv();

}  // namespace gmp
