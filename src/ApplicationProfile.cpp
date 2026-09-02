#include "gmp/ApplicationProfile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace gmp {

namespace {

QString json_string_or(const QJsonObject& obj, const QString& key,
                       const QString& fallback = QString()) {
  const QJsonValue v = obj.value(key);
  if (v.isString()) {
    return v.toString();
  }
  if (v.isDouble()) {
    return QString::number(v.toDouble());
  }
  return fallback;
}

QMap<QString, QString> json_to_string_map(const QJsonObject& obj) {
  QMap<QString, QString> map;
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    const QJsonValue v = it.value();
    if (v.isString()) {
      map.insert(it.key(), v.toString());
    } else if (v.isDouble()) {
      map.insert(it.key(), QString::number(v.toDouble()));
    }
  }
  return map;
}

}  // namespace

ApplicationProfileRegistry::ApplicationProfileRegistry() {
  set_root(default_root());
}

ApplicationProfileRegistry::ApplicationProfileRegistry(const QString& root_dir) {
  set_root(root_dir);
}

void ApplicationProfileRegistry::set_root(const QString& root_dir) {
  root_dir_ = QDir(root_dir).absolutePath();
}

bool ApplicationProfileRegistry::reload() {
  profiles_.clear();
  last_error_.clear();

  if (root_dir_.isEmpty() || !QDir(root_dir_).exists()) {
    last_error_ = QStringLiteral("Application profile root not found: %1").arg(root_dir_);
    return false;
  }

  const QStringList files = QDir(root_dir_).entryList(QStringList() << "*.json",
                                                       QDir::Files, QDir::Name);
  for (const QString& file : files) {
    const QString path = QDir(root_dir_).filePath(file);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
      continue;
    }
    const QByteArray data = f.readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
      last_error_ = QStringLiteral("Invalid JSON in profile: %1").arg(file);
      continue;
    }
    const QJsonObject obj = doc.object();

    ApplicationProfile profile;
    profile.id = json_string_or(obj, "id");
    profile.display_name = json_string_or(obj, "display_name", profile.id);
    profile.version = json_string_or(obj, "version", "0.0.0");
    profile.status = json_string_or(obj, "status", "prototype");
    profile.status_note = json_string_or(obj, "status_note");
    profile.solver_program = json_string_or(obj, "solver_program", profile.id);
    profile.compute_environment = json_string_or(obj, "compute_environment");
    profile.mapping_registry_path = json_string_or(obj.value("mapping_registry").toObject(),
                                                     "path", "../mapping-v1.json");
    profile.mapping_version = json_string_or(obj.value("mapping_registry").toObject(),
                                               "version", "1.0.0");
    profile.check_command = json_string_or(obj, "check_command",
                                              "{solver} -i {input} --check-input");
    profile.support_level = json_string_or(obj, "support_level", profile.status);
    profile.unit_contract = json_to_string_map(obj.value("unit_contract").toObject());
    profile.raw = obj;
    profile.valid = true;

    const QJsonArray physics_arr = obj.value("physics").toArray();
    for (const QJsonValue& pv : physics_arr) {
      if (!pv.isObject()) {
        continue;
      }
      const QJsonObject pobj = pv.toObject();
      ApplicationProfilePhysics physics;
      physics.id = json_string_or(pobj, "id");
      physics.display_name = json_string_or(pobj, "display_name", physics.id);
      const QJsonArray dims = pobj.value("dimensions").toArray();
      for (const QJsonValue& dv : dims) {
        if (dv.isDouble()) {
          physics.dimensions.append(static_cast<int>(dv.toDouble()));
        }
      }
      const QJsonArray blocks = pobj.value("supported_blocks").toArray();
      for (const QJsonValue& bv : blocks) {
        if (bv.isString()) {
          physics.supported_blocks.append(bv.toString());
        }
      }
      profile.physics.append(physics);
    }

    if (profile.id.isEmpty()) {
      last_error_ = QStringLiteral("Profile missing 'id': %1").arg(file);
      continue;
    }
    profiles_.insert(profile.id, profile);
  }

  return !profiles_.isEmpty();
}

bool ApplicationProfileRegistry::is_loaded() const {
  return !profiles_.isEmpty();
}

QList<QString> ApplicationProfileRegistry::profile_ids() const {
  return profiles_.keys();
}

ApplicationProfile ApplicationProfileRegistry::profile(const QString& id) const {
  return profiles_.value(id);
}

QList<ApplicationProfile> ApplicationProfileRegistry::profiles() const {
  return profiles_.values();
}

ApplicationProfile ApplicationProfileRegistry::default_production_profile() const {
  for (const auto& p : profiles_) {
    if (p.status == QStringLiteral("production")) {
      return p;
    }
  }
  return ApplicationProfile();
}

bool ApplicationProfileRegistry::supports_physics(const QString& id,
                                                  const QString& physics_id) const {
  const auto p = profile(id);
  if (!p.valid) {
    return false;
  }
  for (const auto& ph : p.physics) {
    if (ph.id == physics_id) {
      return true;
    }
  }
  return false;
}

bool ApplicationProfileRegistry::supports_block(const QString& id,
                                                const QString& physics_id,
                                                const QString& block_name) const {
  const auto p = profile(id);
  if (!p.valid) {
    return false;
  }
  for (const auto& ph : p.physics) {
    if (ph.id == physics_id && ph.supported_blocks.contains(block_name)) {
      return true;
    }
  }
  return false;
}

QString ApplicationProfileRegistry::last_error() const {
  return last_error_;
}

QString ApplicationProfileRegistry::default_root() {
  const QByteArray env = qgetenv("GMP_MOOSE_PROFILES");
  if (!env.isEmpty()) {
    const QString path = QString::fromLocal8Bit(env);
    if (QDir(path).exists()) {
      return QDir(path).absolutePath();
    }
  }

#ifdef GMP_PROFILE_DIR
  const QString compiled = QString::fromUtf8(GMP_PROFILE_DIR);
  if (QDir(compiled).exists()) {
    return QDir(compiled).absolutePath();
  }
#endif

  const QString app_dir = QCoreApplication::applicationDirPath();
  const QStringList candidates = {
      app_dir + "/templates/moose/profiles",
      app_dir + "/../templates/moose/profiles",
      app_dir + "/../../templates/moose/profiles",
      QDir::currentPath() + "/templates/moose/profiles",
  };
  for (const auto& c : candidates) {
    if (QDir(c).exists()) {
      return QDir(c).absolutePath();
    }
  }
  return QString();
}

}  // namespace gmp
