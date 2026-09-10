#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <vector>

#include "gmp/ApplicationProfile.h"
#include "gmp/MooseMappingRegistry.h"
#include "gmp/MooseSnapshot.h"
#include "gmp/PhysicalGroupManifest.h"
#include "gmp/ProjectSchema.h"
#include "gmp/SimClient.h"
#include "gmp/SketchDocument.h"

namespace {

class TestContext {
 public:
  void expect(bool condition, const QString& message) {
    if (condition) {
      qInfo("PASS: %s", message.toUtf8().constData());
    } else {
      qWarning("FAIL: %s", message.toUtf8().constData());
      ++failures;
    }
  }
  int failures = 0;
};

bool write_file(const QString& path, const QByteArray& data) {
  if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
    return false;
  }
  QFile file(path);
  return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
         file.write(data) == data.size();
}

QByteArray read_file(const QString& path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

gmp::ApplicationProfile valid_profile() {
  gmp::ApplicationProfile profile;
  profile.id = "DamSafetyApp-opt";
  profile.version = "1.0.0";
  profile.solver_program = "DamSafetyApp-opt";
  profile.status = "production";
  profile.support_level = "production";
  profile.mapping_version = "1.0.0";
  profile.check_command = "{solver} -i {input} --check-input";
  for (const auto& pair : {
           qMakePair(QString("name"), QString("SI")),
           qMakePair(QString("length"), QString("m")),
           qMakePair(QString("force"), QString("N")),
           qMakePair(QString("time"), QString("s")),
           qMakePair(QString("mass"), QString("kg")),
           qMakePair(QString("pressure"), QString("Pa")),
           qMakePair(QString("temperature"), QString("K")),
       }) {
    profile.unit_contract.insert(pair.first, pair.second);
  }
  profile.valid = true;
  return profile;
}

gmp::PhysicalGroupManifest valid_physical_groups(const QString& mesh_hash) {
  gmp::PhysicalGroupManifest manifest;
  manifest.mesh_path = "mesh/case.msh";
  manifest.mesh_sha256 = mesh_hash;
  manifest.mesh_dim = 3;
  manifest.node_count = 8;
  manifest.element_count = 1;
  manifest.element_type = "HEX8";
  gmp::PhysicalGroupEntry volume;
  volume.name = "concrete";
  volume.dim = 3;
  volume.tags = {1};
  volume.entity_count = 1;
  volume.element_count = 1;
  volume.bound_object_ids = {"material-1"};
  manifest.groups.append(volume);
  return manifest;
}

void test_project_schema(TestContext& test) {
  using namespace gmp::project_schema;
  const QStringList roots = model_root_nodes();
  for (const QString& required : {QStringLiteral("Assembly"),
                                  QStringLiteral("Physics"),
                                  QStringLiteral("Constraints"),
                                  QStringLiteral("Selections"),
                                  QStringLiteral("Input Cases")}) {
    test.expect(roots.contains(required), "schema root exists: " + required);
  }

  const YAML::Node yaml = YAML::Load(R"(
name: SI
length: m
display_to_solver_factors:
  length: 0.001
  pressure: 1000000.0
aliases: [mm, MPa]
)");
  const QVariantMap units = yaml_map_to_variant_map(yaml);
  const QVariantMap factors = units.value("display_to_solver_factors").toMap();
  test.expect(qFuzzyCompare(factors.value("length").toDouble(), 0.001),
              "nested unit factor loads without string coercion");
  const YAML::Node roundtrip = variant_map_to_yaml(units);
  test.expect(roundtrip["display_to_solver_factors"]["pressure"].as<double>() ==
                  1000000.0,
              "nested unit factor survives YAML round-trip");
  test.expect(roundtrip["aliases"].IsSequence() &&
                  roundtrip["aliases"].size() == 2,
              "list value survives YAML round-trip");

  const auto manifest = valid_physical_groups(QString(64, 'a'));
  const YAML::Node mesh_yaml = mesh_snapshot_to_yaml(manifest);
  test.expect(mesh_yaml["summary"] && !mesh_yaml["mesh_dim"],
              "mesh summary writes canonical nested structure");
  const auto restored = mesh_snapshot_from_yaml(mesh_yaml);
  test.expect(restored.mesh_dim == 3 && restored.groups.size() == 1 &&
                  restored.groups.first().bound_object_ids ==
                      QStringList{"material-1"},
              "mesh snapshot survives canonical round-trip");

  const YAML::Node legacy_flat = YAML::Load(R"(
path: mesh/legacy.msh
sha256: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
mesh_dim: 2
node_count: 4
element_count: 2
element_type: TRI3
physical_groups: []
)");
  test.expect(mesh_snapshot_from_yaml(legacy_flat).mesh_dim == 2,
              "early flat mesh summary remains readable");
}

void test_sketch_entity_translation(TestContext& test) {
  gmp::SketchDocument doc;
  gmp::SketchEntity line;
  line.type = gmp::SketchEntityType::Line;
  line.p1 = {1.0, 2.0};
  line.p2 = {4.0, 6.0};
  const int line_id = doc.add_entity(line);
  gmp::SketchEntity circle;
  circle.type = gmp::SketchEntityType::Circle;
  circle.center = {10.0, 20.0};
  circle.radius = 3.0;
  const int circle_id = doc.add_entity(circle);
  gmp::SketchEntity arc;
  arc.type = gmp::SketchEntityType::Arc;
  arc.center = {-2.0, 5.0};
  arc.radius = 7.0;
  arc.start_angle = 0.25;
  arc.end_angle = 1.5;
  const int arc_id = doc.add_entity(arc);

  test.expect(doc.translate_entities({line_id, circle_id, arc_id}, 5.0, -3.0),
              "sketch move accepts line, circle and arc selections");
  const auto* moved_line = doc.entity(line_id);
  const auto* moved_circle = doc.entity(circle_id);
  const auto* moved_arc = doc.entity(arc_id);
  test.expect(moved_line && moved_line->p1 == gmp::SketchPoint2d{6.0, -1.0} &&
                  moved_line->p2 == gmp::SketchPoint2d{9.0, 3.0},
              "sketch move translates both line endpoints");
  test.expect(moved_circle &&
                  moved_circle->center == gmp::SketchPoint2d{15.0, 17.0} &&
                  moved_circle->radius == 3.0,
              "sketch move translates a circle without changing radius");
  test.expect(moved_arc &&
                  moved_arc->center == gmp::SketchPoint2d{3.0, 2.0} &&
                  moved_arc->radius == 7.0 && moved_arc->start_angle == 0.25 &&
                  moved_arc->end_angle == 1.5,
              "sketch move translates an arc without changing its shape");

  gmp::SketchDocument grouped;
  const int rectangle_shape = grouped.create_shape_id();
  std::vector<int> rectangle_ids;
  const gmp::SketchPoint2d corners[4] = {
      {0.0, 0.0}, {10.0, 0.0}, {10.0, 5.0}, {0.0, 5.0}};
  for (int i = 0; i < 4; ++i) {
    gmp::SketchEntity edge;
    edge.type = gmp::SketchEntityType::Line;
    edge.shape_id = rectangle_shape;
    edge.p1 = corners[i];
    edge.p2 = corners[(i + 1) % 4];
    rectangle_ids.push_back(grouped.add_entity(edge));
  }
  test.expect(grouped.shape_entity_ids(rectangle_ids.front()) == rectangle_ids,
              "rectangle edges retain one logical shape identity");
  grouped.translate_entities(grouped.shape_entity_ids(rectangle_ids.front()),
                             2.0, 3.0);
  test.expect(grouped.entity(rectangle_ids[0])->p1 ==
                      gmp::SketchPoint2d{2.0, 3.0} &&
                  grouped.entity(rectangle_ids[2])->p2 ==
                      gmp::SketchPoint2d{2.0, 8.0},
              "logical rectangle moves as one shape");

  gmp::SketchDocument restored;
  QString restore_error;
  test.expect(restored.from_yaml_string(grouped.to_yaml_string(),
                                        &restore_error) &&
                  restored.shape_entity_ids(rectangle_ids.front()).size() == 4,
              "rectangle shape identity survives YAML round-trip");

  const QString legacy_rectangle = QStringLiteral(R"(
entities:
  - {id: 1, type: line, p1: [0, 0], p2: [1, 0]}
  - {id: 2, type: line, p1: [1, 0], p2: [1, 1]}
  - {id: 3, type: line, p1: [1, 1], p2: [0, 1]}
  - {id: 4, type: line, p1: [0, 1], p2: [0, 0]}
constraints:
  - {id: 1, type: coincident, entity1: 1, role1: end, entity2: 2, role2: start}
  - {id: 2, type: coincident, entity1: 2, role1: end, entity2: 3, role2: start}
  - {id: 3, type: coincident, entity1: 3, role1: end, entity2: 4, role2: start}
  - {id: 4, type: coincident, entity1: 4, role1: end, entity2: 1, role2: start}
)");
  gmp::SketchDocument legacy;
  test.expect(legacy.from_yaml_string(legacy_rectangle, &restore_error) &&
                  legacy.shape_entity_ids(1).size() == 4,
              "legacy rectangle grouping is recovered from coincident edges");
}

void test_profiles_and_mapping(TestContext& test) {
  gmp::ApplicationProfileRegistry profiles(QStringLiteral(GMP_PROFILE_DIR));
  test.expect(profiles.reload(), "application profiles load");
  test.expect(profiles.profile_ids().size() == 3,
              "three application profiles are available");
  test.expect(profiles.last_error().isEmpty(),
              "bundled application profiles pass validation");

  QTemporaryDir profile_parent;
  const QString profile_root = profile_parent.filePath("profiles");
  QDir().mkpath(profile_root);
  QFile::copy(QDir(QStringLiteral(GMP_PROFILE_DIR))
                  .filePath("DamSafetyApp-opt.json"),
              QDir(profile_root).filePath("valid.json"));
  QFile::copy(QDir(QStringLiteral(GMP_PROFILE_DIR)).filePath("../mapping-v1.json"),
              profile_parent.filePath("mapping-v1.json"));
  write_file(QDir(profile_root).filePath("broken.json"), "{broken");
  gmp::ApplicationProfileRegistry mixed_profiles(profile_root);
  test.expect(mixed_profiles.reload(),
              "one broken profile does not disable valid profiles");
  test.expect(!mixed_profiles.last_error().isEmpty(),
              "broken profile produces a readable registry error");

  gmp::MooseMappingRegistry mapping(
      QDir::current().filePath("templates/moose/mapping-v1.json"));
  test.expect(mapping.is_loaded(), "mapping registry loads");
  test.expect(mapping.version() == "1.0.0", "mapping version is validated");
  test.expect(mapping.has_block("GlobalParams") &&
                  mapping.has_block("Materials") &&
                  mapping.has_block("Contact"),
              "mapping registry covers required baseline blocks");

  QTemporaryDir invalid_mapping_dir;
  const QString invalid_mapping = invalid_mapping_dir.filePath("mapping.json");
  write_file(invalid_mapping,
             R"({"version":"bad","blocks":{},"ordering":[]})");
  gmp::MooseMappingRegistry rejected(invalid_mapping);
  test.expect(!rejected.is_loaded() && !rejected.last_error().isEmpty(),
              "invalid mapping version/schema is rejected");
  const QString invalid_ordering =
      invalid_mapping_dir.filePath("mapping-ordering.json");
  write_file(invalid_ordering,
             R"({"version":"1.0.0","blocks":{"Mesh":{"objects":{"FileMesh":{"required_params":[],"param_schema":{}}}}},"ordering":["Unknown"]})");
  gmp::MooseMappingRegistry rejected_ordering(invalid_ordering);
  test.expect(!rejected_ordering.is_loaded(),
              "mapping with unknown ordering entry remains unloaded");
}

void test_physical_groups(TestContext& test) {
  auto manifest = valid_physical_groups(QString(64, 'b'));
  test.expect(manifest.is_valid(), "valid Physical Group manifest passes");
  manifest.groups.first().element_count = 0;
  test.expect(!manifest.is_valid(), "empty Physical Group blocks generation");

  manifest = valid_physical_groups(QString(64, 'b'));
  manifest.groups.append(manifest.groups.first());
  test.expect(!manifest.is_valid(),
              "duplicate Physical Group name in one dimension is rejected");

  manifest = valid_physical_groups(QString(64, 'b'));
  manifest.mesh_path = "../outside.msh";
  test.expect(!manifest.is_valid(), "mesh path traversal is rejected");
}

void test_snapshot_v2(TestContext& test) {
  QTemporaryDir workspace;
  const QString source_dir = workspace.filePath("source");
  const QByteArray mesh_data("gmsh-mesh-data\n");
  write_file(QDir(source_dir).filePath("mesh/case.msh"), mesh_data);
  write_file(QDir(source_dir).filePath("extra/material.csv"),
             "strain,stress\n0,0\n");

  gmp::MooseTemplateInfo tpl;
  tpl.valid = true;
  tpl.key = "phase0-test";
  tpl.dir = source_dir;
  tpl.mesh_files = {"mesh/case.msh"};
  tpl.extra_files = {"extra/material.csv"};

  gmp::SnapshotExportConfig cfg;
  cfg.case_name = "test-case";
  cfg.case_id = "test-001";
  cfg.input_mode = "structured";
  cfg.generator_version = "gmp-ise-test";
  cfg.base_model_hash = QString(64, 'c');
  cfg.profile = valid_profile();
  cfg.physical_groups = valid_physical_groups(gmp::sha256_hex(mesh_data));

  const QString input_text =
      "[Mesh]\n  type = FileMesh\n  file = 'mesh/case.msh'\n[]\n"
      "[Materials]\n  data_file = 'extra/material.csv'\n[]\n";
  const QString dest = workspace.filePath("snapshot-v1");
  const auto result =
      gmp::export_job_snapshot_v2(dest, input_text, "case.i", tpl, cfg);
  test.expect(result.ok, "complete snapshot v2 exports successfully");
  test.expect(QFileInfo::exists(QDir(dest).filePath("mesh/case.msh")) &&
                  QFileInfo::exists(QDir(dest).filePath("extra/material.csv")),
              "referenced relative directory structure is preserved");
  test.expect(result.mesh_files == QStringList{"mesh/case.msh"} &&
                  result.extra_files == QStringList{"extra/material.csv"},
              "manifest contains all required input files");
  const QJsonArray mesh_files = result.manifest.value("input_snapshot")
                                    .toObject()
                                    .value("mesh_files")
                                    .toArray();
  test.expect(result.manifest.value("input_snapshot")
                      .toObject()
                      .value("input_role") == "input_config",
              "main .i file has the input_config role");
  test.expect(mesh_files.size() == 1 &&
                  mesh_files.first().toObject().value("role") == "input_mesh",
              "mesh role and hash entry are present");

  const QByteArray original_input = read_file(QDir(dest).filePath("case.i"));
  const auto overwrite =
      gmp::export_job_snapshot_v2(dest, "changed", "case.i", tpl, cfg);
  test.expect(!overwrite.ok &&
                  read_file(QDir(dest).filePath("case.i")) == original_input,
              "existing snapshot is immutable");

  auto missing_tpl = tpl;
  missing_tpl.mesh_files = {"mesh/missing.msh"};
  const auto missing = gmp::export_job_snapshot_v2(
      workspace.filePath("snapshot-missing"),
      "[Mesh]\n  file = 'mesh/missing.msh'\n[]\n", "case.i", missing_tpl,
      cfg);
  test.expect(!missing.ok && missing.missing_files.contains("mesh/missing.msh"),
              "missing mesh makes snapshot export fail");
  test.expect(!QFileInfo::exists(workspace.filePath("snapshot-missing")),
              "failed snapshot leaves no misleading partial directory");

  const auto unsafe = gmp::export_job_snapshot_v2(
      workspace.filePath("snapshot-unsafe"),
      "[Mesh]\n  file = '../outside.msh'\n[]\n", "case.i", tpl, cfg);
  test.expect(!unsafe.ok, "path traversal in input references is rejected");

  const QString exodus_source = workspace.filePath("exodus-source");
  write_file(QDir(exodus_source).filePath("stage.e"), "exodus\n");
  gmp::MooseTemplateInfo exodus_tpl;
  exodus_tpl.valid = true;
  exodus_tpl.key = "exodus-input";
  exodus_tpl.dir = exodus_source;
  exodus_tpl.mesh_files = {"stage.e"};
  auto exodus_cfg = cfg;
  exodus_cfg.physical_groups.mesh_path = "stage.e";
  exodus_cfg.physical_groups.mesh_sha256 =
      gmp::sha256_hex(QByteArray("exodus\n"));
  const QString exodus_input = "[Mesh]\n  file = 'stage.e'\n[]\n";
  const auto implicit_exodus = gmp::export_job_snapshot_v2(
      workspace.filePath("snapshot-exodus-implicit"), exodus_input, "case.i",
      exodus_tpl, exodus_cfg);
  test.expect(!implicit_exodus.ok,
              "Exodus input without explicit role is rejected");
  exodus_cfg.file_roles.insert("stage.e", "initial_state");
  const auto explicit_exodus = gmp::export_job_snapshot_v2(
      workspace.filePath("snapshot-exodus-explicit"), exodus_input, "case.i",
      exodus_tpl, exodus_cfg);
  test.expect(explicit_exodus.ok,
              "Exodus input with explicit initial_state role is accepted");
}

void test_submission_manifest(TestContext& test) {
  QJsonObject mesh_entry;
  mesh_entry.insert("name", "mesh/case.msh");
  mesh_entry.insert("sha256", QString(64, 'b'));
  mesh_entry.insert("role", "input_mesh");
  QJsonObject snap;
  snap.insert("input_file", "case.i");
  snap.insert("input_sha256", QString(64, 'a'));
  snap.insert("mesh_files", QJsonArray{mesh_entry});
  snap.insert("extra_files", QJsonArray{});

  // v2 快照 manifest（含 application_profile）：solver 经 command 传达；
  // 服务端按严格 schema 校验（additionalProperties=false），提交清单
  // 只允许 7 个合同键，不得附带 solver_program/profile_* 等额外键。
  QJsonObject profile;
  profile.insert("profile_id", "DamSafetyApp-opt");
  profile.insert("profile_version", "1.0.0");
  profile.insert("mapping_version", "1.0.0");
  profile.insert("solver_program", "DamSafetyApp-opt");
  QJsonObject manifest;
  manifest.insert("case_name", "demo");
  manifest.insert("input_snapshot", snap);
  manifest.insert("application_profile", profile);

  QString error;
  const QJsonObject sub = gmp::SimClient::build_submission_manifest(
      manifest, "proj-1", "DamSafetyApp-opt", &error);
  test.expect(!sub.isEmpty() && error.isEmpty(),
              "submission manifest builds from a v2 snapshot");
  test.expect(sub.value("command") == "DamSafetyApp-opt -i case.i",
              "submission command uses the profile solver program");
  const QStringList allowed_keys = {"project_id",  "case_name", "input_file",
                                    "input_sha256", "mesh_files", "extra_files",
                                    "command"};
  bool only_allowed = sub.size() == allowed_keys.size();
  for (auto it = sub.begin(); it != sub.end(); ++it) {
    only_allowed = only_allowed && allowed_keys.contains(it.key());
  }
  test.expect(only_allowed,
              "submission manifest contains only the 7 server contract keys");
  // 文件条目同样裁剪为服务端合同允许的 {name, sha256}（快照里的 role
  // 等溯源字段不进提交报文）。
  const QJsonArray sub_mesh = sub.value("mesh_files").toArray();
  bool mesh_entries_slim = sub_mesh.size() == 1;
  for (const auto& value : sub_mesh) {
    const QJsonObject entry = value.toObject();
    mesh_entries_slim = mesh_entries_slim && entry.size() == 2 &&
                        entry.contains("name") && entry.contains("sha256") &&
                        !entry.contains("role");
  }
  test.expect(mesh_entries_slim,
              "submission file entries are trimmed to name/sha256 only");

  // v1 旧快照 manifest（无 application_profile）：仍可提交，
  // command 用缺省 solver，键集合同样只有 7 个合同键。
  QJsonObject legacy;
  legacy.insert("case_name", "legacy");
  legacy.insert("input_snapshot", snap);
  error.clear();
  const QJsonObject legacy_sub = gmp::SimClient::build_submission_manifest(
      legacy, "proj-1", "DamSafetyApp-opt", &error);
  test.expect(!legacy_sub.isEmpty() && error.isEmpty(),
              "v1 snapshot manifest still builds a submission");
  bool legacy_only_allowed = legacy_sub.size() == allowed_keys.size();
  for (auto it = legacy_sub.begin(); it != legacy_sub.end(); ++it) {
    legacy_only_allowed =
        legacy_only_allowed && allowed_keys.contains(it.key());
  }
  test.expect(legacy_only_allowed &&
                  legacy_sub.value("command") == "DamSafetyApp-opt -i case.i",
              "v1 fallback submits with default solver in command only");

  // 校验失败路径：非法 input_sha256 拒绝并给出可读错误。
  QJsonObject bad_snap = snap;
  bad_snap.insert("input_sha256", "not-a-sha");
  QJsonObject bad;
  bad.insert("input_snapshot", bad_snap);
  error.clear();
  const QJsonObject rejected = gmp::SimClient::build_submission_manifest(
      bad, "proj-1", "DamSafetyApp-opt", &error);
  test.expect(rejected.isEmpty() && !error.isEmpty(),
              "invalid input sha256 is rejected with a readable error");

  // solver 白名单：不允许路径分量。
  error.clear();
  const QJsonObject bad_solver = gmp::SimClient::build_submission_manifest(
      manifest, "proj-1", "../evil/solver", &error);
  test.expect(bad_solver.isEmpty() && !error.isEmpty(),
              "solver program with path components is rejected");
}

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  TestContext test;
  test_project_schema(test);
  test_sketch_entity_translation(test);
  test_profiles_and_mapping(test);
  test_physical_groups(test);
  test_snapshot_v2(test);
  test_submission_manifest(test);
  if (test.failures == 0) {
    qInfo("Phase 0 contract tests PASSED");
  } else {
    qWarning("Phase 0 contract tests FAILED with %d failures", test.failures);
  }
  return test.failures;
}
