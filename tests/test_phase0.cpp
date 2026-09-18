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
#include "gmp/DependencyGraph.h"
#include "gmp/MooseMappingRegistry.h"
#include "gmp/MooseSnapshot.h"
#include "gmp/PhysicalGroupManifest.h"
#include "gmp/ProjectDocument.h"
#include "gmp/ProjectSchema.h"
#include "gmp/PropertyBag.h"
#include "gmp/SimClient.h"
#include "gmp/SketchDocument.h"
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
// Stage 3 迁移候选（当前依赖 MainWindow/GmshPanel，仅由 GUI 巡览覆盖，
// 抽服务后应迁入本级）：
//   - MainWindow::build_*_block / sync_model_to_input 的 .i 全文本不变量
//     （Materials block、BC boundary、Contact primary/secondary、
//     [Executioner]/[Preconditioning] 单一性、Outputs 套餐、A/B 逐字一致）；
//   - MainWindow::build_generation_report 的全对象可追溯行；
//   - GmshPanel 装配构建 → mesh_manifest 清单产出（owner+bbox 恢复）。
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
  // 损坏输入：缺 id / 未知父 / 未知状态必须拒绝且不破坏既有内容。
  test.expect(!roundtrip_target.from_variant_list(
                  QVariantList{QVariantMap{{"name", "no-id"}}}),
              "entries without a stable id are rejected");
  test.expect(roundtrip_target.count() == 2,
              "rejected load leaves the current document untouched");
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
  test_unit_display_contract(test);
  test_submission_manifest(test);
  if (test.failures == 0) {
    qInfo("Phase 0 contract tests PASSED");
  } else {
    qWarning("Phase 0 contract tests FAILED with %d failures", test.failures);
  }
  return test.failures;
}
