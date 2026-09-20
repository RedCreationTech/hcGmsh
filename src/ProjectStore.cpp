#include "gmp/ProjectStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <fstream>

#include "gmp/OperationLog.h"
#include "gmp/ProjectDocument.h"
#include "gmp/ProjectSchema.h"

namespace gmp {

QString project_case_work_dir(const QString& project_path) {
  if (project_path.trimmed().isEmpty()) {
    return {};
  }
  const QFileInfo project_info(project_path);
  QString project_name = project_info.fileName();
  if (project_name.endsWith(".gmp.yaml", Qt::CaseInsensitive)) {
    project_name.chop(QString(".gmp.yaml").size());
  } else {
    project_name = project_info.completeBaseName();
  }
  project_name.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
  if (project_name.isEmpty()) {
    return {};
  }
  return QDir(project_info.absolutePath())
      .absoluteFilePath(".work/case/" + project_name);
}

QString default_mesh_output_path(const QString& project_path,
                                 const QString& mesh_name) {
  const QString file_name =
      (mesh_name.trimmed().isEmpty() ? QStringLiteral("mesh")
                                     : mesh_name.trimmed()) +
      QStringLiteral(".msh");
  const QString project_dir = project_case_work_dir(project_path);
  return project_dir.isEmpty()
             ? QDir::current().absoluteFilePath("out/" + file_name)
             : QDir(project_dir).absoluteFilePath(file_name);
}

bool is_legacy_default_mesh_path(const QString& path) {
  if (path.trimmed().isEmpty()) {
    return false;
  }
  const QString clean_path = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
  const QString legacy_dir =
      QDir::cleanPath(QDir::current().absoluteFilePath("out"));
  return clean_path.startsWith(legacy_dir + QDir::separator());
}

QString enclosing_case_work_dir(const QString& path) {
  if (path.trimmed().isEmpty()) {
    return {};
  }
  const QString clean = QDir::fromNativeSeparators(
      QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
  const QStringList parts = clean.split('/', Qt::SkipEmptyParts);
  for (int i = 0; i + 2 < parts.size(); ++i) {
    if (parts.at(i) == ".work" && parts.at(i + 1) == "case") {
      QString dir = parts.mid(0, i + 3).join('/');
      if (clean.startsWith('/')) {
        dir.prepend('/');
      }
      return dir;
    }
  }
  return {};
}

namespace {

bool same_file_path(const QString& lhs, const QString& rhs) {
  if (lhs.trimmed().isEmpty() || rhs.trimmed().isEmpty()) {
    return false;
  }
  return QDir::cleanPath(QFileInfo(lhs).absoluteFilePath()) ==
         QDir::cleanPath(QFileInfo(rhs).absoluteFilePath());
}

// 标量设置段的类型 coercion：force_string 中的键保持字符串，其余按
// bool/int/double/string 推断（与既有加载行为逐字一致）。
QVariantMap parse_settings_node(const YAML::Node& node,
                                const QSet<QString>& force_string) {
  QVariantMap map;
  if (!node || !node.IsMap()) {
    return map;
  }
  for (const auto& it : node) {
    const QString key = QString::fromStdString(it.first.as<std::string>());
    const YAML::Node value = it.second;
    if (!value.IsScalar()) {
      continue;
    }
    const QString raw = QString::fromStdString(value.as<std::string>());
    if (force_string.contains(key)) {
      map.insert(key, raw);
      continue;
    }
    const QString lower = raw.toLower();
    if (lower == "true" || lower == "false") {
      map.insert(key, lower == "true");
      continue;
    }
    bool ok_int = false;
    const int int_val = raw.toInt(&ok_int);
    if (ok_int && !raw.contains('.') &&
        !raw.contains('e', Qt::CaseInsensitive)) {
      map.insert(key, int_val);
      continue;
    }
    bool ok_double = false;
    const double dbl_val = raw.toDouble(&ok_double);
    if (ok_double) {
      map.insert(key, dbl_val);
      continue;
    }
    map.insert(key, raw);
  }
  return map;
}

void write_settings_node(YAML::Node* node, const QVariantMap& settings) {
  for (auto it = settings.begin(); it != settings.end(); ++it) {
    const QVariant& val = it.value();
    switch (val.typeId()) {
      case QMetaType::Bool:
        (*node)[it.key().toStdString()] = val.toBool();
        break;
      case QMetaType::Int:
        (*node)[it.key().toStdString()] = val.toInt();
        break;
      case QMetaType::Double:
        (*node)[it.key().toStdString()] = val.toDouble();
        break;
      default:
        (*node)[it.key().toStdString()] = val.toString().toStdString();
        break;
    }
  }
}

// 与 MainWindow::unique_child_name 相同规则的数据级去重：base、base_2…
QString unique_entry_name(QSet<QString>* used, const QString& preferred) {
  QString base = preferred.trimmed();
  if (base.isEmpty()) {
    base = "item";
  }
  if (!used->contains(base)) {
    used->insert(base);
    return base;
  }
  for (int suffix = 2;; ++suffix) {
    const QString candidate = QString("%1_%2").arg(base).arg(suffix);
    if (!used->contains(candidate)) {
      used->insert(candidate);
      return candidate;
    }
  }
}

const QSet<QString> kMooseForceString = {"exec_path",   "input_path",
                                         "workdir",     "mesh_path",
                                         "template_key", "extra_args",
                                         "input_text",  "input_mode",
                                         "structured_input", "custom_blocks",
                                         "generation_report", "last_snapshot_dir"};
const QSet<QString> kViewerForceString = {"current_file", "array_key", "preset",
                                          "output_selected"};

}  // namespace

bool ProjectStore::load_file(const QString& path, ProjectData* out,
                             QString* error) const {
  if (!out) {
    return false;
  }
  auto fail = [error](const QString& message) {
    if (error) {
      *error = message;
    }
    return false;
  };
  try {
    const YAML::Node root = YAML::LoadFile(path.toStdString());

    int loaded_schema_version = project_schema::kCurrentVersion;
    if (root["schema_version"] && root["schema_version"].IsScalar()) {
      loaded_schema_version = root["schema_version"].as<int>();
    } else if (root["version"] && root["version"].IsScalar()) {
      const int old_version = root["version"].as<int>();
      if (old_version > 2) {
        return fail("Unsupported project version.");
      }
      loaded_schema_version = project_schema::kCurrentVersion;
    }
    if (loaded_schema_version < 1 ||
        loaded_schema_version > project_schema::kCurrentVersion) {
      return fail(QString("Unsupported schema version: %1")
                      .arg(loaded_schema_version));
    }

    ProjectData data;
    data.schema_version = loaded_schema_version;
    data.application_profile =
        project_schema::yaml_map_to_variant_map(root["application_profile"]);
    data.unit_contract =
        project_schema::yaml_map_to_variant_map(root["unit_contract"]);
    data.mesh_snapshot =
        project_schema::mesh_snapshot_from_yaml(root["mesh_snapshot"]);

    const YAML::Node model = root["model"];
    if (!model || !model.IsMap()) {
      return fail("Invalid project file (missing model).");
    }
    QMap<QString, QSet<QString>> used_names;
    QSet<QString> used_ids;
    for (const auto& it : model) {
      const QString kind = QString::fromStdString(it.first.as<std::string>());
      data.model_roots << kind;
      const YAML::Node list = it.second;
      if (!list.IsSequence()) {
        continue;
      }
      for (const auto& node : list) {
        const QString name =
            QString::fromStdString(node["name"].as<std::string>(""));
        if (name.isEmpty()) {
          continue;
        }
        ProjectModelEntry entry;
        entry.id = QString::fromStdString(node["id"].as<std::string>(""));
        if (entry.id.isEmpty()) {
          do {
            entry.id = core::ObjectId::generate().toString();
          } while (used_ids.contains(entry.id));
        } else if (used_ids.contains(entry.id)) {
          return fail("Duplicate project object id: " + entry.id);
        }
        used_ids.insert(entry.id);
        entry.kind = kind;
        entry.params = project_schema::yaml_map_to_variant_map(node["params"]);
        entry.name = unique_entry_name(&used_names[kind], name);
        entry.status = QString::fromStdString(
            node["status"].as<std::string>(
                entry.params.value("status").toString().toStdString()));
        data.model_entries.append(entry);
      }
    }

    data.gmsh_settings = parse_settings_node(root["gmsh"], {});
    data.moose_settings =
        parse_settings_node(root["moose"], kMooseForceString);
    const YAML::Node moose_node = root["moose"];
    if (moose_node && moose_node.IsMap() &&
        moose_node["input_snapshots"] &&
        moose_node["input_snapshots"].IsSequence()) {
      for (const auto& s : moose_node["input_snapshots"]) {
        data.input_snapshots.append(
            QString::fromStdString(s.as<std::string>("")));
      }
    }
    data.viewer_settings =
        parse_settings_node(root["viewer"], kViewerForceString);

    *out = data;
    return true;
  } catch (const std::exception& e) {
    return fail(QString("Failed to load: %1").arg(e.what()));
  }
}

bool ProjectStore::save_file(const QString& path, const ProjectData& data,
                             QString* error) const {
  auto fail = [error](const QString& message) {
    if (error) {
      *error = message;
    }
    return false;
  };
  try {
    YAML::Node root;
    root["schema_version"] = data.schema_version;
    root["version"] = 2;  // 保留旧字段以兼容只读 version 的工具
    root["saved_at"] =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
    root["application_profile"] =
        project_schema::variant_map_to_yaml(data.application_profile);
    root["unit_contract"] =
        project_schema::variant_map_to_yaml(data.unit_contract);
    root["mesh_snapshot"] =
        project_schema::mesh_snapshot_to_yaml(data.mesh_snapshot);

    // 按 model_roots 顺序输出全部根节点（含空根），根内保持条目顺序。
    QMap<QString, YAML::Node> entries_by_kind;
    for (const auto& entry : data.model_entries) {
      YAML::Node node;
      if (!entry.id.isEmpty()) {
        node["id"] = entry.id.toStdString();
      }
      node["name"] = entry.name.toStdString();
      node["kind"] = entry.kind.toStdString();
      node["status"] = entry.status.toStdString();
      node["params"] = project_schema::variant_map_to_yaml(entry.params);
      if (!entries_by_kind.contains(entry.kind)) {
        entries_by_kind.insert(entry.kind, YAML::Node(YAML::NodeType::Sequence));
      }
      entries_by_kind[entry.kind].push_back(node);
    }
    YAML::Node model(YAML::NodeType::Map);
    for (const QString& root_name : data.model_roots) {
      model[root_name.toStdString()] = entries_by_kind.contains(root_name)
                                           ? entries_by_kind[root_name]
                                           : YAML::Node(YAML::NodeType::Sequence);
    }
    root["model"] = model;

    if (!data.gmsh_settings.isEmpty()) {
      YAML::Node gmsh_node(YAML::NodeType::Map);
      write_settings_node(&gmsh_node, data.gmsh_settings);
      root["gmsh"] = gmsh_node;
    }
    if (!data.moose_settings.isEmpty() || !data.input_snapshots.isEmpty()) {
      YAML::Node moose_node(YAML::NodeType::Map);
      write_settings_node(&moose_node, data.moose_settings);
      if (!data.input_snapshots.isEmpty()) {
        YAML::Node snaps(YAML::NodeType::Sequence);
        for (const QString& s : data.input_snapshots) {
          snaps.push_back(s.toStdString());
        }
        moose_node["input_snapshots"] = snaps;
      }
      root["moose"] = moose_node;
    }
    if (!data.viewer_settings.isEmpty()) {
      YAML::Node viewer_node(YAML::NodeType::Map);
      write_settings_node(&viewer_node, data.viewer_settings);
      root["viewer"] = viewer_node;
    }

    std::ofstream out(path.toStdString());
    out << root;
    out.close();
    return true;
  } catch (const std::exception& e) {
    return fail(QString("Failed to save: %1").arg(e.what()));
  }
}

QString ProjectStore::migrate_mesh_path(const QString& project_path,
                                        const QString& source,
                                        const QString& fallback_name) const {
  if (source.trimmed().isEmpty()) {
    return {};
  }
  const QString own_case_dir = QDir::fromNativeSeparators(
      QDir::cleanPath(project_case_work_dir(project_path)));
  const QString source_case_dir = enclosing_case_work_dir(source);
  const bool foreign_case_dir = !own_case_dir.isEmpty() &&
                                !source_case_dir.isEmpty() &&
                                source_case_dir != own_case_dir;
  if (!is_legacy_default_mesh_path(source) && !foreign_case_dir) {
    return {};
  }
  const QString target =
      QDir(project_case_work_dir(project_path))
          .filePath(QFileInfo(source).fileName().isEmpty()
                        ? fallback_name + ".msh"
                        : QFileInfo(source).fileName());
  bool target_ready = QDir().mkpath(QFileInfo(target).absolutePath());
  if (target_ready && QFileInfo::exists(source) &&
      !QFileInfo::exists(target)) {
    target_ready = QFile::copy(source, target);
  }
  if (target_ready && !QFileInfo::exists(source) &&
      !QFileInfo::exists(target)) {
    gmp::log_operation(
        "project",
        QString("Mesh source is missing; redirected into project "
                "workspace without copying: %1 -> %2")
            .arg(source, target));
  }
  if (!target_ready) {
    return {};
  }
  gmp::log_operation(
      "project",
      QString("Mesh output migrated into project workspace: %1").arg(target));
  return target;
}

QList<QPair<QString, QString>> ProjectStore::migrate_mesh_paths(
    const QString& project_path, ProjectData* data) const {
  QList<QPair<QString, QString>> migrations;
  if (!data) {
    return migrations;
  }
  QStringList project_mesh_paths;
  QString migrated_active_mesh;
  const QString gmsh_output =
      data->gmsh_settings.value("output_path").toString();
  for (auto& entry : data->model_entries) {
    if (entry.kind != "Mesh") {
      continue;
    }
    const QString source = entry.params.value("path").toString();
    QString effective = source;
    const QString target =
        migrate_mesh_path(project_path, source, entry.name);
    if (!target.isEmpty()) {
      effective = target;
      migrations.append(qMakePair(source, target));
      entry.params.insert("path", target);
      if (same_file_path(data->mesh_snapshot.mesh_path, source)) {
        data->mesh_snapshot.mesh_path = target;
      }
      if (same_file_path(gmsh_output, source)) {
        migrated_active_mesh = target;
      }
    }
    if (!effective.trimmed().isEmpty()) {
      project_mesh_paths.append(effective);
    }
  }
  if (!migrated_active_mesh.isEmpty()) {
    data->gmsh_settings.insert("output_path", migrated_active_mesh);
  }
  if (!project_mesh_paths.isEmpty() && !data->moose_settings.isEmpty()) {
    const QString current_mesh =
        data->moose_settings.value("mesh_path").toString();
    // 活动网格跟随自己的迁移目标；未参与迁移且不属于项目网格时（含已被
    // 远程 snapshot 目录污染的情况）才回落到第一个项目网格。
    QString migrated_current_mesh;
    for (const auto& migration : migrations) {
      if (same_file_path(current_mesh, migration.first)) {
        migrated_current_mesh = migration.second;
        break;
      }
    }
    bool current_is_project_mesh = false;
    for (const QString& candidate : project_mesh_paths) {
      current_is_project_mesh =
          current_is_project_mesh || same_file_path(current_mesh, candidate);
    }
    if (!migrated_current_mesh.isEmpty()) {
      data->moose_settings.insert("mesh_path", migrated_current_mesh);
    } else if (!current_is_project_mesh) {
      data->moose_settings.insert("mesh_path", project_mesh_paths.front());
    }
    // 结构化基线与生成报告里的旧路径同步替换，避免切换输入模式或查看
    // 报告时再次显示迁移前路径。
    if (!migrations.isEmpty()) {
      for (const QString& key : {QStringLiteral("input_text"),
                                 QStringLiteral("structured_input"),
                                 QStringLiteral("generation_report")}) {
        QString text = data->moose_settings.value(key).toString();
        for (const auto& migration : migrations) {
          text.replace(migration.first, migration.second);
        }
        data->moose_settings.insert(key, text);
      }
    }
  }
  return migrations;
}

}  // namespace gmp
