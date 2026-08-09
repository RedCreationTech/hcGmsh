#include "gmp/L10n.h"

#include <QAbstractButton>
#include <QAction>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QHash>
#include <QSettings>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVector>
#include <QWidget>

namespace gmp {
namespace l10n {
namespace {

// 英 -> 中 字典: 键为界面英文原文, 未收录的文本不会被翻译
const QHash<QString, QString>& zh_dict() {
  static const QHash<QString, QString> dict = {
      // ===== 菜单 =====
      {"File", "文件"},
      {"New Project", "新建项目"},
      {"Open Project...", "打开项目..."},
      {"Save Project", "保存项目"},
      {"Save Project As...", "项目另存为..."},
      {"Recent Projects", "最近项目"},
      {"Clear Recent", "清空最近记录"},
      {"Export Debug Bundle...", "导出调试包..."},
      {"Save Screenshot...", "保存截图..."},
      {"Model", "模型"},
      {"Sync Model -> MOOSE Input", "同步模型到 MOOSE 输入"},
      {"Mesh", "网格"},
      {"Generate Mesh", "生成网格"},
      {"Preview Mesh...", "预览网格..."},
      {"Job", "作业"},
      {"Run", "运行"},
      {"Check Input", "检查输入"},
      {"Stop", "停止"},
      {"Demos", "演示案例"},
      {"Setup Transient Diffusion", "载入瞬态扩散"},
      {"Run Transient Diffusion", "运行瞬态扩散"},
      {"Setup Thermo-Mechanics", "载入热-力耦合"},
      {"Run Thermo-Mechanics", "运行热-力耦合"},
      {"Setup Nonlinear Heat", "载入非线性热传导"},
      {"Run Nonlinear Heat", "运行非线性热传导"},
      {"Settings", "设置"},
      {"Language", "语言"},
      {"Chinese", "中文"},
      {"English", "English"},
      // ===== 模块页签 =====
      {"Part", "部件"},
      {"Property", "属性"},
      {"Material", "材料"},
      {"Section", "截面"},
      {"Assembly", "装配"},
      {"Step", "分析步"},
      {"Interaction", "相互作用"},
      {"Load", "载荷"},
      {"Mesh Module", "网格模块"},
      {"Job Module", "作业模块"},
      {"Visualization", "可视化"},
      {"Results", "结果"},
      // ===== 面板标题与通用按钮 =====
      {"Model Tree", "模型树"},
      {"Viewport", "视口"},
      {"Current Module", "当前模块"},
      {"Add", "添加"},
      {"Duplicate", "复制"},
      {"Rename", "重命名"},
      {"Remove", "删除"},
      {"Refresh", "刷新"},
      {"Open", "打开"},
      {"Reload", "重新加载"},
      {"Load Selected", "加载所选"},
      {"Retry", "重试"},
      {"Open Log", "打开日志"},
      {"Open Result", "打开结果"},
      {"Apply", "应用"},
      {"Clear", "清除"},
      {"Pick", "拾取"},
      {"Pick Output", "选择输出"},
      {"Select a job to view details.", "选择一个作业查看详情。"},
      {"No results yet.", "暂无结果。"},
      {"No quick actions", "暂无快捷操作"},
      // ===== 模块页按钮 =====
      {"Open Parts Root", "打开部件根节点"},
      {"Open Part Root", "打开部件根节点"},
      {"New Part", "新建部件"},
      {"Open Gmsh Panel", "打开 Gmsh 面板"},
      {"Open Mesh Module", "打开网格模块"},
      {"Open Job Module", "打开作业模块"},
      {"Open Materials Root", "打开材料根节点"},
      {"New Material", "新建材料"},
      {"Open Property Editor", "打开属性编辑器"},
      {"Open Sections Root", "打开截面根节点"},
      {"New Solid Section", "新建实体截面"},
      {"Open Mesh Root", "打开网格根节点"},
      {"Open Assembly", "打开装配"},
      {"Create Assembly Alias", "创建装配别名"},
      {"Open Steps Root", "打开分析步根节点"},
      {"Add Static Step", "添加静力分析步"},
      {"Add Transient Step", "添加瞬态分析步"},
      {"Add Steady Step", "添加稳态分析步"},
      {"Open Interactions Root", "打开相互作用根节点"},
      {"Add Interaction", "添加相互作用"},
      {"Add Tie Interaction", "添加绑定约束"},
      {"Open Loads Root", "打开载荷根节点"},
      {"Add Generic Load", "添加通用载荷"},
      {"Add Body Force", "添加体力"},
      {"Open BC Root", "打开边界条件根节点"},
      {"Add Thermal Source", "添加热源"},
      {"Generate 2D Mesh", "生成 2D 网格"},
      {"Generate 3D Mesh", "生成 3D 网格"},
      {"Generate & Submit", "生成并提交"},
      {"Prepare Workflow Defaults", "补齐流程默认节点"},
      {"Sync to Input", "同步到输入"},
      {"Submit (Mesh + Sync + Run)", "提交 (网格+同步+运行)"},
      {"Open Visualization Tab", "打开可视化页签"},
      {"Show Plot Preview", "显示曲线预览"},
      {"Show Table Preview", "显示表格预览"},
      {"Open Viewport", "打开视口"},
      {"Open Plot", "打开曲线"},
      {"Open Table", "打开表格"},
      {"Open Results Root", "打开结果根节点"},
      {"Refresh List", "刷新列表"},
      {"Open in Viewer", "在视图中打开"},
      {"Open as Text", "以文本打开"},
      {"Open Job Log", "打开作业日志"},
      {"Sync Model to Input", "同步模型到输入"},
      // ===== PropertyEditor =====
      {"General", "常规"},
      {"Parameters", "参数"},
      {"Preview", "预览"},
      {"Kind", "类型"},
      {"Status", "状态"},
      {"Name", "名称"},
      {"Quick Parameters", "快捷参数"},
      {"Advanced Parameters", "高级参数"},
      {"Add Param", "添加参数"},
      {"Remove Param", "删除参数"},
      {"Apply Groups", "应用分组"},
      {"Template", "模板"},
      {"Apply Template", "应用模板"},
      {"Type Defaults", "类型默认值"},
      {"Template Info", "模板说明"},
      {"Validation Summary", "校验汇总"},
      {"Go To Node", "跳转到节点"},
      {"Current Type Only", "仅当前类型"},
      {"Only With Issues", "仅显示问题项"},
      // ===== GmshPanel =====
      {"Gmsh Panel", "Gmsh 面板"},
      {"Model", "模型"},
      {"Geometry", "几何"},
      {"Open Geometry", "打开几何"},
      {"Clear Model", "清空模型"},
      {"Auto mesh after import", "导入后自动剖分"},
      {"Auto reload geometry on project load", "打开项目时自动重载几何"},
      {"Entities", "实体"},
      {"Primitives", "几何基元"},
      {"Add Primitive", "添加基元"},
      {"Transform", "变换"},
      {"Translate", "平移"},
      {"Rotate", "旋转"},
      {"Scale", "缩放"},
      {"Boolean", "布尔运算"},
      {"Fuse", "合并"},
      {"Cut", "剪切"},
      {"Intersect", "求交"},
      {"Remove Object", "删除对象"},
      {"Remove Tool", "删除工具"},
      {"Physical Groups", "物理组"},
      {"Update Selected", "更新所选"},
      {"Delete Selected", "删除所选"},
      {"Mesh Fields", "网格场"},
      {"Apply Field", "应用场"},
      {"Clear Fields", "清除场"},
      {"Mesh Size", "网格尺寸"},
      {"Generate Dim", "生成维度"},
      {"Entity Size", "实体尺寸"},
      {"Element Order", "单元阶数"},
      {"High-Order", "高阶优化"},
      {"MSH Version", "MSH 版本"},
      {"Algorithm 2D", "2D 算法"},
      {"Algorithm 3D", "3D 算法"},
      {"Recombine (quad/hex)", "重组 (四边形/六面体)"},
      {"Smoothing", "光顺"},
      {"Optimize Mesh", "优化网格"},
      {"Mesh Output", "网格输出"},
      {"Export Geometry", "导出几何"},
      {"Use Sample Box", "使用示例盒"},
      // ===== MoosePanel =====
      {"MOOSE Panel", "MOOSE 面板"},
      {"Paths", "路径"},
      {"Executable", "可执行文件"},
      {"Input File", "输入文件"},
      {"Work Dir", "工作目录"},
      {"Mesh File", "网格文件"},
      {"Insert Mesh Block", "插入网格块"},
      {"Insert BCs From Groups", "从物理组生成边界"},
      {"Use mpiexec", "使用 mpiexec"},
      {"MPI Ranks", "MPI 进程数"},
      {"Runner", "运行器"},
      {"Extra Args", "额外参数"},
      {"Input Editor", "输入编辑器"},
      {"Write Input", "写出输入"},
      // ===== VtkViewer =====
      {"Scalar", "标量"},
      {"View", "视图"},
      {"Slice", "切片"},
      {"Time", "时间"},
      {"Vector", "向量"},
      {"Deformation", "变形"},
      {"Probe", "探针"},
      {"Plot", "曲线"},
      {"Table", "表格"},
      {"Faces", "面"},
      {"Edges", "边"},
      {"Shell", "外壳"},
      {"Nodes", "节点"},
      {"Quality", "质量"},
      {"Scalar Bar", "色标条"},
      {"Bar Pos", "色标位置"},
      {"Preset", "色标"},
      {"Repr", "显示方式"},
      {"Auto Range", "自动范围"},
      {"Axes", "坐标轴"},
      {"Outline", "外框"},
      {"Reset Filters", "重置过滤器"},
      {"Apply View", "应用视角"},
      {"Auto Refresh", "自动刷新"},
      {"Enable Deformation", "启用变形"},
      {"Enable Probe", "启用探针"},
      {"Variables", "变量"},
      {"Outputs", "输出"},
      {"Open Visualization", "打开可视化"},
      // ===== MainWindow 模块页 =====
      {"Current entries:", "当前条目:"},
      {"Double click item to jump to model tree.", "双击条目跳转到模型树。"},
      {"Open Selected part", "打开所选部件"},
      {"Open Selected material", "打开所选材料"},
      {"Open Selected section", "打开所选截面"},
      {"Open Selected step", "打开所选分析步"},
      {"Open Selected interaction", "打开所选相互作用"},
      {"Open Selected load", "打开所选载荷"},
      {"Define geometric primitives and manage part-level entities. Parts are a user-facing grouping for your geometry and mesh assignments.",
       "定义几何基元并管理部件级实体。部件是面向用户的几何与网格指派的分组单位。"},
      {"Create material definitions, tune constitutive laws, and keep properties ready for sections.",
       "创建材料定义、调整本构关系, 并为截面准备好属性。"},
      {"Create section assignments to bind materials and options to part regions or sets.",
       "创建截面指派, 将材料与选项绑定到部件区域或集合。"},
      {"Combine and instantiate parts into assembly-level units, then map mesh/topology for job-level binding.",
       "将部件组合实例化为装配级单元, 并映射网格/拓扑以供作业级绑定。"},
      {"Create analysis steps, control time integration and execution options in the current model setup.",
       "创建分析步, 控制当前模型设置中的时间积分与执行选项。"},
      {"Setup contact, ties, and other coupling behaviors between sets/parts.",
       "设置集合/部件之间的接触、绑定及其他耦合行为。"},
      {"Create loads, body forces, pressure and thermal sources and map them to mesh groups.",
       "创建载荷、体力、压力与热源, 并将其映射到网格分组。"},
      {"Control tabs live in this side panel; the viewport stays clean for the 3D scene.",
       "控制页签位于此侧边栏; 视口保持简洁以展示 3D 场景。"},
      {"Review generated outputs and quickly open results in the viewer.",
       "查看生成的输出, 并在视图中快速打开结果。"},
      {"No parts yet.", "暂无部件。"},
      {"No parts yet. Create one from this module or Gmsh panel.",
       "暂无部件。可在此模块或 Gmsh 面板中创建。"},
      {"No materials yet.", "暂无材料。"},
      {"No sections yet.", "暂无截面。"},
      {"No parts available for assembly yet.", "暂无可用于装配的部件。"},
      {"No part entries available for assembly.", "暂无可用于装配的部件条目。"},
      {"No steps yet.", "暂无分析步。"},
      {"No steps yet. Add at least one step before run.",
       "暂无分析步。运行前请至少添加一个分析步。"},
      {"No interactions yet.", "暂无相互作用。"},
      {"No loads yet.", "暂无载荷。"},
      {"No step blocks yet.", "暂无分析步块。"},
      {"Step sequence preview (Executioner uses first step; remaining shown for check):",
       "分析步序列预览 (Executioner 使用第一个分析步; 其余仅供检查):"},
      {"Add Step Preset: steady", "添加分析步预设: steady"},
      {"Plot Preview (from active dataset)", "曲线预览 (来自当前数据集)"},
      {"Table Preview (from active dataset)", "表格预览 (来自当前数据集)"},
      {"Tip: full visualization is in Visualization module.",
       "提示: 完整可视化功能位于“可视化”模块。"},
      {"Select a result item for quick preview.", "选择结果条目以快速预览。"},
      {"No file path for this result.", "此结果没有文件路径。"},
      {"Job/Message Console", "作业/消息控制台"},
      {"Job Log", "作业日志"},
      {"Project: Untitled", "项目: 未命名"},
      {"Saved", "已保存"},
      {"Modified", "已修改"},
      {"(None)", "(空)"},
      // ===== 表头 =====
      {"Start", "开始"},
      {"Duration", "耗时"},
      {"Exec", "程序"},
      {"Result", "结果文件"},
      {"Key", "键"},
      {"Value", "值"},
      {"Node", "节点"},
      {"Issues", "问题"},
      {"Dim", "维度"},
      {"Tag", "标识"},
      {"Elements", "单元数"},
      // ===== 通用下拉项/标签 =====
      {"All", "全部"},
      {"New", "新建"},
      {"Type", "类型"},
      {"Group", "组"},
      {"Groups", "分组"},
      {"Entity", "实体"},
      {"Mode", "模式"},
      {"Variable", "变量"},
      {"Function", "函数"},
      {"Boundary", "边界"},
      {"Templates", "模板"},
      {"Reset", "重置"},
      {"Front", "前视"},
      {"Right", "右视"},
      {"Top", "顶视"},
      {"Iso", "轴测"},
      {"Local", "本地"},
      {"Remote", "远程"},
      {"Automatic", "自动"},
      {"Auto", "自动"},
      {"Linear (1)", "线性 (1)"},
      {"Quadratic (2)", "二次 (2)"},
      {"Cubic (3)", "三次 (3)"},
      {"Quartic (4)", "四次 (4)"},
      {"High-Order Optimize: Off", "高阶优化: 关"},
      {"High-Order Optimize: Simple", "高阶优化: 简单"},
      {"High-Order Optimize: Elastic", "高阶优化: 弹性"},
      {"High-Order Optimize: Fast Curving", "高阶优化: 快速曲面化"},
      {"Solver (.e/.exo)", "求解器 (.e/.exo)"},
      {"Mesh (.msh)", "网格 (.msh)"},
      {"Text (.txt/.csv/.log/.yaml/.yml)", "文本 (.txt/.csv/.log/.yaml/.yml)"},
      // ===== PropertyEditor =====
      {"No Selection", "未选择"},
      {"Select physical groups to apply.", "选择要应用的物理组。"},
      {"Selected:", "已选:"},
      {"Sync", "同步"},
      {"Bidirectional (Recommended)", "双向 (推荐)"},
      {"Quick Form Wins", "快捷表单优先"},
      {"Controls how Advanced Parameters sync with Quick form",
       "控制高级参数与快捷表单之间的同步方式"},
      {"No issues.", "无问题。"},
      {"No validation issues.", "无校验问题。"},
      {"Selected item preview:", "所选条目预览:"},
      {"Select a node in the model tree.", "在模型树中选择一个节点。"},
      {"Apply selection to boundary.", "将所选应用到边界。"},
      {"Apply selection to block.", "将所选应用到块。"},
      {"Boundary Groups", "边界分组"},
      {"Volume Groups", "体分组"},
      {"(none)", "(无)"},
      {"Applies defaults for the selected type.", "应用所选类型的默认值。"},
      {"No description.", "无说明。"},
      {"Prop Names", "属性名"},
      {"Prop Values", "属性值"},
      {"Expression", "表达式"},
      {"Property Name", "属性名称"},
      {"Coupled Vars", "耦合变量"},
      {"Diffusivity", "扩散系数"},
      {"Displacements", "位移"},
      // ===== GmshPanel =====
      {"Summary", "摘要"},
      {"Stats", "统计"},
      {"Fields", "场"},
      {"IDs", "ID 列表"},
      {"Obj", "对象"},
      {"Tool", "工具"},
      {"Size", "尺寸"},
      {"Size X", "尺寸 X"},
      {"Size Y", "尺寸 Y"},
      {"Size Z", "尺寸 Z"},
      {"Radius", "半径"},
      {"Origin/Base x", "原点/基点 x"},
      {"Origin/Base y", "原点/基点 y"},
      {"Origin/Base z", "原点/基点 z"},
      {"Size/Axis dx", "尺寸/轴 dx"},
      {"Size/Axis dy", "尺寸/轴 dy"},
      {"Size/Axis dz", "尺寸/轴 dz"},
      {"Rotate Origin x", "旋转原点 x"},
      {"Rotate Origin y", "旋转原点 y"},
      {"Rotate Origin z", "旋转原点 z"},
      {"Rotate Axis ax", "旋转轴 ax"},
      {"Rotate Axis ay", "旋转轴 ay"},
      {"Rotate Axis az", "旋转轴 az"},
      {"Scale Center x", "缩放中心 x"},
      {"Scale Center y", "缩放中心 y"},
      {"Scale Center z", "缩放中心 z"},
      {"DistMin", "最小距离"},
      {"DistMax", "最大距离"},
      {"SizeMin", "最小尺寸"},
      {"SizeMax", "最大尺寸"},
      {"No geometry loaded", "未加载几何"},
      {"No model.", "无模型。"},
      {"No fields.", "无场。"},
      {"IDs or dim:tag (e.g. 1,2 or 2:5). Empty = all.",
       "ID 或 dim:tag (如 1,2 或 2:5)。留空 = 全部。"},
      {"Object IDs or dim:tag (e.g. 1,2 or 3:4)",
       "对象 ID 或 dim:tag (如 1,2 或 3:4)"},
      {"Tool IDs or dim:tag (e.g. 3 or 3:5)", "工具 ID 或 dim:tag (如 3 或 3:5)"},
      {"Entity IDs or dim:tag list", "实体 ID 或 dim:tag 列表"},
      {"IDs or dim:tag list", "ID 或 dim:tag 列表"},
      {"Output mesh path (*.msh)", "输出网格路径 (*.msh)"},
      // ===== MoosePanel =====
      {"Auto-detect MOOSE executable", "自动检测 MOOSE 可执行文件"},
      {"Path to moose executable", "MOOSE 可执行文件路径"},
      {"Input file path (*.i)", "输入文件路径 (*.i)"},
      {"Working directory (optional)", "工作目录 (可选)"},
      {"Path to mesh file (.msh)", "网格文件路径 (.msh)"},
      {"No boundary groups detected yet.", "尚未检测到边界分组。"},
      {"Extra args (e.g. --n-threads=4)", "额外参数 (如 --n-threads=4)"},
      {"GeneratedMesh (Transient Diffusion)", "GeneratedMesh (瞬态扩散)"},
      {"FileMesh (Transient Diffusion)", "FileMesh (瞬态扩散)"},
      {"GeneratedMesh (Nonlinear Heat)", "GeneratedMesh (非线性热传导)"},
      {"GeneratedMesh (Thermo-Mechanics)", "GeneratedMesh (热-力耦合)"},
      {"FileMesh (Thermo-Mechanics)", "FileMesh (热-力耦合)"},
      // ===== VtkViewer =====
      {"No file loaded", "未加载文件"},
      {"Failed to load mesh", "加载网格失败"},
      {"Mesh preview requires libgmsh", "网格预览需要 libgmsh"},
      {"No data", "无数据"},
      {"No array selected", "未选择数组"},
      {"Invalid array", "无效数组"},
      {"vtk disabled", "VTK 已禁用"},
      {"No stats", "无统计"},
      {"No plot widget", "无曲线控件"},
      {"No table widget", "无表格控件"},
      {"No vector data loaded", "未加载向量数据"},
      {"No vector arrays (>=2 components)", "无向量数组 (>=2 个分量)"},
      {"No selectable vector array", "无可选向量数组"},
      {"Array must have 2+ components", "数组必须至少有 2 个分量"},
      {"No compatible vector data", "无兼容向量数据"},
      {"Pick: disabled", "拾取: 已禁用"},
      {"Pick: cleared", "拾取: 已清除"},
      {"Pick: none", "拾取: 无"},
      {"Pick: click to inspect", "拾取: 点击图元查看信息"},
      {"Probe: disabled", "探针: 已禁用"},
      {"Probe: cleared", "探针: 已清除"},
      {"Probe: none", "探针: 无"},
      {"Groups: none", "分组: 无"},
      {"Apply to Deform", "应用到变形"},
      {"Auto-sync deformation vector", "自动同步变形向量"},
      {"Applies warping using the selected vector array.",
       "使用所选向量数组施加变形。"},
      {"Rows", "行数"},
      {"Opacity", "不透明度"},
      {"Shrink", "收缩"},
      {"VTK Viewer Disabled\n(Rebuild with GMP_ENABLE_VTK_VIEWER=ON)",
       "VTK 查看器已禁用\n(使用 GMP_ENABLE_VTK_VIEWER=ON 重新构建)"},
      // ===== 模块标题 (页签名 + " Module") =====
      {"Part Module", "部件模块"},
      {"Property Module", "属性模块"},
      {"Material Module", "材料模块"},
      {"Section Module", "截面模块"},
      {"Assembly Module", "装配模块"},
      {"Step Module", "分析步模块"},
      {"Interaction Module", "相互作用模块"},
      {"Load Module", "载荷模块"},
      {"Visualization Module", "可视化模块"},
      {"Results Module", "结果模块"},
  };
  return dict;
}

// 中 -> 英 反向字典 (用于切回 English)
const QHash<QString, QString>& en_dict() {
  static const QHash<QString, QString> reverse = [] {
    QHash<QString, QString> rev;
    const auto& dict = zh_dict();
    for (auto it = dict.begin(); it != dict.end(); ++it) {
      rev.insert(it.value(), it.key());
    }
    return rev;
  }();
  return reverse;
}

// 动态拼接串的前缀翻译: 整串匹配失败时, 若文本以表中英文前缀开头,
// 则仅替换前缀部分 (如 "Missing required fields: dt, end_time")
const QVector<QPair<QString, QString>>& zh_prefixes() {
  static const QVector<QPair<QString, QString>> prefixes = {
      {"Missing required fields: ", "缺失必填字段: "},
      {"Workflow status: ", "流程状态: "},
      {"Invalid entities: ", "无效实体: "},
      {"No file attached for: ", "无关联文件: "},
      {"Entities: ", "实体: "},
      {"Project: ", "项目: "},
      {"Preview: ", "预览: "},
      {"Groups: ", "分组: "},
      {"Pick: ", "拾取: "},
      {"Probe: ", "探针: "},
      {"status: ", "状态: "},
      {"Add ", "添加 "},
  };
  return prefixes;
}

QString translate_text(const QString& text, Language lang) {
  if (text.isEmpty()) {
    return text;
  }
  // 菜单标题可能带快捷键标记 (如 "&File"), 去掉 & 翻译后还原
  if (text.startsWith('&')) {
    const QString rest = text.mid(1);
    const QString translated = translate_text(rest, lang);
    return translated == rest ? text : "&" + translated;
  }
  if (lang == Language::Chinese) {
    const auto it = zh_dict().constFind(text);
    if (it != zh_dict().constEnd()) {
      return it.value();
    }
    for (const auto& p : zh_prefixes()) {
      if (text.startsWith(p.first)) {
        return p.second + text.mid(p.first.size());
      }
    }
    return text;
  }
  const auto it = en_dict().constFind(text);
  if (it != en_dict().constEnd()) {
    return it.value();
  }
  for (const auto& p : zh_prefixes()) {
    if (text.startsWith(p.second)) {
      return p.first + text.mid(p.second.size());
    }
  }
  return text;
}

void translate_widget(QWidget* w, Language lang) {
  if (!w) {
    return;
  }
  if (auto* btn = qobject_cast<QAbstractButton*>(w)) {
    btn->setText(translate_text(btn->text(), lang));
    btn->setToolTip(translate_text(btn->toolTip(), lang));
  } else if (auto* box = qobject_cast<QGroupBox*>(w)) {
    box->setTitle(translate_text(box->title(), lang));
  } else if (auto* label = qobject_cast<QLabel*>(w)) {
    label->setText(translate_text(label->text(), lang));
  } else if (auto* list = qobject_cast<QListWidget*>(w)) {
    for (int i = 0; i < list->count(); ++i) {
      if (auto* item = list->item(i)) {
        item->setText(translate_text(item->text(), lang));
        item->setToolTip(translate_text(item->toolTip(), lang));
      }
    }
    list->setToolTip(translate_text(list->toolTip(), lang));
  } else if (auto* table = qobject_cast<QTableWidget*>(w)) {
    for (int i = 0; i < table->columnCount(); ++i) {
      if (auto* header = table->horizontalHeaderItem(i)) {
        header->setText(translate_text(header->text(), lang));
      }
    }
  } else if (auto* edit = qobject_cast<QLineEdit*>(w)) {
    // 只翻占位符; 编辑框内容是用户数据/路径, 不动
    edit->setPlaceholderText(translate_text(edit->placeholderText(), lang));
  } else if (auto* plain = qobject_cast<QPlainTextEdit*>(w)) {
    plain->setPlaceholderText(translate_text(plain->placeholderText(), lang));
    // 只读 plain text 的内容是程序生成的固定提示 (如 "No data"),
    // 整串在字典中命中才替换, 日志/数据内容不受影响
    if (plain->isReadOnly()) {
      plain->setPlainText(translate_text(plain->toPlainText(), lang));
    }
  }
  if (auto* tree = qobject_cast<QTreeWidget*>(w)) {
    if (auto* header = tree->headerItem()) {
      for (int i = 0; i < header->columnCount(); ++i) {
        header->setText(i, translate_text(header->text(i), lang));
      }
    }
  }
  if (auto* tabs = qobject_cast<QTabWidget*>(w)) {
    for (int i = 0; i < tabs->count(); ++i) {
      tabs->setTabText(i, translate_text(tabs->tabText(i), lang));
    }
  }
  if (auto* bar = qobject_cast<QTabBar*>(w)) {
    for (int i = 0; i < bar->count(); ++i) {
      bar->setTabText(i, translate_text(bar->tabText(i), lang));
    }
  }
  if (auto* combo = qobject_cast<QComboBox*>(w)) {
    for (int i = 0; i < combo->count(); ++i) {
      combo->setItemText(i, translate_text(combo->itemText(i), lang));
    }
    combo->setToolTip(translate_text(combo->toolTip(), lang));
  }
}

}  // namespace

Language current_language() {
  QSettings s("gmp-ise", "gmp_ise");
  return s.value("ui_language", "zh").toString() == "zh" ? Language::Chinese
                                                         : Language::English;
}

void set_language(Language lang) {
  QSettings s("gmp-ise", "gmp_ise");
  s.setValue("ui_language", lang == Language::Chinese ? "zh" : "en");
}

void apply(QWidget* root) {
  if (!root) {
    return;
  }
  const Language lang = current_language();
  const auto widgets = root->findChildren<QWidget*>();
  for (auto* w : widgets) {
    translate_widget(w, lang);
  }
  // QAction (菜单项/工具栏动作) 不属于控件树, 单独处理
  const auto actions = root->findChildren<QAction*>();
  for (auto* a : actions) {
    a->setText(translate_text(a->text(), lang));
  }
  // 顶层菜单标题
  if (auto* window = root->window()) {
    const auto menus = window->findChildren<QMenu*>();
    for (auto* m : menus) {
      m->setTitle(translate_text(m->title(), lang));
    }
  }
}

}  // namespace l10n
}  // namespace gmp
