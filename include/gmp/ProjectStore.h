#pragma once

// v0.2 Stage 2 app 层（hc_app 过渡形态，Q5 批复：逻辑边界先行）。
// ProjectStore：.gmp.yaml 持久化的唯一实现点——schema v2 读写、模型文档
// 转换、网格路径迁移（legacy out/ 与他项目 .work/case 两种）。只依赖
// Qt Core + yaml-cpp + project_schema/PhysicalGroupManifest，不依赖
// Qt Widgets/Gmsh/VTK/网络；加载/保存失败经错误串上报，由 MainWindow
// 呈现（本层不弹窗）。
//
// 模型数据直接产出/消费 ProjectDocument；面板设置与网格快照等
// 非模型元数据仍由 ProjectData 承载。

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "gmp/PhysicalGroupManifest.h"

namespace gmp {

namespace core {
class ProjectDocument;
}

// ---- 路径工具（自 MainWindow.cpp 匿名命名空间下沉，行为不变） ----

// 项目自有工作目录：<项目目录>/.work/case/<项目名>（项目名为文件名去
// 扩展名并做文件名安全化）。空路径返回空。
QString project_case_work_dir(const QString& project_path);
// 项目自有默认网格输出路径；无项目路径时回落到应用工作目录 out/。
QString default_mesh_output_path(const QString& project_path,
                                 const QString& mesh_name);
// 旧版默认网格路径判定：应用工作目录 out/ 下的共享路径。
bool is_legacy_default_mesh_path(const QString& path);
// 返回 path 所属的 .work/case/<X>/ 目录（干净路径）；不在任何项目 case
// 工作目录下时返回空。
QString enclosing_case_work_dir(const QString& path);

// ---- 纯数据项目模型 ----

struct ProjectModelEntry {
  QString id;       // schema v2 可选稳定 ObjectId；旧文件加载时生成
  QString parent_id;  // 可选；缺省时挂载到 kind 对应根节点
  QString name;
  QString kind;     // 根节点名（Parts/Materials/…/Input Cases/…）
  QString status;   // ready|incomplete|invalid|stale|disabled 或空
  QVariantMap params;
};

struct ProjectData {
  int schema_version = 2;
  QVariantMap application_profile;
  QVariantMap unit_contract;
  PhysicalGroupManifest mesh_snapshot;
  QList<ProjectModelEntry> model_entries;  // 按 YAML 中根节点顺序
  // 全部根节点名（含空根），按模型树/YAML 顺序；保存时据此输出全部
  // 根键（空根写作空序列），保持 schema v2 文件形态不变。
  QStringList model_roots;
  QVariantMap gmsh_settings;
  QVariantMap moose_settings;
  QStringList input_snapshots;
  QVariantMap viewer_settings;
};

class ProjectStore {
 public:
  // 加载 .gmp.yaml。版本不受支持/缺少 model 段/文件不可读时返回 false
  // 并写 error（文案与既有弹窗一致）。模型条目名按既有
  // unique_child_name 规则（base、base_2、base_3…）去重。
  bool load_file(const QString& path, ProjectData* out, QString* error,
                 core::ProjectDocument* document = nullptr) const;
  // 保存为 schema v2（saved_at 由本层写入当前 UTC 时间）。
  // document 非空时，模型段以 Document 为准，ProjectData 只提供元数据。
  bool save_file(const QString& path, const ProjectData& data,
                 QString* error,
                 const core::ProjectDocument* document = nullptr) const;

  // 单条网格路径迁移：legacy out/ 或他项目 .work/case/<X>/ 下的路径
  // 迁移到 project_path 自有工作目录（复制文件，目标已存在不覆盖；源
  // 缺失仍重定向并写日志）。返回迁移目标；无需迁移或目标不可用返回空。
  QString migrate_mesh_path(const QString& project_path, const QString& source,
                            const QString& fallback_name) const;
  // 数据级整体迁移：Mesh 条目 path、mesh_snapshot.mesh_path、
  // gmsh output_path、moose mesh_path（活动网格跟随自己的迁移目标，否则
  // 回落到第一个项目网格）与 input_text/structured_input/generation_report
  // 派生文本替换。返回迁移映射（source→target）。
  QList<QPair<QString, QString>> migrate_mesh_paths(const QString& project_path,
                                                    ProjectData* data) const;
};

}  // namespace gmp
