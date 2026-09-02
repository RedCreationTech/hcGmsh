#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include "gmp/ApplicationProfile.h"
#include "gmp/MooseMappingRegistry.h"
#include "gmp/MooseSnapshot.h"
#include "gmp/PhysicalGroupManifest.h"
#include "gmp/ProjectSchema.h"

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
                                  QStringLiteral("Selections")}) {
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

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  TestContext test;
  test_project_schema(test);
  test_profiles_and_mapping(test);
  test_physical_groups(test);
  test_snapshot_v2(test);
  if (test.failures == 0) {
    qInfo("Phase 0 contract tests PASSED");
  } else {
    qWarning("Phase 0 contract tests FAILED with %d failures", test.failures);
  }
  return test.failures;
}
