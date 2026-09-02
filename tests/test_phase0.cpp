#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include "gmp/ApplicationProfile.h"
#include "gmp/MooseMappingRegistry.h"
#include "gmp/MooseSnapshot.h"
#include "gmp/PhysicalGroupManifest.h"

namespace gmp {

int run_phase0_tests(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  int failures = 0;

  // 1. ApplicationProfileRegistry
  {
    ApplicationProfileRegistry registry;
    if (!registry.is_loaded()) {
      // default_root may be empty in build dir; set manually
      registry.set_root(QDir::currentPath() + "/templates/moose/profiles");
      if (!registry.reload()) {
        qWarning("Profile registry failed to load: %s",
                 registry.last_error().toUtf8().constData());
        ++failures;
      }
    }
    if (registry.is_loaded()) {
      const auto ids = registry.profile_ids();
      qInfo("Loaded %lld profiles", static_cast<long long>(ids.size()));
      for (const QString& id : ids) {
        const auto p = registry.profile(id);
        qInfo("  %s: %s (%s)", id.toUtf8().constData(),
              p.display_name.toUtf8().constData(),
              p.status.toUtf8().constData());
      }
      const auto dam = registry.profile("DamSafetyApp-opt");
      if (!dam.valid || dam.status != "production") {
        qWarning("DamSafetyApp-opt profile unexpected");
        ++failures;
      }
      if (!registry.supports_block("DamSafetyApp-opt", "small_strain_solid", "Materials")) {
        qWarning("DamSafetyApp-opt should support Materials block");
        ++failures;
      }
    }
  }

  // 2. MooseMappingRegistry
  {
    MooseMappingRegistry registry(
        QDir::currentPath() + "/templates/moose/mapping-v1.json");
    if (!registry.is_loaded()) {
      qWarning("Mapping registry failed to load: %s",
               registry.last_error().toUtf8().constData());
      ++failures;
    } else {
      qInfo("Mapping registry version: %s",
            registry.version().toUtf8().constData());
      if (!registry.has_block("Materials")) {
        qWarning("Materials block missing");
        ++failures;
      }
      if (!registry.has_object_type("Materials",
                                   "ComputeIsotropicElasticityTensor")) {
        qWarning("ComputeIsotropicElasticityTensor missing");
        ++failures;
      }
      const QStringList order = registry.block_order();
      if (order.isEmpty() || order.first() != "Mesh") {
        qWarning("Block order should start with Mesh");
        ++failures;
      }
    }
  }

  // 3. PhysicalGroupManifest
  {
    PhysicalGroupManifest manifest;
    manifest.mesh_path = "mesh/case.msh";
    manifest.mesh_sha256 =
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    manifest.mesh_dim = 3;
    manifest.node_count = 1331;
    manifest.element_count = 1000;
    manifest.element_type = "TET4";

    PhysicalGroupEntry g1;
    g1.name = "concrete";
    g1.dim = 3;
    g1.tags = {1};
    g1.entity_count = 1;
    g1.element_count = 1000;
    manifest.groups.append(g1);

    PhysicalGroupEntry g2;
    g2.name = "bottom";
    g2.dim = 2;
    g2.tags = {2};
    g2.entity_count = 1;
    g2.element_count = 100;
    manifest.groups.append(g2);

    if (!manifest.is_valid()) {
      qWarning("PhysicalGroupManifest should be valid");
      ++failures;
    }
    const auto issues = manifest.validate();
    if (!issues.isEmpty()) {
      qWarning("Unexpected validation issues: %lld",
               static_cast<long long>(issues.size()));
      ++failures;
    }
    if (!manifest.has_group("concrete", 3)) {
      qWarning("Should find concrete volume group");
      ++failures;
    }

    // 测试重复名称检测
    PhysicalGroupEntry dup = g1;
    dup.tags = {3};
    manifest.groups.append(dup);
    const auto issues2 = manifest.validate();
    bool found_dup = false;
    for (const auto& i : issues2) {
      if (i.message.contains("Duplicate")) {
        found_dup = true;
      }
    }
    if (!found_dup) {
      qWarning("Should detect duplicate physical group name");
      ++failures;
    }
  }

  // 4. Snapshot v2 manifest generation (mock)
  {
    MooseTemplateInfo tpl;
    tpl.valid = false;

    SnapshotExportConfig cfg;
    cfg.case_name = "test-case";
    cfg.case_id = "test-001";
    cfg.input_mode = "structured";
    cfg.generator_version = "gmp-ise-test";
    cfg.base_model_hash = "abc123";
    cfg.extension_hash = "";

    ApplicationProfile profile;
    profile.id = "DamSafetyApp-opt";
    profile.version = "1.0.0";
    profile.solver_program = "DamSafetyApp-opt";
    profile.status = "production";
    profile.support_level = "production";
    profile.check_command = "{solver} -i {input} --check-input";
    profile.unit_contract.insert("name", "SI");
    profile.unit_contract.insert("length", "m");
    profile.valid = true;
    cfg.profile = profile;

    PhysicalGroupManifest pg;
    pg.mesh_path = "case.msh";
    pg.mesh_sha256 =
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    pg.mesh_dim = 3;
    PhysicalGroupEntry e;
    e.name = "concrete";
    e.dim = 3;
    e.tags = {1};
    pg.groups.append(e);
    cfg.physical_groups = pg;

    const QString dest = QDir::tempPath() + "/gmp_test_snapshot_" +
                         QString::number(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
    const QString input_text = "[Mesh]\n  type = FileMesh\n  file = 'case.msh'\n[]\n";
    SnapshotExportResult res = export_job_snapshot_v2(
        dest, input_text, "case.i", tpl, cfg);
    if (!res.ok) {
      qWarning("Snapshot v2 export failed: %s",
               res.error.toUtf8().constData());
      ++failures;
    } else {
      const QJsonObject manifest = res.manifest;
      if (manifest.value("contract_version").toString() != "2.0.0") {
        qWarning("Manifest contract_version mismatch");
        ++failures;
      }
      if (!manifest.contains("application_profile")) {
        qWarning("Manifest missing application_profile");
        ++failures;
      }
      if (!manifest.contains("traceability")) {
        qWarning("Manifest missing traceability");
        ++failures;
      }
      qInfo("Generated v2 manifest:\n%s",
            QJsonDocument(manifest).toJson(QJsonDocument::Indented).constData());
    }
  }

  if (failures == 0) {
    qInfo("Phase 0 self-tests PASSED");
  } else {
    qWarning("Phase 0 self-tests FAILED with %d failures", failures);
  }
  return failures;
}

}  // namespace gmp

int main(int argc, char* argv[]) {
  return gmp::run_phase0_tests(argc, argv);
}
