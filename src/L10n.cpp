#include "gmp/L10n.h"

#include <QAbstractButton>
#include <QAction>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QMenu>
#include <QHash>
#include <QSettings>
#include <QTabBar>
#include <QTabWidget>
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

QString translate_text(const QString& text, Language lang) {
  if (text.isEmpty()) {
    return text;
  }
  if (lang == Language::Chinese) {
    return zh_dict().value(text, text);
  }
  return en_dict().value(text, text);
}

void translate_widget(QWidget* w, Language lang) {
  if (!w) {
    return;
  }
  if (auto* btn = qobject_cast<QAbstractButton*>(w)) {
    btn->setText(translate_text(btn->text(), lang));
  } else if (auto* box = qobject_cast<QGroupBox*>(w)) {
    box->setTitle(translate_text(box->title(), lang));
  } else if (auto* label = qobject_cast<QLabel*>(w)) {
    label->setText(translate_text(label->text(), lang));
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
  }
}

}  // namespace

Language current_language() {
  QSettings s("gmp-ise", "gmp_ise");
  return s.value("ui_language", "en").toString() == "zh" ? Language::Chinese
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
