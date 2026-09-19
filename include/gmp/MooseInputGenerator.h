#pragma once

// v0.2 Stage 3 hc_simulation 过渡层（Q5 批复：逻辑边界先行）。
// MooseInputGenerator：模型树 → MOOSE .i 各块文本 + 生成报告的唯一实现
// 点。输入为纯数据（ProjectModelEntry 清单 + 解析好的上下文值），不依赖
// Qt Widgets/Gmsh/VTK/网络；状态栏/控制台/弹窗等 UI 呈现由 MainWindow
// 负责（经 Output 的警告通道回传）。实现逐字搬运自原 MainWindow 的
// build_*_block 系列（行为冻结红线：G1 项目生成 .i 必须逐字一致）。

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "gmp/PhysicalGroupManifest.h"
#include "gmp/ProjectStore.h"

namespace gmp {

class MooseInputGenerator {
 public:
  struct Input {
    QList<ProjectModelEntry> entries;  // 全部模型条目，保持模型树顺序
    QString displacements;             // [GlobalParams]（档案解析结果注入）
    QString application_profile_id;    // 生成报告用
    bool mapping_registry_loaded = false;
    QString mapping_registry_version;  // 生成报告用
    QString input_mode;                // 生成报告用
    QString mesh_path;                 // 生成报告 [Mesh/file] 行
    PhysicalGroupManifest mesh_snapshot;  // 生成报告 Physical Groups 段
    bool chinese_ui = false;  // 警告文案语言（替代 l10n 依赖）
  };

  struct Output {
    QString functions;
    QString variables;
    QString materials;
    QString bcs;
    QString loads;  // [Kernels]（非 Pressure 载荷）
    QString outputs;
    QString executioner;  // 含 [Preconditioning/ 子文本（若有）
    QString global_params;
    QStringList physics_headers;  // 与 physics_blocks 对齐
    QStringList physics_blocks;
    QString interactions;  // [Contact]
    QString aux_variables;
    QString aux_kernels;
    QString postprocessors;
    QString times;
    QString times_header;
    QString generation_report;
    QStringList console_warnings;  // 控制台警告（CDP 无 block 等）
    QString status_warning;        // 执行器多 Step 警告（控制台+状态栏双通道）
  };

  static Output generate(const Input& input);

  // 文本工具（W-03b/W-03d/W-03e 幂等 upsert 语义；自 MainWindow 匿名
  // 命名空间搬走，行为不变）。
  static QString upsert_generated_block(const QString& input,
                                        const QString& header,
                                        const QString& block_text);
  static QStringList generated_headers_with_prefix(const QString& input,
                                                   const QString& prefix);
  // MOOSE 值引用规则：含空格的值需单引号包裹（已带引号的原样返回）。
  static QString quote_moose_value_if_needed(const QString& value);

  // W-01b：Section 指派语义（材料 → 体组名解析）。供生成器内部与
  // MainWindow 默认参数通道共用；多组指派经 warnings 上报。
  static QString resolve_assigned_block(const QList<ProjectModelEntry>& entries,
                                        const QString& material_name,
                                        QStringList* warnings = nullptr);
};

}  // namespace gmp
