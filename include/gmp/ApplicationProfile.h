#pragma once

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

namespace gmp {

// MOOSE 应用档案中的一条物理场能力声明。
struct ApplicationProfilePhysics {
  QString id;
  QString display_name;
  QList<int> dimensions;
  QStringList supported_blocks;
  QJsonObject extra;  // 应用专属扩展字段
};

// MOOSE 应用档案（Application Profile）。
// 对应需求 REQ-013 中“求解应用/物理配置”的合同化描述。
struct ApplicationProfile {
  QString id;                       // 唯一标识，如 "DamSafetyApp-opt"
  QString display_name;             // 展示名
  QString version;                  // 档案版本
  QString status;                   // production | prototype | unsupported
  QString status_note;              // 状态说明（prototype 的限制）
  QString solver_program;           // 可执行程序名或命令白名单
  QString compute_environment;      // 计算环境标识或描述
  QList<ApplicationProfilePhysics> physics;
  QString mapping_registry_path;    // 相对档案目录的映射注册表路径
  QString mapping_version;            // 映射注册表版本
  QString check_command;            // 校验命令模板，如 "{solver} -i {input} --check-input"
  QString support_level;            // production | prototype
  QMap<QString, QString> unit_contract;  // 单位合同键值
  QJsonObject raw;                  // 原始 JSON 对象
  bool valid = false;
};

// 应用档案注册表。
// 负责从 profiles 目录加载所有 *.json，提供查询与版本校验。
class ApplicationProfileRegistry {
 public:
  ApplicationProfileRegistry();
  explicit ApplicationProfileRegistry(const QString& root_dir);

  void set_root(const QString& root_dir);
  bool reload();
  bool is_loaded() const;

  QList<QString> profile_ids() const;
  ApplicationProfile profile(const QString& id) const;
  QList<ApplicationProfile> profiles() const;

  // 返回第一个 status == production 的 profile；没有则返回空 profile。
  ApplicationProfile default_production_profile() const;

  // 检查指定 profile 是否支持给定物理场和 block。
  bool supports_physics(const QString& id, const QString& physics_id) const;
  bool supports_block(const QString& id, const QString& physics_id,
                      const QString& block_name) const;

  QString last_error() const;

  // 注册表根目录定位：环境变量 GMP_MOOSE_PROFILES > 编译期 GMP_PROFILE_DIR >
  // 可执行文件旁/源码树内 templates/moose/profiles。
  static QString default_root();

 private:
  QString root_dir_;
  QMap<QString, ApplicationProfile> profiles_;
  QString last_error_;
};

}  // namespace gmp
