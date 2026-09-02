#include "gmp/ApplicationProfile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSet>

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
  QStringList errors;

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
      errors.append(QStringLiteral("Cannot read profile: %1").arg(file));
      continue;
    }
    const QByteArray data = f.readAll();
    QJsonParseError parse_error;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parse_error);
    if (!doc.isObject()) {
      errors.append(QStringLiteral("Invalid JSON in profile %1: %2")
                        .arg(file, parse_error.errorString()));
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

    QStringList profile_errors;
    if (profile.id.trimmed().isEmpty()) {
      profile_errors.append(QStringLiteral("missing id"));
    }
    if (profiles_.contains(profile.id)) {
      profile_errors.append(QStringLiteral("duplicate id"));
    }
    static const QSet<QString> allowed_status = {
        QStringLiteral("production"), QStringLiteral("prototype"),
        QStringLiteral("unsupported")};
    if (!allowed_status.contains(profile.status)) {
      profile_errors.append(QStringLiteral("invalid status '%1'").arg(profile.status));
    }
    if (!allowed_status.contains(profile.support_level)) {
      profile_errors.append(
          QStringLiteral("invalid support_level '%1'").arg(profile.support_level));
    }
    if (profile.status == QStringLiteral("production") &&
        profile.support_level != QStringLiteral("production")) {
      profile_errors.append(QStringLiteral(
          "production status requires production support_level"));
    }
    if (profile.solver_program.trimmed().isEmpty()) {
      profile_errors.append(QStringLiteral("missing solver_program"));
    }
    if (profile.physics.isEmpty()) {
      profile_errors.append(QStringLiteral("physics list is empty"));
    }
    QSet<QString> physics_ids;
    for (const auto& physics : profile.physics) {
      if (physics.id.trimmed().isEmpty() || physics_ids.contains(physics.id)) {
        profile_errors.append(QStringLiteral("physics ids must be non-empty and unique"));
      }
      physics_ids.insert(physics.id);
      if (physics.dimensions.isEmpty() || physics.supported_blocks.isEmpty()) {
        profile_errors.append(
            QStringLiteral("physics '%1' has empty dimensions or supported_blocks")
                .arg(physics.id));
      }
      for (int dim : physics.dimensions) {
        if (dim < 1 || dim > 3) {
          profile_errors.append(
              QStringLiteral("physics '%1' has invalid dimension %2")
                  .arg(physics.id)
                  .arg(dim));
        }
      }
    }
    const QStringList required_units = {"name", "length", "force", "time",
                                        "mass", "pressure", "temperature"};
    for (const QString& key : required_units) {
      if (profile.unit_contract.value(key).trimmed().isEmpty()) {
        profile_errors.append(
            QStringLiteral("unit_contract is missing '%1'").arg(key));
      }
    }
    if (profile.mapping_registry_path.trimmed().isEmpty() ||
        profile.mapping_version.trimmed().isEmpty()) {
      profile_errors.append(QStringLiteral("mapping registry path/version is required"));
    } else {
      const QString mapping_path =
          QFileInfo(QDir(root_dir_).filePath(profile.mapping_registry_path))
              .absoluteFilePath();
      QFile mapping_file(mapping_path);
      if (!mapping_file.open(QIODevice::ReadOnly)) {
        profile_errors.append(
            QStringLiteral("mapping registry not found: %1")
                .arg(profile.mapping_registry_path));
      } else {
        const QJsonDocument mapping_doc = QJsonDocument::fromJson(mapping_file.readAll());
        const QString actual_version =
            mapping_doc.isObject()
                ? mapping_doc.object().value("version").toString()
                : QString();
        if (actual_version != profile.mapping_version) {
          profile_errors.append(
              QStringLiteral("mapping version mismatch: expected %1, got %2")
                  .arg(profile.mapping_version, actual_version));
        }
      }
    }
    if (!profile_errors.isEmpty()) {
      errors.append(QStringLiteral("Invalid profile %1: %2")
                        .arg(file, profile_errors.join(", ")));
      continue;
    }
    profile.valid = true;
    profiles_.insert(profile.id, profile);
  }

  last_error_ = errors.join(QStringLiteral("; "));
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
