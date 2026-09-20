#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <vector>

#include "gmp/ApplicationProfile.h"
#include "gmp/AssemblyGeometryService.h"
#include "gmp/GmshMesher.h"
#include "gmp/DependencyGraph.h"
#include "gmp/MooseMappingRegistry.h"
#include "gmp/MooseInputGenerator.h"
#include "gmp/MooseSnapshot.h"
#include "gmp/PhysicalGroupManifest.h"
#include "gmp/PhysicalGroupService.h"
#include "gmp/ProjectDocument.h"
#include "gmp/ProjectSchema.h"
#include "gmp/ProjectStore.h"
#include "gmp/PropertyBag.h"
#include "gmp/SimClient.h"
#include "gmp/SketchDocument.h"
#include "gmp/SnapshotService.h"
#include "gmp/ViewportCamera.h"
#include "gmp/ViewportSelection.h"
#include "gmp/TransactionManager.h"
#include "gmp/UnitDisplay.h"

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
  // 根节点清单冻结合同（A-019）：代码清单与 doc/schema/project-v2.md 顶层
  // model 示例必须完全一致，防止文档再次漂移。
  const QStringList frozen_roots = {
      QStringLiteral("Parts"),       QStringLiteral("Sketches"),
      QStringLiteral("Features"),    QStringLiteral("Datums"),
      QStringLiteral("Materials"),   QStringLiteral("Sections"),
      QStringLiteral("Assembly"),    QStringLiteral("Physics"),
      QStringLiteral("Steps"),       QStringLiteral("BC"),
      QStringLiteral("Loads"),       QStringLiteral("Interactions"),
      QStringLiteral("Constraints"), QStringLiteral("Selections"),
      QStringLiteral("Functions"),   QStringLiteral("Variables"),
      QStringLiteral("Outputs"),     QStringLiteral("Mesh"),
      QStringLiteral("Input Cases"), QStringLiteral("Jobs"),
      QStringLiteral("Results"),
  };
  test.expect(model_root_nodes() == frozen_roots,
              "model root nodes match the frozen schema contract list");

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
  test.expect(mapping.has_object_type("BCs", "Pressure") &&
                  mapping.has_object_type("Contact", "Contact"),
              "G1 pressure/contact object mappings are available");
  const QJsonObject contact =
      mapping.object_schema("Contact", "Contact");
  const QJsonArray required = contact.value("required_params").toArray();
  bool has_primary = false;
  bool has_secondary = false;
  bool has_legacy_master = false;
  for (const auto& value : required) {
    has_primary = has_primary || value.toString() == "primary";
    has_secondary = has_secondary || value.toString() == "secondary";
    has_legacy_master = has_legacy_master || value.toString() == "master";
  }
  test.expect(has_primary && has_secondary && !has_legacy_master,
              "G1 contact mapping uses primary/secondary syntax");

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
      "[Mesh/file]\n  type = FileMeshGenerator\n  file = 'mesh/case.msh'\n[]\n"
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

// TASK-V02-003：G1 关键链路（装配 → 网格清单 → .i 生成 → 快照导出）中
// 已可无 QWidget 调用的环节，在此固化为服务级合同测试。
// Stage 3/4 迁移状态：
//   - .i 全文本不变量与 A/B 确定性：已由 MooseInputGenerator 下沉
//     （TASK-V02-030），见 test_moose_input_generator_contract；
//   - 生成报告可追溯行：同上已覆盖；
//   - 装配构建 → mesh_manifest 清单产出仍依赖 GmshPanel/gmsh（Stage 4
//     TASK-V02-040 候选），当前由 GUI 巡览 assembly_instance_contract 等覆盖。
void test_physical_group_manifest_contract(TestContext& test) {
  // G1 基线形态的清单（2 体组 + 6 面组）必须零错误通过。
  auto make_entry = [](const QString& name, int dim, int tag, int elements) {
    gmp::PhysicalGroupEntry entry;
    entry.name = name;
    entry.dim = dim;
    entry.tags = {tag};
    entry.entity_count = 1;
    entry.element_count = elements;
    return entry;
  };
  gmp::PhysicalGroupManifest g1;
  g1.mesh_path = "mesh/mesh_assembly_g1.msh";
  g1.mesh_sha256 = QString(64, 'a');
  g1.mesh_dim = 3;
  g1.node_count = 175086;
  g1.element_count = 152950;
  g1.element_type = "HEX8";
  g1.quality_summary.insert("minSICN", 0.999962);
  g1.groups.append(make_entry("instance_plate", 3, 1, 67510));
  g1.groups.append(make_entry("instance_concrete", 3, 2, 85440));
  g1.groups.append(make_entry("instance_plate_surface", 2, 3, 56438));
  g1.groups.append(make_entry("instance_concrete_surface", 2, 4, 22568));
  g1.groups.append(make_entry("fixed_bottom", 2, 5, 8544));
  g1.groups.append(make_entry("load_top", 2, 6, 27004));
  g1.groups.append(make_entry("contact_concrete", 2, 7, 8544));
  g1.groups.append(make_entry("contact_plate", 2, 8, 27004));
  g1.groups.last().bound_object_ids = {"contact_plate_concrete"};
  bool has_error = false;
  for (const auto& issue : g1.validate()) {
    has_error = has_error || issue.severity == "error";
  }
  test.expect(!has_error && g1.is_valid(),
              "G1-shaped manifest (2 volumes + 6 surfaces) validates clean");

  // 组查询语义：维度过滤、缺失组回退、名称去重。
  test.expect(g1.group_names(3) == QStringList{"instance_plate", "instance_concrete"},
              "volume group names are dim-3 filtered and ordered");
  test.expect(g1.group_names(2).size() == 6 &&
                  g1.group_names().size() == 8,
              "boundary group names are dim-2 filtered");
  test.expect(g1.has_group("contact_plate", 2) &&
                  !g1.has_group("contact_plate", 3) &&
                  !g1.has_group("missing", 2),
              "has_group honors the dimension argument");
  test.expect(!gmp::PhysicalGroupEntry().is_valid() &&
                  !g1.group("missing").is_valid(),
              "missing group lookup returns an invalid entry");

  // 项目 mesh_snapshot_ 持久化路径：to/from_variant_map 必须无损。
  const auto restored =
      gmp::PhysicalGroupManifest::from_variant_map(g1.to_variant_map());
  const gmp::PhysicalGroupManifest& original = g1;
  bool roundtrip_equal =
      restored.mesh_path == original.mesh_path &&
      restored.mesh_sha256 == original.mesh_sha256 &&
      restored.mesh_dim == original.mesh_dim &&
      restored.node_count == original.node_count &&
      restored.element_count == original.element_count &&
      restored.element_type == original.element_type &&
      restored.quality_summary == original.quality_summary &&
      restored.groups.size() == original.groups.size();
  for (int i = 0; roundtrip_equal && i < original.groups.size(); ++i) {
    const auto& lhs = original.groups.at(i);
    const auto& rhs = restored.groups.at(i);
    roundtrip_equal = lhs.name == rhs.name && lhs.dim == rhs.dim &&
                      lhs.tags == rhs.tags &&
                      lhs.entity_count == rhs.entity_count &&
                      lhs.element_count == rhs.element_count &&
                      lhs.bound_object_ids == rhs.bound_object_ids;
  }
  test.expect(roundtrip_equal && restored.is_valid(),
              "manifest survives the variant-map persistence round-trip");

  // 拒绝路径：每条校验规则都要能把坏清单打成 error。
  auto expect_rejected = [&test](gmp::PhysicalGroupManifest manifest,
                                 const QString& message) {
    bool rejected = false;
    for (const auto& issue : manifest.validate()) {
      rejected = rejected || issue.severity == "error";
    }
    test.expect(rejected && !manifest.is_valid(), message);
  };
  expect_rejected([&] {
    auto m = g1;
    m.groups.first().dim = 4;
    return m;
  }(), "group dimension out of range is rejected");
  expect_rejected([&] {
    auto m = g1;
    m.mesh_dim = 2;  // 体组 dim=3 超过 mesh_dim
    return m;
  }(), "group dimension exceeding mesh_dim is rejected");
  expect_rejected([&] {
    auto m = g1;
    m.groups.first().tags.clear();
    return m;
  }(), "group without entity tags is rejected");
  expect_rejected([&] {
    auto m = g1;
    m.groups.first().tags = {1, -2};
    return m;
  }(), "non-positive entity tag is rejected");
  expect_rejected([&] {
    auto m = g1;
    m.groups.first().tags = {1, 1};
    return m;
  }(), "duplicate entity tags in one group are rejected");
  expect_rejected([&] {
    auto m = g1;
    m.groups.first().entity_count = 0;
    return m;
  }(), "group without entities is rejected");
  expect_rejected([&] {
    auto m = g1;
    m.groups.first().element_count = 0;
    return m;
  }(), "group without mesh elements is rejected");
  expect_rejected([&] {
    auto m = g1;
    m.groups.first().name = "   ";
    return m;
  }(), "whitespace-only group name is rejected");
  expect_rejected([&] {
    auto m = g1;
    m.groups.append(make_entry("fixed_bottom", 2, 9, 1));
    return m;
  }(), "duplicate group name within one dimension is rejected");
  expect_rejected([&] {
    auto m = g1;
    m.mesh_sha256 = QString(63, 'a');
    return m;
  }(), "malformed mesh sha256 is rejected");
  expect_rejected([&] {
    auto m = g1;
    m.mesh_dim = 0;
    return m;
  }(), "mesh_dim outside 1..3 is rejected");
  expect_rejected([&] {
    auto m = g1;
    m.mesh_path = "/abs/outside.msh";
    return m;
  }(), "absolute mesh path is rejected");
}

void test_snapshot_v2_manifest_contract(TestContext& test) {
  QTemporaryDir workspace;
  const QString source_dir = workspace.filePath("g1-source");
  const QByteArray mesh_data("$MeshFormat\n2.2 0 8\n$EndMeshFormat\n");
  const QByteArray csv_data("strain,stress\n0,0\n");
  write_file(QDir(source_dir).filePath("mesh/mesh_assembly_g1.msh"), mesh_data);
  write_file(QDir(source_dir).filePath("extra/compression_hardening.csv"),
             csv_data);

  gmp::MooseTemplateInfo tpl;
  tpl.valid = true;
  tpl.key = "g1-assembly";
  tpl.dir = source_dir;
  tpl.mesh_files = {"mesh/mesh_assembly_g1.msh"};
  tpl.extra_files = {"extra/compression_hardening.csv"};

  gmp::SnapshotExportConfig cfg;
  cfg.case_name = "phase5-g1-assembly-contact";
  cfg.case_id = "g1-001";
  cfg.input_mode = "structured";
  cfg.generator_version = "gmp-ise-test";
  cfg.base_model_hash = QString(64, 'c');
  cfg.profile = valid_profile();
  cfg.physical_groups = valid_physical_groups(gmp::sha256_hex(mesh_data));
  cfg.physical_groups.mesh_path = "mesh/mesh_assembly_g1.msh";

  const QString input_text =
      "[Mesh/file]\n  type = FileMeshGenerator\n"
      "  file = 'mesh/mesh_assembly_g1.msh'\n[]\n"
      "[Materials]\n  compression_hardening_file = "
      "'extra/compression_hardening.csv'\n[]\n";

  // 哈希与角色：manifest 中的哈希必须等于内容真实 SHA-256。
  const auto result = gmp::export_job_snapshot_v2(
      workspace.filePath("snap-a"), input_text, "case.i", tpl, cfg);
  test.expect(result.ok, "G1-shaped snapshot v2 exports");
  const QString input_hash = gmp::sha256_hex(input_text.toUtf8());
  test.expect(result.input_sha256 == input_hash &&
                  result.manifest.value("final_input_hash").toString() ==
                      input_hash,
              "manifest input hash matches the exported .i content");
  const QJsonObject snapshot_obj =
      result.manifest.value("input_snapshot").toObject();
  test.expect(snapshot_obj.value("input_sha256").toString() == input_hash &&
                  snapshot_obj.value("input_role").toString() ==
                      "input_config",
              "input snapshot carries the input_config role and hash");
  const QJsonArray mesh_entries = snapshot_obj.value("mesh_files").toArray();
  const QJsonArray extra_entries = snapshot_obj.value("extra_files").toArray();
  test.expect(mesh_entries.size() == 1 &&
                  mesh_entries.first().toObject().value("sha256").toString() ==
                      gmp::sha256_hex(mesh_data) &&
                  mesh_entries.first().toObject().value("role").toString() ==
                      "input_mesh",
              "mesh entry hash and role match the source file");
  test.expect(extra_entries.size() == 1 &&
                  extra_entries.first().toObject().value("sha256").toString() ==
                      gmp::sha256_hex(csv_data),
              "extra file entry hash matches the source file");

  // A/B 确定性：同一输入两次导出，manifest 除 created_at 外逐字节一致。
  const auto result_b = gmp::export_job_snapshot_v2(
      workspace.filePath("snap-b"), input_text, "case.i", tpl, cfg);
  QJsonObject manifest_a = result.manifest;
  QJsonObject manifest_b = result_b.manifest;
  manifest_a.remove("created_at");
  manifest_b.remove("created_at");
  test.expect(result_b.ok &&
                  QJsonDocument(manifest_a) == QJsonDocument(manifest_b) &&
                  read_file(QDir(workspace.filePath("snap-a"))
                                .filePath("case.i")) ==
                      read_file(QDir(workspace.filePath("snap-b"))
                                     .filePath("case.i")),
              "repeated export is byte-identical apart from created_at");

  // 拒绝路径：合同缺项或不安全引用必须失败且给出可读错误。
  auto expect_export_rejected = [&test, &tpl](gmp::SnapshotExportConfig bad_cfg,
                                              const QString& text,
                                              const QString& file,
                                              const QString& message,
                                              const QString& dest_name) {
    QTemporaryDir dir;
    const auto rejected =
        gmp::export_job_snapshot_v2(dir.filePath(dest_name), text, file, tpl,
                                    bad_cfg);
    test.expect(!rejected.ok && !rejected.error.isEmpty(), message);
  };
  auto bad_mode = cfg;
  bad_mode.input_mode = "bogus";
  expect_export_rejected(bad_mode, input_text, "case.i",
                         "invalid input_mode is rejected", "reject-mode");
  auto bad_case = cfg;
  bad_case.case_id.clear();
  expect_export_rejected(bad_case, input_text, "case.i",
                         "missing case_id is rejected", "reject-case");
  auto bad_profile = cfg;
  bad_profile.profile.valid = false;
  expect_export_rejected(bad_profile, input_text, "case.i",
                         "invalid application profile is rejected",
                         "reject-profile");
  auto bad_groups = cfg;
  bad_groups.physical_groups = gmp::PhysicalGroupManifest();
  expect_export_rejected(bad_groups, input_text, "case.i",
                         "invalid physical group manifest is rejected",
                         "reject-groups");
  expect_export_rejected(cfg, input_text, "../case.i",
                         "unsafe input file name is rejected", "reject-name");
  expect_export_rejected(cfg,
                         "[Mesh]\n  file = '/abs/elsewhere.msh'\n[]\n",
                         "case.i",
                         "absolute file reference in input is rejected",
                         "reject-absref");

  // 引用扫描与相对路径规则的单元级合同。
  test.expect(gmp::scan_input_file_refs(
                  "[Mesh]\n  file = 'mesh/case.msh'\n[]\n"
                  "[Materials]\n  data_file = none\n"
                  "  compression_hardening_file = \"ch.csv\"\n[]\n") ==
                  QStringList{"mesh/case.msh", "ch.csv"},
              "input file reference scan keeps order and skips none values");
  test.expect(gmp::is_safe_snapshot_relative_path("mesh/case.msh") &&
                  !gmp::is_safe_snapshot_relative_path("../outside.msh") &&
                  !gmp::is_safe_snapshot_relative_path("/abs/outside.msh") &&
                  !gmp::is_safe_snapshot_relative_path(""),
              "snapshot relative path rules accept in-root and reject "
              "traversal/absolute/empty");
}

// TASK-V02-010：ProjectDocument / ProjectObject / ObjectId 骨架合同。
// 本测试不得构造 QTreeWidget（或任何 QWidget）。
void test_project_document_contract(TestContext& test) {
  using namespace gmp::core;

  // CRUD 与 ID 稳定性：显式 ID 原样保留，缺省 ID 由文档分配且唯一。
  ProjectDocument doc;
  const ObjectId part_id(QStringLiteral("obj-part-1"));
  test.expect(doc.addObject(std::make_unique<ProjectObject>(
                  QStringLiteral("Parts"), QStringLiteral("part_concrete"),
                  part_id)) == part_id,
              "explicit object id is preserved on add");
  const ObjectId generated =
      doc.addObject(std::make_unique<ProjectObject>(QStringLiteral("Materials"),
                                                    QStringLiteral("mat")));
  test.expect(generated.isValid() && generated != part_id &&
                  doc.contains(generated),
              "document assigns a unique stable id when absent");
  test.expect(doc.addObject(std::make_unique<ProjectObject>(
                  QStringLiteral("Parts"), QStringLiteral("dup"), part_id)) ==
                  ObjectId(),
              "duplicate object id is rejected");
  test.expect(doc.count() == 2 && doc.object(part_id) &&
                  doc.object(part_id)->name() == "part_concrete",
              "objects are retrievable by id");

  // 层级：挂载顺序保持、子树递归删除、无效父拒绝。
  const ObjectId sketch_id =
      doc.addObject(std::make_unique<ProjectObject>(QStringLiteral("Sketches"),
                                                    QStringLiteral("sk")),
                    part_id);
  const ObjectId feature_id =
      doc.addObject(std::make_unique<ProjectObject>(QStringLiteral("Features"),
                                                    QStringLiteral("feat")),
                    part_id);
  test.expect(sketch_id.isValid() && feature_id.isValid() &&
                  doc.children(part_id) == QList<ObjectId>{sketch_id, feature_id} &&
                  doc.parentOf(feature_id) == part_id &&
                  doc.roots().contains(part_id) &&
                  !doc.roots().contains(sketch_id),
              "hierarchy keeps mount order and parent linkage");
  test.expect(doc.addObject(std::make_unique<ProjectObject>(
                  QStringLiteral("Mesh"), QStringLiteral("orphan"),
                  ObjectId(QStringLiteral("obj-orphan"))),
                  ObjectId(QStringLiteral("obj-missing"))) == ObjectId(),
              "mounting under an unknown parent is rejected");
  test.expect(doc.removeObject(part_id) && !doc.contains(part_id) &&
                  !doc.contains(sketch_id) && !doc.contains(feature_id) &&
                  doc.count() == 1 && !doc.removeObject(part_id),
              "removeObject deletes the whole subtree exactly once");

  // 状态机：五个状态与现有 status 字符串逐字对齐，空串/大小写兼容。
  test.expect(to_string(ObjectStatus::Ready) == "ready" &&
                  to_string(ObjectStatus::Incomplete) == "incomplete" &&
                  to_string(ObjectStatus::Invalid) == "invalid" &&
                  to_string(ObjectStatus::Stale) == "stale" &&
                  to_string(ObjectStatus::Disabled) == "disabled",
              "object status strings match the schema vocabulary");
  bool parse_ok = false;
  test.expect(object_status_from_string("Stale", &parse_ok) ==
                  ObjectStatus::Stale &&
                  parse_ok,
              "status parsing is case-insensitive");
  test.expect(object_status_from_string("", &parse_ok) == ObjectStatus::Ready &&
                  parse_ok,
              "empty status parses as ready (legacy projects)");
  test.expect(object_status_from_string("bogus", &parse_ok) ==
                  ObjectStatus::Ready &&
                  !parse_ok,
              "unknown status is reported without crashing");
  test.expect(doc.setStatus(generated, ObjectStatus::Stale) &&
                  doc.object(generated)->status() == ObjectStatus::Stale &&
                  !doc.setStatus(ObjectId(QStringLiteral("obj-missing")),
                                 ObjectStatus::Ready),
              "status transitions apply to existing objects only");

  // 序列化 round-trip：ID/名称/类别/状态/层级/参数全部稳定。
  ProjectDocument roundtrip_source;
  const ObjectId root_id(QStringLiteral("11111111-1111-4111-8111-111111111111"));
  const ObjectId child_id(QStringLiteral("22222222-2222-4222-8222-222222222222"));
  auto root_object = std::make_unique<ProjectObject>(
      QStringLiteral("Parts"), QStringLiteral("part_concrete"), root_id);
  root_object->properties().set(QStringLiteral("type"),
                                QStringLiteral("Part"));
  root_object->properties().set(QStringLiteral("gmsh_volume_tag"), 1);
  roundtrip_source.addObject(std::move(root_object));
  auto child_object = std::make_unique<ProjectObject>(
      QStringLiteral("Features"), QStringLiteral("feature_1"), child_id);
  child_object->setStatus(ObjectStatus::Stale);
  roundtrip_source.addObject(std::move(child_object), root_id);
  ProjectDocument roundtrip_target;
  QString load_error;
  test.expect(roundtrip_target.from_variant_list(
                  roundtrip_source.to_variant_list(), &load_error) &&
                  load_error.isEmpty(),
              "document variant-list round-trip loads");
  const ProjectObject* reloaded = roundtrip_target.object(child_id);
  test.expect(reloaded && reloaded->name() == "feature_1" &&
                  reloaded->kind() == "Features" &&
                  reloaded->status() == ObjectStatus::Stale &&
                  roundtrip_target.parentOf(child_id) == root_id &&
                  roundtrip_target.children(root_id) == QList<ObjectId>{child_id},
              "object identity and hierarchy survive serialization");
  const ProjectObject* reloaded_root = roundtrip_target.object(root_id);
  test.expect(reloaded_root &&
                  reloaded_root->properties().get<int>(
                      QStringLiteral("gmsh_volume_tag")) == 1 &&
                  reloaded_root->properties().get<QString>(
                      QStringLiteral("type")) == "Part",
              "object properties survive serialization with value types");

  auto entry = [](const QString& id, const QString& parent = QString()) {
    return QVariantMap{{"id", id},
                       {"name", id},
                       {"kind", "Fixture"},
                       {"status", "ready"},
                       {"parent", parent},
                       {"params", QVariantMap{}}};
  };

  // 乱序与深层装载：child-before-parent 不影响根/同父输入顺序；
  // 序列化使用层级先序，不使用 ID 字典序表达业务顺序。
  const ObjectId root_a(QStringLiteral("root-a"));
  const ObjectId root_b(QStringLiteral("root-b"));
  const ObjectId child_1(QStringLiteral("child-1"));
  const ObjectId child_2(QStringLiteral("child-2"));
  const ObjectId child_3(QStringLiteral("child-3"));
  const ObjectId grandchild(QStringLiteral("grandchild"));
  const QVariantList unordered = {
      entry(grandchild.toString(), child_1.toString()),
      entry(child_2.toString(), root_a.toString()),
      entry(root_b.toString()),
      entry(child_1.toString(), root_a.toString()),
      entry(root_a.toString()),
      entry(child_3.toString(), root_a.toString()),
  };
  ProjectDocument unordered_target;
  test.expect(unordered_target.from_variant_list(unordered, &load_error) &&
                  unordered_target.roots() ==
                      QList<ObjectId>{root_b, root_a} &&
                  unordered_target.children(root_a) ==
                      QList<ObjectId>{child_2, child_1, child_3} &&
                  unordered_target.children(child_1) ==
                      QList<ObjectId>{grandchild},
              "unordered load supports deep child-before-parent hierarchy and "
              "keeps root/sibling input order");
  const QVariantList hierarchy_order = unordered_target.to_variant_list();
  QStringList hierarchy_ids;
  for (const QVariant& value : hierarchy_order) {
    hierarchy_ids.append(value.toMap().value("id").toString());
  }
  ProjectDocument hierarchy_roundtrip;
  test.expect(hierarchy_ids ==
                  QStringList{"root-b", "root-a", "child-2", "child-1",
                              "grandchild", "child-3"} &&
                  hierarchy_roundtrip.from_variant_list(hierarchy_order,
                                                        &load_error) &&
                  hierarchy_roundtrip.to_variant_list() == hierarchy_order,
              "hierarchical serialization order is stable across round-trip");

  // 损坏输入：缺 ID、重复 ID、缺失父、自引用和环必须拒绝，
  // 且任何失败都不破坏当前文档。
  const QVariantList before_rejected_load = roundtrip_target.to_variant_list();
  const QVariantMap duplicate = entry(QStringLiteral("duplicate"));
  test.expect(
      !roundtrip_target.from_variant_list(
          QVariantList{QVariantMap{{"name", "no-id"}}}) &&
          !roundtrip_target.from_variant_list(
              QVariantList{duplicate, duplicate}) &&
          !roundtrip_target.from_variant_list(QVariantList{entry(
              QStringLiteral("orphan"), QStringLiteral("missing-parent"))}) &&
          !roundtrip_target.from_variant_list(QVariantList{entry(
              QStringLiteral("self"), QStringLiteral("self"))}) &&
          !roundtrip_target.from_variant_list(
              QVariantList{entry(QStringLiteral("cycle-a"),
                                 QStringLiteral("cycle-b")),
                           entry(QStringLiteral("cycle-b"),
                                 QStringLiteral("cycle-a"))}) &&
          roundtrip_target.to_variant_list() == before_rejected_load,
      "invalid hierarchy loads are rejected atomically");
}

// TASK-V02-011：PropertyBag ↔ QVariantMap 双向无损合同。
// 样本取自 phase5-g1-assembly-contact.gmp.yaml 的 model 段（Phase 5 真实
// 节点类型；CDP 样本取自内置模板 "CDP Concrete (Abaqus)" 参数集）。
void test_property_bag_contract(TestContext& test) {
  using namespace gmp::core;

  const QList<QPair<QString, QVariantMap>> g1_samples = {
      {QStringLiteral("Part"),
       {{"type", "Part"},
        {"sketch", "sketch_concrete"},
        {"feature", "feature_1"},
        {"brep", "/work/features/extrude_1.brep"},
        {"mesh", "/work/features/extrude_1.msh"},
        {"description", ""},
        {"gmsh_volume_tag", 1},
        {"gmsh_volume_tags", QVariantList{1}}}},
      {QStringLiteral("Feature"),
       {{"type", "Extrude"},
        {"sketch", "sketch_concrete"},
        {"part", "part_concrete"},
        {"distance", 20.0},
        {"brep", "/work/features/extrude_1.brep"},
        {"mesh", "/work/features/extrude_1.msh"},
        {"gmsh_volume_tag", 1},
        {"gmsh_volume_tags", QVariantList{1}}}},
      {QStringLiteral("Assembly instance"),
       {{"type", "PartInstance"},
        {"part", "part_plate"},
        {"order", 1},
        {"translate_x", 0.0},
        {"translate_y", 0.0},
        {"translate_z", 20.0},
        {"rotate_x", 0.0},
        {"rotate_y", 0.0},
        {"rotate_z", 30.0},
        {"scale_x", 1.0},
        {"scale_y", 1.0},
        {"scale_z", 1.0},
        {"visible", true}}},
      {QStringLiteral("Material (isotropic)"),
       {{"type", "ComputeIsotropicElasticityTensor"},
        {"block", "instance_concrete"},
        {"youngs_modulus", 29791459780.0},
        {"poissons_ratio", 0.2}}},
      {QStringLiteral("Material (CDP)"),
       {{"type", "AbaqusCDP"},
        {"youngs_modulus", "29791500000"},
        {"poissons_ratio", "0.2"},
        {"dilation_angle", "36"},
        {"eccentricity", "0.1"},
        {"biaxial_to_uniaxial_compression_ratio", "1.16"},
        {"tensile_meridian_ratio", "0.667"},
        {"viscosity", "5e-4"},
        {"tension_recovery", "0"},
        {"compression_recovery", "1"},
        {"maximum_substeps", "256"},
        {"maximum_strain_increment", "2.5e-5"},
        {"enable_performance_diagnostics", "true"},
        {"unit_factor_stress", "1000000"}}},
      {QStringLiteral("Section"),
       {{"type", "SolidSection"},
        {"material", "concrete_elasticity"},
        {"block", "instance_concrete"}}},
      {QStringLiteral("Physics"),
       {{"action", "QuasiStatic"},
        {"add_variables", true},
        {"block", "instance_plate instance_concrete"},
        {"generate_output", "stress_xx stress_yy vonmises_stress"},
        {"incremental", true},
        {"save_in_resid", true},
        {"strain", "SMALL"},
        {"volumetric_locking_correction", true}}},
      {QStringLiteral("Step"),
       {{"type", "Transient"},
        {"start_time", 0.0},
        {"end_time", 1.0},
        {"solve_type", "NEWTON"},
        {"line_search", "bt"},
        {"automatic_scaling", true},
        {"nl_rel_tol", 1e-09},
        {"nl_abs_tol", 1e-08},
        {"nl_max_its", 50},
        {"num_steps", 100000},
        {"dtmin", 1e-15},
        {"dtmax", 1.0},
        {"timestepper_type", "IterationAdaptiveDT"},
        {"dt", 0.01},
        {"optimal_iterations", 8},
        {"iteration_window", 3},
        {"growth_factor", 1.15},
        {"cutback_factor", 0.5},
        {"preconditioning_type", "SMP"},
        {"preconditioning_full", true},
        {"petsc_options_iname", "-pc_type -pc_factor_mat_solver_type"},
        {"petsc_options_value", "lu mumps"},
        {"variable", "disp_z"}}},
      {QStringLiteral("BC (Dirichlet)"),
       {{"type", "DirichletBC"},
        {"boundary", "fixed_bottom"},
        {"value", 0},
        {"variable", "disp_x"}}},
      {QStringLiteral("BC (FunctionDirichlet)"),
       {{"type", "FunctionDirichletBC"},
        {"boundary", "load_top"},
        {"function", "loading_curve"},
        {"value", 0},
        {"variable", "disp_z"}}},
      {QStringLiteral("Function (PiecewiseLinear)"),
       {{"type", "PiecewiseLinear"},
        {"expression", "1"},
        {"x", "0 1"},
        {"y", "0 -2.5e-5"}}},
      {QStringLiteral("Interaction (Contact)"),
       {{"type", "Contact"},
        {"model", "coulomb"},
        {"formulation", "kinematic"},
        {"friction_coefficient", 0.15},
        {"tangential_tolerance", 0.0005},
        {"penalty", 1000000000000.0},
        {"normalize_penalty", true},
        {"primary", "contact_plate"},
        {"secondary", "contact_concrete"}}},
      {QStringLiteral("Outputs"),
       {{"type", "Exodus"},
        {"field_outputs", ""},
        {"file_base", ""},
        {"hist_boundary", "load_top"},
        {"hist_disp_variable", "disp_z"},
        {"hist_displacement_avg", true},
        {"hist_extremum", false},
        {"hist_extremum_types", "min max"},
        {"hist_extremum_variables", ""},
        {"hist_reaction_force", true},
        {"output_csv", true},
        {"output_exodus", true},
        {"times_enabled", true},
        {"times_start", 0.0},
        {"times_end", 1.0},
        {"times_interval", 0.01},
        {"times_name", "field_output_times"}}},
      {QStringLiteral("Mesh"),
       {{"model_source", "assembly: active"},
        {"path", "/work/.work/case/proj/mesh_assembly_g1.msh"},
        {"source", "gmsh"},
        {"status", "New"},
        {"physical_group_names",
         QVariantList{"instance_plate_surface", "fixed_bottom"}}}},
  };
  for (const auto& sample : g1_samples) {
    const QVariantMap& params = sample.second;
    const PropertyBag bag = PropertyBag::from_variant_map(params);
    const QVariantMap roundtrip = bag.to_variant_map();
    test.expect(roundtrip == params &&
                    PropertyBag::from_variant_map(roundtrip).to_variant_map() ==
                        params,
                sample.first + ": params round-trip is lossless");
    test.expect(bag.keys().size() == params.size(),
                sample.first + ": no key is injected or dropped");
  }

  // 类型化读取。
  const PropertyBag assembly = PropertyBag::from_variant_map(
      g1_samples.at(2).second);
  test.expect(assembly.get<QString>(QStringLiteral("part")) == "part_plate" &&
                  assembly.get<double>(QStringLiteral("rotate_z")) == 30.0 &&
                  assembly.get<bool>(QStringLiteral("visible")) &&
                  assembly.get<int>(QStringLiteral("order")) == 1,
              "typed getters return stored value types");

  // 变更回调只在值真正变化时触发。
  PropertyBag bag = PropertyBag::from_variant_map(g1_samples.at(3).second);
  QStringList changed_keys;
  bag.setOnChanged([&changed_keys](const QString& key) {
    changed_keys << key;
  });
  test.expect(!bag.set(QStringLiteral("poissons_ratio"), 0.2) &&
                  changed_keys.isEmpty(),
              "rewriting an identical value does not fire changed");
  test.expect(bag.set(QStringLiteral("poissons_ratio"), 0.25) &&
                  changed_keys == QStringList{"poissons_ratio"} &&
                  bag.get<double>(QStringLiteral("poissons_ratio")) == 0.25,
              "value change fires changed exactly once");

  // 定义驱动校验：required / enum / validator 钩子；未定义键不受影响。
  QList<PropertyDefinition> definitions;
  PropertyDefinition type_def;
  type_def.key = QStringLiteral("type");
  type_def.displayName = QStringLiteral("Type");
  type_def.type = QStringLiteral("enum");
  type_def.required = true;
  type_def.enumValues = {QStringLiteral("ComputeIsotropicElasticityTensor"),
                         QStringLiteral("AbaqusCDP")};
  definitions << type_def;
  PropertyDefinition young_def;
  young_def.key = QStringLiteral("youngs_modulus");
  young_def.displayName = QStringLiteral("Young's Modulus");
  young_def.type = QStringLiteral("number");
  young_def.unit = QStringLiteral("pressure");
  young_def.required = true;
  young_def.validator = [](const QVariant& value, QString* error) {
    bool ok = false;
    const double v = value.toDouble(&ok);
    if (!ok || v <= 0.0) {
      if (error) {
        *error = QStringLiteral("youngs_modulus must be > 0");
      }
      return false;
    }
    return true;
  };
  definitions << young_def;
  PropertyDefinition block_def;
  block_def.key = QStringLiteral("block");
  block_def.type = QStringLiteral("reference");
  block_def.referenceKind = QStringLiteral("Mesh");
  definitions << block_def;
  bag.setDefinitions(definitions);
  test.expect(bag.validate().isEmpty(),
              "defined properties validate clean against the sample");
  PropertyBag broken = PropertyBag::from_variant_map(
      QVariantMap{{"type", "NotAType"}, {"youngs_modulus", "-1"}});
  broken.setDefinitions(definitions);
  const QStringList violations = broken.validate();
  test.expect(violations.contains("type") &&
                  violations.contains("youngs_modulus"),
              "enum violation and validator failure are both reported");
  PropertyBag missing = PropertyBag::from_variant_map(
      QVariantMap{{"block", "instance_concrete"}});
  missing.setDefinitions(definitions);
  test.expect(missing.validate().contains("type") &&
                  missing.validate().contains("youngs_modulus"),
              "required keys missing from params are reported");

  // 默认值只经显式 apply_defaults 注入；round-trip 不隐式改写加载值。
  PropertyDefinition dt_def;
  dt_def.key = QStringLiteral("dt");
  dt_def.type = QStringLiteral("number");
  dt_def.defaultValue = 0.01;
  PropertyBag with_defaults = PropertyBag::from_variant_map(
      QVariantMap{{"type", "Transient"}});
  with_defaults.setDefinitions({dt_def});
  test.expect(with_defaults.to_variant_map() == QVariantMap{{"type", "Transient"}},
              "round-trip never injects definition defaults implicitly");
  with_defaults.apply_defaults();
  test.expect(with_defaults.get<double>(QStringLiteral("dt")) == 0.01,
              "apply_defaults fills only missing defined keys");
  with_defaults.apply_defaults();
  test.expect(with_defaults.get<double>(QStringLiteral("dt")) == 0.01,
              "apply_defaults never overwrites an existing value");
}

// TASK-V02-012：DependencyGraph 机制 + legacy stale 规则对照合同。
// 期望矩阵手工转录自 MainWindow::invalidate_downstream_from()
// （src/MainWindow.cpp 7543-7561）：两侧任一改动的分歧都会变红。
void test_dependency_graph_contract(TestContext& test) {
  using namespace gmp::core;

  // 机制：自环/重复边拒绝，下游/上游查询按登记序，remove 生效。
  DependencyGraph graph;
  test.expect(!graph.addDependency("A", "A") &&
                  !graph.addDependency(QString(), "B"),
              "self-loop and empty endpoints are rejected");
  test.expect(graph.addDependency("A", "B") && graph.addDependency("A", "C") &&
                  graph.addDependency("B", "D") &&
                  !graph.addDependency("A", "B") &&
                  graph.edgeCount() == 3,
              "edges register once in declaration order");
  test.expect(graph.downstream("A") == QStringList{"B", "C"} &&
                  graph.upstream("D") == QStringList{"B"},
              "direct downstream/upstream queries follow registration order");
  test.expect(graph.removeDependency("A", "C") &&
                  !graph.removeDependency("A", "C") &&
                  graph.downstream("A") == QStringList{"B"},
              "removeDependency detaches exactly the named edge");

  // 传播闭包与拓扑排序。
  DependencyGraph chain;
  chain.addDependency("a", "b");
  chain.addDependency("b", "c");
  chain.addDependency("a", "d");
  test.expect(chain.markStaleFrom("a") == QStringList{"b", "d", "c"} &&
                  chain.markStaleFrom("b") == QStringList{"c"} &&
                  chain.markStaleFrom("c").isEmpty(),
              "markStaleFrom returns the transitive downstream closure");
  const QStringList order = chain.topologicalOrder();
  test.expect(order.indexOf("a") < order.indexOf("b") &&
                  order.indexOf("b") < order.indexOf("c") &&
                  order.indexOf("a") < order.indexOf("d"),
              "topological order places upstream before downstream");
  chain.addDependency("c", "a");
  test.expect(chain.hasCycle() && chain.topologicalOrder().isEmpty(),
              "cyclic graph is detected and yields no topological order");
  test.expect(!graph.hasCycle() && !DependencyGraph().hasCycle(),
              "acyclic and empty graphs report no cycle");

  // 对照测试：legacy_stale_rule_edges() 注册的 kind 级边集，其传播闭包
  // 必须与 invalidate_downstream_from() 的手工转录矩阵逐条一致。
  // （旧规则中 run/queue/submit 状态对象跳过是应用侧过滤，非边规则。）
  DependencyGraph legacy;
  for (const auto& edge : legacy_stale_rule_edges()) {
    legacy.addDependency(edge.first, edge.second);
  }
  auto sorted = [](QStringList values) {
    values.sort();
    return values;
  };
  const QStringList mesh_chain = {"Input Cases", "Jobs"};
  const QStringList full_chain = {"Input Cases", "Jobs", "Mesh"};
  const QMap<QString, QStringList> expected_matrix = {
      {"Parts", sorted({"Assembly", "Mesh", "Input Cases", "Jobs"})},
      {"Features", sorted({"Assembly", "Mesh", "Input Cases", "Jobs"})},
      {"Sketches", sorted({"Assembly", "Mesh", "Input Cases", "Jobs"})},
      {"Mesh", sorted(mesh_chain)},
      {"Input Cases", {"Jobs"}},
      {"Jobs", {}},
      {"Results", {}},
  };
  for (auto it = expected_matrix.cbegin(); it != expected_matrix.cend(); ++it) {
    test.expect(sorted(legacy.markStaleFrom(it.key())) == it.value(),
                "legacy stale matrix matches graph closure: " + it.key());
  }
  // 其余一切根节点（Materials/Sections/Assembly/Physics/Steps/BC/Loads/
  // Interactions/Constraints/Selections/Functions/Variables/Outputs/Datums）
  // 在旧 else 分支下都传播到 Mesh/Input Cases/Jobs。
  bool others_match = true;
  for (const QString& root : gmp::project_schema::model_root_nodes()) {
    if (expected_matrix.contains(root)) {
      continue;
    }
    others_match =
        others_match && sorted(legacy.markStaleFrom(root)) == full_chain;
  }
  test.expect(others_match,
              "all remaining root kinds propagate to Mesh/Input Cases/Jobs");
}

// TASK-V02-013：TransactionManager 骨架合同。参考用法映射
// FloatingPropertyForm 缓冲语义：打开表单=begin、编辑=execute、
// 确定=commit、取消/校验失败=rollback。Q4 红线：不提供用户可见
// Undo/Redo 入口（本层无此类 API）。
void test_transaction_manager_contract(TestContext& test) {
  using namespace gmp::core;

  PropertyBag form_buffer = PropertyBag::from_variant_map(
      QVariantMap{{"type", "ComputeIsotropicElasticityTensor"},
                  {"youngs_modulus", 29791459780.0},
                  {"poissons_ratio", 0.2}});

  TransactionManager tm;
  test.expect(!tm.commit() && !tm.rollback() &&
                  !tm.execute(std::make_unique<SetPropertyValueCommand>(
                      form_buffer, "poissons_ratio", 0.3)),
              "commit/rollback/execute without a transaction are rejected");

  // 取消路径：编辑后 rollback，缓冲还原且审计记录 rolled back。
  test.expect(tm.begin("edit Materials/concrete_elasticity") &&
                  !tm.begin("nested"),
              "nested begin is rejected while a transaction is active");
  test.expect(tm.execute(std::make_unique<SetPropertyValueCommand>(
                  form_buffer, "poissons_ratio", 0.25)),
              "command executes inside the transaction");
  test.expect(form_buffer.get<double>("poissons_ratio") == 0.25,
              "edits are visible on the buffer before commit");
  test.expect(tm.auditLog().isEmpty(),
              "open transaction is not yet in the audit log");
  test.expect(tm.rollback() &&
                  form_buffer.get<double>("poissons_ratio") == 0.2,
              "rollback restores the buffer to its pre-edit state");
  test.expect(tm.auditLog().size() == 1 && !tm.auditLog().first().committed &&
                  tm.auditLog().first().label ==
                      "edit Materials/concrete_elasticity" &&
                  tm.auditLog().first().commands ==
                      QStringList{"set poissons_ratio"} &&
                  tm.auditLog().first().before.first() ==
                      QVariantMap{{"poissons_ratio", 0.2}} &&
                  tm.auditLog().first().after.first() ==
                      QVariantMap{{"poissons_ratio", 0.25}},
              "rolled-back transaction keeps a full audit record");

  // 确定路径：多命令提交后生效，审计记录 committed。
  test.expect(tm.begin("edit Materials/concrete_elasticity (retry)"),
              "second transaction begins after rollback");
  tm.execute(std::make_unique<SetPropertyValueCommand>(
      form_buffer, "youngs_modulus", 3.0e10));
  tm.execute(std::make_unique<SetPropertyValueCommand>(form_buffer, "note",
                                                       "added"));
  test.expect(tm.commit() &&
                  form_buffer.get<double>("youngs_modulus") == 3.0e10 &&
                  form_buffer.get<QString>("note") == "added",
              "committed transaction keeps all edits");
  const TransactionRecord& committed = tm.auditLog().last();
  test.expect(committed.committed && committed.commands.size() == 2 &&
                  committed.after.first().value("youngs_modulus") == 3.0e10,
              "committed transaction records before/after per command");

  // 校验失败即取消：新增键在 rollback 后必须完全消失。
  test.expect(tm.begin("edit with invalid field"),
              "third transaction begins after commit");
  tm.execute(std::make_unique<SetPropertyValueCommand>(form_buffer, "temp_key",
                                                       "temp"));
  test.expect(form_buffer.contains("temp_key"), "new key visible pre-commit");
  test.expect(tm.rollback() && !form_buffer.contains("temp_key"),
              "rollback removes keys that did not exist before the edit");
  test.expect(tm.auditLog().size() == 3,
              "every finished transaction appends one audit record");
}

// 缺陷 2026-09-19-025/026：单位换算显示合同（可无 Widget 测试部分）。
void test_unit_display_contract(TestContext& test) {
  // 显示格式：不暴露 double 伪精度。
  const QString young_display =
      gmp::format_unit_display_value(29791459780.0, 1e6);
  test.expect(young_display == "29791.45978" &&
                  !young_display.contains("0000001"),
              "Pa->MPa display does not expose double noise");
  test.expect(gmp::format_unit_display_value(206000000000.0, 1e6) == "206000" &&
                  gmp::format_unit_display_value(500.0, 1e6) == "0.0005" &&
                  gmp::format_unit_display_value(0.0, 1e6) == "0",
              "unit display stays clean for integer/scientific samples");
  test.expect(gmp::format_unit_display_value(29791459780.0, 0.0) ==
                  "29791459780",
              "non-positive factor falls back to 1 without division by zero");

  // 往返：显示值回解析 ×factor 与存储值在双精度噪声内一致。
  bool parse_ok = false;
  const double roundtrip = young_display.toDouble(&parse_ok) * 1e6;
  test.expect(parse_ok && qFuzzyCompare(roundtrip + 1.0, 29791459780.0 + 1.0),
              "display value round-trips to the stored value within double "
              "noise");

  // 单位键登记：youngs_modulus 有单位机制，其余键如实返回无。
  gmp::UnitKeyInfo info;
  test.expect(gmp::unit_key_info("youngs_modulus", &info) &&
                  info.quantity == "pressure" && info.stored_unit == "Pa" &&
                  info.display_unit == "MPa",
              "youngs_modulus is registered as Pa-stored/MPa-displayed");
  test.expect(!gmp::unit_key_info("poissons_ratio", nullptr) &&
                  !gmp::unit_key_info("dt", nullptr),
              "keys without a unit mechanism report none");
}

// TASK-V02-020：ProjectStore 持久化服务级合同（无 Widget，临时目录）。
void test_project_store_contract(TestContext& test) {
  gmp::ProjectStore store;
  QTemporaryDir workspace;

  // ---- save → load round-trip ----
  gmp::ProjectData data;
  data.schema_version = 2;
  data.application_profile = {{"id", "DamSafetyApp-opt"}, {"version", "1.0.0"}};
  data.unit_contract = {{"name", "SI"}, {"pressure", "Pa"}};
  data.mesh_snapshot = valid_physical_groups(QString(64, 'd'));
  data.model_roots = {"Parts", "Materials", "Input Cases", "Mesh"};
  gmp::ProjectModelEntry part;
  part.name = "part_concrete";
  part.kind = "Parts";
  part.status = "Ready";
  part.params = {{"type", "Part"},
                 {"sketch", "sketch_concrete"},
                 {"gmsh_volume_tag", qlonglong(1)},
                 {"gmsh_volume_tags", QVariantList{qlonglong(1)}},
                 {"visible", true},
                 {"distance", 0.25}};
  gmp::ProjectModelEntry mat;
  mat.name = "concrete_elasticity";
  mat.kind = "Materials";
  mat.params = {{"type", "ComputeIsotropicElasticityTensor"},
                {"block", "instance_concrete"},
                {"youngs_modulus", 29791459780.0},
                {"poissons_ratio", 0.2}};
  data.model_entries = {part, mat};
  data.gmsh_settings = {{"model_source", "assembly: active"},
                        {"auto_reload_geometry", true},
                        {"mesh_dim", 3},
                        {"mesh_size", 0.5}};
  data.moose_settings = {{"input_path", "123"},  // force-string：保持字符串
                         {"workdir", "/tmp/case"},
                         {"input_text", "[Mesh]\n[]\n"},
                         {"mpi_ranks", 4},
                         {"use_mpi", true}};
  data.input_snapshots = {"snapshots/v1", "snapshots/v2"};
  data.viewer_settings = {{"current_file", "out/view.msh"},
                          {"mesh_opacity", 0.5},
                          {"auto_range", true}};
  const QString project_file = workspace.filePath("roundtrip.gmp.yaml");
  QString error;
  test.expect(store.save_file(project_file, data, &error) && error.isEmpty(),
              "project store saves a v2 project file");
  gmp::ProjectData loaded;
  test.expect(store.load_file(project_file, &loaded, &error),
              "project store loads the saved file");
  bool entries_equal = loaded.model_entries.size() == 2;
  for (int i = 0; entries_equal && i < 2; ++i) {
    const auto& expected = data.model_entries.at(i);
    const auto& actual = loaded.model_entries.at(i);
    entries_equal = actual.name == expected.name &&
                    actual.kind == expected.kind &&
                    actual.status == expected.status &&
                    actual.params == expected.params;
  }
  test.expect(entries_equal,
              "model entries round-trip with typed params intact");
  test.expect(loaded.model_roots == data.model_roots,
              "all model roots (including empty ones) are preserved");
  test.expect(loaded.gmsh_settings == data.gmsh_settings &&
                  loaded.moose_settings == data.moose_settings &&
                  loaded.viewer_settings == data.viewer_settings,
              "panel settings round-trip with type coercion intact");
  test.expect(loaded.moose_settings.value("input_path").toString() == "123" &&
                  loaded.moose_settings.value("input_path").typeId() ==
                      QMetaType::QString,
              "force-string fields stay strings even when numeric-looking");
  test.expect(loaded.input_snapshots == data.input_snapshots &&
                  loaded.mesh_snapshot.mesh_sha256 ==
                      data.mesh_snapshot.mesh_sha256 &&
                  loaded.mesh_snapshot.groups.size() == 1,
              "input snapshots and mesh manifest round-trip");
  test.expect(read_file(project_file).contains("Input Cases"),
              "saved file keeps the Input Cases root key (schema shape)");

  // 二次 round-trip 稳定（第一圈之后的类型形态不再漂移）。
  const QString second_file = workspace.filePath("roundtrip2.gmp.yaml");
  gmp::ProjectData reloaded;
  test.expect(store.save_file(second_file, loaded, &error) &&
                  store.load_file(second_file, &reloaded, &error) &&
                  reloaded.model_entries.first().params ==
                      loaded.model_entries.first().params &&
                  reloaded.gmsh_settings == loaded.gmsh_settings,
              "second round-trip is a fixed point");

  // 名称去重：与 unique_child_name 同款 base/base_2 规则。
  gmp::ProjectData dup;
  dup.model_roots = {"Parts"};
  gmp::ProjectModelEntry a;
  a.name = "part";
  a.kind = "Parts";
  a.params = {{"type", "Part"}};
  gmp::ProjectModelEntry b = a;
  dup.model_entries = {a, b};
  // 手工构造重名 YAML：save 不会去重（调用方保证唯一），load 去重。
  const QString dup_file = workspace.filePath("dup.gmp.yaml");
  QFile dup_out(dup_file);
  dup_out.open(QIODevice::WriteOnly | QIODevice::Text);
  dup_out.write("schema_version: 2\nmodel:\n  Parts:\n"
                "    - {name: part, kind: Parts, params: {type: Part}}\n"
                "    - {name: part, kind: Parts, params: {type: Part}}\n");
  dup_out.close();
  gmp::ProjectData dup_loaded;
  test.expect(store.load_file(dup_file, &dup_loaded, &error) &&
                  dup_loaded.model_entries.size() == 2 &&
                  dup_loaded.model_entries.at(0).name == "part" &&
                  dup_loaded.model_entries.at(1).name == "part_2",
              "duplicate entry names are deduplicated with _2 suffix");

  // ---- 加载错误路径 ----
  gmp::ProjectData unused;
  test.expect(!store.load_file(workspace.filePath("missing.gmp.yaml"),
                               &unused, &error) &&
                  error.contains("Failed to load"),
              "missing file fails with a readable error");
  const QString bad_version = workspace.filePath("bad_version.gmp.yaml");
  write_file(bad_version, "version: 3\nmodel: {}\n");
  test.expect(!store.load_file(bad_version, &unused, &error) &&
                  error == "Unsupported project version.",
              "legacy version > 2 is rejected");
  const QString bad_schema = workspace.filePath("bad_schema.gmp.yaml");
  write_file(bad_schema, "schema_version: 99\nmodel: {}\n");
  test.expect(!store.load_file(bad_schema, &unused, &error) &&
                  error == "Unsupported schema version: 99",
              "unknown schema_version is rejected");
  const QString no_model = workspace.filePath("no_model.gmp.yaml");
  write_file(no_model, "schema_version: 2\n");
  test.expect(!store.load_file(no_model, &unused, &error) &&
                  error == "Invalid project file (missing model).",
              "missing model section is rejected");

  // ---- 路径迁移（foreign case，真实复制） ----
  const QString proj_a = workspace.filePath("proj_a.gmp.yaml");
  const QString proj_b = workspace.filePath("proj_b.gmp.yaml");
  const QString mesh_a = gmp::project_case_work_dir(proj_a) + "/mesh_g1.msh";
  const QString mesh_b = gmp::project_case_work_dir(proj_b) + "/mesh_g1.msh";
  write_file(mesh_a, "gmsh-data\n");
  gmp::ProjectData foreign;
  foreign.model_roots = {"Mesh"};
  gmp::ProjectModelEntry mesh_entry;
  mesh_entry.name = "mesh_g1";
  mesh_entry.kind = "Mesh";
  mesh_entry.params = {{"path", mesh_a}, {"source", "gmsh"}};
  foreign.model_entries = {mesh_entry};
  foreign.mesh_snapshot = valid_physical_groups(QString(64, 'e'));
  foreign.mesh_snapshot.mesh_path = mesh_a;
  foreign.gmsh_settings = {{"output_path", mesh_a}};
  foreign.moose_settings = {{"mesh_path", mesh_a},
                            {"input_text", "  file = " + mesh_a + "\n"}};
  const auto migrations = store.migrate_mesh_paths(proj_b, &foreign);
  test.expect(migrations.size() == 1 &&
                  migrations.first().second == mesh_b &&
                  foreign.model_entries.first().params.value("path")
                          .toString() == mesh_b &&
                  foreign.mesh_snapshot.mesh_path == mesh_b &&
                  foreign.gmsh_settings.value("output_path").toString() ==
                      mesh_b &&
                  foreign.moose_settings.value("mesh_path").toString() ==
                      mesh_b &&
                  foreign.moose_settings.value("input_text")
                          .toString()
                          .contains(mesh_b),
              "foreign case mesh path migrates with all linked fields");
  test.expect(read_file(mesh_b) == "gmsh-data\n" &&
                  read_file(mesh_a) == "gmsh-data\n",
              "migration copies the mesh file and leaves the source intact");

  // 已在自有工作目录 → 不动；case 之外的外部路径 → 不动。
  const auto none_own = store.migrate_mesh_paths(proj_b, &foreign);
  test.expect(none_own.isEmpty() &&
                  foreign.model_entries.first().params.value("path")
                          .toString() == mesh_b,
              "paths already inside the own case dir are untouched");
  const QString external = workspace.filePath("elsewhere.msh");
  write_file(external, "ext\n");
  gmp::ProjectData ext_data;
  ext_data.model_roots = {"Mesh"};
  gmp::ProjectModelEntry ext_entry = mesh_entry;
  ext_entry.params.insert("path", external);
  ext_data.model_entries = {ext_entry};
  store.migrate_mesh_paths(proj_b, &ext_data);
  test.expect(ext_data.model_entries.first().params.value("path").toString() ==
                  external,
              "user-chosen external mesh paths are never migrated");

  // legacy out/ 路径：源缺失时仍重定向、不复制、有迁移记录。
  const QString legacy =
      QDir::current().absoluteFilePath("out/v02_020_nonexistent.msh");
  gmp::ProjectData legacy_data;
  legacy_data.model_roots = {"Mesh"};
  gmp::ProjectModelEntry legacy_entry = mesh_entry;
  legacy_entry.name = "legacy_mesh";
  legacy_entry.params.insert("path", legacy);
  legacy_data.model_entries = {legacy_entry};
  legacy_data.moose_settings = {{"mesh_path", legacy}};
  const auto legacy_migrations =
      store.migrate_mesh_paths(proj_a, &legacy_data);
  const QString legacy_target = gmp::project_case_work_dir(proj_a) +
                                "/v02_020_nonexistent.msh";
  test.expect(legacy_migrations.size() == 1 &&
                  legacy_data.model_entries.first()
                          .params.value("path")
                          .toString() == legacy_target &&
                  legacy_data.moose_settings.value("mesh_path").toString() ==
                      legacy_target &&
                  !QFileInfo::exists(legacy_target),
              "missing legacy out/ source redirects without copying");
}

// TASK-V02-030：MooseInputGenerator 服务级合同。G1 形态条目集（取自
// phase5-g1-assembly-contact.gmp.yaml model 段）→ 块文本/报告/确定性。
void test_moose_input_generator_contract(TestContext& test) {
  auto entry = [](const QString& kind, const QString& name,
                  const QVariantMap& params) {
    gmp::ProjectModelEntry e;
    e.kind = kind;
    e.name = name;
    e.params = params;
    return e;
  };
  gmp::MooseInputGenerator::Input input;
  input.entries = {
      entry("Materials", "concrete_elasticity",
            {{"type", "ComputeIsotropicElasticityTensor"},
             {"block", "instance_concrete"},
             {"poissons_ratio", 0.2},
             {"youngs_modulus", 29791459780.0}}),
      entry("Materials", "concrete_stress",
            {{"type", "ComputeLinearElasticStress"},
             {"block", "instance_concrete"}}),
      entry("Materials", "steel_elasticity",
            {{"type", "ComputeIsotropicElasticityTensor"},
             {"block", "instance_plate"},
             {"poissons_ratio", 0.3},
             {"youngs_modulus", 206000000000.0}}),
      entry("Materials", "steel_stress",
            {{"type", "ComputeLinearElasticStress"},
             {"block", "instance_plate"}}),
      entry("Sections", "section_concrete",
            {{"type", "SolidSection"},
             {"material", "concrete_elasticity"},
             {"block", "instance_concrete"}}),
      entry("Assembly", "instance_concrete",
            {{"type", "PartInstance"}, {"part", "part_concrete"}}),
      entry("Physics", "physics_g1",
            {{"action", "QuasiStatic"},
             {"block", "instance_plate instance_concrete"},
             {"volumetric_locking_correction", true},
             {"add_variables", true},
             {"incremental", true},
             {"strain", "SMALL"},
             {"generate_output", "stress_xx vonmises_stress"},
             {"save_in_resid", "true"}}),
      entry("Steps", "step_g1",
            {{"type", "Transient"},
             {"start_time", 0.0},
             {"end_time", 1.0},
             {"solve_type", "NEWTON"},
             {"timestepper_type", "IterationAdaptiveDT"},
             {"dt", 0.01},
             {"preconditioning_type", "SMP"},
             {"preconditioning_full", "true"}}),
      entry("BC", "disp_x",
            {{"type", "DirichletBC"},
             {"boundary", "fixed_bottom"},
             {"value", 0},
             {"variable", "disp_x"}}),
      entry("BC", "load_disp_z",
            {{"type", "FunctionDirichletBC"},
             {"boundary", "load_top"},
             {"function", "loading_curve"},
             {"variable", "disp_z"}}),
      entry("Functions", "loading_curve",
            {{"type", "PiecewiseLinear"}, {"x", "0 1"}, {"y", "0 -2.5e-5"}}),
      entry("Interactions", "contact_plate_concrete",
            {{"type", "Contact"},
             {"model", "coulomb"},
             {"formulation", "kinematic"},
             {"friction_coefficient", 0.15},
             {"tangential_tolerance", 0.0005},
             {"penalty", 1000000000000.0},
             {"normalize_penalty", true},
             {"primary", "contact_plate"},
             {"secondary", "contact_concrete"}}),
      entry("Outputs", "outputs_g1",
            {{"type", "Exodus"},
             {"field_outputs", "stress_xx"},
             {"hist_boundary", "load_top"},
             {"hist_disp_variable", "disp_z"},
             {"hist_displacement_avg", true},
             {"hist_reaction_force", true},
             {"output_csv", true},
             {"output_exodus", true},
             {"times_enabled", true},
             {"times_start", 0.0},
             {"times_end", 1.0},
             {"times_interval", 0.01},
             {"times_name", "field_output_times"}}),
  };
  input.displacements = "disp_x disp_y disp_z";
  input.application_profile_id = "DamSafetyApp-opt";
  input.mapping_registry_loaded = true;
  input.mapping_registry_version = "1.0.0";
  input.input_mode = "structured";
  input.mesh_path = "/work/.work/case/proj/mesh_assembly_g1.msh";
  input.mesh_snapshot = valid_physical_groups(QString(64, 'f'));

  const auto out = gmp::MooseInputGenerator::generate(input);
  test.expect(out.materials.contains("[Materials]") &&
                  out.materials.contains("[concrete_elasticity]") &&
                  out.materials.contains("block = instance_concrete") &&
                  out.materials.contains("youngs_modulus = 29791459780") &&
                  out.materials.contains("block = instance_plate"),
              "materials block carries typed values and assignments");
  test.expect(out.bcs.contains("[BCs]") &&
                  out.bcs.contains("boundary = fixed_bottom") &&
                  out.bcs.contains("[load_disp_z]") &&
                  out.bcs.contains("function = loading_curve"),
              "BCs block carries Dirichlet and FunctionDirichlet entries");
  test.expect(out.interactions.contains("[Contact]") &&
                  out.interactions.contains("[contact_plate_concrete]") &&
                  out.interactions.contains("primary = contact_plate") &&
                  out.interactions.contains("secondary = contact_concrete") &&
                  out.interactions.contains("friction_coefficient = 0.15") &&
                  !out.interactions.contains("master"),
              "contact block uses primary/secondary syntax");
  test.expect(out.executioner.contains("[Executioner]") &&
                  out.executioner.contains("type = Transient") &&
                  out.executioner.contains("[TimeStepper]") &&
                  out.executioner.contains("type = IterationAdaptiveDT") &&
                  out.executioner.contains("[Preconditioning/smp]") &&
                  out.executioner.contains("full = true"),
              "executioner carries TimeStepper sub-block and Preconditioning");
  test.expect(out.global_params ==
                  "[GlobalParams]\n  displacements = 'disp_x disp_y disp_z'\n[]\n",
              "global params carry the injected displacements");
  test.expect(out.physics_headers ==
                  QStringList{"Physics/SolidMechanics/QuasiStatic/physics_g1"} &&
                  out.physics_blocks.first().contains(
                      "block = 'instance_plate instance_concrete'") &&
                  out.physics_blocks.first().contains(
                      "save_in = 'resid_x resid_y resid_z'"),
              "physics action block quotes multi-value block and save_in");
  test.expect(out.outputs.contains("[field_exodus]") &&
                  out.outputs.contains("[history_csv]") &&
                  out.times_header == "Times/field_output_times" &&
                  out.times.contains("time_interval = 0.01"),
              "outputs package emits exodus/csv and the Times object");
  test.expect(out.aux_variables.contains("[resid_x]") &&
                  out.aux_kernels.contains("type = MaterialRealAux") &&
                  out.aux_kernels.contains("block = 'instance_plate'") &&
                  out.postprocessors.contains("[load_top_reaction_x]") &&
                  out.postprocessors.contains("[load_top_disp_avg]"),
              "aux variables/kernels and history postprocessors follow the "
              "outputs package");
  test.expect(out.console_warnings.isEmpty() && out.status_warning.isEmpty(),
              "clean G1-shaped input produces no warnings");

  // 报告可追溯行（ TASK-V02-003 候选转正 ）。
  test.expect(out.generation_report.contains(
                  "Application profile: DamSafetyApp-opt") &&
                  out.generation_report.contains("Mapping registry: 1.0.0") &&
                  out.generation_report.contains(
                      "[Mesh/file] <- Mesh path /work/.work/case/proj/"
                      "mesh_assembly_g1.msh") &&
                  out.generation_report.contains(
                      "[Contact/contact_plate_concrete] <- Model Tree "
                      "Interactions/contact_plate_concrete "
                      "(primary=contact_plate, secondary=contact_concrete, "
                      "mapping=Contact/Contact)") &&
                  out.generation_report.contains(
                      "[Executioner] <- Model Tree Steps/step_g1") &&
                  out.generation_report.contains("Physical Groups:"),
              "generation report carries full traceability lines");

  // 确定性：两次生成逐字一致。
  const auto out_b = gmp::MooseInputGenerator::generate(input);
  auto flatten = [](const gmp::MooseInputGenerator::Output& o) {
    return o.functions + o.variables + o.materials + o.bcs + o.loads +
           o.outputs + o.executioner + o.global_params +
           o.physics_blocks.join("") + o.interactions + o.aux_variables +
           o.aux_kernels + o.postprocessors + o.times + o.generation_report;
  };
  test.expect(flatten(out) == flatten(out_b),
              "generation is deterministic across repeated calls");

  // 警告通道：多 Step 时控制台+状态栏双通道警告。
  auto multi = input;
  multi.entries.append(entry("Steps", "step_second",
                             {{"type", "Transient"}}));
  const auto warned = gmp::MooseInputGenerator::generate(multi);
  test.expect(!warned.status_warning.isEmpty() &&
                  warned.console_warnings.contains(warned.status_warning),
              "multiple Steps raise the dual-channel warning");

  // 文本工具单元合同：引用规则与 upsert 幂等。
  test.expect(gmp::MooseInputGenerator::quote_moose_value_if_needed("a b") ==
                  "'a b'" &&
                  gmp::MooseInputGenerator::quote_moose_value_if_needed(
                      "'a b'") == "'a b'" &&
                  gmp::MooseInputGenerator::quote_moose_value_if_needed(
                      "abc") == "abc",
              "MOOSE value quoting follows the whitespace rule");
  const QString base_input = "[Mesh]\n[]\n";
  const QString upserted = gmp::MooseInputGenerator::upsert_generated_block(
      base_input, "GlobalParams", "[GlobalParams]\n  displacements = 'disp_x'\n[]\n");
  test.expect(upserted.contains("[GlobalParams]") &&
                  gmp::MooseInputGenerator::upsert_generated_block(
                      upserted, "GlobalParams",
                      "[GlobalParams]\n  displacements = 'disp_x'\n[]\n") ==
                      upserted,
              "upsert appends once and is idempotent for identical blocks");
  test.expect(gmp::MooseInputGenerator::generated_headers_with_prefix(
                  upserted, "Global") == QStringList{"GlobalParams"},
              "generated header scan finds the upserted block");
}

// TASK-V02-031：SnapshotService 编排层合同（无 Widget，临时目录）。
// manifest 等价判据：服务编排产物与直调 export_job_snapshot_v2 的合同
// 字段一致（哈希/角色/相对路径/包内 basename 网格路径），除 created_at
// 与 case_id（时间字段）外不变。
void test_snapshot_service_contract(TestContext& test) {
  QTemporaryDir workspace;
  const QString source_dir = workspace.filePath("svc-source");
  const QByteArray mesh_data("gmsh-mesh-v31\n");
  const QByteArray csv_data("strain,stress\n0,0\n");
  write_file(QDir(source_dir).filePath("mesh/case.msh"), mesh_data);
  write_file(QDir(source_dir).filePath("extra/ch.csv"), csv_data);

  // ---- preflight 闸门 ----
  auto manifest = valid_physical_groups(gmp::sha256_hex(mesh_data));
  manifest.mesh_path = "mesh/case.msh";
  test.expect(gmp::SnapshotService::preflight_error(valid_profile(), manifest)
                  .isEmpty(),
              "preflight passes with a valid profile and manifest");
  gmp::ApplicationProfile no_profile;
  test.expect(gmp::SnapshotService::preflight_error(no_profile, manifest)
                  .contains("application profile"),
              "preflight rejects a missing application profile");
  test.expect(gmp::SnapshotService::preflight_error(
                  valid_profile(), gmp::PhysicalGroupManifest())
                  .contains("Physical group manifest"),
              "preflight rejects an incomplete physical group manifest");

  // ---- normalize_refs ----
  const QString abs_mesh = QDir(source_dir).filePath("mesh/case.msh");
  const QString abs_csv = QDir(source_dir).filePath("extra/ch.csv");
  QString text = "[Mesh]\n  file = '" + abs_mesh + "'\n[]\n"
                 "[Materials]\n  compression_hardening_file = '" + abs_csv +
                 "'\n[]\n";
  QMap<QString, QString> sources;
  QMap<QString, QString> roles;
  QMap<QString, QString> extra = {{"other.csv", "/elsewhere/other.csv"}};
  test.expect(gmp::SnapshotService::normalize_refs(abs_mesh, extra, &text,
                                                   &sources, &roles)
                  .isEmpty() &&
                  !text.contains(abs_mesh) && !text.contains(abs_csv) &&
                  text.contains("case.msh") &&
                  sources.value("case.msh") == abs_mesh &&
                  sources.value("ch.csv") == abs_csv &&
                  sources.value("other.csv") == "/elsewhere/other.csv",
              "normalize rewrites absolute refs to basenames and merges "
              "explicit sources without overriding");
  QString missing_text = "[Mesh]\n  file = '/nonexistent/none.msh'\n[]\n";
  test.expect(gmp::SnapshotService::normalize_refs(QString(), {}, 
                                                   &missing_text, &sources,
                                                   &roles)
                  .contains("does not exist"),
              "normalize rejects references to missing files");
  // 同名冲突：不同目录的同名文件拒绝。
  const QString other_dir = workspace.filePath("other");
  write_file(QDir(other_dir).filePath("case.msh"), "different\n");
  QString conflict_text = "[Mesh]\n  file = '" + abs_mesh + "'\n"
                          "  second_file = '" +
                          QDir(other_dir).filePath("case.msh") + "'\n[]\n";
  test.expect(gmp::SnapshotService::normalize_refs(QString(), {},
                                                   &conflict_text, &sources,
                                                   &roles)
                  .contains("same"),
              "normalize rejects same-basename files from different dirs");

  // ---- export_snapshot 编排 ----
  gmp::SnapshotExportRequest request;
  request.dest_parent = workspace.filePath("exports");
  QDir().mkpath(request.dest_parent);
  request.input_text = text;
  request.input_path_text = "/work/proj/case.i";
  request.mesh_path_text = abs_mesh;
  request.input_mode = "structured";
  request.project_path = workspace.filePath("proj.gmp.yaml");
  write_file(request.project_path, "schema_version: 2\nmodel: {}\n");
  request.profile = valid_profile();
  request.unit_contract = {{"display_to_solver_factors",
                            QVariantMap{{"pressure", 1000000.0}}}};
  request.file_sources = sources;
  request.file_roles = roles;
  manifest.mesh_path = abs_mesh;  // 项目内清单允许绝对路径
  request.physical_groups = manifest;
  request.generator_version = "gmp-ise-test";
  const auto outcome = gmp::SnapshotService::export_snapshot(request);
  test.expect(outcome.ok && outcome.dir_name.startsWith("case-") &&
                  outcome.result.dir ==
                      QDir(request.dest_parent).filePath(outcome.dir_name),
              "service allocates a timestamped case dir and exports");
  const QJsonObject manifest_json = outcome.result.manifest;
  test.expect(manifest_json.value("contract_version").toString() == "2.0.0" &&
                  manifest_json.value("case_id").toString() ==
                      outcome.dir_name &&
                  !manifest_json.value("traceability")
                       .toObject()
                       .value("project_version")
                       .toString()
                       .isEmpty() &&
                  manifest_json.value("unit_contract")
                          .toObject()
                          .value("display_to_solver_factors")
                          .toObject()
                          .value("pressure")
                          .toDouble() == 1000000.0,
              "manifest carries contract v2 fields, project hash and unit "
              "factors");
  const QJsonArray mesh_files = manifest_json.value("input_snapshot")
                                    .toObject()
                                    .value("mesh_files")
                                    .toArray();
  test.expect(mesh_files.size() == 1 &&
                  mesh_files.first().toObject().value("name") == "case.msh" &&
                  mesh_files.first().toObject().value("sha256") ==
                      gmp::sha256_hex(mesh_data),
              "packaged manifest references the mesh by in-package basename "
              "with content hash");
  test.expect(outcome.result.input_sha256 == gmp::sha256_hex(text.toUtf8()),
              "input hash matches the normalized .i content");
  // 版本目录撞名：第二次导出落到 -2 后缀。
  const auto second = gmp::SnapshotService::export_snapshot(request);
  test.expect(second.ok && second.dir_name != outcome.dir_name,
              "repeated export allocates a fresh version directory");
  // manifest 等价：两次导出除 created_at/case_id 外逐字段一致。
  QJsonObject a = manifest_json;
  QJsonObject b = second.result.manifest;
  a.remove("created_at");
  a.remove("case_id");
  b.remove("created_at");
  b.remove("case_id");
  test.expect(QJsonDocument(a) == QJsonDocument(b),
              "service manifests are equivalent apart from time fields");

  // 失败路径：无效清单不产出目录。
  gmp::SnapshotExportRequest bad = request;
  bad.physical_groups = gmp::PhysicalGroupManifest();
  const auto rejected = gmp::SnapshotService::export_snapshot(bad);
  test.expect(!rejected.ok && !rejected.error.isEmpty() &&
                  !QFileInfo::exists(QDir(request.dest_parent)
                                         .filePath(rejected.dir_name)),
              "invalid manifest fails without leaving a snapshot directory");
}

// TASK-V02-040：PhysicalGroupService 无 gmsh 部分合同（JSON 持久化/定义
// 管理/文本解析）。gmsh 会话路径（owner+bbox 重绑定、组校验唯一性）由
// 巡览 assembly_instance_contract 与真实 G1 重开路径覆盖。
void test_physical_group_service_contract(TestContext& test) {
  // G1 项目的 custom_physical_groups_json 样本（fixed_bottom 等四组形态）。
  const QVariantList g1_defs = {
      QVariantMap{{"version", 1},
                  {"name", "fixed_bottom"},
                  {"dim", 2},
                  {"entities",
                   QVariantList{QVariantMap{
                       {"tag", 11},
                       {"owner", "instance_concrete"},
                       {"bbox", QVariantList{-37.24, -22.71, 0.0, 51.25,
                                             24.78, 0.0}}}}}},
      QVariantMap{{"version", 1},
                  {"name", "load_top"},
                  {"dim", 2},
                  {"entities",
                   QVariantList{QVariantMap{
                       {"tag", 6},
                       {"owner", "instance_plate"},
                       {"bbox", QVariantList{-82.79, -77.09, 25.0, 95.90,
                                             75.67, 25.0}}}}}},
  };
  gmp::PhysicalGroupService service;
  service.set_definitions(g1_defs);
  const QString json = service.to_json();
  gmp::PhysicalGroupService restored;
  QString error;
  test.expect(restored.load_json(json, &error) && error.isEmpty() &&
                  restored.definitions() == g1_defs,
              "custom physical group definitions survive the JSON round-trip");
  test.expect(restored.load_json("[]", &error) &&
                  restored.definitions().isEmpty(),
              "empty JSON array loads as zero definitions");
  test.expect(!restored.load_json("{broken", &error) &&
                  error == "Saved custom Physical Groups are invalid and "
                           "were ignored." &&
                  restored.definitions().isEmpty(),
              "invalid JSON resets definitions with the legacy message");
  restored.set_definitions(g1_defs);
  restored.forget("fixed_bottom");
  test.expect(restored.definitions().size() == 1 &&
                  restored.definitions().first().toMap().value("name") ==
                      "load_top",
              "forget removes exactly the named definition");
  restored.forget("missing");
  test.expect(restored.definitions().size() == 1,
              "forget of an unknown name is a no-op");
  restored.clear();
  test.expect(restored.definitions().isEmpty() && restored.to_json() == "[]",
              "clear empties the definition store");

  // 文本解析：dim:tag 与裸 tag 混合、逗号/空白分隔、非法片段跳过。
  const auto tokens =
      gmp::PhysicalGroupService::parse_dim_tag_tokens("3:5, 2:7 11 xx 0:1");
  test.expect(tokens.size() == 4 && tokens[0].has_dim &&
                  tokens[0].dim == 3 && tokens[0].tag == 5 &&
                  tokens[1].dim == 2 && tokens[1].tag == 7 &&
                  !tokens[2].has_dim && tokens[2].tag == 11 &&
                  tokens[3].dim == 0 && tokens[3].tag == 1,
              "dim:tag token parsing matches the panel semantics");
}

// TASK-V02-040：装配/网格服务可无 Widget 调用的边界合同。本测试目标未
// 启用 GMP_ENABLE_GMSH_GUI，断言守护分支；真实 gmsh 会话路径（装配重建、
// 组恢复、清单产出）由巡览 assembly_instance_contract/mesh_manifest_summary
// 与真实 G1 重开路径覆盖。
void test_assembly_mesher_service_contract(TestContext& test) {
  gmp::PhysicalGroupService groups;
  const auto result = gmp::AssemblyGeometryService::build(
      QVariantList{QVariantMap{{"name", "inst"}, {"source_path", "x.brep"}}},
      &groups);
  test.expect(!result.ok && !result.error.isEmpty(),
              "assembly build without gmsh reports a readable error");

  gmp::MeshJobSpec spec;
  bool threw = false;
  QString message;
  try {
    gmp::GmshMesher::prepare(spec, nullptr);
  } catch (const gmp::MeshJobError& ex) {
    threw = true;
    message = ex.message();
  }
  // 守护桩构建（无 gmsh）拒绝“未启用”；真实 gmsh 构建下空模型同样被
  // 预检拒绝——两种形态都必须是可读 MeshJobError。
  test.expect(threw && !message.isEmpty() &&
                  (message.contains("Gmsh is not enabled") ||
                   message.contains("No geometry in the current model")),
              "mesher prepare without a model throws a readable MeshJobError");
}

// TASK-V02-050：视口共享底座纯逻辑合同（相机数学 + 选择状态机）。
void test_viewport_foundation_contract(TestContext& test) {
  // 视角预设：与 VtkViewer 原内联计算逐点一致。
  const double bounds[6] = {0.0, 10.0, 0.0, 20.0, 0.0, 5.0};
  {
    const auto r = gmp::view_preset_camera(bounds, 0);
    test.expect(r.fit, "preset 0 is the fit/reset-camera case");
  }
  {
    const auto r = gmp::view_preset_camera(bounds, 1);  // Front (+X)
    // center=(5,10,2.5)，max_extent=20，dist=50
    test.expect(!r.fit && r.position[0] == 55.0 && r.position[1] == 10.0 &&
                    r.position[2] == 2.5 && r.focal[0] == 5.0 &&
                    r.focal[1] == 10.0 && r.focal[2] == 2.5 &&
                    r.view_up[2] == 1.0,
                "front preset matches the legacy camera math");
  }
  {
    const auto r = gmp::view_preset_camera(bounds, 2);  // Right (+Y)
    test.expect(r.position[0] == 5.0 && r.position[1] == 60.0 &&
                    r.position[2] == 2.5 && r.view_up[2] == 1.0,
                "right preset matches the legacy camera math");
  }
  {
    const auto r = gmp::view_preset_camera(bounds, 3);  // Top (+Z)
    test.expect(r.position[2] == 52.5 && r.view_up[1] == 1.0 &&
                    r.view_up[2] == 0.0,
                "top preset uses the +Y view-up override");
  }
  {
    const auto r = gmp::view_preset_camera(bounds, 4);  // Iso
    test.expect(r.position[0] == 55.0 && r.position[1] == 60.0 &&
                    r.position[2] == 52.5 && r.view_up[2] == 1.0,
                "iso preset matches the legacy camera math");
  }
  // 预览对焦：法线方向 + view-up 规则 + 无效输入。
  {
    const double mesh_bounds[6] = {0.0, 100.0, 0.0, 100.0, 0.0, 100.0};
    const double face[6] = {0.0, 10.0, 0.0, 10.0, 20.0, 20.0};
    const auto r = gmp::preview_focus_camera(face, mesh_bounds, 0.0, 0.0, 1.0);
    // center=(5,5,20)，distance=1.35*sqrt(30000)≈233.8，方向 (0,0,1)
    test.expect(r.valid && r.focal[0] == 5.0 && r.focal[1] == 5.0 &&
                    r.focal[2] == 20.0 &&
                    std::abs(r.position[2] - (20.0 + r.position[2])) >= 0.0 &&
                    r.view_up[1] == 1.0 && r.view_up[2] == 0.0,
                "preview focus along +Z uses the +Y view-up rule");
    test.expect(std::abs(r.position[0] - 5.0) < 1e-9 &&
                    std::abs(r.position[1] - 5.0) < 1e-9 &&
                    r.position[2] > 20.0,
                "preview focus positions the camera along the view direction");
    const auto bad =
        gmp::preview_focus_camera(face, mesh_bounds, 0.0, 0.0, 0.0);
    test.expect(!bad.valid, "zero view direction yields no focus camera");
  }

  // 选择状态机。
  gmp::ViewportSelection sel;
  sel.set_group_filter(2, 7);
  test.expect(sel.group_dim_ == 2 && sel.group_id_ == 7 &&
                  sel.cell_id_ == -1,
              "group filter sets the group and clears the cell");
  sel.set_entity_filter(2, 12);
  sel.set_preview(2, 12);
  sel.set_group_filter(-1, -1);
  test.expect(sel.group_dim_ == -1 && sel.entity_dim_ == -1 &&
                  sel.cell_id_ == -1 && sel.is_previewed(2, 12),
              "clearing the group filter resets entity/cell but keeps preview");
  sel.set_entity_filter(3, 4);
  test.expect(sel.entity_dim_ == 3 && sel.entity_tag_ == 4 &&
                  sel.group_dim_ == -1,
              "entity filter is independent of the cleared group");
  sel.set_preview(-1, -1);
  test.expect(!sel.has_preview() && !sel.is_previewed(2, 12),
              "negative preview clears the overlay state");
  sel.set_group_filter(2, 1);
  sel.set_preview(2, 1);
  sel.clear_all();
  test.expect(sel.group_dim_ == -1 && sel.preview_dim_ == -1,
              "clear_all resets every selection channel");
}

// TASK-V02-061：CRUD 事务命令与旁路审计合同（无 Widget 的存储模型）。
void test_closure_command_audit_contract(TestContext& test) {
  using namespace gmp::core;

  // 模拟对象存储（代替模型树）：验证 ClosureCommand 的 execute/revert 与
  // 审计摘要（label、committed、before/after）。
  QVariantMap store;
  TransactionManager tm;
  test.expect(tm.begin("add Materials/mat_a"), "create transaction begins");
  test.expect(tm.execute(std::make_unique<ClosureCommand>(
                  "create Materials object", QVariantMap(),
                  QVariantMap{{"kind", "Materials"}, {"name", "mat_a"}},
                  [&store]() { store.insert("mat_a", "created"); },
                  [&store]() { store.remove("mat_a"); })),
              "create command executes");
  test.expect(store.contains("mat_a"), "apply ran during execute");
  test.expect(tm.commit(), "create transaction commits");
  test.expect(tm.auditLog().size() == 1 && tm.auditLog().first().committed &&
                  tm.auditLog().first().label == "add Materials/mat_a" &&
                  tm.auditLog().first().commands ==
                      QStringList{"create Materials object"} &&
                  tm.auditLog().first().before.first().isEmpty() &&
                  tm.auditLog().first().after.first().value("name") == "mat_a",
              "create audit record carries label/committed/before/after");

  test.expect(tm.begin("remove Materials/mat_a"), "remove transaction begins");
  test.expect(tm.execute(std::make_unique<ClosureCommand>(
                  "remove Materials object",
                  QVariantMap{{"kind", "Materials"}, {"name", "mat_a"}},
                  QVariantMap(), [&store]() { store.remove("mat_a"); },
                  [&store]() { store.insert("mat_a", "restored"); })),
              "remove command executes");
  test.expect(tm.rollback() && store.value("mat_a") == "restored",
              "rollback restores the store via revert");
  test.expect(tm.auditLog().size() == 2 &&
                  !tm.auditLog().last().committed &&
                  tm.auditLog().last().before.first().value("name") ==
                      "mat_a",
              "rolled-back removal keeps a full audit record");

  // 旁路审计：动作已由调用方完成，只登记 committed 记录。
  tm.record_committed("edit Materials/mat_a", "property form commit",
                      QVariantMap{{"name", "mat_a"}},
                      QVariantMap{{"name", "mat_a_renamed"}});
  test.expect(tm.auditLog().size() == 3 && tm.auditLog().last().committed &&
                  tm.auditLog().last().label == "edit Materials/mat_a" &&
                  tm.auditLog().last().after.first().value("name") ==
                      "mat_a_renamed",
              "record_committed lands a bypass audit record");
}

void test_submission_manifest(TestContext& test) {

  const QByteArray unicode_disposition =
      gmp::SimClient::multipart_file_content_disposition(
          QString::fromUtf8("测试03.i"));
  test.expect(unicode_disposition.contains(
                  QString::fromUtf8("测试03.i").toUtf8()) &&
                  !unicode_disposition.contains("%E6"),
              "multipart filename preserves the manifest Unicode name");

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
  test_physical_group_manifest_contract(test);
  test_snapshot_v2(test);
  test_snapshot_v2_manifest_contract(test);
  test_project_document_contract(test);
  test_property_bag_contract(test);
  test_dependency_graph_contract(test);
  test_transaction_manager_contract(test);
  test_closure_command_audit_contract(test);
  test_unit_display_contract(test);
  test_project_store_contract(test);
  test_moose_input_generator_contract(test);
  test_snapshot_service_contract(test);
  test_physical_group_service_contract(test);
  test_assembly_mesher_service_contract(test);
  test_viewport_foundation_contract(test);
  test_submission_manifest(test);
  if (test.failures == 0) {
    qInfo("Phase 0 contract tests PASSED");
  } else {
    qWarning("Phase 0 contract tests FAILED with %d failures", test.failures);
  }
  return test.failures;
}
