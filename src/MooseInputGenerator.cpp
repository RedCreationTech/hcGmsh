#include "gmp/MooseInputGenerator.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace gmp {

namespace {

using EntryView = QList<const ProjectModelEntry*>;

EntryView entries_of(const QList<ProjectModelEntry>& entries,
                     const QString& kind) {
  EntryView out;
  for (const auto& entry : entries) {
    if (entry.kind == kind) {
      out.append(&entry);
    }
  }
  return out;
}

// W-03d：Outputs 套餐命名空间键（旧通用路径与通用子块输出时跳过）。
const QStringList kOutputsPackageKeys = {
    "field_outputs",          "history_profile",
    "hist_reaction_force",
    "hist_displacement_avg",  "hist_extremum",
    "hist_boundary",          "hist_disp_variable",
    "hist_extremum_variables", "hist_extremum_types",
    "times_enabled",          "times_name",
    "times_start",            "times_end",
    "times_interval",         "output_exodus",
    "output_csv",             "file_base"};

QString build_block_from_root(const EntryView& items, const QString& block_name,
                              const QString& default_type,
                              const QStringList& skip_keys) {
  if (items.isEmpty()) {
    return QString();
  }
  QString out;
  out += QString("[%1]\n").arg(block_name);
  for (const auto* child : items) {
    const QString name = child->name;
    out += QString("  [%1]\n").arg(name);
    const QVariantMap params = child->params;
    QString type = params.value("type").toString();
    if (type.isEmpty()) {
      type = default_type;
    }
    if (!type.isEmpty()) {
      out += QString("    type = %1\n").arg(type);
    }
    for (auto it = params.begin(); it != params.end(); ++it) {
      if (it.key() == "type") {
        continue;
      }
      if (skip_keys.contains(it.key())) {
        continue;
      }
      out += QString("    %1 = %2\n")
                 .arg(it.key())
                 .arg(it.value().toString());
    }
    out += "  []\n";
  }
  out += "[]\n";
  return out;
}

QString build_functions_block(const EntryView& items) {
  if (items.isEmpty()) {
    return QString();
  }
  QString out;
  out += "[Functions]\n";
  for (const auto* child : items) {
    const QString name = child->name;
    out += QString("  [%1]\n").arg(name);
    const QVariantMap params = child->params;
    QString type = params.value("type").toString();
    if (type.isEmpty()) {
      type = "ParsedFunction";
    }
    out += QString("    type = %1\n").arg(type);
    const bool piecewise = (type == "PiecewiseLinear");
    for (auto it = params.begin(); it != params.end(); ++it) {
      if (it.key() == "type") {
        continue;
      }
      // 函数类型切换后项目数据中可能保留另一类型的字段。表单虽会隐藏
      // 不适用字段，生成器仍需按当前 type 过滤，避免 PiecewiseLinear
      // 输出无效 expression，或 ParsedFunction 输出无效 x/y。
      if ((piecewise && it.key() == "expression") ||
          (!piecewise && (it.key() == "x" || it.key() == "y"))) {
        continue;
      }
      QString value = it.value().toString();
      if (piecewise && (it.key() == "x" || it.key() == "y")) {
        // v01 复载曲线写法：x/y 数据对始终单引号包裹。
        const QString trimmed = value.trimmed();
        if (!trimmed.startsWith('\'') && !trimmed.startsWith('"')) {
          value = "'" + trimmed + "'";
        }
      }
      out += QString("    %1 = %2\n").arg(it.key()).arg(value);
    }
    out += "  []\n";
  }
  out += "[]\n";
  return out;
}

QString build_variables_block(const EntryView& items) {
  if (items.isEmpty()) {
    return QString();
  }
  QString out;
  out += "[Variables]\n";
  for (const auto* child : items) {
    const QString name = child->name;
    out += QString("  [%1]\n").arg(name);
    const QVariantMap params = child->params;
    const QString order = params.value("order", "FIRST").toString();
    const QString family = params.value("family", "LAGRANGE").toString();
    out += QString("    order = %1\n").arg(order);
    out += QString("    family = %1\n").arg(family);
    for (auto it = params.begin(); it != params.end(); ++it) {
      if (it.key() == "order" || it.key() == "family" ||
          it.key() == "type") {
        continue;
      }
      out += QString("    %1 = %2\n")
                 .arg(it.key())
                 .arg(it.value().toString());
    }
    out += "  []\n";
  }
  out += "[]\n";
  return out;
}

QString build_bcs_block(const EntryView& items,
                        const EntryView& load_items) {
  bool has_pressure = false;
  for (const auto* child : load_items) {
    has_pressure = has_pressure ||
                   child->params.value("type").toString() == "Pressure";
  }
  if (items.isEmpty() && !has_pressure) {
    return QString();
  }
  QString out;
  out += "[BCs]\n";
  auto append_bc = [&out](const ProjectModelEntry* child, bool pressure) {
    const QString name = child->name;
    out += QString("  [%1]\n").arg(name);
    const QVariantMap params = child->params;
    QString type = params.value("type").toString();
    if (type.isEmpty()) {
      type = pressure ? "Pressure" : "DirichletBC";
    }
    out += QString("    type = %1\n").arg(type);
    for (auto it = params.begin(); it != params.end(); ++it) {
      if (it.key() == "type") {
        continue;
      }
      // W-03c：type 切换后 params 中可能滞留互斥旧键，按类型过滤。
      if (type == "FunctionDirichletBC" && it.key() == "value") {
        continue;
      }
      if (type == "DirichletBC" && it.key() == "function") {
        continue;
      }
      if (type == "Pressure" &&
          !QStringList{"variable", "boundary", "factor", "function",
                       "component", "use_displaced_mesh"}
               .contains(it.key())) {
        continue;
      }
      if (type == "Pressure" && it.value().toString().trimmed().isEmpty()) {
        continue;
      }
      out += QString("    %1 = %2\n")
                 .arg(it.key())
                 .arg(MooseInputGenerator::quote_moose_value_if_needed(
                     it.value().toString()));
    }
    out += "  []\n";
  };
  for (const auto* child : items) {
    append_bc(child, false);
  }
  for (const auto* child : load_items) {
    if (child->params.value("type").toString() == "Pressure") {
      append_bc(child, true);
    }
  }
  out += "[]\n";
  return out;
}

QString build_loads_block(const EntryView& items) {
  QString out;
  for (const auto* child : items) {
    if (child->params.value("type").toString() == "Pressure") {
      continue;
    }
    if (out.isEmpty()) {
      out = "[Kernels]\n";
    }
    const QVariantMap params = child->params;
    out += QString("  [%1]\n").arg(child->name);
    out += QString("    type = %1\n")
               .arg(params.value("type", "BodyForce").toString());
    for (auto it = params.begin(); it != params.end(); ++it) {
      if (it.key() == "type" || it.key() == "section" ||
          it.value().toString().trimmed().isEmpty()) {
        continue;
      }
      out += QString("    %1 = %2\n")
                 .arg(it.key(),
                      MooseInputGenerator::quote_moose_value_if_needed(
                          it.value().toString()));
    }
    out += "  []\n";
  }
  if (!out.isEmpty()) {
    out += "[]\n";
  }
  return out;
}

QString build_interactions_block(const EntryView& items) {
  if (items.isEmpty()) {
    return QString();
  }
  QString out;
  const QStringList ordered_keys = {
      "primary",          "secondary",        "model",
      "formulation",      "friction_coefficient",
      "normal_smoothing_distance", "tangential_tolerance",
      "penalty",          "normalize_penalty"};
  for (const auto* child : items) {
    const QVariantMap params = child->params;
    if (params.value("type").toString() != "Contact") {
      continue;
    }
    if (out.isEmpty()) {
      out = "[Contact]\n";
    }
    out += QString("  [%1]\n").arg(child->name);
    for (const auto& key : ordered_keys) {
      const QString value = params.value(key).toString().trimmed();
      if (value.isEmpty() ||
          (key == "friction_coefficient" &&
           params.value("model").toString() != "coulomb")) {
        continue;
      }
      out += QString("    %1 = %2\n")
                 .arg(key,
                      MooseInputGenerator::quote_moose_value_if_needed(value));
    }
    out += "  []\n";
  }
  if (!out.isEmpty()) {
    out += "[]\n";
  }
  return out;
}

// W-01b：Section 指派语义。在 Sections 条目中查找 material==material_name
// 的子项，取其 block 列表（空格分隔）的第一个体组名；多组指派时经
// warnings 提示（由调用方决定呈现）。无指派返回空串。
QString resolve_assigned_block_in(const EntryView& section_items,
                                  const QString& material_name,
                                  QStringList* warnings) {
  const QString target = material_name.trimmed();
  if (target.isEmpty()) {
    return QString();
  }
  QStringList assigned;
  for (const auto* child : section_items) {
    const QVariantMap params = child->params;
    if (params.value("material").toString().trimmed() != target) {
      continue;
    }
    const QStringList groups =
        params.value("block")
            .toString()
            .split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (const auto& group : groups) {
      if (!assigned.contains(group)) {
        assigned << group;
      }
    }
  }
  if (assigned.size() > 1 && warnings) {
    warnings->append(
        QString("Warning: material '%1' is assigned to multiple physical "
                "volumes (%2); using '%3'.")
            .arg(target, assigned.join(", "), assigned.first()));
  }
  return assigned.isEmpty() ? QString() : assigned.first();
}

QString build_materials_block(const EntryView& items,
                              const EntryView& section_items,
                              QStringList* warnings) {
  if (items.isEmpty()) {
    return QString();
  }
  bool has_cdp = false;
  for (const auto* child : items) {
    if (child->params.value("type").toString() == "AbaqusCDP") {
      has_cdp = true;
      break;
    }
  }
  if (!has_cdp) {
    // 无 CDP 子项时完全沿用原通用生成路径（demo 流程不受影响）。
    return build_block_from_root(items, "Materials", "GenericConstantMaterial",
                                 {});
  }

  // W-03a：type=AbaqusCDP 子项生成 v01 式三对象；其余子项保持通用生成。
  const QStringList cdp_scalars = {"maximum_substeps",
                                   "maximum_strain_increment",
                                   "enable_performance_diagnostics",
                                   "youngs_modulus",
                                   "poissons_ratio",
                                   "dilation_angle",
                                   "eccentricity",
                                   "biaxial_to_uniaxial_compression_ratio",
                                   "tensile_meridian_ratio",
                                   "viscosity",
                                   "tension_recovery",
                                   "compression_recovery"};
  const QStringList cdp_files = {"compression_hardening_file",
                                 "compression_damage_file",
                                 "tension_stiffening_file",
                                 "tension_damage_file"};
  const QStringList skip_keys = {
      "type", "block", "section", "status", "state", "unit_factor_stress",
      // 从其他材料类型切换到 CDP 的旧项目可能仍保存这些字段；它们都不
      // 属于 AbaqusCDPStressUpdate，生成时必须过滤。
      "prop_names", "prop_values", "expression", "property_name",
      "coupled_variables", "fill_method", "C_ijkl",
      "thermal_expansion_coeff", "temperature", "stress_free_temperature",
      "eigenstrain_name", "displacements"};
  QString out;
  out += "[Materials]\n";
  for (const auto* child : items) {
    const QString name = child->name;
    const QVariantMap params = child->params;
    const QString type = params.value("type").toString();
    if (type != "AbaqusCDP") {
      out += QString("  [%1]\n").arg(name);
      out += QString("    type = %1\n")
                 .arg(type.isEmpty() ? QString("GenericConstantMaterial")
                                     : type);
      for (auto it = params.begin(); it != params.end(); ++it) {
        if (it.key() == "type") {
          continue;
        }
        out += QString("    %1 = %2\n")
                   .arg(it.key())
                   .arg(it.value().toString());
      }
      out += "  []\n";
      continue;
    }

    // block 取子项的 block/section 指派参数；均无则查 Sections 根的
    // 材料↔体组指派（W-01b Section 指派语义）；再无则留空字符串并警告。
    QString block = params.value("block").toString().trimmed();
    if (block.isEmpty()) {
      block = params.value("section").toString().trimmed();
    }
    if (block.isEmpty()) {
      block = resolve_assigned_block_in(section_items, name, warnings);
    }
    if (block.isEmpty() && warnings) {
      warnings->append(
          QString("Warning: CDP material '%1' has no block/section "
                  "assignment; emitting an empty block parameter (assign a "
                  "section/physical volume before running).")
              .arg(name));
    }
    const QString stress_update = name + "_cdp_stress_update";
    out += QString("  [%1_elasticity]\n").arg(name);
    out += "    type = ComputeIsotropicElasticityTensor\n";
    out += QString("    block = '%1'\n").arg(block);
    out += QString("    youngs_modulus = %1\n")
               .arg(params.value("youngs_modulus").toString());
    out += QString("    poissons_ratio = %1\n")
               .arg(params.value("poissons_ratio").toString());
    out += "  []\n";
    out += QString("  [%1_stress]\n").arg(name);
    out += "    type = ComputeMultipleInelasticStress\n";
    out += QString("    block = '%1'\n").arg(block);
    out += QString("    inelastic_models = %1\n").arg(stress_update);
    out += "    perform_finite_strain_rotations = false\n";
    out += "  []\n";
    out += QString("  [%1]\n").arg(stress_update);
    out += "    type = AbaqusCDPStressUpdate\n";
    out += QString("    block = '%1'\n").arg(block);
    for (const auto& key : cdp_scalars) {
      const QString value = params.value(key).toString();
      if (!value.isEmpty()) {
        out += QString("    %1 = %2\n").arg(key).arg(value);
      }
    }
    for (const auto& key : cdp_files) {
      const QString value = params.value(key).toString().trimmed();
      if (!value.isEmpty()) {
        // CSV 以 basename 相对引用；绝对来源经 file_sources 通道打包。
        out += QString("    %1 = %2\n")
                   .arg(key)
                   .arg(QFileInfo(value).fileName());
      }
    }
    // 透传其余非空自定义键（高级参数），跳过表单/元数据键。
    const QStringList consumed = cdp_scalars + cdp_files + skip_keys;
    for (auto it = params.begin(); it != params.end(); ++it) {
      if (consumed.contains(it.key())) {
        continue;
      }
      const QString value = it.value().toString();
      if (value.isEmpty()) {
        continue;
      }
      out += QString("    %1 = %2\n").arg(it.key()).arg(value);
    }
    out += "  []\n";
  }
  out += "[]\n";
  return out;
}

QString build_executioner_block(const EntryView& items, bool chinese_ui,
                                QStringList* warnings,
                                QString* status_warning) {
  if (items.isEmpty()) {
    return QString();
  }
  const auto* step = items.first();
  const QVariantMap params = step->params;
  QString type = params.value("type").toString();
  if (type.isEmpty()) {
    type = "Transient";
  }

  // W-03e：v01 口径键分组。timestepper_*/preconditioning_* 为表单命名
  // 空间键，分别落入 [TimeStepper] 子块与 [Preconditioning/smp] 块。
  const QStringList timestepper_keys = {"timestepper_type", "optimal_iterations",
                                        "iteration_window", "growth_factor",
                                        "cutback_factor"};
  const QStringList preconditioning_keys = {"preconditioning_type",
                                            "preconditioning_full"};
  // Executioner 级有序输出（v01 验收基线顺序）。
  const QStringList ordered_keys = {"start_time",      "end_time",
                                    "solve_type",      "line_search",
                                    "automatic_scaling",
                                    "nl_rel_tol",      "nl_abs_tol",
                                    "nl_max_its",      "num_steps",
                                    "dtmin",           "dtmax",
                                    "petsc_options_iname",
                                    "petsc_options_value"};
  const bool has_timestepper =
      !params.value("timestepper_type").toString().trimmed().isEmpty();

  QStringList consumed = QStringList{"type", "status", "state"} +
                         timestepper_keys + preconditioning_keys + ordered_keys;
  if (has_timestepper) {
    // dt 在 v01 中位于 [TimeStepper] 子块；无 timestepper_type 时（demo
    // 旧数据）dt 保持 Executioner 级平铺。
    consumed << "dt";
  }

  QString out;
  out += "[Executioner]\n";
  out += QString("  type = %1\n").arg(type);
  for (const auto& key : ordered_keys) {
    const QString value = params.value(key).toString().trimmed();
    if (!value.isEmpty()) {
      out += QString("  %1 = %2\n").arg(key).arg(
          MooseInputGenerator::quote_moose_value_if_needed(value));
    }
  }
  // 透传其余键（demo 旧键 dt/scheme/l_max_its/l_tol 等），保持旧通用行为。
  for (auto it = params.begin(); it != params.end(); ++it) {
    if (consumed.contains(it.key())) {
      continue;
    }
    out += QString("  %1 = %2\n")
               .arg(it.key())
               .arg(MooseInputGenerator::quote_moose_value_if_needed(
                   it.value().toString()));
  }
  if (has_timestepper) {
    out += "  [TimeStepper]\n";
    out += QString("    type = %1\n")
               .arg(params.value("timestepper_type").toString().trimmed());
    const QStringList ts_keys = {"dt", "optimal_iterations", "iteration_window",
                                 "growth_factor", "cutback_factor"};
    for (const auto& key : ts_keys) {
      const QString value = params.value(key).toString().trimmed();
      if (!value.isEmpty()) {
        out += QString("    %1 = %2\n").arg(key).arg(value);
      }
    }
    out += "  []\n";
  }
  out += "[]\n";
  const QString preconditioning_type =
      params.value("preconditioning_type").toString().trimmed();
  if (!preconditioning_type.isEmpty()) {
    // v01 口径：[Preconditioning/smp] 独立块（type=SMP full=true）。
    out += "\n[Preconditioning/smp]\n";
    out += QString("  type = %1\n").arg(preconditioning_type);
    const QString full =
        params.value("preconditioning_full").toString().trimmed();
    if (!full.isEmpty()) {
      out += QString("  full = %1\n").arg(full);
    }
    out += "[]\n";
  }
  if (items.size() > 1) {
    // v01 口径：多 Step 不支持串联执行，明示而非静默取第一个。
    const QString warning =
        chinese_ui ? QString::fromUtf8(
                         "警告：检测到多个 Step；不支持串联执行，仅取第一个 "
                         "Step 生成 [Executioner]。")
                   : QString("Warning: multiple Steps found; chained execution "
                             "is not supported, only the first Step is used for "
                             "[Executioner].");
    if (warnings) {
      warnings->append(warning);
    }
    if (status_warning) {
      *status_warning = warning;
    }
  }
  return out;
}

QString build_physics_action_block(const ProjectModelEntry* child,
                                   QString* header, QStringList* warnings) {
  if (!child) {
    return QString();
  }
  const QVariantMap params = child->params;
  QString action = params.value("action").toString().trimmed();
  if (action.isEmpty()) {
    action = "QuasiStatic";
  }
  const QString name = child->name;
  const QString block_header =
      QString("Physics/SolidMechanics/%1/%2").arg(action, name);
  if (header) {
    *header = block_header;
  }
  const QString block = params.value("block").toString().trimmed();
  if (block.isEmpty() && warnings) {
    warnings->append(
        QString("Warning: Physics action '%1' has no block assignment; "
                "emitting an empty block parameter (assign a section/"
                "physical volume before running).")
            .arg(name));
  }
  const bool save_in_resid =
      params.value("save_in_resid").toString().trimmed() == "true";
  QString out;
  out += QString("[%1]\n").arg(block_header);
  // v01 验收基线顺序：volumetric_locking_correction / add_variables /
  // incremental / block / strain / generate_output / save_in。
  for (const auto& key : {"volumetric_locking_correction", "add_variables",
                          "incremental"}) {
    const QString value = params.value(key).toString().trimmed();
    if (!value.isEmpty()) {
      out += QString("  %1 = %2\n").arg(QString::fromLatin1(key), value);
    }
  }
  out += QString("  block = %1\n").arg(
      MooseInputGenerator::quote_moose_value_if_needed(block));
  const QString strain = params.value("strain").toString().trimmed();
  if (!strain.isEmpty()) {
    out += QString("  strain = %1\n").arg(strain);
  }
  const QString generate_output =
      params.value("generate_output").toString().trimmed();
  if (!generate_output.isEmpty()) {
    // v01 多行折行风格简化为单行（语义等价）。
    out += QString("  generate_output = '%1'\n").arg(generate_output);
  }
  if (save_in_resid) {
    out += "  save_in = 'resid_x resid_y resid_z'\n";
  }
  // 透传其余非空自定义键（高级参数），跳过表单/元数据键。
  const QStringList consumed = {"action",
                                "block",
                                "volumetric_locking_correction",
                                "add_variables",
                                "incremental",
                                "strain",
                                "generate_output",
                                "save_in_resid",
                                "status",
                                "state"};
  for (auto it = params.begin(); it != params.end(); ++it) {
    if (consumed.contains(it.key())) {
      continue;
    }
    const QString value = it.value().toString().trimmed();
    if (value.isEmpty()) {
      continue;
    }
    out += QString("  %1 = %2\n").arg(it.key(), value);
  }
  out += "[]\n";
  return out;
}

bool physics_save_in_resid(const EntryView& physics_items) {
  for (const auto* child : physics_items) {
    if (child->params.value("save_in_resid").toString().trimmed() == "true") {
      return true;
    }
  }
  return false;
}

QString physics_block_group(const EntryView& physics_items,
                            const EntryView& material_items,
                            const EntryView& section_items,
                            QStringList* warnings) {
  // AuxKernels 的 block：优先取第一个 Physics 子项的 block 参数；
  // 无 Physics 子项时回退到第一个 CDP 材料的 Section 指派体组。
  for (const auto* child : physics_items) {
    const QString block =
        child->params.value("block").toString().trimmed();
    if (!block.isEmpty()) {
      return block.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)
          .value(0);
    }
  }
  for (const auto* child : material_items) {
    if (child->params.value("type").toString() == "AbaqusCDP") {
      const QString block =
          resolve_assigned_block_in(section_items, child->name, warnings);
      if (!block.isEmpty()) {
        return block;
      }
    }
  }
  return QString();
}

QVariantMap outputs_package_config(const EntryView& output_items) {
  // W-03d：合并 Outputs 根各套餐子项的勾选项（带 field_outputs 键的视为
  // 套餐子项；demo 旧式子项无该键不参与）。布尔取或、列表去重合并、
  // 标量取第一个非空。
  QVariantMap cfg;
  QStringList field_vars;
  QStringList extremum_vars;
  QStringList extremum_types;
  QString history_profile;
  bool hist_reaction = false;
  bool hist_disp_avg = false;
  bool hist_extremum = false;
  QString hist_boundary;
  QString disp_variable;
  bool times_enabled = false;
  QString times_name;
  QString times_start;
  QString times_end;
  QString times_interval;
  bool exodus_on = false;
  bool csv_on = false;
  bool any_exodus_key = false;
  bool any_csv_key = false;
  QString file_base;
  auto split_list = [](const QString& raw) {
    return raw.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
  };
  for (const auto* child : output_items) {
    const QVariantMap params = child->params;
    if (!params.contains("field_outputs")) {
      continue;
    }
    for (const auto& var : split_list(params.value("field_outputs")
                                          .toString())) {
      if (!field_vars.contains(var)) {
        field_vars << var;
      }
    }
    const auto enabled = [&params](const QString& key) {
      return params.value(key).toString().trimmed() == "true";
    };
    const QString candidate_profile =
        params.value("history_profile").toString().trimmed();
    if (!candidate_profile.isEmpty() && candidate_profile != "custom") {
      history_profile = candidate_profile;
    } else if (history_profile.isEmpty()) {
      history_profile = candidate_profile;
    }
    hist_reaction = hist_reaction || enabled("hist_reaction_force");
    hist_disp_avg = hist_disp_avg || enabled("hist_displacement_avg");
    hist_extremum = hist_extremum || enabled("hist_extremum");
    if (hist_boundary.isEmpty()) {
      hist_boundary = params.value("hist_boundary").toString().trimmed();
    }
    if (disp_variable.isEmpty()) {
      disp_variable = params.value("hist_disp_variable").toString().trimmed();
    }
    for (const auto& var : split_list(
             params.value("hist_extremum_variables").toString())) {
      if (!extremum_vars.contains(var)) {
        extremum_vars << var;
      }
    }
    if (extremum_types.isEmpty()) {
      extremum_types = split_list(
          params.value("hist_extremum_types").toString());
    }
    times_enabled = times_enabled || enabled("times_enabled");
    if (times_name.isEmpty()) {
      times_name = params.value("times_name").toString().trimmed();
    }
    if (times_start.isEmpty()) {
      times_start = params.value("times_start").toString().trimmed();
    }
    if (times_end.isEmpty()) {
      times_end = params.value("times_end").toString().trimmed();
    }
    if (times_interval.isEmpty()) {
      times_interval = params.value("times_interval").toString().trimmed();
    }
    if (params.contains("output_exodus")) {
      any_exodus_key = true;
      exodus_on = exodus_on || enabled("output_exodus");
    }
    if (params.contains("output_csv")) {
      any_csv_key = true;
      csv_on = csv_on || enabled("output_csv");
    }
    if (file_base.isEmpty()) {
      file_base = params.value("file_base").toString().trimmed();
    }
  }
  // 旧式子项（无套餐键）被勾选套餐时缺省补 Exodus（v01 落盘语义）。
  if (!any_exodus_key) {
    exodus_on = true;
  }
  const bool package_active = !field_vars.isEmpty() ||
                              history_profile == "cdp_uniaxial_z" ||
                              hist_reaction ||
                              hist_disp_avg || hist_extremum || times_enabled;
  cfg.insert("field_outputs", field_vars);
  cfg.insert("history_profile", history_profile);
  cfg.insert("hist_reaction_force", hist_reaction);
  cfg.insert("hist_displacement_avg", hist_disp_avg);
  cfg.insert("hist_extremum", hist_extremum);
  cfg.insert("hist_boundary", hist_boundary);
  cfg.insert("hist_disp_variable",
             disp_variable.isEmpty() ? QString("disp_z") : disp_variable);
  cfg.insert("hist_extremum_variables", extremum_vars);
  cfg.insert("hist_extremum_types",
             extremum_types.isEmpty() ? QStringList{"min", "max"}
                                      : extremum_types);
  cfg.insert("times_enabled", times_enabled);
  cfg.insert("times_name", times_name.isEmpty()
                               ? QString("field_output_times")
                               : times_name);
  cfg.insert("times_start", times_start.isEmpty() ? QString("0") : times_start);
  cfg.insert("times_end", times_end.isEmpty() ? QString("1") : times_end);
  cfg.insert("times_interval",
             times_interval.isEmpty() ? QString("0.01") : times_interval);
  cfg.insert("output_exodus", exodus_on);
  cfg.insert("output_csv", csv_on);
  cfg.insert("file_base", file_base);
  cfg.insert("package_active", package_active);
  return cfg;
}

QString build_outputs_block(const EntryView& items,
                            const QVariantMap& cfg) {
  if (items.isEmpty()) {
    return QString();
  }
  if (!cfg.value("package_active").toBool()) {
    // 未勾任何套餐：保持旧行为（demo 流程不受影响）。
    return build_block_from_root(items, "Outputs", "Exodus",
                                 kOutputsPackageKeys);
  }
  QString out;
  out += "[Outputs]\n";
  // 旧式子项（无套餐键）按通用子块输出，保留既有语义。
  for (const auto* child : items) {
    const QVariantMap params = child->params;
    if (params.contains("field_outputs")) {
      continue;
    }
    out += QString("  [%1]\n").arg(child->name);
    QString type = params.value("type").toString();
    if (type.isEmpty()) {
      type = "Exodus";
    }
    out += QString("    type = %1\n").arg(type);
    for (auto it = params.begin(); it != params.end(); ++it) {
      if (it.key() == "type" || kOutputsPackageKeys.contains(it.key())) {
        continue;
      }
      out += QString("    %1 = %2\n").arg(it.key(), it.value().toString());
    }
    out += "  []\n";
  }
  const bool times = cfg.value("times_enabled").toBool();
  const QString times_name = cfg.value("times_name").toString();
  const QString file_base = cfg.value("file_base").toString();
  bool exodus_on = cfg.value("output_exodus").toBool();
  const bool csv_on = cfg.value("output_csv").toBool();
  if (!exodus_on && !csv_on) {
    exodus_on = true;  // 兜底：勾选套餐后至少保留一路落盘。
  }
  auto emit_output_subblock = [&](const QString& name, const QString& type) {
    out += QString("  [%1]\n").arg(name);
    out += QString("    type = %1\n").arg(type);
    out += "    execute_on = 'initial timestep_end'\n";
    if (times) {
      out += QString("    sync_times_object = %1\n").arg(times_name);
      out += "    sync_only = true\n";
    }
    if (!file_base.isEmpty()) {
      out += QString("    file_base = %1\n").arg(file_base);
    }
    out += "  []\n";
  };
  if (exodus_on) {
    emit_output_subblock("field_exodus", "Exodus");
  }
  if (csv_on) {
    emit_output_subblock("history_csv", "CSV");
  }
  out += "[]\n";
  return out;
}

QString build_aux_variables_block(const QVariantMap& cfg,
                                  const EntryView& physics_items) {
  const QStringList field_vars = cfg.value("field_outputs").toStringList();
  // resid_*：Physics save_in_resid=true 或勾选反力历史输出时生成
  // （普通变量，非 MONOMIAL）。
  const bool resid =
      physics_save_in_resid(physics_items) ||
      cfg.value("hist_reaction_force").toBool();
  if (!resid && field_vars.isEmpty()) {
    return QString();
  }
  QString out;
  out += "[AuxVariables]\n";
  if (resid) {
    for (const auto& axis : {"x", "y", "z"}) {
      out += QString("  [resid_%1]\n").arg(QLatin1String(axis));
      out += "  []\n";
    }
  }
  for (const auto& var : field_vars) {
    out += QString("  [%1]\n").arg(var);
    out += "    order = CONSTANT\n";
    out += "    family = MONOMIAL\n";
    out += "  []\n";
  }
  out += "[]\n";
  return out;
}

QString build_aux_kernels_block(const QVariantMap& cfg,
                                const EntryView& physics_items,
                                const EntryView& material_items,
                                const EntryView& section_items,
                                QStringList* warnings) {
  const QStringList field_vars = cfg.value("field_outputs").toStringList();
  if (field_vars.isEmpty()) {
    return QString();
  }
  const QString block =
      physics_block_group(physics_items, material_items, section_items,
                          warnings);
  if (block.isEmpty() && warnings) {
    warnings->append(
        "Warning: field output AuxKernels have no block (no Physics block "
        "or CDP section assignment); emitting an empty block parameter.");
  }
  QString out;
  out += "[AuxKernels]\n";
  for (const auto& var : field_vars) {
    // cdp_* 命名约定：DamageC/DamageT 同名，其余 cdp_<名>。
    const QString property = (var == "DamageC" || var == "DamageT")
                                 ? var
                                 : QString("cdp_%1").arg(var);
    out += QString("  [%1]\n").arg(var);
    out += "    type = MaterialRealAux\n";
    out += QString("    variable = %1\n").arg(var);
    out += QString("    property = %1\n").arg(property);
    out += QString("    block = '%1'\n").arg(block);
    out += "    execute_on = 'initial timestep_end'\n";
    out += "  []\n";
  }
  out += "[]\n";
  return out;
}

QString build_postprocessors_block(const QVariantMap& cfg,
                                   QStringList* warnings) {
  if (cfg.value("history_profile").toString() == "cdp_uniaxial_z") {
    return QString::fromUtf8(R"([Postprocessors]
  [min_stress_zz]
    type = ElementExtremeValue
    variable = stress_zz
    value_type = min
  []
  [RP1_Force]
    type = NodalSum
    variable = resid_z
    boundary = top
  []
  [Bottom_Force]
    type = NodalSum
    variable = resid_z
    boundary = bottom
  []
  [Top_Force_X]
    type = NodalSum
    variable = resid_x
    boundary = top
  []
  [Top_Force_Y]
    type = NodalSum
    variable = resid_y
    boundary = top
  []
  [RP1_Displacement]
    type = AverageNodalVariableValue
    variable = disp_z
    boundary = top
  []
  [max_damagec]
    type = ElementExtremeValue
    variable = DamageC
    value_type = max
  []
  [max_damaget]
    type = ElementExtremeValue
    variable = DamageT
    value_type = max
  []
  [max_mises]
    type = ElementExtremeValue
    variable = vonmises_stress
    value_type = max
  []
  [max_stress_zz]
    type = ElementExtremeValue
    variable = stress_zz
    value_type = max
  []
  [max_local_iterations]
    type = ElementExtremeValue
    variable = local_iterations
    value_type = max
  []
  [max_accepted_substeps]
    type = ElementExtremeValue
    variable = accepted_substeps
    value_type = max
  []
  [max_jacobian_fallbacks]
    type = ElementExtremeValue
    variable = jacobian_fallbacks
    value_type = max
  []
[]
)");
  }
  const bool hist_reaction = cfg.value("hist_reaction_force").toBool();
  const bool hist_disp_avg = cfg.value("hist_displacement_avg").toBool();
  const bool hist_extremum = cfg.value("hist_extremum").toBool();
  if (!hist_reaction && !hist_disp_avg && !hist_extremum) {
    return QString();
  }
  const QString boundary = cfg.value("hist_boundary").toString();
  if ((hist_reaction || hist_disp_avg) && boundary.isEmpty() && warnings) {
    warnings->append(
        "Warning: history output package (reaction force / displacement "
        "average) needs a boundary; skipped the boundary-based "
        "postprocessors.");
  }
  QString out;
  out += "[Postprocessors]\n";
  if (hist_reaction && !boundary.isEmpty()) {
    for (const auto& axis : {"x", "y", "z"}) {
      out += QString("  [%1_reaction_%2]\n").arg(boundary, QLatin1String(axis));
      out += "    type = NodalSum\n";
      out += QString("    variable = resid_%1\n").arg(QLatin1String(axis));
      out += QString("    boundary = %1\n").arg(boundary);
      out += "  []\n";
    }
  }
  if (hist_disp_avg && !boundary.isEmpty()) {
    out += QString("  [%1_disp_avg]\n").arg(boundary);
    out += "    type = AverageNodalVariableValue\n";
    out += QString("    variable = %1\n")
               .arg(cfg.value("hist_disp_variable").toString());
    out += QString("    boundary = %1\n").arg(boundary);
    out += "  []\n";
  }
  if (hist_extremum) {
    const QStringList vars =
        cfg.value("hist_extremum_variables").toStringList();
    const QStringList types = cfg.value("hist_extremum_types").toStringList();
    for (const auto& var : vars) {
      for (const auto& value_type : types) {
        out += QString("  [%1_%2]\n").arg(value_type, var.toLower());
        out += "    type = ElementExtremeValue\n";
        out += QString("    variable = %1\n").arg(var);
        out += QString("    value_type = %1\n").arg(value_type);
        out += "  []\n";
      }
    }
  }
  out += "[]\n";
  return out;
}

QString build_times_block(const QVariantMap& cfg, QString* header) {
  if (!cfg.value("times_enabled").toBool()) {
    return QString();
  }
  const QString name = cfg.value("times_name").toString();
  if (header) {
    *header = QString("Times/%1").arg(name);
  }
  QString out;
  out += QString("[Times/%1]\n").arg(name);
  out += "  type = TimeIntervalTimes\n";
  out += QString("  start_time = %1\n").arg(cfg.value("times_start").toString());
  out += QString("  end_time = %1\n").arg(cfg.value("times_end").toString());
  out += QString("  time_interval = %1\n")
             .arg(cfg.value("times_interval").toString());
  out += "[]\n";
  return out;
}

}  // namespace

QString MooseInputGenerator::resolve_assigned_block(
    const QList<ProjectModelEntry>& entries, const QString& material_name,
    QStringList* warnings) {
  return resolve_assigned_block_in(entries_of(entries, "Sections"),
                                   material_name, warnings);
}

namespace {

QString build_generation_report(const MooseInputGenerator::Input& input) {
  QStringList lines;
  lines << "GMP-ISE Model Tree Generation Report";
  lines << QString("Application profile: %1")
               .arg(input.application_profile_id.isEmpty()
                        ? QString("(not selected)")
                        : input.application_profile_id);
  lines << QString("Mapping registry: %1")
               .arg(input.mapping_registry_loaded
                        ? input.mapping_registry_version
                        : QString("(not loaded)"));
  lines << QString("Input mode: %1")
               .arg(input.input_mode.isEmpty() ? QString("structured")
                                               : input.input_mode);
  lines << QString();

  auto append_children = [&lines](const QList<ProjectModelEntry>& all_items,
                                  const QString& root_name,
                                  const QString& block_root) {
    for (const auto& entry : all_items) {
      const ProjectModelEntry* child = &entry;
      if (child->kind != root_name) {
        continue;
      }
      lines << QString("[%1/%2] <- Model Tree %3/%2")
                   .arg(block_root, child->name, root_name);
    }
  };

  if (!input.mesh_path.isEmpty()) {
    lines << QString("[Mesh/file] <- Mesh path %1").arg(input.mesh_path);
  }
  for (const auto* child : entries_of(input.entries, "Materials")) {
    if (child->params.value("type").toString() == "AbaqusCDP") {
      lines << QString("[Materials/%1_elasticity] <- Model Tree Materials/%1")
                   .arg(child->name);
      lines << QString("[Materials/%1_stress] <- Model Tree Materials/%1")
                   .arg(child->name);
      lines << QString("[Materials/%1_cdp_stress_update] <- Model Tree Materials/%1")
                   .arg(child->name);
    } else {
      lines << QString("[Materials/%1] <- Model Tree Materials/%1")
                   .arg(child->name);
    }
  }
  for (const auto* child : entries_of(input.entries, "Sections")) {
    lines << QString("Section %1: material=%2 -> Physical Volume(s)=%3")
                 .arg(child->name, child->params.value("material").toString(),
                      child->params.value("block").toString());
  }
  for (const auto* child : entries_of(input.entries, "Physics")) {
    QString header;
    build_physics_action_block(child, &header, nullptr);
    if (!header.isEmpty()) {
      lines << QString("[%1] <- Model Tree Physics/%2")
                   .arg(header, child->name);
    }
  }
  append_children(input.entries, "Functions", "Functions");
  append_children(input.entries, "Variables", "Variables");
  append_children(input.entries, "BC", "BCs");
  for (const auto* child : entries_of(input.entries, "Loads")) {
    lines << QString("[%1/%2] <- Model Tree Loads/%2 (Physical Group=%3)")
                 .arg(child->params.value("type").toString() == "Pressure"
                          ? "BCs"
                          : "Kernels",
                      child->name,
                      child->params.value("boundary").toString());
  }
  for (const auto* child : entries_of(input.entries, "Interactions")) {
    lines << QString("[Contact/%1] <- Model Tree Interactions/%1 "
                     "(primary=%2, secondary=%3, mapping=Contact/Contact)")
                 .arg(child->name, child->params.value("primary").toString(),
                      child->params.value("secondary").toString());
  }
  const EntryView steps = entries_of(input.entries, "Steps");
  if (!steps.isEmpty()) {
    lines << QString("[Executioner] <- Model Tree Steps/%1")
                 .arg(steps.first()->name);
    lines << QString("[TimeStepper] <- Model Tree Steps/%1")
                 .arg(steps.first()->name);
    lines << QString("[Preconditioning/smp] <- Model Tree Steps/%1")
                 .arg(steps.first()->name);
    if (steps.size() > 1) {
      lines << QString("WARNING: %1 Steps saved; only the first is generated in this phase.")
                   .arg(steps.size());
    }
  }
  const EntryView outputs = entries_of(input.entries, "Outputs");
  if (!outputs.isEmpty()) {
    for (const auto* child : outputs) {
      lines << QString("[Outputs] package <- Model Tree Outputs/%1")
                   .arg(child->name);
    }
    lines << "[AuxVariables]/[AuxKernels]/[Postprocessors]/[Times] <- Outputs package";
  }
  if (!input.mesh_snapshot.groups.isEmpty()) {
    lines << QString();
    lines << "Physical Groups:";
    for (const auto& group : input.mesh_snapshot.groups) {
      lines << QString("- %1 (dim=%2, entities=%3, elements=%4)")
                   .arg(group.name)
                   .arg(group.dim)
                   .arg(group.entity_count)
                   .arg(group.element_count);
    }
  }
  return lines.join("\n");
}

}  // namespace

QString MooseInputGenerator::quote_moose_value_if_needed(const QString& value) {
  const QString trimmed = value.trimmed();
  if (trimmed.isEmpty() || trimmed.startsWith('\'') ||
      trimmed.startsWith('"')) {
    return value;
  }
  if (trimmed.contains(QRegularExpression("\\s"))) {
    return "'" + trimmed + "'";
  }
  return value;
}

QString MooseInputGenerator::upsert_generated_block(const QString& input,
                                                    const QString& header,
                                                    const QString& block_text) {
  const QString trimmed = block_text.trimmed();
  if (header.trimmed().isEmpty()) {
    return input;
  }
  if (!trimmed.isEmpty() && input.contains(trimmed)) {
    return input;
  }
  const QStringList lines = input.split('\n');
  const QString open_line = "[" + header + "]";
  int start = -1;
  for (int i = 0; i < lines.size(); ++i) {
    if (lines[i].trimmed() == open_line) {
      start = i;
      break;
    }
  }
  if (start < 0) {
    if (trimmed.isEmpty()) {
      return input;
    }
    QString out = input.trimmed();
    if (!out.isEmpty()) {
      out += "\n\n";
    }
    out += trimmed;
    out += "\n";
    return out;
  }
  int end = static_cast<int>(lines.size());  // 不含：替换区间 [start, end)
  for (int i = start + 1; i < lines.size(); ++i) {
    // 顶层收尾行：列 0 的 []（嵌套子块的收尾行带缩进，不会命中）。
    if (lines.at(i) == "[]") {
      end = i + 1;
      break;
    }
  }
  QStringList out_lines = lines.mid(0, start);
  if (!trimmed.isEmpty()) {
    out_lines += trimmed.split('\n');
  }
  out_lines += lines.mid(end);
  QString out = out_lines.join('\n');
  out.replace(QRegularExpression("\\n{3,}"), "\n\n");
  return out.trimmed().isEmpty() ? QString() : out.trimmed() + "\n";
}

QStringList MooseInputGenerator::generated_headers_with_prefix(
    const QString& input, const QString& prefix) {
  QStringList headers;
  const QRegularExpression header_re(R"((?m)^\[([^\]\r\n]+)\][ \t]*\r?$)");
  auto matches = header_re.globalMatch(input);
  while (matches.hasNext()) {
    const QString header = matches.next().captured(1);
    if (header.startsWith(prefix) && !headers.contains(header)) {
      headers << header;
    }
  }
  return headers;
}

MooseInputGenerator::Output MooseInputGenerator::generate(const Input& input) {
  Output out;
  const EntryView functions = entries_of(input.entries, "Functions");
  const EntryView variables = entries_of(input.entries, "Variables");
  const EntryView materials = entries_of(input.entries, "Materials");
  const EntryView sections = entries_of(input.entries, "Sections");
  const EntryView bcs = entries_of(input.entries, "BC");
  const EntryView loads = entries_of(input.entries, "Loads");
  const EntryView interactions = entries_of(input.entries, "Interactions");
  const EntryView outputs = entries_of(input.entries, "Outputs");
  const EntryView steps = entries_of(input.entries, "Steps");
  const EntryView physics = entries_of(input.entries, "Physics");

  out.functions = build_functions_block(functions);
  out.variables = build_variables_block(variables);
  out.materials =
      build_materials_block(materials, sections, &out.console_warnings);
  out.bcs = build_bcs_block(bcs, loads);
  out.loads = build_loads_block(loads);
  const QVariantMap outputs_cfg = outputs_package_config(outputs);
  out.outputs = build_outputs_block(outputs, outputs_cfg);
  out.executioner = build_executioner_block(steps, input.chinese_ui,
                                            &out.console_warnings,
                                            &out.status_warning);
  if (!physics.isEmpty()) {
    out.global_params = QString("[GlobalParams]\n  displacements = '%1'\n[]\n")
                            .arg(input.displacements);
    for (const auto* child : physics) {
      QString header;
      const QString block =
          build_physics_action_block(child, &header, &out.console_warnings);
      out.physics_headers << header;
      out.physics_blocks << block;
    }
  }
  out.interactions = build_interactions_block(interactions);
  out.aux_variables = build_aux_variables_block(outputs_cfg, physics);
  out.aux_kernels = build_aux_kernels_block(outputs_cfg, physics, materials,
                                            sections, &out.console_warnings);
  out.postprocessors =
      build_postprocessors_block(outputs_cfg, &out.console_warnings);
  out.times = build_times_block(outputs_cfg, &out.times_header);
  out.generation_report = build_generation_report(input);
  return out;
}

}  // namespace gmp
