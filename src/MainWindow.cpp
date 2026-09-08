#include "gmp/MainWindow.h"
#include "gmp/L10n.h"

#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QComboBox>
#include <QActionGroup>
#include <QSizePolicy>
#include <QScrollArea>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QListWidget>
#include <QHeaderView>
#include <QStatusBar>
#include <QSplitter>
#include <QDockWidget>
#include <QCloseEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QFrame>
#include <QFormLayout>
#include <QCheckBox>
#include <QProgressBar>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QAbstractItemView>
#include <QVBoxLayout>
#include <QStyle>
#include <QStyleFactory>
#include <QKeySequence>
#include <QShortcut>
#include <QApplication>
#include <QGuiApplication>
#include <QWindow>
#include <QClipboard>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QVariantMap>
#include <QMetaType>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QSignalBlocker>
#include <QLabel>
#include <QTextStream>
#include <QDateTime>
#include <QTimer>
#include <memory>
#include <functional>
#include <stdexcept>
#include <vector>

#include <fstream>
#include <QFileInfo>
#include <yaml-cpp/yaml.h>

#include "gmp/GmshPanel.h"
#include "gmp/FloatingPropertyForm.h"
#include "gmp/MoosePanel.h"
#include "gmp/OccBridge.h"
#include "gmp/OperationLog.h"
#include "gmp/PartFeaturePanel.h"
#include "gmp/PropertyEditor.h"
#include "gmp/ProjectSchema.h"
#include "gmp/SketchDocument.h"
#include "gmp/StageLeftToolbar.h"
#include "gmp/SketchPanel.h"
#include "gmp/VtkViewer.h"

namespace gmp {

namespace {

QList<int> volume_tags_from_params(const QVariantMap& params) {
  QList<int> tags;
  for (const QVariant& value : params.value("gmsh_volume_tags").toList()) {
    const int tag = value.toInt();
    if (tag > 0 && !tags.contains(tag)) {
      tags.append(tag);
    }
  }
  const int legacy_tag = params.value("gmsh_volume_tag", 0).toInt();
  if (tags.isEmpty() && legacy_tag > 0) {
    tags.append(legacy_tag);
  }
  return tags;
}

QVariantList volume_tags_to_variant(const QList<int>& tags) {
  QVariantList values;
  for (const int tag : tags) {
    if (tag > 0) {
      values.append(tag);
    }
  }
  return values;
}

// QStackedWidget 默认以所有页面的最大 size hint 作为自身尺寸，复杂的 Mesh
// 页面会因此把简单的 Sketch Editor 也撑成同样的大窗。工作窗只显示一个
// 页面，应由当前页面决定尺寸；各页面仍保留自己的最小尺寸与滚动策略。
class CurrentPageStackedWidget final : public QStackedWidget {
 public:
  explicit CurrentPageStackedWidget(QWidget* parent = nullptr)
      : QStackedWidget(parent) {
    connect(this, &QStackedWidget::currentChanged, this,
            [this]() { updateGeometry(); });
  }

  QSize sizeHint() const override {
    return currentWidget() ? currentWidget()->sizeHint()
                           : QStackedWidget::sizeHint();
  }

  QSize minimumSizeHint() const override {
    if (!currentWidget()) {
      return QStackedWidget::minimumSizeHint();
    }
    return currentWidget()->minimumSizeHint().expandedTo(
        currentWidget()->minimumSize());
  }
};

enum class IconGlyph {
  NewFile,
  OpenFolder,
  SaveDisk,
  Sync,
  Mesh,
  Run,
  Check,
  Stop,
  Part,
  Material,
  Section,
  Step,
  Function,
  Variable,
  BC,
  Load,
  Output,
  Interaction,
  Job,
  Result,
  AddItem,
  DuplicateItem,
  RenameItem,
  RemoveItem,
  Undo,
  Redo,
  Display,
  Pick,
  ClearSelection,
  Slice,
};

constexpr int kNavigationKindRole = Qt::UserRole + 100;
constexpr int kNavigationNameRole = Qt::UserRole + 101;
constexpr int kNavigationPathRole = Qt::UserRole + 102;

bool apply_name_filter(QTreeWidgetItem* item, const QString& needle) {
  if (!item) {
    return false;
  }
  bool child_matches = false;
  for (int i = 0; i < item->childCount(); ++i) {
    child_matches = apply_name_filter(item->child(i), needle) || child_matches;
  }
  const bool own_match = needle.isEmpty() ||
                         item->text(0).contains(needle, Qt::CaseInsensitive);
  const bool visible = own_match || child_matches;
  item->setHidden(!visible);
  if (!needle.isEmpty() && child_matches) {
    item->setExpanded(true);
  }
  return visible;
}

QIcon MakeIcon(IconGlyph glyph, int size = 18) {
  QPixmap pix(size, size);
  pix.fill(Qt::transparent);
  QPainter p(&pix);
  p.setRenderHint(QPainter::Antialiasing, true);
  QPen pen(QColor("#2b2b2b"));
  pen.setWidthF(1.6);
  p.setPen(pen);

  const int s = size;
  const int m = 3;
  const QRect r(m, m, s - 2 * m, s - 2 * m);

  switch (glyph) {
    case IconGlyph::NewFile: {
      // 折角文档 + 蓝色加号，避免在 18 px 下与四宫格网格图标混淆。
      QPolygon page;
      page << QPoint(m + 2, m) << QPoint(s - m - 4, m)
           << QPoint(s - m, m + 4) << QPoint(s - m, s - m)
           << QPoint(m + 2, s - m) << QPoint(m + 2, m);
      p.drawPolyline(page);
      p.drawLine(s - m - 4, m, s - m - 4, m + 4);
      p.drawLine(s - m - 4, m + 4, s - m, m + 4);
      QPen plus_pen(QColor("#2f6fed"));
      plus_pen.setWidthF(1.8);
      p.setPen(plus_pen);
      p.drawLine(m + 4, s - m - 5, m + 10, s - m - 5);
      p.drawLine(m + 7, s - m - 8, m + 7, s - m - 2);
      break;
    }
    case IconGlyph::OpenFolder: {
      QRect folder(m, m + 4, s - 2 * m, s - m - 6);
      p.drawRect(folder);
      p.drawLine(m + 2, m + 4, s / 2, m + 4);
      p.drawLine(m + 2, m + 4, m + 6, m + 1);
      break;
    }
    case IconGlyph::SaveDisk: {
      p.drawRect(r);
      p.drawLine(m + 3, m + 5, s - m - 3, m + 5);
      p.drawRect(QRect(m + 4, m + 8, s - 2 * m - 8, 5));
      break;
    }
    case IconGlyph::Sync: {
      p.drawArc(r, 40 * 16, 220 * 16);
      p.drawArc(r, 260 * 16, 220 * 16);
      p.drawLine(s - m - 2, s / 2, s - m - 6, s / 2 - 3);
      p.drawLine(s - m - 2, s / 2, s - m - 6, s / 2 + 3);
      break;
    }
    case IconGlyph::Mesh: {
      for (int i = 0; i < 3; ++i) {
        int x = m + i * (r.width() / 2);
        p.drawLine(x, m, x, s - m);
        int y = m + i * (r.height() / 2);
        p.drawLine(m, y, s - m, y);
      }
      break;
    }
    case IconGlyph::Run: {
      QPolygon poly;
      poly << QPoint(m + 2, m + 1) << QPoint(s - m - 2, s / 2)
           << QPoint(m + 2, s - m - 1);
      p.setBrush(QColor("#2b2b2b"));
      p.drawPolygon(poly);
      break;
    }
    case IconGlyph::Check: {
      p.drawLine(m + 2, s / 2, s / 2 - 1, s - m - 2);
      p.drawLine(s / 2 - 1, s - m - 2, s - m - 2, m + 3);
      break;
    }
    case IconGlyph::Stop: {
      p.setBrush(QColor("#2b2b2b"));
      p.drawRect(QRect(m + 3, m + 3, s - 2 * m - 6, s - 2 * m - 6));
      break;
    }
    case IconGlyph::Part: {
      QRect back(m + 3, m + 1, s - 2 * m - 6, s - 2 * m - 6);
      QRect front(m, m + 4, s - 2 * m - 6, s - 2 * m - 6);
      p.drawRect(back);
      p.drawRect(front);
      p.drawLine(front.topLeft(), back.topLeft());
      p.drawLine(front.topRight(), back.topRight());
      p.drawLine(front.bottomLeft(), back.bottomLeft());
      break;
    }
    case IconGlyph::Material: {
      p.setBrush(QColor("#2b2b2b"));
      p.drawEllipse(r.adjusted(2, 2, -2, -2));
      break;
    }
    case IconGlyph::Section: {
      p.drawLine(m + 2, m + 4, s - m - 2, m + 4);
      p.drawLine(m + 2, s / 2, s - m - 2, s / 2);
      p.drawLine(m + 2, s - m - 4, s - m - 2, s - m - 4);
      break;
    }
    case IconGlyph::Step: {
      QPolygon poly;
      poly << QPoint(m + 2, m + 1) << QPoint(s - m - 2, s / 2)
           << QPoint(m + 2, s - m - 1);
      p.drawPolygon(poly);
      break;
    }
    case IconGlyph::Function: {
      QPainterPath path;
      path.moveTo(m + 1, s - m - 2);
      path.cubicTo(s / 3, m + 1, s / 2, s - m - 2, s - m - 1, m + 2);
      p.drawPath(path);
      break;
    }
    case IconGlyph::Variable: {
      p.drawLine(m + 2, m + 2, s - m - 2, s - m - 2);
      p.drawLine(m + 2, s - m - 2, s - m - 2, m + 2);
      break;
    }
    case IconGlyph::BC: {
      p.drawRect(r);
      p.drawLine(m, m, s - m, m);
      break;
    }
    case IconGlyph::Load: {
      p.drawLine(s / 2, m + 2, s / 2, s - m - 2);
      p.drawLine(s / 2, m + 2, s / 2 - 3, m + 6);
      p.drawLine(s / 2, m + 2, s / 2 + 3, m + 6);
      break;
    }
    case IconGlyph::Output: {
      p.drawRect(r);
      p.drawLine(s / 2, m + 2, s / 2, s - m - 6);
      p.drawLine(s / 2, s - m - 6, s / 2 - 3, s - m - 9);
      p.drawLine(s / 2, s - m - 6, s / 2 + 3, s - m - 9);
      break;
    }
    case IconGlyph::Interaction: {
      p.drawLine(m + 2, s / 2, s - m - 2, s / 2);
      p.drawLine(m + 2, s / 2, m + 6, s / 2 - 3);
      p.drawLine(m + 2, s / 2, m + 6, s / 2 + 3);
      p.drawLine(s - m - 2, s / 2, s - m - 6, s / 2 - 3);
      p.drawLine(s - m - 2, s / 2, s - m - 6, s / 2 + 3);
      break;
    }
    case IconGlyph::Job: {
      p.drawRect(r);
      p.drawLine(m + 2, m + 2, s - m - 2, s - m - 2);
      p.drawLine(m + 2, s - m - 2, s - m - 2, m + 2);
      break;
    }
    case IconGlyph::Result: {
      p.drawRect(r);
      p.drawLine(m + 2, s - m - 3, s / 2, s / 2);
      p.drawLine(s / 2, s / 2, s - m - 2, m + 3);
      break;
    }
    case IconGlyph::AddItem: {
      p.drawEllipse(r);
      p.drawLine(s / 2, m + 4, s / 2, s - m - 4);
      p.drawLine(m + 4, s / 2, s - m - 4, s / 2);
      break;
    }
    case IconGlyph::DuplicateItem: {
      QRect back(m + 4, m + 1, s - 2 * m - 5, s - 2 * m - 5);
      QRect front(m + 1, m + 4, s - 2 * m - 5, s - 2 * m - 5);
      p.drawRect(back);
      p.setBrush(QColor("#ffffff"));
      p.drawRect(front);
      break;
    }
    case IconGlyph::RenameItem: {
      // 铅笔: 斜向笔身 + 笔尖
      p.drawLine(m + 3, s - m - 3, s - m - 4, m + 2);
      p.drawLine(m + 5, s - m - 1, s - m - 2, m + 4);
      p.drawLine(m + 3, s - m - 3, m + 2, s - m - 1);
      p.drawLine(m + 2, s - m - 1, m + 5, s - m - 1);
      break;
    }
    case IconGlyph::RemoveItem: {
      // 垃圾桶: 桶身 + 盖子 + 提手
      p.drawLine(m + 2, m + 4, s - m - 2, m + 4);
      p.drawLine(s / 2 - 3, m + 4, s / 2 - 3, m + 2);
      p.drawLine(s / 2 - 3, m + 2, s / 2 + 3, m + 2);
      p.drawLine(s / 2 + 3, m + 2, s / 2 + 3, m + 4);
      QPolygon bin;
      bin << QPoint(m + 3, m + 4) << QPoint(s - m - 3, m + 4)
          << QPoint(s - m - 4, s - m - 1) << QPoint(m + 4, s - m - 1);
      p.drawPolyline(bin);
      break;
    }
    case IconGlyph::Undo:
    case IconGlyph::Redo: {
      const bool redo = glyph == IconGlyph::Redo;
      const QRect arc_rect(m + 2, m + 3, s - 2 * m - 4, s - 2 * m - 5);
      p.drawArc(arc_rect, (redo ? -35 : 35) * 16, (redo ? 250 : -250) * 16);
      const int x = redo ? s - m - 2 : m + 2;
      p.drawLine(x, m + 4, redo ? x - 4 : x + 4, m + 2);
      p.drawLine(x, m + 4, redo ? x - 2 : x + 2, m + 8);
      break;
    }
    case IconGlyph::Display: {
      p.drawRect(r);
      p.drawEllipse(r.adjusted(3, 3, -3, -3));
      break;
    }
    case IconGlyph::Pick: {
      QPolygon cursor;
      cursor << QPoint(m + 1, m + 1) << QPoint(s - m - 2, s / 2)
             << QPoint(s / 2 + 1, s / 2 + 1)
             << QPoint(s / 2 + 4, s - m - 1);
      p.drawPolyline(cursor);
      break;
    }
    case IconGlyph::ClearSelection: {
      p.drawLine(m + 2, m + 2, s - m - 2, s - m - 2);
      p.drawLine(m + 2, s - m - 2, s - m - 2, m + 2);
      break;
    }
    case IconGlyph::Slice: {
      p.drawRect(r);
      p.setBrush(QColor("#2f6fed"));
      p.drawRect(QRect(m + 5, m, 3, s - 2 * m));
      break;
    }
  }

  return QIcon(pix);
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("GMP-ISE");
  setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks |
                 QMainWindow::AllowTabbedDocks);
  if (auto* screen = QGuiApplication::primaryScreen()) {
    const QRect avail = screen->availableGeometry();
    const int w = qBound(980, int(avail.width() * 0.95), avail.width() - 24);
    const int h = qBound(700, int(avail.height() * 0.85), avail.height() - 24);
    resize(w, h);
    move(avail.x() + (avail.width() - width()) / 2,
         avail.y() + (avail.height() - height()) / 2);
  } else {
    resize(1440, 900);
  }

  build_menu();
  build_toolbar();
  if (const auto styles = QStyleFactory::keys(); styles.contains("Fusion")) {
    if (auto* fusion = QStyleFactory::create("Fusion")) {
      QApplication::setStyle(fusion);
    }
  }
  apply_theme();

  auto* central = new QWidget(this);
  auto* main_layout = new QVBoxLayout(central);
  main_layout->setContentsMargins(5, 4, 5, 4);
  main_layout->setSpacing(4);

  auto* module_bar = new QWidget(central);
  module_bar->setObjectName("moduleBar");
  auto* module_bar_layout = new QHBoxLayout(module_bar);
  module_bar_layout->setContentsMargins(6, 2, 6, 2);
  module_bar_layout->setSpacing(6);
  module_bar->setFixedHeight(36);

  module_tabs_ = new QTabBar(module_bar);
  module_tabs_->addTab("Sketch");
  module_tabs_->addTab("Part");
  module_tabs_->addTab("Property");
  module_tabs_->addTab("Material");
  module_tabs_->addTab("Section");
  module_tabs_->addTab("Assembly");
  module_tabs_->addTab("Step");
  module_tabs_->addTab("Interaction");
  module_tabs_->addTab("Load");
  module_tabs_->addTab("Mesh");
  module_tabs_->addTab("Job");
  module_tabs_->addTab("Visualization");
  module_tabs_->addTab("Results");
  // 保留 QTabBar 作为现有模块索引/信号的内部状态机；L-04 起不再作为
  // 可见导航，避免 13 个等宽页签长期占据顶部空间。
  module_tabs_->hide();

  auto* module_label = new QLabel("Module:", module_bar);
  module_selector_ = new QComboBox(module_bar);
  module_selector_->setObjectName("workContextModule");
  module_selector_->setMinimumWidth(132);
  for (int i = 0; i < module_tabs_->count(); ++i) {
    module_selector_->addItem(module_tabs_->tabText(i), i);
  }
  module_selector_->setToolTip("Select the active work module.");

  auto* project_label = new QLabel("Project:", module_bar);
  context_project_label_ = new QLabel("Untitled", module_bar);
  context_project_label_->setObjectName("workContextProject");
  context_project_label_->setMinimumWidth(100);
  context_project_label_->setMaximumWidth(180);
  context_project_label_->setToolTip("Current project (read-only).");

  auto* object_label = new QLabel("Object:", module_bar);
  context_object_selector_ = new QComboBox(module_bar);
  context_object_selector_->setObjectName("workContextObject");
  context_object_selector_->setMinimumWidth(150);
  context_object_selector_->setMaximumWidth(220);
  context_object_selector_->setToolTip(
      "Select the active object for the current module.");

  auto* module_toolbar = new QWidget(module_bar);
  module_toolbar->setObjectName("moduleToolbar");
  module_toolbar->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  module_toolbar->setFixedHeight(30);
  auto* module_toolbar_layout = new QHBoxLayout(module_toolbar);
  module_toolbar_layout->setContentsMargins(2, 0, 0, 0);
  module_toolbar_layout->setSpacing(4);
  auto* command_host = new QWidget(module_toolbar);
  auto* command_layout = new QHBoxLayout(command_host);
  command_layout->setContentsMargins(0, 0, 0, 0);
  command_layout->setSpacing(4);
  command_layout->addStretch(1);
  module_toolbar_layout->addWidget(command_host, 1);
  module_bar_layout->addWidget(module_label);
  module_bar_layout->addWidget(module_selector_);
  module_bar_layout->addSpacing(6);
  module_bar_layout->addWidget(project_label);
  module_bar_layout->addWidget(context_project_label_);
  module_bar_layout->addSpacing(6);
  module_bar_layout->addWidget(object_label);
  module_bar_layout->addWidget(context_object_selector_);
  module_bar_layout->addWidget(module_toolbar, 1);
  main_layout->addWidget(module_bar);

  auto module_tab_index = [this](const QString& label) {
    for (int i = 0; i < module_tabs_->count(); ++i) {
      if (module_tabs_->tabText(i) == label) {
        return i;
      }
    }
    return -1;
  };
  std::vector<std::vector<std::pair<QString, std::function<void()>>>>
      module_toolbar_actions(13);

  const auto part_tab = module_tab_index("Part");
  const auto property_tab = module_tab_index("Property");
  const auto material_tab = module_tab_index("Material");
  const auto section_tab = module_tab_index("Section");
  const auto assembly_tab = module_tab_index("Assembly");
  const auto step_tab = module_tab_index("Step");
  const auto interaction_tab = module_tab_index("Interaction");
  const auto load_tab = module_tab_index("Load");
  const auto sketch_tab = module_tab_index("Sketch");
  const auto mesh_tab = module_tab_index("Mesh");
  const auto job_tab = module_tab_index("Job");
  const auto viz_tab = module_tab_index("Visualization");
  const auto results_tab = module_tab_index("Results");

  auto assign_module_actions =
      [&module_toolbar_actions](int idx,
                               std::vector<std::pair<QString, std::function<void()>>> actions) {
        if (idx >= 0 && idx < static_cast<int>(module_toolbar_actions.size())) {
          module_toolbar_actions[idx] = std::move(actions);
        }
      };

  auto make_module_page = [](const QString& title,
                            const QString& description,
                            const std::vector<std::pair<QString, std::function<void()>>> &buttons) {
    auto* container = new QWidget();
    container->setObjectName("modulePage");
    auto* outer = new QVBoxLayout(container);
    outer->setObjectName("modulePageLayout");
    outer->setContentsMargins(18, 16, 18, 16);
    outer->setSpacing(10);

    auto* heading = new QLabel(title, container);
    heading->setObjectName("modulePageHeading");
    QFont hfont = heading->font();
    hfont.setPointSize(hfont.pointSize() + 2);
    hfont.setBold(true);
    heading->setFont(hfont);
    outer->addWidget(heading);

    auto* desc = new QLabel(description, container);
    desc->setObjectName("modulePageDescription");
    desc->setWordWrap(true);
    outer->addWidget(desc);

    if (!buttons.empty()) {
      auto* actions = new QWidget(container);
      actions->setObjectName("modulePrimaryActions");
      auto* actions_layout = new QHBoxLayout(actions);
      actions_layout->setContentsMargins(0, 2, 0, 2);
      actions_layout->setSpacing(8);
      for (const auto& button : buttons) {
        auto* btn = new QPushButton(button.first, actions);
        btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        const auto action = button.second;
        connect(btn, &QPushButton::clicked, container,
                [action]() { action(); });
        actions_layout->addWidget(btn);
      }
      actions_layout->addStretch(1);
      outer->addWidget(actions);
    }

    auto* separator = new QFrame(container);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    outer->addWidget(separator);
    outer->addStretch(1);
    return container;
  };

  auto make_module_node_page = [&](const QString& title,
                                   const QString& description,
                                   const QString& root_name,
                                   QListWidget*& list_out,
                                   const QString& empty_text,
                                   const QString& selected_label,
                                   const std::vector<std::pair<QString, std::function<void()>>> &buttons) {
    auto* container = make_module_page(title, description, buttons);
    auto* panel = new QWidget(container);
    panel->setObjectName("moduleNodeContent");
    auto* list_layout = new QVBoxLayout(panel);
    list_layout->setContentsMargins(0, 0, 0, 0);
    list_layout->setSpacing(6);

    auto* list = new QListWidget(panel);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setMinimumHeight(120);
    list->setAlternatingRowColors(true);
    list->setToolTip("Double click item to jump to model tree.");
    list_out = list;

    auto* refresh_btn = new QPushButton("Refresh", panel);
    auto* list_action_row = new QHBoxLayout();
    list_action_row->addStretch(1);
    list_action_row->addWidget(refresh_btn);
    auto* list_action_bar = new QWidget(panel);
    list_action_bar->setLayout(list_action_row);

    list_layout->addWidget(new QLabel("Current entries:", panel));
    list_layout->addWidget(list, 1);
    list_layout->addWidget(list_action_bar);
    auto* page = qobject_cast<QVBoxLayout*>(container->layout());
    if (page) {
      if (page->count() > 0 && page->itemAt(page->count() - 1)->spacerItem()) {
        delete page->takeAt(page->count() - 1);
      }
      page->addWidget(panel, 1);
    }

    const QString safe_root = root_name;
    const auto resolve_selected_item = [this, list, safe_root]() -> QTreeWidgetItem* {
      if (!list || !model_tree_) {
        return nullptr;
      }
      const auto* current = list->currentItem();
      if (!current) {
        return nullptr;
      }
      auto* root = find_root_item(safe_root);
      if (!root) {
        return nullptr;
      }
      const int row = list->row(const_cast<QListWidgetItem*>(current));
      if (row < 0 || row >= root->childCount()) {
        return nullptr;
      }
      return root->child(row);
    };

    auto* open_selected_btn =
        new QPushButton(QString("Open Selected %1").arg(selected_label), panel);
    auto* rename_btn = new QPushButton("Rename", panel);
    auto* duplicate_btn = new QPushButton("Duplicate", panel);
    auto* remove_btn = new QPushButton("Remove", panel);
    connect(open_selected_btn, &QPushButton::clicked, this,
            [this, resolve_selected_item, module_tab_index]() {
      auto* target = resolve_selected_item();
      if (!target) {
        return;
      }
      model_tree_->setCurrentItem(target);
      const int prop_tab = module_tab_index("Property");
      if (prop_tab >= 0) {
        module_tabs_->setCurrentIndex(prop_tab);
      }
      property_editor_->set_item(target);
    });
    connect(rename_btn, &QPushButton::clicked, this,
            [this, resolve_selected_item]() {
      if (auto* target = resolve_selected_item()) {
        rename_item(target);
      }
    });
    connect(duplicate_btn, &QPushButton::clicked, this, [this, resolve_selected_item]() {
      duplicate_item(resolve_selected_item());
    });
    connect(remove_btn, &QPushButton::clicked, this, [this, resolve_selected_item]() {
      remove_item(resolve_selected_item());
    });
    connect(list, &QListWidget::itemDoubleClicked, this,
            [this, list, safe_root, module_tab_index](QListWidgetItem*) {
              // Part 使用专用的工作窗编辑入口，在 part_page 创建后单独接线。
              if (safe_root == "Parts") {
                return;
              }
              const auto* item = list->currentItem();
              if (!item || !model_tree_) {
                return;
              }
              const int row = list->row(const_cast<QListWidgetItem*>(item));
              if (row < 0) {
                return;
              }
              auto* root = find_root_item(safe_root);
              if (!root || row < 0 || row >= root->childCount()) {
                return;
              }
              auto* target = root->child(row);
              if (target) {
                model_tree_->setCurrentItem(target);
                const int prop_tab = module_tab_index("Property");
                if (prop_tab >= 0) {
                  module_tabs_->setCurrentIndex(prop_tab);
                }
                property_editor_->set_item(target);
              }
            });
    connect(refresh_btn, &QPushButton::clicked, this, [this]() {
      refresh_module_pages();
    });

    list_action_row->insertWidget(0, open_selected_btn);
    list_action_row->insertWidget(1, rename_btn);
    list_action_row->insertWidget(2, duplicate_btn);
    list_action_row->insertWidget(3, remove_btn);
    return container;
  };

  vertical_split_ = new QSplitter(Qt::Vertical, central);
  auto* vertical_split = vertical_split_;
  vertical_split->setObjectName("mainVerticalSplit");
  vertical_split->setChildrenCollapsible(false);
  main_layout->addWidget(vertical_split, 1);

  // 不允许整栏折叠消失; 内容最小宽度已通过紧凑样式压小, 可拖到很窄
  main_split_ = new QSplitter(Qt::Horizontal, vertical_split);
  auto* main_split = main_split_;
  main_split->setObjectName("mainHorizontalSplit");
  main_split->setChildrenCollapsible(false);
  main_split->setHandleWidth(4);
  vertical_split->addWidget(main_split);

  auto* tree_panel = new QFrame(main_split);
  tree_panel->setObjectName("treePanel");
  tree_panel->setFrameShape(QFrame::StyledPanel);
  tree_panel->setFrameShadow(QFrame::Sunken);
  auto* tree_outer = new QVBoxLayout(tree_panel);
  tree_outer->setContentsMargins(0, 0, 0, 0);
  tree_outer->setSpacing(0);

  navigation_tabs_ = new QTabWidget(tree_panel);
  navigation_tabs_->setObjectName("navigationTabs");
  tree_outer->addWidget(navigation_tabs_, 1);

  auto* model_navigation_page = new QWidget(navigation_tabs_);
  auto* tree_layout = new QVBoxLayout(model_navigation_page);
  tree_layout->setContentsMargins(6, 6, 6, 6);
  tree_layout->setSpacing(6);
  navigation_tabs_->addTab(model_navigation_page, "Model");

  auto* model_filter_row = new QHBoxLayout();
  model_tree_filter_ = new QLineEdit(model_navigation_page);
  model_tree_filter_->setObjectName("modelTreeFilter");
  model_tree_filter_->setClearButtonEnabled(true);
  model_tree_filter_->setPlaceholderText("Filter model objects...");
  model_filter_row->addWidget(model_tree_filter_, 1);
  tree_layout->addLayout(model_filter_row);

  workflow_status_label_ = new QLabel("Workflow: Part [0], Material [0], Section [0], "
                                     "Steps [0], BC [0], Loads [0], Mesh [0]",
                                     model_navigation_page);
  workflow_status_label_->setObjectName("workflowStatus");
  workflow_status_label_->setWordWrap(true);
  workflow_status_label_->setTextFormat(Qt::PlainText);
  workflow_status_label_->setStyleSheet("color: #404040;");
  tree_layout->addWidget(workflow_status_label_);

  auto* tree_actions = new QHBoxLayout();
  auto make_tree_action = [](IconGlyph glyph, const QString& tip) {
    auto* b = new QPushButton();
    b->setIcon(MakeIcon(glyph, 16));
    b->setIconSize(QSize(16, 16));
    b->setFixedSize(30, 30);
    b->setToolTip(tip);
    return b;
  };
  auto* add_btn = make_tree_action(IconGlyph::AddItem, "Add");
  auto* dup_btn = make_tree_action(IconGlyph::DuplicateItem, "Duplicate");
  auto* rename_btn = make_tree_action(IconGlyph::RenameItem, "Rename");
  auto* remove_btn = make_tree_action(IconGlyph::RemoveItem, "Remove");
  tree_actions->addWidget(add_btn);
  tree_actions->addWidget(dup_btn);
  tree_actions->addWidget(rename_btn);
  tree_actions->addWidget(remove_btn);
  tree_actions->addStretch(1);
  auto* tree_actions_container = new QWidget(model_navigation_page);
  tree_actions_container->setLayout(tree_actions);
  tree_layout->addWidget(tree_actions_container);

  model_tree_ = new QTreeWidget(model_navigation_page);
  model_tree_->setObjectName("modelTree");
  model_tree_->setColumnCount(2);
  model_tree_->setHeaderLabels({"Object", "Status"});
  model_tree_->header()->setSectionResizeMode(QHeaderView::Interactive);
  model_tree_->header()->setStretchLastSection(false);
  model_tree_->header()->setSectionsMovable(false);
  model_tree_->header()->setMinimumSectionSize(60);
  model_tree_->header()->resizeSection(0, 165);
  model_tree_->header()->resizeSection(1, 100);
  model_tree_->setUniformRowHeights(true);
  // 不设最小宽度: 允许分割条自由拖动, 过窄时树内部出现横向滚动条
  model_tree_->setEditTriggers(QAbstractItemView::SelectedClicked |
                               QAbstractItemView::EditKeyPressed);
  tree_layout->addWidget(model_tree_, 1);
  build_model_tree();

  auto* results_navigation_page = new QWidget(navigation_tabs_);
  auto* results_navigation_layout = new QVBoxLayout(results_navigation_page);
  results_navigation_layout->setContentsMargins(6, 6, 6, 6);
  results_navigation_layout->setSpacing(6);
  navigation_tabs_->addTab(results_navigation_page, "Results");

  auto* results_filter_row = new QHBoxLayout();
  results_tree_filter_ = new QLineEdit(results_navigation_page);
  results_tree_filter_->setObjectName("resultsTreeFilter");
  results_tree_filter_->setClearButtonEnabled(true);
  results_tree_filter_->setPlaceholderText("Filter jobs and results...");
  results_filter_row->addWidget(results_tree_filter_, 1);
  results_navigation_layout->addLayout(results_filter_row);

  results_navigation_tree_ = new QTreeWidget(results_navigation_page);
  results_navigation_tree_->setObjectName("resultsNavigationTree");
  results_navigation_tree_->setColumnCount(2);
  results_navigation_tree_->setHeaderLabels({"Result / Job", "Status"});
  results_navigation_tree_->header()->setSectionResizeMode(
      QHeaderView::Interactive);
  results_navigation_tree_->header()->setStretchLastSection(false);
  results_navigation_tree_->header()->setSectionsMovable(false);
  results_navigation_tree_->header()->setMinimumSectionSize(60);
  results_navigation_tree_->header()->resizeSection(0, 165);
  results_navigation_tree_->header()->resizeSection(1, 100);
  results_navigation_tree_->setUniformRowHeights(true);
  results_navigation_layout->addWidget(results_navigation_tree_, 1);

  connect(model_tree_filter_, &QLineEdit::textChanged, this,
          &MainWindow::apply_model_tree_filter);
  connect(results_tree_filter_, &QLineEdit::textChanged, this,
          &MainWindow::apply_results_tree_filter);
  connect(navigation_tabs_, &QTabWidget::currentChanged, this,
          [this](int index) {
            if (index == 1) {
              refresh_results_navigation();
            }
          });
  connect(results_navigation_tree_, &QTreeWidget::itemSelectionChanged, this,
          [this]() {
            select_model_item_from_results_navigation(
                results_navigation_tree_->currentItem());
          });
  // 结果导航树右键菜单：根节点工作窗入口/根级操作，子节点按类型分流。
  results_navigation_tree_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(results_navigation_tree_, &QWidget::customContextMenuRequested, this,
          [this](const QPoint& pos) {
            auto* item = results_navigation_tree_->itemAt(pos);
            if (!item) {
              return;
            }
            results_navigation_tree_->setCurrentItem(item);
            QMenu menu(this);
            build_results_navigation_menu(&menu, item);
            l10n::apply(&menu);
            if (!menu.isEmpty()) {
              menu.exec(results_navigation_tree_->viewport()->mapToGlobal(pos));
            }
          });
  connect(results_navigation_tree_, &QTreeWidget::itemDoubleClicked, this,
          [this](QTreeWidgetItem* item, int) {
            if (!item || !item->parent()) {
              return;
            }
            select_model_item_from_results_navigation(item);
            const QString kind = item->data(0, kNavigationKindRole).toString();
            const QString path = item->data(0, kNavigationPathRole).toString();
            if (kind == "Results") {
              if (!path.isEmpty() && viewer_) {
                const QString ext = QFileInfo(path).suffix().toLower();
                if (ext == "e" || ext == "exo" || ext == "exodus") {
                  viewer_->set_exodus_file(path);
                } else if (ext == "msh") {
                  viewer_->set_mesh_file(path);
                }
              }
              if (results_work_window_) {
                results_work_window_->show();
                results_work_window_->raise();
                results_work_window_->activateWindow();
              }
            } else if (kind == "Jobs" && job_work_window_) {
              job_work_window_->show();
              job_work_window_->raise();
              job_work_window_->activateWindow();
            }
          });
  refresh_results_navigation();

  auto* center_panel = new QFrame(main_split);
  center_panel->setObjectName("centerPanel");
  center_panel->setFrameShape(QFrame::StyledPanel);
  center_panel->setFrameShadow(QFrame::Sunken);
  auto* center_layout = new QVBoxLayout(center_panel);
  center_layout->setContentsMargins(0, 0, 0, 0);
  center_layout->setSpacing(3);
  auto* center_title = new QLabel("Viewport", center_panel);
  QFont center_title_font = center_title->font();
  center_title_font.setBold(true);
  center_title->setFont(center_title_font);
  center_layout->addWidget(center_title);
  auto* center_tabs = new QTabWidget(center_panel);
  viewer_ = new VtkViewer(center_tabs);
  center_tabs->addTab(viewer_, "Viewport");
  auto* plot_page = new QWidget();
  auto* plot_layout = new QVBoxLayout(plot_page);
  plot_layout->setContentsMargins(8, 8, 8, 8);
  plot_layout->setSpacing(6);
  auto* plot_head = new QLabel("Plot Preview (from active dataset)", plot_page);
  QFont plot_font = plot_head->font();
  plot_font.setBold(true);
  plot_head->setFont(plot_font);
  plot_layout->addWidget(plot_head);
  auto* plot_open_row = new QHBoxLayout();
  auto* plot_open_btn = new QPushButton("Open Visualization", plot_page);
  auto* plot_refresh_btn = new QPushButton("Refresh", plot_page);
  auto* plot_help = new QLabel("Tip: full visualization is in Visualization module.", plot_page);
  auto* plot_status = new QLabel("No data", plot_page);
  plot_open_row->addWidget(plot_open_btn);
  plot_open_row->addWidget(plot_refresh_btn);
  plot_open_row->addWidget(plot_status, 1);
  plot_open_row->addWidget(plot_help);
  plot_layout->addLayout(plot_open_row);
  auto* plot_view = new QPlainTextEdit(plot_page);
  plot_view->setReadOnly(true);
  plot_view->setLineWrapMode(QPlainTextEdit::NoWrap);
  QFont mono;
  mono.setFamilies({"SFMono-Regular", "Monaco", "Consolas", "Menlo"});
  mono.setStyleHint(QFont::Monospace);
  plot_view->setFont(mono);
  plot_layout->addWidget(plot_view, 1);

  auto* table_page = new QWidget();
  auto* table_layout = new QVBoxLayout(table_page);
  table_layout->setContentsMargins(8, 8, 8, 8);
  table_layout->setSpacing(6);
  auto* table_head = new QLabel("Table Preview (from active dataset)", table_page);
  QFont table_font = table_head->font();
  table_font.setBold(true);
  table_head->setFont(table_font);
  table_layout->addWidget(table_head);
  auto* table_open_row = new QHBoxLayout();
  auto* table_open_btn = new QPushButton("Open Visualization", table_page);
  auto* table_refresh_btn = new QPushButton("Refresh", table_page);
  auto* table_status = new QLabel("No data", table_page);
  table_open_row->addWidget(table_open_btn);
  table_open_row->addWidget(table_refresh_btn);
  table_open_row->addWidget(table_status, 1);
  table_layout->addLayout(table_open_row);
  auto* table_view = new QPlainTextEdit(table_page);
  table_view->setReadOnly(true);
  table_view->setLineWrapMode(QPlainTextEdit::NoWrap);
  table_view->setFont(mono);
  table_layout->addWidget(table_view, 1);
  // 中央区域只保留 Viewport；Plot/Table 在 Results 工作窗中展示。
  center_tabs->tabBar()->hide();
  center_tabs->setCurrentIndex(0);
  // 中栏包滚动区: 视口控制行最宽可达 1300+px, 不允许它撑死布局;
  // 窗口较窄时由内部横向滚动条承载, 分割条保持可拖动
  auto* center_scroll = new QScrollArea(center_panel);
  center_scroll->setWidgetResizable(true);
  center_scroll->setFrameShape(QFrame::NoFrame);
  center_scroll->setMinimumWidth(0);
  center_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  center_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  center_scroll->setWidget(center_tabs);
  auto* stage_host = new QWidget(center_panel);
  auto* stage_layout = new QHBoxLayout(stage_host);
  stage_layout->setContentsMargins(0, 0, 0, 0);
  stage_layout->setSpacing(2);
  auto* stage_toolbar_scroll = new QScrollArea(stage_host);
  stage_toolbar_scroll->setObjectName("stageLeftToolbarScroll");
  stage_toolbar_scroll->setWidgetResizable(true);
  stage_toolbar_scroll->setFrameShape(QFrame::NoFrame);
  stage_toolbar_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  stage_toolbar_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  stage_toolbar_scroll->setStyleSheet("QScrollBar:vertical { width: 4px; }");
  stage_toolbar_scroll->setFixedWidth(48);
  stage_left_toolbar_ = new StageLeftToolbar();
  stage_toolbar_scroll->setWidget(stage_left_toolbar_);
  stage_layout->addWidget(stage_toolbar_scroll);
  stage_layout->addWidget(center_scroll, 1);
  center_layout->addWidget(stage_host, 1);

  module_work_window_ = new QDockWidget("Module Workspace", this);
  module_work_window_->setObjectName("moduleWorkspaceWindow");
  module_work_window_->setFeatures(QDockWidget::DockWidgetClosable |
                                   QDockWidget::DockWidgetMovable |
                                   QDockWidget::DockWidgetFloatable);
  module_work_window_->setMinimumSize(620, 400);
  module_work_window_->resize(680, 560);
  addDockWidget(Qt::RightDockWidgetArea, module_work_window_);
  module_work_window_->setFloating(true);
  module_work_window_->setAllowedAreas(Qt::NoDockWidgetArea);
  // 双击标题栏会切换 floating；工作窗不允许停靠（NoDockWidgetArea），
  // 直接进入非法吸附态（贴边小窗）。统一守卫：一律保持浮动。
  connect(module_work_window_, &QDockWidget::topLevelChanged, this,
          [this](bool floating) {
            if (!floating && module_work_window_) {
              module_work_window_->setFloating(true);
            }
          });

  auto* property_panel = new QFrame(module_work_window_);
  property_panel->setObjectName("propertyPanel");
  property_panel->setFrameShape(QFrame::StyledPanel);
  property_panel->setFrameShadow(QFrame::Sunken);
  auto* property_layout = new QVBoxLayout(property_panel);
  property_layout->setContentsMargins(0, 0, 0, 0);
  property_layout->setSpacing(0);

  property_stack_ = new CurrentPageStackedWidget(property_panel);
  property_stack_->setObjectName("moduleWorkspaceStack");
  // 页面直接进入栈；禁止再套兼容右栏滚动层。需要滚动的复杂页面只允许
  // 在自身内部保留一层滚动，从结构上消除双竖向滚动条。
  property_layout->addWidget(property_stack_, 1);
  module_work_window_->setWidget(property_panel);
  module_work_window_->hide();
  if (view_menu_) {
    auto* toggle_workspace = module_work_window_->toggleViewAction();
    toggle_workspace->setText("Module Workspace");
    view_menu_->addAction(toggle_workspace);
  }

  auto make_floating_workspace = [this](const QString& title,
                                        const QString& object_name,
                                        const QSize& initial_size) {
    auto* workspace = new QDockWidget(title, this);
    workspace->setObjectName(object_name);
    workspace->setFeatures(QDockWidget::DockWidgetClosable |
                           QDockWidget::DockWidgetMovable |
                           QDockWidget::DockWidgetFloatable);
    workspace->setMinimumSize(400, 240);
    workspace->resize(initial_size);
    addDockWidget(Qt::RightDockWidgetArea, workspace);
    workspace->setFloating(true);
    workspace->setAllowedAreas(Qt::NoDockWidgetArea);
    // 双击标题栏守卫：无停靠区工作窗一律保持浮动，避免非法吸附态。
    connect(workspace, &QDockWidget::topLevelChanged, workspace,
            [this, workspace](bool floating) {
              if (!floating) {
                workspace->setFloating(true);
              }
              // 拖出/拖回后向窗口系统显式请求重绘，避免工具条区域
              // 原生合成层滞留空白。
              if (windowHandle()) {
                windowHandle()->requestUpdate();
              }
            });
    workspace->hide();
    if (view_menu_) {
      auto* toggle = workspace->toggleViewAction();
      toggle->setText(title);
      view_menu_->addAction(toggle);
    }
    return workspace;
  };
  job_work_window_ = make_floating_workspace(
      "Job Workspace", "jobWorkspaceWindow", QSize(820, 560));
  visualization_work_window_ = make_floating_workspace(
      "Visualization Workspace", "visualizationWorkspaceWindow",
      QSize(660, 540));
  results_work_window_ = make_floating_workspace(
      "Results Workspace", "resultsWorkspaceWindow", QSize(640, 400));
  mesh_work_window_ = make_floating_workspace(
      "Mesh Workspace", "meshWorkspaceWindow", QSize(760, 520));

  main_split->addWidget(tree_panel);
  main_split->addWidget(center_panel);

  std::function<void(int)> apply_toolbar_actions;

  property_editor_ = new PropertyEditor(property_stack_);
  auto* mesh_page = new GmshPanel(property_stack_);
  auto* job_page = new MoosePanel(property_stack_);
  moose_panel_ = job_page;
  gmsh_panel_ = mesh_page;

  auto* job_container = new QWidget(property_stack_);
  auto* job_layout = new QVBoxLayout(job_container);
  job_layout->setContentsMargins(0, 0, 0, 0);
  job_layout->setSpacing(0);

  auto* job_tabs = new QTabWidget(job_container);
  job_tabs->setObjectName("jobWorkspaceTabs");
  auto* job_manager_page = new QWidget(job_tabs);
  auto* job_manager_layout = new QVBoxLayout(job_manager_page);
  job_manager_layout->setContentsMargins(8, 8, 8, 8);
  job_manager_layout->setSpacing(8);

  auto* job_actions = new QHBoxLayout();
  auto* job_run_btn = new QPushButton("Run");
  auto* job_stop_btn = new QPushButton("Stop");
  auto* job_retry_btn = new QPushButton("Retry");
  job_run_button_ = job_run_btn;
  job_stop_button_ = job_stop_btn;
  job_retry_button_ = job_retry_btn;
  auto* job_log_btn = new QPushButton("Open Log");
  auto* job_result_btn = new QPushButton("Open Result");
  job_actions->addWidget(job_run_btn);
  job_actions->addWidget(job_stop_btn);
  job_actions->addWidget(job_retry_btn);
  job_actions->addWidget(job_log_btn);
  job_actions->addWidget(job_result_btn);
  // 作业监控筛选与刷新（参照 LIMS 任务监控）。
  auto* filter_label = new QLabel("  State:", job_manager_page);
  job_state_filter_ = new QComboBox(job_manager_page);
  job_state_filter_->setObjectName("jobStateFilter");
  job_state_filter_->addItem("All", "all");
  job_state_filter_->addItem("Queued", "Queued");
  job_state_filter_->addItem("Running", "Running");
  job_state_filter_->addItem("Completed", "Completed");
  job_state_filter_->addItem("Failed", "Failed");
  job_state_filter_->addItem("Canceled", "Canceled");
  auto* job_refresh_btn = new QPushButton("Refresh", job_manager_page);
  job_refresh_btn->setObjectName("jobMonitorRefresh");
  job_auto_refresh_ = new QCheckBox("Auto (5s)", job_manager_page);
  job_auto_refresh_->setObjectName("jobAutoRefresh");
  job_auto_refresh_->setChecked(true);
  job_actions->addWidget(filter_label);
  job_actions->addWidget(job_state_filter_);
  job_actions->addWidget(job_refresh_btn);
  job_actions->addWidget(job_auto_refresh_);
  job_actions->addStretch(1);
  auto* job_actions_container = new QWidget(job_manager_page);
  job_actions_container->setLayout(job_actions);
  job_manager_layout->addWidget(job_actions_container);

  auto* job_info_split = new QSplitter(Qt::Horizontal, job_manager_page);
  job_info_split->setChildrenCollapsible(false);
  job_table_ = new QTableWidget(job_info_split);
  job_table_->setColumnCount(9);
  job_table_->setHorizontalHeaderLabels({"Name", "Status", "Case", "Progress",
                                         "Start", "Duration", "Type", "Exec",
                                         "Result"});
  job_table_->horizontalHeader()->setStretchLastSection(true);
  job_table_->verticalHeader()->setVisible(false);
  job_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  job_table_->setSelectionMode(QAbstractItemView::SingleSelection);
  job_table_->setMinimumHeight(58);
  job_table_->setMinimumWidth(420);

  // 右侧详情面板：占位页 / 内容页。
  job_detail_stack_ = new QStackedWidget(job_info_split);
  auto* detail_placeholder = new QLabel("Select a job to view details.",
                                        job_detail_stack_);
  detail_placeholder->setAlignment(Qt::AlignCenter);
  job_detail_stack_->addWidget(detail_placeholder);

  auto* detail_scroll = new QScrollArea(job_detail_stack_);
  detail_scroll->setWidgetResizable(true);
  detail_scroll->setFrameShape(QFrame::NoFrame);
  auto* detail_content = new QWidget(detail_scroll);
  auto* detail_layout = new QVBoxLayout(detail_content);
  detail_layout->setContentsMargins(10, 10, 10, 10);
  detail_layout->setSpacing(8);

  job_detail_title_ = new QLabel("-", detail_content);
  job_detail_title_->setObjectName("jobDetailTitle");
  job_detail_title_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  QFont detail_title_font = job_detail_title_->font();
  detail_title_font.setBold(true);
  detail_title_font.setPointSize(detail_title_font.pointSize() + 1);
  job_detail_title_->setFont(detail_title_font);
  job_detail_title_->setWordWrap(true);
  detail_layout->addWidget(job_detail_title_);

  auto* detail_btns = new QHBoxLayout();
  auto* detail_refresh_btn = new QPushButton("Refresh", detail_content);
  job_cancel_button_ = new QPushButton("Cancel", detail_content);
  job_cancel_button_->setObjectName("jobRemoteCancel");
  auto* detail_taskmd_btn = new QPushButton("task.md", detail_content);
  auto* detail_log_btn = new QPushButton("Log", detail_content);
  auto* detail_result_btn = new QPushButton("Result", detail_content);
  detail_btns->addWidget(detail_refresh_btn);
  detail_btns->addWidget(job_cancel_button_);
  detail_btns->addWidget(detail_taskmd_btn);
  detail_btns->addWidget(detail_log_btn);
  detail_btns->addWidget(detail_result_btn);
  detail_btns->addStretch(1);
  auto* detail_btns_row = new QWidget(detail_content);
  detail_btns_row->setLayout(detail_btns);
  detail_layout->addWidget(detail_btns_row);

  job_progress_bar_ = new QProgressBar(detail_content);
  job_progress_bar_->setObjectName("jobProgressBar");
  job_progress_bar_->setRange(0, 100);
  job_progress_bar_->setValue(0);
  job_progress_text_ = new QLabel("-", detail_content);
  job_progress_text_->setObjectName("jobProgressText");
  job_progress_text_->setWordWrap(true);
  detail_layout->addWidget(job_progress_bar_);
  detail_layout->addWidget(job_progress_text_);

  auto* detail_grid_box = new QGroupBox("Execution Details", detail_content);
  auto* detail_grid = new QFormLayout(detail_grid_box);
  detail_grid->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  const QStringList field_keys = {"input_file", "pid",      "parallel",
                                  "cpu",        "memory",   "step",
                                  "dt",         "physical", "converged",
                                  "avg_step",   "elapsed",  "eta",
                                  "heartbeat",  "health"};
  const QStringList field_names = {"Input",      "PID",       "Parallel",
                                   "CPU",        "Memory",    "Step",
                                   "dt",         "Phy. Time", "Converged",
                                   "Avg Step",   "Elapsed",   "ETA",
                                   "Heartbeat",  "Health"};
  for (int i = 0; i < field_keys.size(); ++i) {
    auto* value = new QLabel("-", detail_grid_box);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    value->setObjectName("jobDetail_" + field_keys.at(i));
    detail_grid->addRow(field_names.at(i) + ":", value);
    job_detail_fields_.insert(field_keys.at(i), value);
  }
  detail_layout->addWidget(detail_grid_box);

  auto* files_box = new QGroupBox("Artifacts", detail_content);
  auto* files_layout = new QVBoxLayout(files_box);
  auto* files_btn_row = new QHBoxLayout();
  auto* files_refresh_btn = new QPushButton("Refresh Files", files_box);
  auto* files_download_btn = new QPushButton("Download Selected", files_box);
  files_download_btn->setObjectName("jobFileDownload");
  files_btn_row->addWidget(files_refresh_btn);
  files_btn_row->addWidget(files_download_btn);
  files_btn_row->addStretch(1);
  files_layout->addLayout(files_btn_row);
  job_files_table_ = new QTableWidget(files_box);
  job_files_table_->setObjectName("jobFilesTable");
  job_files_table_->setColumnCount(5);
  job_files_table_->setHorizontalHeaderLabels(
      {"Kind", "Name", "Size", "Modified", "Snapshot"});
  job_files_table_->horizontalHeader()->setStretchLastSection(true);
  job_files_table_->verticalHeader()->setVisible(false);
  job_files_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  job_files_table_->setSelectionMode(QAbstractItemView::SingleSelection);
  job_files_table_->setMinimumHeight(90);
  files_layout->addWidget(job_files_table_);
  detail_layout->addWidget(files_box);

  auto* local_box = new QGroupBox("Local Job", detail_content);
  auto* local_layout = new QVBoxLayout(local_box);
  job_detail_ = new QPlainTextEdit(local_box);
  job_detail_->setReadOnly(true);
  job_detail_->setPlaceholderText("Local job details.");
  job_detail_->setMaximumHeight(160);
  local_layout->addWidget(job_detail_);
  detail_layout->addWidget(local_box);
  detail_layout->addStretch(1);
  detail_scroll->setWidget(detail_content);
  job_detail_stack_->addWidget(detail_scroll);
  job_detail_stack_->setCurrentIndex(0);

  job_info_split->addWidget(job_table_);
  job_info_split->addWidget(job_detail_stack_);
  job_info_split->setStretchFactor(0, 3);
  job_info_split->setStretchFactor(1, 2);
  job_manager_layout->addWidget(job_info_split, 1);

  job_tabs->addTab(job_manager_page, "Jobs");
  job_tabs->addTab(job_page, "MOOSE Setup");
  job_layout->addWidget(job_tabs, 1);

  // 部件的显式编辑入口：新建、模型树双击和部件列表打开都复用此路径。
  const auto open_part_editor = [this, part_tab](QTreeWidgetItem* target) {
    if (!target ||
        target->data(0, PropertyEditor::kKindRole).toString() != "Parts") {
      statusBar()->showMessage("Select a part first.", 2000);
      return;
    }
    if (module_tabs_->currentIndex() != part_tab) {
      module_tabs_->setCurrentIndex(part_tab);
    }
    // currentChanged 在同一页签不会再次发出，显式保证工作窗显示 Part 页面。
    if (property_stack_->count() > 1) {
      property_stack_->setCurrentIndex(1);
    }
    model_tree_->setCurrentItem(target);
    refresh_module_pages();
    if (module_part_list_ && target->parent()) {
      module_part_list_->setCurrentRow(target->parent()->indexOfChild(target));
    }
    module_work_window_->setWindowTitle(
        QString("Part Editor — %1").arg(target->text(0)));
    module_work_window_->show();
    module_work_window_->raise();
    module_work_window_->activateWindow();
    statusBar()->showMessage(
        QString("Editing part '%1'.").arg(target->text(0)), 4000);
  };

  // 新建部件: 有草图时弹框选择草图; 无草图时跳到草图页签提示先建草图
  std::function<void()> create_part_from_sketch =
      [this, module_tab_index, open_part_editor]() {
    auto* sketches_root = find_root_item("Sketches");
    if (!sketches_root || sketches_root->childCount() == 0) {
      QMessageBox::information(
          this, "New Part",
          "No sketches yet. Create a sketch in the Sketch module first.");
      const int target = module_tab_index("Sketch");
      if (target >= 0) {
        module_tabs_->setCurrentIndex(target);
      }
      return;
    }
    QStringList names;
    for (int i = 0; i < sketches_root->childCount(); ++i) {
      if (auto* child = sketches_root->child(i)) {
        names << child->text(0);
      }
    }
    bool ok = false;
    const QString chosen =
        QInputDialog::getItem(this, "New Part", "Select sketch for the new part:",
                              names, 0, false, &ok);
    if (!ok || chosen.isEmpty()) {
      return;
    }
    if (auto* root = find_root_item("Parts")) {
      int suffix = root->childCount() + 1;
      QString name;
      bool exists = false;
      do {
        name = QString("part_%1").arg(suffix++);
        exists = false;
        for (int i = 0; i < root->childCount(); ++i) {
          if (root->child(i) && root->child(i)->text(0) == name) {
            exists = true;
            break;
          }
        }
      } while (exists);
      if (auto* item = add_child_item(
              root, name, "Parts",
              {{"type", "Part"}, {"sketch", chosen}})) {
        open_part_editor(item);
      }
    }
  };

  auto* part_page = make_module_node_page(
      "Part",
      "Manage part-level entities. Use Part Features below to turn a sketch into 3D (extrude, revolve, loft, sweep); the result is meshed and shown in the viewport.",
      "Parts",
      module_part_list_,
      "No parts yet. Create one from this module or Gmsh panel.",
      "part",
      {
          {"Open Parts Root",
           [this]() {
             auto* root = find_root_item("Parts");
             if (root) {
               model_tree_->setCurrentItem(root);
               root->setExpanded(true);
             }
           }},
          {"New Part",
            [create_part_from_sketch]() {
             create_part_from_sketch();
           }},
          {"Open Gmsh Panel",
           [this, module_tab_index]() {
             const int mesh_tab = module_tab_index("Mesh");
             if (mesh_tab >= 0) {
               module_tabs_->setCurrentIndex(mesh_tab);
             }
           }},
      });

  // 特征操作区 (WS3): 在模块页的说明/主操作之后、部件列表之前显示。
  // PartFeaturePanel 自身使用操作页签，避免四组长表单纵向堆叠。
  part_feature_panel_ = new PartFeaturePanel(part_page);
  if (auto* page_layout = qobject_cast<QVBoxLayout*>(part_page->layout())) {
    if (auto* node_content =
            part_page->findChild<QWidget*>("moduleNodeContent")) {
      const int node_index = page_layout->indexOf(node_content);
      page_layout->insertWidget(qMax(0, node_index), part_feature_panel_);
    }
  }

  // Part 的列表双击改为“打开部件编辑工作窗”，不沿用其他模块跳到属性页
  // 的默认语义。
  connect(module_part_list_, &QListWidget::itemDoubleClicked, this,
          [this, open_part_editor](QListWidgetItem* item) {
            auto* root = find_root_item("Parts");
            const int row = module_part_list_ ? module_part_list_->row(item) : -1;
            if (root && row >= 0 && row < root->childCount()) {
              open_part_editor(root->child(row));
            }
          });
  connect(module_part_list_, &QListWidget::currentRowChanged, this,
          [this](int row) {
            auto* root = find_root_item("Parts");
            if (root && row >= 0 && row < root->childCount() &&
                model_tree_->currentItem() != root->child(row)) {
              model_tree_->setCurrentItem(root->child(row));
            }
          });
  for (auto* button : part_page->findChildren<QPushButton*>()) {
    if (button && button->text() == "Open Selected part") {
      disconnect(button, nullptr, this, nullptr);
      connect(button, &QPushButton::clicked, this,
              [this, open_part_editor]() {
                auto* root = find_root_item("Parts");
                const int row = module_part_list_
                                    ? module_part_list_->currentRow()
                                    : -1;
                if (root && row >= 0 && row < root->childCount()) {
                  open_part_editor(root->child(row));
                }
              });
      break;
    }
  }

  // 按草图名从模型树加载草图文档 (params["data"] 为 YAML 字符串)
  auto load_sketch_doc = [this](const QString& name, SketchDocument* out,
                                QString* error) -> bool {
    auto* root = find_root_item("Sketches");
    for (int i = 0; root && i < root->childCount(); ++i) {
      auto* child = root->child(i);
      if (child && child->text(0) == name) {
        const QVariantMap params =
            child->data(0, PropertyEditor::kParamsRole).toMap();
        const QString data = params.value("data").toString();
        if (data.trimmed().isEmpty()) {
          if (error) {
            *error = QString("Sketch '%1' is empty.").arg(name);
          }
          return false;
        }
        return out->from_yaml_string(data, error);
      }
    }
    if (error) {
      *error = QString("Sketch '%1' not found.").arg(name);
    }
    return false;
  };

  // 特征 brep 落盘目录: 项目已保存则放项目旁 features/, 否则用临时目录
  auto feature_out_dir = [this]() -> QString {
    QDir base(project_path_.isEmpty()
                  ? QDir::tempPath() + "/gmp_features"
                  : QFileInfo(project_path_).absoluteDir().absoluteFilePath(
                        "features"));
    base.mkpath(".");
    return base.absolutePath();
  };

  const auto require_part_target = [this](const QString& operation) {
    auto* part = active_part_item();
    if (!part) {
      QMessageBox::information(
          this, operation,
          "Select an existing Part, or create a new Part, before adding a "
          "feature.");
    }
    return part;
  };

  // 特征结果统一处理: 失败弹框; 成功挂 Features 根、写回当前 Part、自动划分网格显示
  auto handle_feature_result = [this, feature_out_dir](
                                   QTreeWidgetItem* target_part,
                                   const QString& type,
                                   const QVariantMap& params,
                                   const FeatureResult& res) {
    if (!res.ok) {
      QMessageBox::warning(this, type, res.error);
      return;
    }
    QList<int> volume_tags;
    for (const int tag : res.gmsh_volume_tags) {
      if (tag > 0) {
        volume_tags.append(tag);
      }
    }
    auto* feature_item = attach_feature_to_part(
        target_part, type, params, res.gmsh_volume_tag, volume_tags,
        res.brep_path);
    if (!feature_item) {
      QMessageBox::warning(
          this, type,
          "The target Part is no longer available. Select a Part and retry.");
      return;
    }
    // 即时可视化: 对刚导入的模型划分网格并加载到视口
    QString msh;
    if (!res.brep_path.isEmpty() && res.brep_path.endsWith(".brep")) {
      msh = res.brep_path;
      msh.chop(5);
      msh += ".msh";
    } else {
      msh = QString("%1/feature_%2.msh")
                .arg(feature_out_dir())
                .arg(QDateTime::currentMSecsSinceEpoch());
    }
    QStringList volume_labels;
    for (const int tag : volume_tags) {
      volume_labels.append(QString::number(tag));
    }
    const QString volume_label = volume_labels.isEmpty()
                                     ? QString::number(res.gmsh_volume_tag)
                                     : volume_labels.join(", ");
    QString mesh_err;
    QString msg =
        QString("%1 ok: updated Part '%2' via %3; imported to gmsh as "
                "volume(s) %4 (brep: %5).")
            .arg(type)
            .arg(target_part->text(0))
            .arg(feature_item->text(0))
            .arg(volume_label)
            .arg(res.brep_path.isEmpty() ? "-" : res.brep_path);
    if (mesh_current_model(msh, &mesh_err)) {
      for (auto* item : {target_part, feature_item}) {
        if (!item) {
          continue;
        }
        QVariantMap item_params =
            item->data(0, PropertyEditor::kParamsRole).toMap();
        item_params.insert("mesh", msh);
        item->setData(0, PropertyEditor::kParamsRole, item_params);
      }
      if (viewer_) {
        viewer_->set_mesh_file(msh);
      }
      msg += QString(" Meshed and shown in viewport (mesh: %1).").arg(msh);
    } else {
      msg += QString(" Auto mesh failed (%1); mesh it manually in the Mesh "
                     "module to visualize.")
                 .arg(mesh_err);
    }
    gmp::log_operation("part", msg);
    statusBar()->showMessage(msg, 5000);
  };

#ifdef GMP_ENABLE_GMSH_GUI
  connect(part_feature_panel_, &PartFeaturePanel::extrude_requested, this,
          [this, load_sketch_doc, feature_out_dir, require_part_target,
           handle_feature_result](const QString& sketch, double distance) {
            auto* target_part = require_part_target("Extrude");
            if (!target_part) {
              return;
            }
            SketchDocument doc;
            QString err;
            if (!load_sketch_doc(sketch, &doc, &err)) {
              QMessageBox::warning(this, "Extrude", err);
              return;
            }
            const QString brep =
                QString("%1/extrude_%2.brep")
                    .arg(feature_out_dir())
                    .arg(QDateTime::currentMSecsSinceEpoch());
            handle_feature_result(
                target_part, "Extrude",
                {{"sketch", sketch}, {"distance", distance}},
                extrude_sketch(doc, distance, brep));
          });
  connect(part_feature_panel_, &PartFeaturePanel::revolve_requested, this,
          [this, load_sketch_doc, feature_out_dir, require_part_target,
           handle_feature_result](const QString& sketch, double angle_deg) {
            auto* target_part = require_part_target("Revolve");
            if (!target_part) {
              return;
            }
            SketchDocument doc;
            QString err;
            if (!load_sketch_doc(sketch, &doc, &err)) {
              QMessageBox::warning(this, "Revolve", err);
              return;
            }
            const QString brep =
                QString("%1/revolve_%2.brep")
                    .arg(feature_out_dir())
                    .arg(QDateTime::currentMSecsSinceEpoch());
            handle_feature_result(
                target_part, "Revolve",
                {{"sketch", sketch}, {"angle_deg", angle_deg}},
                revolve_sketch(doc, angle_deg, brep));
          });
  connect(part_feature_panel_, &PartFeaturePanel::loft_requested, this,
          [this, load_sketch_doc, feature_out_dir, require_part_target,
           handle_feature_result](const QStringList& sketches) {
            if (sketches.size() != 2) {
              return;
            }
            auto* target_part = require_part_target("Loft");
            if (!target_part) {
              return;
            }
            SketchDocument doc1, doc2;
            QString err;
            if (!load_sketch_doc(sketches.at(0), &doc1, &err) ||
                !load_sketch_doc(sketches.at(1), &doc2, &err)) {
              QMessageBox::warning(this, "Loft", err);
              return;
            }
            const double z2 = part_feature_panel_->loft_second_z();
            const QString brep =
                QString("%1/loft_%2.brep")
                    .arg(feature_out_dir())
                    .arg(QDateTime::currentMSecsSinceEpoch());
            const std::vector<std::pair<const SketchDocument*, double>>
                sections{{&doc1, 0.0}, {&doc2, z2}};
            handle_feature_result(
                target_part, "Loft",
                {{"sketch", sketches.at(0)},
                 {"sketch2", sketches.at(1)},
                 {"z2", z2}},
                loft_sketches(sections, /*solid=*/true, brep));
          });
  connect(part_feature_panel_, &PartFeaturePanel::sweep_requested, this,
          [this, load_sketch_doc, feature_out_dir, require_part_target,
           handle_feature_result](const QString& profile, const QString& path) {
            auto* target_part = require_part_target("Sweep");
            if (!target_part) {
              return;
            }
            SketchDocument prof_doc, path_doc;
            QString err;
            if (!load_sketch_doc(profile, &prof_doc, &err) ||
                !load_sketch_doc(path, &path_doc, &err)) {
              QMessageBox::warning(this, "Sweep", err);
              return;
            }
            const QString brep =
                QString("%1/sweep_%2.brep")
                    .arg(feature_out_dir())
                    .arg(QDateTime::currentMSecsSinceEpoch());
            handle_feature_result(
                target_part, "Sweep",
                {{"sketch", profile}, {"path", path}},
                sweep_sketch(prof_doc, path_doc, brep));
          });
#else
  // 无 Gmsh/OCC 构建下特征不可用
  part_feature_panel_->setEnabled(false);
#endif

  auto* material_page = make_module_node_page(
      "Material",
      "Create material definitions, tune constitutive laws, and keep properties ready for sections.",
      "Materials",
      module_material_list_,
      "No materials yet.",
      "material",
      {
          {"Open Materials Root", [this]() {
             if (auto* root = find_root_item("Materials")) {
               model_tree_->setCurrentItem(root);
               root->setExpanded(true);
             }
           }},
          {"New Material", [this]() {
             if (auto* root = find_root_item("Materials")) {
               const QVariantMap preset{{"type", "GenericConstantMaterial"},
                                       {"prop_names", "density"},
                                       {"prop_values", "1.0"}};
               add_child_item(root, "material_1", "Materials", preset);
             }
           }},
          {"Open Property Editor", [this, module_tab_index]() {
             const int prop_tab = module_tab_index("Property");
             if (prop_tab >= 0) {
               module_tabs_->setCurrentIndex(prop_tab);
             }
           }},
      });

  auto* section_page = make_module_node_page(
      "Section",
      "Create section assignments to bind materials and options to part regions or sets.",
      "Sections",
      module_section_list_,
      "No sections yet.",
      "section",
      {
          {"Open Sections Root", [this]() {
             if (auto* root = find_root_item("Sections")) {
               model_tree_->setCurrentItem(root);
               root->setExpanded(true);
             }
           }},
          {"New Solid Section", [this]() {
             if (auto* root = find_root_item("Sections")) {
               const QVariantMap preset{{"type", "SolidSection"},
                                       {"material", "material_1"},
                                       {"block", "solid"}};
               add_child_item(root, "section_1", "Sections", preset);
             }
           }},
          {"Open Property Editor", [this, module_tab_index]() {
             const int prop_tab = module_tab_index("Property");
             if (prop_tab >= 0) {
               module_tabs_->setCurrentIndex(prop_tab);
             }
           }},
      });

  auto* assembly_page = make_module_node_page(
      "Assembly",
      "Combine and instantiate parts into assembly-level units, then map mesh/topology for job-level binding.",
      "Parts",
      module_assembly_list_,
      "No parts available for assembly yet.",
      "part",
      {
          {"Open Mesh Root", [this]() {
             if (auto* root = find_root_item("Mesh")) {
               model_tree_->setCurrentItem(root);
             }
           }},
          {"Create Assembly Alias", [this]() {
             if (auto* root = find_root_item("Parts")) {
               const QVariantMap preset{{"type", "Assembly"}, {"description", ""}};
               add_child_item(root, "assembly_1", "Parts", preset);
             }
           }},
      });

  auto* step_page = make_module_node_page(
      "Step",
      "Create analysis steps, control time integration and execution options in the current model setup.",
      "Steps",
      module_step_list_,
      "No steps yet. Add at least one step before run.",
      "step",
      {
          {"Open Steps Root", [this]() {
             if (auto* root = find_root_item("Steps")) {
               model_tree_->setCurrentItem(root);
               root->setExpanded(true);
             }
           }},
          {"Add Static Step", [this]() {
             if (auto* root = find_root_item("Steps")) {
               const QVariantMap preset{{"type", "Static"},
                                       {"dt", "0.0"},
                                       {"end_time", "1.0"}};
               add_child_item(root, "Static", "Steps", preset);
             }
           }},
          {"Add Transient Step", [this]() {
             if (auto* root = find_root_item("Steps")) {
               const QVariantMap preset{{"type", "Transient"},
                                       {"dt", "0.1"},
                                       {"end_time", "1.0"}};
               add_child_item(root, "Transient", "Steps", preset);
             }
           }},
          {"Add Step Preset: steady",
           [this]() {
             if (auto* root = find_root_item("Steps")) {
               const QVariantMap preset{{"type", "Steady"},
                                       {"dt", "1.0"},
                                       {"end_time", "1.0"}};
               add_child_item(root, "steady", "Steps", preset);
             }
           }},
      });

  auto* interaction_page = make_module_node_page(
      "Interaction",
      "Setup contact, ties, and other coupling behaviors between sets/parts.",
      "Interactions",
      module_interaction_list_,
      "No interactions yet.",
      "interaction",
      {
          {"Open Interactions Root", [this]() {
             if (auto* root = find_root_item("Interactions")) {
               model_tree_->setCurrentItem(root);
               root->setExpanded(true);
             }
           }},
          {"Add Interaction", [this]() {
             if (auto* root = find_root_item("Interactions")) {
               add_item_under_root(root);
             }
           }},
          {"Add Tie Interaction", [this]() {
             if (auto* root = find_root_item("Interactions")) {
               add_child_item(root, "tie_1", "Interactions",
                              {{"type", "Tie"},
                               {"master", ""},
                               {"slave", ""}});
             }
           }},
      });

  auto* load_page = make_module_node_page(
      "Load",
      "Create loads, body forces, pressure and thermal sources and map them to mesh groups.",
      "Loads",
      module_load_list_,
      "No loads yet.",
      "load",
      {
          {"Open Loads Root", [this]() {
             if (auto* root = find_root_item("Loads")) {
               model_tree_->setCurrentItem(root);
               root->setExpanded(true);
             }
           }},
          {"Add Generic Load", [this]() {
             if (auto* root = find_root_item("Loads")) {
               add_child_item(root, "load_1", "Loads",
                             { {"type", "BodyForce"},
                               {"variable", "u"},
                               {"value", "0"} });
             }
           }},
          {"Open BC Root", [this]() {
             if (auto* root = find_root_item("BC")) {
               model_tree_->setCurrentItem(root);
               root->setExpanded(true);
             }
           }},
          {"Add Thermal Source", [this]() {
             if (auto* root = find_root_item("Loads")) {
               add_child_item(root, "thermal_source", "Loads",
                             {{"type", "BodyForce"},
                              {"variable", "temperature"},
                              {"value", "1.0"}});
             }
           }},
      });

  // 草图模块页: v1 骨架, 行为接线到模型树 Sketches 根与视口 2D 模式
  sketch_panel_ = new SketchPanel(property_stack_);
  // 列表行号与 Sketches 根下子节点一一对应
  auto resolve_selected_sketch = [this]() -> QTreeWidgetItem* {
    if (!sketch_panel_ || !model_tree_) {
      return nullptr;
    }
    auto* list = sketch_panel_->sketch_list();
    const int row = list ? list->currentRow() : -1;
    auto* root = find_root_item("Sketches");
    if (!root || row < 0 || row >= root->childCount()) {
      return nullptr;
    }
    return root->child(row);
  };
  // 草图编辑会话: 序列化当前编辑中的草图回模型树 params["data"]
  auto save_active_sketch = [this]() {
    if (!active_sketch_doc_ || !active_sketch_item_) {
      return;
    }
    QVariantMap params =
        active_sketch_item_->data(0, PropertyEditor::kParamsRole).toMap();
    params.insert("data", active_sketch_doc_->to_yaml_string());
    active_sketch_item_->setData(0, PropertyEditor::kParamsRole, params);
    set_project_dirty(true);
  };
  // 退出编辑: 保存 -> 舞台切为当前草图只读预览 -> 面板退出编辑态。
  auto close_sketch_editor = [this, save_active_sketch]() {
    if (!active_sketch_doc_) {
      return;
    }
    save_active_sketch();
    auto* finished_doc = active_sketch_doc_;
    if (viewer_) {
      viewer_->set_sketch_preview(finished_doc);
    }
    if (sketch_panel_) {
      sketch_panel_->set_editing(false);
    }
    active_sketch_doc_ = nullptr;
    active_sketch_item_ = nullptr;
    delete finished_doc;
    sketch_undo_stack_.clear();
    sketch_redo_stack_.clear();
    active_sketch_yaml_.clear();
    if (action_undo_) {
      action_undo_->setEnabled(false);
    }
    if (action_redo_) {
      action_redo_->setEnabled(false);
    }
    if (module_work_window_) {
      module_work_window_->hide();
      apply_module_workspace_profile(false);
    }
    sync_active_ui_context();
    update_command_availability();
  };

  const auto preview_sketch = [this](QTreeWidgetItem* target) {
    if (!viewer_ || active_sketch_doc_) {
      return;
    }
    if (!target || !target->parent()) {
      viewer_->set_sketch_preview(nullptr);
      return;
    }
    const QVariantMap params =
        target->data(0, PropertyEditor::kParamsRole).toMap();
    SketchDocument preview;
    const QString data = params.value("data").toString();
    if (!data.trimmed().isEmpty()) {
      QString error;
      if (!preview.from_yaml_string(data, &error)) {
        statusBar()->showMessage(
            QString("Failed to preview sketch '%1': %2")
                .arg(target->text(0), error),
            4000);
        viewer_->set_sketch_preview(nullptr);
        return;
      }
    }
    viewer_->set_sketch_preview(&preview);
  };
  // 打开指定草图节点进入编辑会话 (供"打开编辑"按钮与页签切换复用)
  auto open_sketch_editor = [this, close_sketch_editor](QTreeWidgetItem* target) {
    if (!target) {
      statusBar()->showMessage("Select a sketch first.", 2000);
      return;
    }
    close_sketch_editor();  // 先保存并退出正在编辑的草图
    auto* doc = new SketchDocument();
    const QVariantMap params =
        target->data(0, PropertyEditor::kParamsRole).toMap();
    const QString data = params.value("data").toString();
    if (!data.trimmed().isEmpty()) {
      QString err;
      if (!doc->from_yaml_string(data, &err)) {
        QMessageBox::warning(this, "Open Sketch",
                             "Failed to parse sketch data: " + err);
        delete doc;
        return;
      }
    }
    active_sketch_doc_ = doc;
    active_sketch_item_ = target;
    sketch_undo_stack_.clear();
    sketch_redo_stack_.clear();
    active_sketch_yaml_ = doc->to_yaml_string();
    sketch_panel_->set_undo_redo_state(false, false);
    if (action_undo_) {
      action_undo_->setEnabled(false);
    }
    if (action_redo_) {
      action_redo_->setEnabled(false);
    }
    model_tree_->setCurrentItem(target);
    if (viewer_) {
      viewer_->set_sketch_document(doc);  // 自动进入 2D 模式并渲染
    }
    sketch_panel_->set_editing(true, target->text(0));
    if (module_work_window_) {
      apply_module_workspace_profile(true);
      module_work_window_->setWindowTitle(
          QString("Sketch Editor — %1").arg(target->text(0)));
      module_work_window_->show();
      module_work_window_->raise();
      module_work_window_->activateWindow();
    }
    sync_active_ui_context();
    update_command_availability();
    statusBar()->showMessage(
        QString("Editing sketch '%1'.").arg(target->text(0)), 4000);
  };
  auto create_and_open_sketch = [this, open_sketch_editor]() {
    auto* root = find_root_item("Sketches");
    if (!root) {
      return;
    }
    auto* item = add_child_item(
        root, QString("sketch_%1").arg(root->childCount() + 1), "Sketches",
        {{"type", "Sketch2D"}, {"plane", "XY"}});
    if (!item) {
      return;
    }
    if (sketch_panel_ && sketch_panel_->sketch_list()) {
      sketch_panel_->sketch_list()->setCurrentRow(root->indexOfChild(item));
    }
    open_sketch_editor(item);
  };
  connect(sketch_panel_, &SketchPanel::new_sketch_requested, this,
          create_and_open_sketch);
  connect(module_work_window_, &QDockWidget::visibilityChanged, this,
          [this, close_sketch_editor](bool visible) {
            if (!visible && active_sketch_doc_) {
              close_sketch_editor();
            }
          });
  connect(sketch_panel_, &SketchPanel::open_edit_requested, this,
          [resolve_selected_sketch, open_sketch_editor]() {
            open_sketch_editor(resolve_selected_sketch());
          });
  connect(sketch_panel_, &SketchPanel::rename_requested, this,
          [this, resolve_selected_sketch]() {
            if (auto* target = resolve_selected_sketch()) {
              rename_item(target);
            }
          });
  connect(sketch_panel_, &SketchPanel::duplicate_requested, this,
          [this, resolve_selected_sketch]() {
            duplicate_item(resolve_selected_sketch());
          });
  connect(sketch_panel_, &SketchPanel::remove_requested, this,
          [this, resolve_selected_sketch]() {
            remove_item(resolve_selected_sketch());
          });
  connect(sketch_panel_, &SketchPanel::refresh_requested, this,
          [this]() { refresh_module_pages(); });
  // 编辑工具区接线: 工具/约束/尺寸 -> 视口; 视口回调 -> 面板/持久化
  connect(sketch_panel_, &SketchPanel::tool_selected, this, [this](int tool) {
    if (viewer_) {
      viewer_->set_sketch_tool(tool);
    }
  });
  // viewer 是草图工具状态的唯一事实源。面板、舞台工具栏和顶部拾取动作
  // 均只消费该状态，避免任一入口的回写再次把其他入口重置成 Select。
  connect(viewer_, &VtkViewer::sketch_tool_changed, this, [this](int tool) {
    if (stage_left_toolbar_) {
      stage_left_toolbar_->set_sketch_tool_checked(tool);
    }
    if (sketch_panel_) {
      sketch_panel_->set_tool_checked(tool);
    }
  });
  connect(sketch_panel_, &SketchPanel::constraint_requested, this,
          [this](int type) {
            if (viewer_) {
              viewer_->add_constraint_for_selection(type);
            }
          });
  connect(sketch_panel_, &SketchPanel::dimension_requested, this,
          [this](int type, double value) {
            if (viewer_) {
              viewer_->add_dimension_for_selection(type, value);
            }
          });
  connect(sketch_panel_, &SketchPanel::finish_edit_requested, this,
          close_sketch_editor);
  // 撤销/重做: 以 YAML 快照为步进单位, 每次修改信号 = 一步
  auto update_ur_state = [this]() {
    if (sketch_panel_) {
      sketch_panel_->set_undo_redo_state(!sketch_undo_stack_.isEmpty(),
                                         !sketch_redo_stack_.isEmpty());
    }
    if (action_undo_) {
      action_undo_->setEnabled(active_sketch_doc_ &&
                               !sketch_undo_stack_.isEmpty());
    }
    if (action_redo_) {
      action_redo_->setEnabled(active_sketch_doc_ &&
                               !sketch_redo_stack_.isEmpty());
    }
  };
  auto sketch_undo = [this, save_active_sketch, update_ur_state]() {
    if (!active_sketch_doc_ || sketch_undo_stack_.isEmpty()) {
      return;
    }
    sketch_redo_stack_.append(active_sketch_yaml_);
    const QString yaml = sketch_undo_stack_.takeLast();
    QString err;
    if (active_sketch_doc_->from_yaml_string(yaml, &err)) {
      active_sketch_yaml_ = yaml;
      if (viewer_) {
        viewer_->refresh_sketch();
      }
      save_active_sketch();  // 同步回模型树 (不再入栈)
    }
    update_ur_state();
  };
  auto sketch_redo = [this, save_active_sketch, update_ur_state]() {
    if (!active_sketch_doc_ || sketch_redo_stack_.isEmpty()) {
      return;
    }
    sketch_undo_stack_.append(active_sketch_yaml_);
    const QString yaml = sketch_redo_stack_.takeLast();
    QString err;
    if (active_sketch_doc_->from_yaml_string(yaml, &err)) {
      active_sketch_yaml_ = yaml;
      if (viewer_) {
        viewer_->refresh_sketch();
      }
      save_active_sketch();
    }
    update_ur_state();
  };
  connect(sketch_panel_, &SketchPanel::undo_requested, this, sketch_undo);
  connect(sketch_panel_, &SketchPanel::redo_requested, this, sketch_redo);
  if (action_undo_) {
    connect(action_undo_, &QAction::triggered, this, sketch_undo);
  }
  if (action_redo_) {
    connect(action_redo_, &QAction::triggered, this, sketch_redo);
  }
  connect(viewer_, &VtkViewer::sketch_modified, this,
          [this, save_active_sketch, update_ur_state]() {
            if (!active_sketch_doc_) {
              return;
            }
            // 修改前的快照入撤销栈 (redo 清空), 再保存新状态
            sketch_undo_stack_.append(active_sketch_yaml_);
            sketch_redo_stack_.clear();
            save_active_sketch();
            active_sketch_yaml_ = active_sketch_doc_->to_yaml_string();
            update_ur_state();
          });
  connect(viewer_, &VtkViewer::sketch_cursor_moved, this,
          [this](double x, double y) {
            if (sketch_panel_) {
              sketch_panel_->set_cursor_pos(x, y);
            }
          });
  connect(viewer_, &VtkViewer::sketch_selection_changed, this, [this]() {
    if (sketch_panel_ && viewer_) {
      const int n = viewer_->sketch_selection().size();
      sketch_panel_->set_status_text(n > 0 ? QString("Selected: %1").arg(n)
                                           : QString());
    }
    update_command_availability();
  });
  connect(sketch_panel_->sketch_list(), &QListWidget::itemDoubleClicked, this,
          [resolve_selected_sketch, open_sketch_editor](QListWidgetItem*) {
            open_sketch_editor(resolve_selected_sketch());
          });

  auto* step_preview_label = new QLabel(
      "Step sequence preview (Executioner uses first step; remaining shown for check):",
      step_page);
  step_preview_label->setWordWrap(true);
  step_sequence_preview_ = new QPlainTextEdit(step_page);
  step_sequence_preview_->setReadOnly(true);
  step_sequence_preview_->setLineWrapMode(QPlainTextEdit::NoWrap);
  step_sequence_preview_->setPlaceholderText("No steps yet.");
  step_sequence_preview_->setMinimumHeight(96);
  if (auto* page_layout = qobject_cast<QVBoxLayout*>(step_page->layout())) {
    page_layout->addWidget(step_preview_label);
    page_layout->addWidget(step_sequence_preview_);
  }

  auto* visualization_page = make_module_page(
      "Visualization",
      "Control tabs live in this side panel; the viewport stays clean for the 3D scene.",
      {
          {"Focus Viewport", [this]() {
             if (viewer_) {
               viewer_->setFocus();
             }
           }},
          {"Show Plot Preview", [this]() {
             if (results_work_tabs_) {
               results_work_tabs_->setCurrentIndex(1);
             }
             if (results_work_window_) {
               results_work_window_->show();
               results_work_window_->raise();
               results_work_window_->activateWindow();
             }
           }},
          {"Show Table Preview", [this]() {
             if (results_work_tabs_) {
               results_work_tabs_->setCurrentIndex(2);
             }
             if (results_work_window_) {
               results_work_window_->show();
               results_work_window_->raise();
               results_work_window_->activateWindow();
             }
           }},
      });

  // 视口控制页签(Scalar/Mesh/View/...)进入独立工作窗；页面本身不再
  // 嵌套滚动区，工具条和页签直接参与可伸缩布局。
  if (auto* ct = viewer_ ? viewer_->control_tabs() : nullptr) {
    if (auto* page_layout =
            qobject_cast<QVBoxLayout*>(visualization_page->layout())) {
      const int ct_pos = qMax(0, page_layout->count() - 1);
      ct->setParent(visualization_page);
      page_layout->insertWidget(ct_pos, ct, 1);
      ct->show();
      if (auto* tb = viewer_->top_bar()) {
        tb->setParent(visualization_page);
        page_layout->insertWidget(ct_pos, tb);
        tb->show();
      }
    }
  }

  auto* results_page = new QWidget(property_stack_);
  auto* results_layout = new QVBoxLayout(results_page);
  results_layout->setContentsMargins(10, 10, 10, 10);
  results_layout->setSpacing(6);

  auto* results_head = new QLabel("Results", results_page);
  QFont results_font = results_head->font();
  results_font.setPointSize(results_font.pointSize() + 3);
  results_font.setBold(true);
  results_head->setFont(results_font);
  results_layout->addWidget(results_head);

  auto* results_desc = new QLabel(
      "Review generated outputs and quickly open results in the viewer.",
      results_page);
  results_desc->setWordWrap(true);
  results_layout->addWidget(results_desc);

  auto* results_actions = new QHBoxLayout();
  auto* results_open_root = new QPushButton("Open Results Root", results_page);
  auto* results_refresh = new QPushButton("Refresh List", results_page);
  auto* results_import = new QPushButton("Import Result File...", results_page);
  results_import->setObjectName("resultsImportFile");
  results_import->setToolTip(
      "Import an external result file (.e/.exo/.msh/.csv/.txt/.log) into "
      "the results list.");
  auto* results_open_view = new QPushButton("Open in Viewer", results_page);
  auto* results_open_text = new QPushButton("Open as Text", results_page);
  auto* results_preview_toggle = new QPushButton("Preview", results_page);
  results_preview_toggle->setObjectName("resultsPreviewToggle");
  results_preview_toggle->setCheckable(true);
  results_preview_toggle->setChecked(false);
  results_preview_toggle->setToolTip(
      "Show or hide the result preview pane.");
  auto* results_new_compare = new QPushButton("New Comparison Window", results_page);
  results_new_compare->setObjectName("resultsNewCompareButton");
  results_new_compare->setToolTip(
      "Open an additional results window for side-by-side comparison.");
  auto* results_filter_label = new QLabel("Type", results_page);
  results_type_filter_ = new QComboBox(results_page);
  results_type_filter_->addItem("All", "all");
  results_type_filter_->addItem("Solver (.e/.exo)", "e");
  results_type_filter_->addItem("Mesh (.msh)", "msh");
  results_type_filter_->addItem("Text (.txt/.csv/.log/.yaml/.yml)", "txt");
  results_actions->addWidget(results_open_root);
  results_actions->addWidget(results_refresh);
  results_actions->addWidget(results_import);
  connect(results_import, &QPushButton::clicked, this, [this]() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Import Result File", QDir::homePath(),
        "Result Files (*.e *.exo *.exodus *.msh *.csv *.txt *.log *.yaml "
        "*.yml);;All Files (*)");
    if (!path.isEmpty()) {
      import_result_file(path);
    }
  });
  results_actions->addWidget(results_open_view);
  results_actions->addWidget(results_open_text);
  results_actions->addWidget(results_preview_toggle);
  results_actions->addWidget(results_new_compare);
  connect(results_new_compare, &QPushButton::clicked, this,
          [this]() { create_results_compare_window(); });
  results_actions->addStretch(1);
  results_actions->addWidget(results_filter_label);
  results_actions->addWidget(results_type_filter_);
  results_actions->addStretch(1);
  auto* results_actions_row = new QWidget(results_page);
  results_actions_row->setLayout(results_actions);
  results_layout->addWidget(results_actions_row);

  results_list_ = new QListWidget(results_page);
  results_list_->setSelectionMode(QAbstractItemView::SingleSelection);
  results_list_->setMinimumHeight(96);
  results_layout->addWidget(results_list_);

  results_preview_ = new QPlainTextEdit(results_page);
  results_preview_->setReadOnly(true);
  results_preview_->setPlaceholderText("Select a result item for quick preview.");
  results_preview_->setLineWrapMode(QPlainTextEdit::NoWrap);
  // 弹窗精简：预览区默认折叠，按需展开；结果列表为默认唯一内容区。
  results_preview_->setVisible(false);
  results_layout->addWidget(results_preview_, 1);
  connect(results_preview_toggle, &QPushButton::toggled, results_preview_,
          &QWidget::setVisible);

  auto open_result_in_viewer = [this](const QListWidgetItem* row) {
    if (!row || !viewer_) {
      return;
    }
    const QString path = row->data(Qt::UserRole).toString();
    if (path.isEmpty()) {
      statusBar()->showMessage("Selected result has no path.", 2000);
      return;
    }
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "e" || ext == "exo" || ext == "exodus") {
      viewer_->set_exodus_file(path);
    } else {
      viewer_->set_mesh_file(path);
    }
    sync_results_tree_selection(row);
    viewer_->setFocus();
    statusBar()->showMessage("Opened result in viewer.", 1500);
  };

  auto open_result_as_text = [this](const QListWidgetItem* row) {
    if (!row || !results_preview_) {
      return;
    }
    // 先取数据副本：sync_results_tree_selection 经模型树选择链可能
    // 刷新结果列表并销毁 row 指向的条目（18:29 崩溃即此路径）。
    const QString path = row->data(Qt::UserRole).toString();
    sync_results_tree_selection(row);
    if (path.isEmpty()) {
      results_preview_->setPlainText("No file path for this result.");
      return;
    }
    const QString ext = QFileInfo(path).suffix().toLower();
    if (!(ext == "txt" || ext == "csv" || ext == "log" || ext == "yaml" ||
          ext == "yml")) {
      results_preview_->setPlainText(
          QString("Text open is intended for text outputs only.\n"
                  "Use Open in Viewer for: %1")
              .arg(path));
      return;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
      results_preview_->setPlainText(QString("Failed to open file: %1").arg(path));
      return;
    }
    const QString all = QString::fromUtf8(f.readAll());
    if (all.size() > 8192) {
      results_preview_->setPlainText(
          all.left(8192) + "\n\n... (truncated to 8192 bytes)");
    } else {
      results_preview_->setPlainText(all);
    }
  };

  auto open_results_root = [this]() {
    if (auto* root = find_root_item("Results")) {
      model_tree_->setCurrentItem(root);
      root->setExpanded(true);
    }
  };

  connect(results_open_root, &QPushButton::clicked, this,
          open_results_root);
  connect(results_refresh, &QPushButton::clicked, this,
          [this]() { refresh_results_panel(); });
  connect(results_type_filter_,
          &QComboBox::currentTextChanged,
          this,
          [this]() { refresh_results_panel(); });
  connect(results_open_view, &QPushButton::clicked, this,
          [this, open_result_in_viewer]() {
            if (!results_list_) {
              return;
            }
            const auto* item = results_list_->currentItem();
            if (!item) {
              statusBar()->showMessage("Select a result first.", 2000);
              return;
            }
            open_result_in_viewer(item);
          });
  connect(results_open_text, &QPushButton::clicked, this,
          [this, open_result_as_text]() {
            if (!results_list_ || !results_preview_) {
              return;
            }
            const auto* item = results_list_->currentItem();
            if (!item) {
              statusBar()->showMessage("Select a result first.", 2000);
              return;
            }
            open_result_as_text(item);
          });
  connect(results_list_, &QListWidget::itemDoubleClicked, this,
          [this, open_result_in_viewer, open_result_as_text](QListWidgetItem* item) {
            if (!item) {
              return;
            }
            const QString path = item->data(Qt::UserRole).toString();
            if (path.isEmpty()) {
              return;
            }
            const QString ext = QFileInfo(path).suffix().toLower();
            if (ext == "txt" || ext == "csv" || ext == "log" || ext == "yaml" ||
                ext == "yml") {
              open_result_as_text(item);
            } else {
              open_result_in_viewer(item);
            }
          });
  connect(results_list_, &QListWidget::currentItemChanged, this,
          [this](QListWidgetItem* current, QListWidgetItem*) {
            if (!results_preview_ || !current) {
              if (results_preview_) {
                results_preview_->clear();
              }
              return;
            }
            // 先取数据副本：sync_results_tree_selection 经模型树选择链
            // 可能刷新结果列表并销毁 current 指向的条目（崩溃栈确认）。
            const QString path = current->data(Qt::UserRole).toString();
            const QString job = current->data(Qt::UserRole + 1).toString();
            const QString text = current->text();
            sync_results_tree_selection(current);
            if (path.isEmpty()) {
              results_preview_->setPlainText(
                  QString("No file attached for: %1").arg(text));
              return;
            }
            const QString ext = QFileInfo(path).suffix().toLower();
            const QFileInfo fi(path);
            QString details;
            details += QString("Result: %1").arg(text);
            details += QString("\nPath: %1").arg(path);
            if (!job.isEmpty()) {
              details += QString("\nJob: %1").arg(job);
            }
            if (fi.exists()) {
              details +=
                  QString("\nSize: %1 bytes\nModified: %2")
                      .arg(fi.size())
                      .arg(fi.lastModified().toString(Qt::ISODate));
            }
            if (ext == "e" || ext == "exo" || ext == "exodus") {
              details += "\nType: Solver result (.e)";
              details +=
                  "\nAction: Open in Viewer";
            } else if (ext == "msh") {
              details += "\nType: Mesh (.msh)";
              details +=
                  "\nAction: Open in Viewer or import in Gmsh via menu/mesh action.";
            } else if (ext == "txt" || ext == "csv" || ext == "log" ||
                       ext == "yaml" || ext == "yml") {
              details += "\nType: Text";
            } else {
              details += "\nType: Other";
            }
            if (fi.size() > 0 && (ext == "txt" || ext == "csv" || ext == "log" ||
                                  ext == "yaml" || ext == "yml")) {
              QFile f(path);
              if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QStringList lines;
                for (int i = 0; i < 6; ++i) {
                  const QByteArray chunk = f.readLine();
                  if (chunk.isEmpty()) {
                    break;
                  }
                  lines << QString::fromUtf8(chunk).trimmed();
                }
                if (!lines.isEmpty()) {
                  details += "\n\nPreview:\n" + lines.join("\n");
                }
              }
            }
            results_preview_->setPlainText(details);
          });

  results_work_tabs_ = new QTabWidget(results_work_window_);
  results_work_tabs_->setObjectName("resultsWorkspaceTabs");
  results_work_tabs_->addTab(results_page, "Results");
  results_work_tabs_->addTab(plot_page, "Plot");
  results_work_tabs_->addTab(table_page, "Table");
  results_work_window_->setWidget(results_work_tabs_);
  job_work_window_->setWidget(job_container);
  visualization_work_window_->setWidget(visualization_page);
  mesh_work_window_->setWidget(mesh_page);
  job_work_window_->resize(820, 560);
  visualization_work_window_->resize(660, 540);
  results_work_window_->resize(640, 400);
  mesh_work_window_->resize(760, 520);
  plot_open_btn->setText("Focus Viewport");
  table_open_btn->setText("Focus Viewport");

  auto show_workspace = [this](QDockWidget* workspace) {
    if (!workspace) {
      return;
    }
    // 每次激活前复用公共越界恢复，第二屏移除后窗口回到可视区。
    clamp_window_to_screen(workspace);
    workspace->show();
    workspace->raise();
    workspace->activateWindow();
  };

  connect(stage_left_toolbar_, &StageLeftToolbar::interaction_mode_requested,
          this, [this](int mode) {
            if (viewer_) {
              viewer_->set_stage_interaction_mode(mode);
            }
            if (mode != 0 && viewer_ && viewer_->sketch_document() &&
                sketch_panel_) {
              sketch_panel_->set_tool_checked(-1);
            }
          });
  connect(stage_left_toolbar_, &StageLeftToolbar::view_preset_requested,
          viewer_, &VtkViewer::apply_stage_view);
  connect(stage_left_toolbar_, &StageLeftToolbar::picking_toggled,
          viewer_, &VtkViewer::set_stage_picking);
  connect(stage_left_toolbar_, &StageLeftToolbar::clear_selection_requested,
          this, [this]() {
            if (viewer_) {
              viewer_->clear_stage_selection();
            }
            active_ui_context_.stage_selections.clear();
            update_command_availability();
          });
  connect(stage_left_toolbar_, &StageLeftToolbar::slice_toggled,
          viewer_, &VtkViewer::set_stage_slice);
  connect(stage_left_toolbar_,
          &StageLeftToolbar::representation_cycle_requested,
          viewer_, &VtkViewer::cycle_stage_representation);
  connect(stage_left_toolbar_, &StageLeftToolbar::sketch_tool_requested,
          this, [this, resolve_selected_sketch, open_sketch_editor](int tool) {
            // 只读预览仍显示草图工具栏。用户选择任一修改工具时直接进入
            // 当前草图的编辑会话，避免按钮看似可点、实际又被退回 Select。
            if (tool != SketchToolSelect && !active_sketch_doc_) {
              auto* target = resolve_selected_sketch();
              if (!target && model_tree_) {
                auto* current = model_tree_->currentItem();
                if (current && current->parent() &&
                    current->data(0, PropertyEditor::kKindRole).toString() ==
                        "Sketches") {
                  target = current;
                }
              }
              open_sketch_editor(target);
              if (!active_sketch_doc_) {
                return;
              }
            }
            if (viewer_) {
              viewer_->set_sketch_tool(tool);
            }
          });
  connect(viewer_, &VtkViewer::stage_picking_changed, stage_left_toolbar_,
          &StageLeftToolbar::set_picking_checked);
  connect(viewer_, &VtkViewer::stage_slice_changed, stage_left_toolbar_,
          &StageLeftToolbar::set_slice_checked);
  connect(viewer_, &VtkViewer::stage_picking_changed, this,
          [this](bool enabled) {
            if (enabled && viewer_ && viewer_->sketch_document() &&
                stage_left_toolbar_) {
              stage_left_toolbar_->set_sketch_tool_checked(SketchToolSelect);
              if (sketch_panel_) {
                sketch_panel_->set_tool_checked(SketchToolSelect);
              }
            }
            if (!action_stage_pick_) {
              return;
            }
            const QSignalBlocker blocker(action_stage_pick_);
            action_stage_pick_->setChecked(enabled);
          });
  connect(viewer_, &VtkViewer::stage_slice_changed, this,
          [this](bool enabled) {
            if (!action_stage_slice_) {
              return;
            }
            const QSignalBlocker blocker(action_stage_slice_);
            action_stage_slice_->setChecked(enabled);
          });
  connect(viewer_, &VtkViewer::stage_command_feedback, this,
          [this](const QString& message) {
            statusBar()->showMessage(message, 4000);
            if (console_ && !message.isEmpty()) {
              console_->appendPlainText(message);
            }
          });
  connect(stage_left_toolbar_, &StageLeftToolbar::mesh_workspace_requested,
          this, [this, module_tab_index, show_workspace]() {
            const int target = module_tab_index("Mesh");
            if (target >= 0) {
              module_tabs_->setCurrentIndex(target);
            }
            show_workspace(mesh_work_window_);
          });
  connect(stage_left_toolbar_, &StageLeftToolbar::mesh_generate_requested,
          this, [this]() {
            if (gmsh_panel_) {
              gmsh_panel_->generate_mesh();
            }
          });
  connect(stage_left_toolbar_,
          &StageLeftToolbar::visualization_workspace_requested, this,
          [this, show_workspace]() {
            show_workspace(visualization_work_window_);
          });
  connect(stage_left_toolbar_, &StageLeftToolbar::results_workspace_requested,
          this, [this, show_workspace]() {
            if (results_work_tabs_) {
              results_work_tabs_->setCurrentIndex(0);
            }
            show_workspace(results_work_window_);
          });
  auto* stage_escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  stage_escape->setContext(Qt::WindowShortcut);
  connect(stage_escape, &QShortcut::activated, stage_left_toolbar_,
          &StageLeftToolbar::reset_temporary_modes);

  assign_module_actions(part_tab,
                       {
                           {"Open Part Root", [this]() {
                             if (auto* root = find_root_item("Parts")) {
                               model_tree_->setCurrentItem(root);
                               root->setExpanded(true);
                             }
                           }},
                           {"New Part", [create_part_from_sketch]() {
                             create_part_from_sketch();
                           }},
                           {"Open Mesh Module", [this, module_tab_index]() {
                             const int target = module_tab_index("Mesh");
                             if (target >= 0) {
                               module_tabs_->setCurrentIndex(target);
                             }
                           }},
                       });

  assign_module_actions(property_tab,
                       {
                           {"Sync Model to Input", [this]() { sync_model_to_input(); }},
                           {"Open Mesh Module", [this, module_tab_index]() {
                             const int target = module_tab_index("Mesh");
                             if (target >= 0) {
                               module_tabs_->setCurrentIndex(target);
                             }
                           }},
                           {"Open Job Module",
                            [this, module_tab_index, show_workspace]() {
                             const int target = module_tab_index("Job");
                             if (target >= 0) {
                               module_tabs_->setCurrentIndex(target);
                             }
                             show_workspace(job_work_window_);
                           }},
                       });

  assign_module_actions(material_tab,
                       {
                           {"Open Materials Root", [this]() {
                             if (auto* root = find_root_item("Materials")) {
                               model_tree_->setCurrentItem(root);
                               root->setExpanded(true);
                             }
                           }},
                           {"New Material", [this]() {
                             if (auto* root = find_root_item("Materials")) {
                               const QVariantMap preset{{"type", "GenericConstantMaterial"},
                                                       {"prop_names", "density"},
                                                       {"prop_values", "1.0"}};
                               add_child_item(root, "material_1", "Materials", preset);
                             }
                           }},
                       });

  assign_module_actions(section_tab,
                       {
                           {"Open Sections Root", [this]() {
                             if (auto* root = find_root_item("Sections")) {
                               model_tree_->setCurrentItem(root);
                               root->setExpanded(true);
                             }
                           }},
                           {"New Solid Section", [this]() {
                             if (auto* root = find_root_item("Sections")) {
                               const QVariantMap preset{{"type", "SolidSection"},
                                                       {"material", "material_1"},
                                                       {"block", "solid"}};
                               add_child_item(root, "section_1", "Sections", preset);
                             }
                           }},
                       });

  assign_module_actions(assembly_tab,
                       {
                           {"Open Assembly", [this, module_tab_index]() {
                             const int mesh_tab = module_tab_index("Mesh");
                             if (mesh_tab >= 0) {
                               module_tabs_->setCurrentIndex(mesh_tab);
                             }
                           }},
                           {"Create Assembly Alias", [this]() {
                             if (auto* root = find_root_item("Parts")) {
                               add_child_item(root, "assembly_1", "Parts",
                                             {{"type", "Assembly"},
                                              {"description", ""}});
                             }
                           }},
                       });

  assign_module_actions(step_tab,
                       {
                           {"Open Steps Root", [this]() {
                             if (auto* root = find_root_item("Steps")) {
                               model_tree_->setCurrentItem(root);
                               root->setExpanded(true);
                             }
                           }},
                           {"Add Steady Step", [this]() {
                             if (auto* root = find_root_item("Steps")) {
                               const QVariantMap preset{{"type", "Steady"},
                                                       {"dt", "1.0"},
                                                       {"end_time", "1.0"}};
                               add_child_item(root, "steady", "Steps", preset);
                             }
                           }},
                           {"Add Static Step", [this]() {
                             if (auto* root = find_root_item("Steps")) {
                               const QVariantMap preset{{"type", "Static"},
                                                       {"dt", "0.0"},
                                                       {"end_time", "1.0"}};
                               add_child_item(root, "static", "Steps", preset);
                             }
                           }},
                       });

  assign_module_actions(interaction_tab,
                       {
                           {"Open Interactions Root", [this]() {
                             if (auto* root = find_root_item("Interactions")) {
                               model_tree_->setCurrentItem(root);
                               root->setExpanded(true);
                             }
                           }},
                           {"Add Interaction", [this]() {
                             if (auto* root = find_root_item("Interactions")) {
                               add_item_under_root(root);
                             }
                           }},
                       });

  assign_module_actions(load_tab,
                       {
                           {"Open Loads Root", [this]() {
                             if (auto* root = find_root_item("Loads")) {
                               model_tree_->setCurrentItem(root);
                               root->setExpanded(true);
                             }
                           }},
                           {"Add Body Force", [this]() {
                             if (auto* root = find_root_item("Loads")) {
                               add_child_item(root, "load_body_force", "Loads",
                                             {{"type", "BodyForce"},
                                              {"variable", "u"},
                                              {"value", "0"}});
                             }
                           }},
                           {"Open BC Root", [this]() {
                             if (auto* root = find_root_item("BC")) {
                               model_tree_->setCurrentItem(root);
                               root->setExpanded(true);
                             }
                           }},
                       });

  assign_module_actions(sketch_tab,
                       {
                           {"New Sketch", [create_and_open_sketch]() {
                              create_and_open_sketch();
                            }},
                           {"Open Sketches Root", [this]() {
                             if (auto* root = find_root_item("Sketches")) {
                               model_tree_->setCurrentItem(root);
                               root->setExpanded(true);
                             }
                           }},
                       });

  assign_module_actions(mesh_tab,
                       {
                           {"Generate Mesh", [this]() {
                             if (gmsh_panel_) {
                               gmsh_panel_->generate_mesh();
                             }
                           }},
                           {"Generate 2D Mesh", [this]() {
                             if (gmsh_panel_) {
                               gmsh_panel_->set_mesh_generation_dim(2);
                               gmsh_panel_->generate_mesh();
                             }
                           }},
                           {"Generate 3D Mesh", [this]() {
                             if (gmsh_panel_) {
                               gmsh_panel_->set_mesh_generation_dim(3);
                               gmsh_panel_->generate_mesh();
                             }
                           }},
                           {"Open Mesh Root", [this]() {
                             if (auto* root = find_root_item("Mesh")) {
                               model_tree_->setCurrentItem(root);
                               root->setExpanded(true);
                             }
                           }},
                           {"Generate & Submit", [this]() {
                             if (gmsh_panel_) {
                               gmsh_panel_->set_mesh_generation_dim(3);
                               gmsh_panel_->generate_mesh();
                             }
                             start_submit_workflow();
                           }},
                       });

  assign_module_actions(job_tab,
                       {
                           {"Open Job Workspace", [this, show_workspace]() {
                             show_workspace(job_work_window_);
                           }},
                           {"Prepare Workflow Defaults",
                            [this]() { ensure_basic_workflow_nodes(); }},
                           {"Sync to Input", [this]() { sync_model_to_input(); }},
                           {"Submit (Mesh + Sync + Run)", [this]() {
                             start_submit_workflow();
                           }},
                           {"Run", [this]() {
                             if (moose_panel_) {
                               moose_panel_->run_job();
                             }
                           }},
                           {"Check Input", [this]() {
                             if (moose_panel_) {
                               moose_panel_->check_input();
                             }
                           }},
                           {"Stop", [this]() {
                             if (moose_panel_) {
                               moose_panel_->stop_job();
                             }
                           }},
                       });

  assign_module_actions(viz_tab,
                       {
                           {"Open Visualization Workspace",
                            [this, show_workspace]() {
                              show_workspace(visualization_work_window_);
                            }},
                           {"Focus Viewport", [this]() {
                              if (viewer_) {
                                viewer_->setFocus();
                              }
                            }},
                           {"Open Plot", [this, show_workspace]() {
                              if (results_work_tabs_) {
                                results_work_tabs_->setCurrentIndex(1);
                              }
                              show_workspace(results_work_window_);
                            }},
                           {"Open Table", [this, show_workspace]() {
                              if (results_work_tabs_) {
                                results_work_tabs_->setCurrentIndex(2);
                              }
                              show_workspace(results_work_window_);
                            }},
                       });

  auto open_selected_result_in_viewer = [this]() {
    if (!results_list_) {
      return;
    }
    const auto* row = results_list_->currentItem();
    if (!row) {
      statusBar()->showMessage("Select a result first.", 2000);
      return;
    }
    const QString path = row->data(Qt::UserRole).toString();
    if (path.isEmpty() || !viewer_) {
      return;
    }
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "e" || ext == "exo" || ext == "exodus") {
      viewer_->set_exodus_file(path);
    } else {
      viewer_->set_mesh_file(path);
    }
    viewer_->setFocus();
    sync_results_tree_selection(row);
  };
  auto open_selected_result_as_text = [this, open_result_as_text]() {
    if (!results_list_ || !results_preview_) {
      return;
    }
    const auto* row = results_list_->currentItem();
    if (!row) {
      statusBar()->showMessage("Select a result first.", 2000);
      return;
    }
    sync_results_tree_selection(row);
    const QString path = row->data(Qt::UserRole).toString();
    if (path.isEmpty()) {
      return;
    }
    open_result_as_text(row);
  };

  assign_module_actions(results_tab,
                       {
                           {"Open Results Workspace",
                            [this, show_workspace]() {
                              if (results_work_tabs_) {
                                results_work_tabs_->setCurrentIndex(0);
                              }
                              show_workspace(results_work_window_);
                            }},
                           {"Refresh Results", [this]() {
                             refresh_results_panel();
                           }},
                           {"Open in Viewer", [open_selected_result_in_viewer]() {
                             open_selected_result_in_viewer();
                           }},
                           {"Open as Text", [open_selected_result_as_text]() {
                             open_selected_result_as_text();
                           }},
                           {"Open Job Log", [this]() {
                             if (!moose_panel_) {
                               return;
                             }
                             QDialog dialog(this);
                             dialog.setWindowTitle("Job Log");
                             dialog.resize(800, 500);
                             auto* layout = new QVBoxLayout(&dialog);
                             auto* log_view = new QPlainTextEdit(&dialog);
                             log_view->setReadOnly(true);
                             log_view->setPlainText(moose_panel_->log_text());
                             layout->addWidget(log_view);
                             dialog.exec();
                           }},
                       });

  apply_toolbar_actions = [this,
                          command_layout,
                          command_host,
                          mesh_tab,
                          job_tab,
                          viz_tab,
                          results_tab,
                          toolbar_actions = std::move(module_toolbar_actions)](int index) {
    const QString title =
        (index >= 0) ? module_tabs_->tabText(index) : QString("Modules");
    // Mesh/Job/Visualization/Results 已有独立工作窗，不再回写通用窗口标题。
    if (module_work_window_ && index != mesh_tab && index != job_tab &&
        index != viz_tab && index != results_tab) {
      module_work_window_->setWindowTitle(title + " Workspace");
    }

    while (QLayoutItem* item = command_layout->takeAt(0)) {
      if (item->widget()) {
        item->widget()->deleteLater();
      }
      delete item;
    }

    const int clamped_index =
        (index >= 0 && index < static_cast<int>(toolbar_actions.size()) ? index : -1);
    if (clamped_index < 0) {
      command_layout->addStretch(1);
      return;
    }

    const auto& actions = toolbar_actions[clamped_index];
    if (actions.empty()) {
      auto* hint = new QLabel("No quick actions", command_host);
      QFont hint_font = hint->font();
      hint_font.setItalic(true);
      hint->setFont(hint_font);
      command_layout->addWidget(hint);
      command_layout->addStretch(1);
      return;
    }

    for (const auto& action : actions) {
      auto* action_btn = new QPushButton(action.first, command_host);
      action_btn->setProperty("moduleAction", action.first);
      action_btn->setMinimumHeight(24);
      connect(action_btn, &QPushButton::clicked, action_btn,
              [action]() { action.second(); });
      command_layout->addWidget(action_btn);
    }
    command_layout->addStretch(1);
  };

  auto make_workspace_launcher =
      [this, show_workspace](const QString& title, const QString& description,
                             QDockWidget* workspace) {
        auto* page = new QWidget(property_stack_);
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(18, 18, 18, 18);
        layout->setSpacing(10);
        auto* heading = new QLabel(title, page);
        QFont heading_font = heading->font();
        heading_font.setBold(true);
        heading_font.setPointSize(heading_font.pointSize() + 2);
        heading->setFont(heading_font);
        auto* note = new QLabel(description, page);
        note->setWordWrap(true);
        auto* open = new QPushButton("Open " + title, page);
        connect(open, &QPushButton::clicked, page,
                [workspace, show_workspace]() { show_workspace(workspace); });
        layout->addWidget(heading);
        layout->addWidget(note);
        layout->addWidget(open);
        layout->addStretch(1);
        return page;
      };
  auto* job_launcher = make_workspace_launcher(
      "Job Workspace",
      "Job management runs in an independent non-modal window. Closing the "
      "window does not stop an active job.",
      job_work_window_);
  auto* visualization_launcher = make_workspace_launcher(
      "Visualization Workspace",
      "Visualization controls run beside the central viewport without "
      "replacing it.",
      visualization_work_window_);
  auto* results_launcher = make_workspace_launcher(
      "Results Workspace",
      "Results, Plot and Table use an independent window; the central "
      "viewport remains visible.",
      results_work_window_);
  auto* mesh_launcher = make_workspace_launcher(
      "Mesh Workspace",
      "Mesh generation runs in an independent non-modal window. Closing the "
      "window does not interrupt an active generation task.",
      mesh_work_window_);
  property_stack_->addWidget(property_editor_);
  property_stack_->addWidget(part_page);
  property_stack_->addWidget(material_page);
  property_stack_->addWidget(section_page);
  property_stack_->addWidget(assembly_page);
  property_stack_->addWidget(step_page);
  property_stack_->addWidget(interaction_page);
  property_stack_->addWidget(load_page);
  property_stack_->addWidget(sketch_panel_);
  property_stack_->addWidget(mesh_launcher);
  property_stack_->addWidget(job_launcher);
  property_stack_->addWidget(visualization_launcher);
  property_stack_->addWidget(results_launcher);
  // QStackedWidget 的最小宽度默认取所有页面的最大值(如 Job 页的7列表格),
  // 会把整个右栏撑宽并迫使每页都出现横向滚动条。
  // 各页改为 Ignored, 栈只按当前页内容计算宽度。
  for (int i = 0; i < property_stack_->count(); ++i) {
    property_stack_->widget(i)->setSizePolicy(QSizePolicy::Ignored,
                                              QSizePolicy::Expanding);
  }
  refresh_module_pages();

  console_ = new QPlainTextEdit(vertical_split);
  console_->setReadOnly(true);
  console_->setMinimumHeight(46);
  console_->setPlaceholderText("Job/Message Console");
  vertical_split->addWidget(console_);
  // 操作日志同步进控制台：文件持久化 + UI 可见，一处埋点两处留痕。
  gmp::set_operation_log_console_hook([this](const QString& line) {
    if (console_) {
      console_->appendPlainText(line);
    }
  });

  vertical_split->setStretchFactor(0, 4);
  vertical_split->setStretchFactor(1, 1);
  main_split->setStretchFactor(0, 0);
  main_split->setStretchFactor(1, 1);
  // I-02 增加状态列后给导航树一个可读的默认宽度；仍不设置 minimumWidth，
  // 用户可随时向左收窄，把空间还给中央舞台。
  const int left_w = qBound(250, int(width() * 0.22), 340);
  const int center_w = std::max(480, width() - left_w);
  main_split->setSizes({left_w, center_w});
  vertical_split->setSizes({std::max(480, height() - 90), 80});

  connect(module_tabs_, &QTabBar::currentChanged, this,
          [this, apply_toolbar_actions, results_tab, sketch_tab, mesh_tab, viz_tab,
           close_sketch_editor, preview_sketch](
              int index) {
            if (active_sketch_doc_ && index != sketch_tab) {
              const QSignalBlocker tab_blocker(module_tabs_);
              module_tabs_->setCurrentIndex(sketch_tab);
              if (module_selector_) {
                const QSignalBlocker selector_blocker(module_selector_);
                const int selector_index =
                    module_selector_->findData(sketch_tab);
                module_selector_->setCurrentIndex(selector_index);
              }
              statusBar()->showMessage(
                  "Finish or close the active Sketch edit before switching modules.",
                  3500);
              update_command_availability();
              return;
            }
            remember_active_object_for_module(active_ui_context_.module_index);
            active_ui_context_.module_index = index;
            // 页签顺序: Sketch, Part, Property, Material, Section, Assembly,
            // Step, Interaction, Load, Mesh, Job, Visualization, Results
            // 右侧堆栈顺序: property_editor, part, material, section, assembly,
            // step, interaction, load, sketch, mesh, job, viz, results
            static constexpr int module_to_property[] = {8, 1, 0, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12};
            constexpr int module_count = 13;
            const int target =
                (index >= 0 && index < module_count) ? module_to_property[index] : 0;
            // 只有新建/双击/打开编辑等显式入口才进入草图 2D 会话；
            // 离开草图页签时保存退出 2D，恢复 3D 场景。
            // 注: 用构造期解析的 sketch_tab 索引比较, 不能用 tabText
            // (中文界面下 tabText 已被翻译)
            if (index != sketch_tab) {
              if (active_sketch_doc_) {
                close_sketch_editor();
              }
              // 完成编辑后保留的预览只属于 Sketch 上下文；离开模块时恢复
              // 原有 3D 场景，避免预览覆盖 Part/Mesh/Results 舞台。
              if (viewer_ && viewer_->sketch_document()) {
                viewer_->set_sketch_preview(nullptr);
              }
            } else if (!active_sketch_doc_) {
              auto* item = model_tree_ ? model_tree_->currentItem() : nullptr;
              if (item && item->parent() &&
                  item->data(0, PropertyEditor::kKindRole).toString() ==
                      "Sketches") {
                preview_sketch(item);
              }
            }
            refresh_module_pages();
            if (target >= 0 && target < property_stack_->count()) {
              property_stack_->setCurrentIndex(target);
            }
            if (stage_left_toolbar_) {
              QString stage_context;
              if (index == sketch_tab) {
                stage_context = "Sketch";
              } else if (index == mesh_tab) {
                stage_context = "Mesh";
              } else if (index == viz_tab) {
                stage_context = "Visualization";
              } else if (index == results_tab) {
                stage_context = "Results";
              }
              stage_left_toolbar_->set_context(stage_context);
            }
            apply_toolbar_actions(index);
            restore_active_object_for_module(index);
            refresh_work_context();
            sync_active_ui_context();
            update_command_availability();
            if (index == results_tab) {
              refresh_results_panel();
            }
            if (target == 0 && property_editor_) {
              property_editor_->set_item(model_tree_->currentItem());
            }
            // 模块页与命令条是动态重建的, 中文模式下需重新翻译新控件
            if (l10n::current_language() == l10n::Language::Chinese) {
              QTimer::singleShot(0, this, [this]() { l10n::apply(this); });
            }
          });
  connect(module_selector_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int combo_index) {
            if (combo_index < 0 || !module_tabs_) {
              return;
            }
            const int module_index =
                module_selector_->itemData(combo_index).toInt();
            if (module_tabs_->currentIndex() != module_index) {
              module_tabs_->setCurrentIndex(module_index);
            }
            if (module_tabs_->currentIndex() != module_index) {
              refresh_work_context();
              return;
            }
            // 选择器是原可见页签的替代入口，保持“主动切换即打开对应工作窗”。
            QMetaObject::invokeMethod(module_tabs_, "tabBarClicked",
                                      Qt::DirectConnection,
                                      Q_ARG(int, module_index));
          });
  connect(context_object_selector_,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int combo_index) {
            if (combo_index < 0 || !model_tree_) {
              return;
            }
            if (combo_index == 0) {
              model_tree_->clearSelection();
              model_tree_->setCurrentItem(nullptr);
              refresh_work_context();
              return;
            }
            const QString root_name =
                context_object_selector_->property("contextRoot").toString();
            auto* root = find_root_item(root_name);
            const int row =
                context_object_selector_->itemData(combo_index).toInt();
            if (!root || row < 0 || row >= root->childCount()) {
              return;
            }
            auto* item = root->child(row);
            model_tree_->setCurrentItem(item);
            model_tree_->scrollToItem(item);
          });
  connect(module_tabs_, &QTabBar::tabBarClicked, this,
          [this, mesh_tab, job_tab, viz_tab, results_tab, show_workspace](int index) {
            if (!layout_ready_) {
              return;
            }
            if (index == mesh_tab) {
              show_workspace(mesh_work_window_);
            } else if (index == job_tab) {
              show_workspace(job_work_window_);
              // 打开 Job 工作窗时自动同步 LIMS 任务列表；网络失败只记日志。
              // 巡览模式下跳过，避免依赖外部服务或污染树节点断言。
              if (moose_panel_ &&
                  !qEnvironmentVariableIsSet("GMP_SCREENSHOT_DIR")) {
                moose_panel_->on_refresh_job();
              }
            } else if (index == viz_tab) {
              show_workspace(visualization_work_window_);
            } else if (index == results_tab) {
              if (results_work_tabs_) {
                results_work_tabs_->setCurrentIndex(0);
              }
              show_workspace(results_work_window_);
            } else {
              show_workspace(module_work_window_);
            }
          });
  if (part_tab >= 0) {
    module_tabs_->setCurrentIndex(part_tab);
  } else {
    module_tabs_->setCurrentIndex(0);
  }
  property_stack_->setCurrentIndex(1);
  apply_toolbar_actions(module_tabs_->currentIndex());

  const QString initial_plot = viewer_->plot_snapshot_text();
  const QString initial_plot_stats = viewer_->plot_stats_snapshot();
  const QString initial_table = viewer_->table_snapshot_text();
  const QString initial_table_stats = viewer_->table_stats_snapshot();
  plot_view->setPlainText(initial_plot);
  plot_status->setText(initial_plot_stats);
  table_view->setPlainText(initial_table);
  table_status->setText(initial_table_stats);

  connect(gmsh_panel_, &GmshPanel::mesh_written, plot_refresh_btn,
          [plot_refresh_btn]() { plot_refresh_btn->click(); });
  connect(plot_refresh_btn, &QPushButton::clicked, this, [this, plot_view, plot_status]() {
    if (viewer_) {
      plot_view->setPlainText(viewer_->plot_snapshot_text());
      plot_status->setText(viewer_->plot_stats_snapshot());
    }
  });
  connect(table_refresh_btn, &QPushButton::clicked, this, [this, table_view, table_status]() {
    if (viewer_) {
      table_view->setPlainText(viewer_->table_snapshot_text());
      table_status->setText(viewer_->table_stats_snapshot());
    }
  });
  connect(job_page, &MoosePanel::exodus_ready, table_refresh_btn,
          [table_refresh_btn]() { table_refresh_btn->click(); });
  connect(job_page, &MoosePanel::exodus_ready, plot_refresh_btn,
          [plot_refresh_btn]() { plot_refresh_btn->click(); });
  connect(gmsh_panel_, &GmshPanel::mesh_written, table_refresh_btn,
          [table_refresh_btn]() { table_refresh_btn->click(); });
  connect(results_work_tabs_, &QTabWidget::currentChanged, this,
          [this, plot_view, plot_status, table_view, table_status]() {
            if (!viewer_) {
              return;
            }
            if (plot_view) {
              plot_view->setPlainText(viewer_->plot_snapshot_text());
            }
            if (plot_status) {
              plot_status->setText(viewer_->plot_stats_snapshot());
            }
            if (table_view) {
              table_view->setPlainText(viewer_->table_snapshot_text());
            }
            if (table_status) {
              table_status->setText(viewer_->table_stats_snapshot());
            }
          });
  connect(plot_open_btn, &QPushButton::clicked, this,
          [this]() {
            if (viewer_) {
              viewer_->setFocus();
            }
          });
  connect(table_open_btn, &QPushButton::clicked, this,
          [this]() {
            if (viewer_) {
              viewer_->setFocus();
            }
          });

  connect(mesh_page, &GmshPanel::mesh_written, job_page,
          &MoosePanel::set_mesh_path);
  connect(mesh_page, &GmshPanel::boundary_groups, job_page,
          &MoosePanel::set_boundary_groups);
  connect(mesh_page, &GmshPanel::boundary_groups, property_editor_,
          &PropertyEditor::set_boundary_groups);
  connect(mesh_page, &GmshPanel::volume_groups, property_editor_,
          &PropertyEditor::set_volume_groups);
  connect(mesh_page, &GmshPanel::mesh_written, viewer_,
          &VtkViewer::set_mesh_file);
  connect(mesh_page, &GmshPanel::physical_group_selected, viewer_,
          &VtkViewer::set_mesh_group_filter);
  connect(viewer_, &VtkViewer::mesh_group_picked, mesh_page,
          &GmshPanel::select_physical_group);
  connect(viewer_, &VtkViewer::mesh_entity_picked, mesh_page,
          &GmshPanel::apply_entity_pick);
  connect(viewer_, &VtkViewer::mesh_group_picked, this,
          [this](int dim, int tag) {
            active_ui_context_.stage_selections = {{dim, tag}};
            select_model_item_for_mesh_reference(dim, tag, true);
            sync_active_ui_context();
            update_command_availability();
          });
  connect(viewer_, &VtkViewer::mesh_entity_picked, this,
          [this](int dim, int tag) {
            active_ui_context_.stage_selections = {{dim, tag}};
            select_model_item_for_mesh_reference(dim, tag, false);
            sync_active_ui_context();
            update_command_availability();
          });
  connect(mesh_page, &GmshPanel::mesh_generation_started, this, [this]() {
    active_ui_context_.mesh_running = true;
    if (moose_panel_) {
      moose_panel_->set_external_busy(true);
    }
    update_command_availability();
    const QVariantMap mesh_settings =
        gmsh_panel_ ? gmsh_panel_->gmsh_settings() : QVariantMap();
    gmp::log_operation(
        "mesh",
        QString("Mesh generation started (output=%1, dim=%2, size=%3, "
                "geometry=%4)")
            .arg(mesh_settings.value("output_path").toString())
            .arg(mesh_settings.value("mesh_dim").toInt())
            .arg(mesh_settings.value("mesh_size").toDouble())
            .arg(mesh_settings.value("geometry_path").toString()));
    statusBar()->showMessage("Generating mesh...", 0);
  });
  connect(mesh_page, &GmshPanel::mesh_generation_finished, this,
          [this](bool success, const QString& message) {
            active_ui_context_.mesh_running = false;
            if (moose_panel_) {
              moose_panel_->set_external_busy(false);
            }
            update_command_availability();
            gmp::log_operation(
                "mesh",
                QString("Mesh generation %1%2")
                    .arg(success ? "succeeded" : "FAILED")
                    .arg(message.isEmpty() ? QString() : ": " + message));
            statusBar()->showMessage(
                message.isEmpty()
                    ? (success ? QString("Mesh generated.")
                               : QString("Mesh generation failed."))
                    : message,
                3500);
          });
  connect(mesh_page, &GmshPanel::mesh_written, this,
          [this](const QString& path) {
            gmp::log_operation("mesh", "Mesh written and sent to stage: " +
                                           path);
            upsert_mesh_item(path);
            statusBar()->showMessage("Mesh generated.", 2000);
          });
  connect(job_page, &MoosePanel::exodus_ready, viewer_,
          &VtkViewer::set_exodus_file);
  connect(job_page, &MoosePanel::exodus_history, viewer_,
          &VtkViewer::set_exodus_history);
  // 远程（LIMS）作业登记：提交成功与状态刷新统一落到 Jobs 树和作业列表，
  // 与本地作业同一视图；远程作业不参与本地运行态/停止命令。
  connect(job_page, &MoosePanel::remote_job_event, this,
          [this](const QVariantMap& info) {
            auto* root = find_root_item("Jobs");
            const QString job_id = info.value("job_id").toString();
            if (!root || job_id.isEmpty()) {
              return;
            }
            QTreeWidgetItem* item = nullptr;
            for (int i = 0; i < root->childCount(); ++i) {
              if (root->child(i) && root->child(i)->text(0) == job_id) {
                item = root->child(i);
                break;
              }
            }
            QVariantMap params =
                item ? item->data(0, PropertyEditor::kParamsRole).toMap()
                     : QVariantMap();
            const QString raw_state = info.value("state").toString();
            // LIMS/C06 状态词汇映射到列表状态列；树状态图标按关键词匹配。
            static const QHash<QString, QString> kStateMap = {
                {"queued", "Queued"},   {"preparing", "Running"},
                {"running", "Running"}, {"succeeded", "Completed"},
                {"success", "Completed"}, {"failed", "Failed"},
                {"canceled", "Canceled"}};
            const QString mapped =
                kStateMap.value(raw_state.toLower(), QString());
            params.insert("status", mapped.isEmpty()
                                        ? (raw_state.isEmpty() ? QString("Queued")
                                                               : raw_state)
                                        : mapped);
            params.insert("state", raw_state);
            params.insert("job_id", job_id);
            params.insert("remote", true);
            const QString server = info.value("server").toString();
            if (!server.isEmpty()) {
              params.insert("exec", "remote: " + server);
            }
            const QString case_name = info.value("case_name").toString();
            if (!case_name.isEmpty()) {
              params.insert("case", case_name);
            }
            const QString snapshot = info.value("snapshot").toString();
            if (!snapshot.isEmpty()) {
              params.insert("mesh", snapshot);
            }
            const QString created = info.value("created_at").toString();
            if (params.value("start_time").toString().isEmpty()) {
              params.insert("start_time", created.isEmpty()
                                              ? info.value("submit_time").toString()
                                              : created);
            }
            QString progress = info.value("progress").toString();
            if (progress.isEmpty() && info.contains("percent")) {
              progress = QString("percent=%1")
                             .arg(info.value("percent").toDouble(), 0, 'f', 1);
            }
            if (!progress.isEmpty()) {
              params.insert("progress", progress);
            }
            // 耗时：优先起止时间差；运行中的作业用开始时间到当前。
            const QDateTime started =
                QDateTime::fromString(info.value("started_at").toString(),
                                      Qt::ISODate);
            const QDateTime finished =
                QDateTime::fromString(info.value("finished_at").toString(),
                                      Qt::ISODate);
            if (started.isValid()) {
              const QDateTime end =
                  finished.isValid() ? finished : QDateTime::currentDateTime();
              params.insert("duration",
                            QString::number(started.secsTo(end)) + "s");
            }
            if (item) {
              item->setData(0, PropertyEditor::kParamsRole, params);
            } else {
              add_child_item(root, job_id, "Jobs", params);
            }
            refresh_job_table();
            refresh_tree_statuses();
            refresh_results_navigation();
          });
  // ---- 作业监控信号 ----
  connect(job_page, &MoosePanel::remote_execution_status, this,
          [this](const QVariantMap& status) {
            if (status.value("job_id").toString() == selected_job_id_) {
              update_remote_job_detail(status);
            }
          });
  connect(job_page, &MoosePanel::remote_files, this,
          [this](const QVariantMap& body) {
            if (body.value("job_id").toString() == selected_job_id_) {
              update_remote_job_files(body);
            }
          });
  connect(job_page, &MoosePanel::remote_log, this,
          [this](const QString& job_id, const QString& text) {
            QDialog dialog(this);
            dialog.setWindowTitle("Remote Job Log — " + job_id);
            dialog.resize(820, 520);
            auto* layout = new QVBoxLayout(&dialog);
            auto* log_view = new QPlainTextEdit(&dialog);
            log_view->setReadOnly(true);
            log_view->setPlainText(text);
            layout->addWidget(log_view);
            dialog.exec();
          });
  connect(job_page, &MoosePanel::remote_cancel_done, this,
          [this](const QVariantMap& result) {
            statusBar()->showMessage(
                "Remote cancel accepted: " +
                    result.value("job_id").toString(),
                3000);
          });
  connect(job_page, &MoosePanel::remote_file_downloaded, this,
          [this](const QString& job_id, const QString& file_path,
                 const QString& local_path) {
            statusBar()->showMessage("Downloaded: " + local_path, 4000);
            const QString ext = QFileInfo(local_path).suffix().toLower();
            if (ext == "e" || ext == "exo" || ext == "exodus") {
              // Exodus 制品：注册 Results 节点（追溯 job_id）并询问是否载入。
              upsert_result_item(local_path, job_id);
              const auto answer = QMessageBox::question(
                  this, "Load Result",
                  QString("Load downloaded result into the viewport?\n%1")
                      .arg(local_path));
              if (answer == QMessageBox::Yes && viewer_) {
                viewer_->set_exodus_file(local_path);
              }
            }
          });
  connect(job_page, &MoosePanel::job_started, this,
          [this](const QVariantMap& info) {
            active_ui_context_.job_running = true;
            if (gmsh_panel_) {
              gmsh_panel_->set_external_busy(true);
            }
            update_command_availability();
            auto* root = find_root_item("Jobs");
            if (!root) {
              return;
            }
            const QString input_path = info.value("input").toString();
            const QString base = QFileInfo(input_path).baseName();
            const QString name =
                base.isEmpty()
                    ? QString("job_%1").arg(root->childCount() + 1)
                    : base;
            active_job_item_ = add_child_item(root, name, "Jobs", info);
            gmp::log_operation("job", QString("Job started: %1 (input=%2)")
                                          .arg(name, input_path));
            statusBar()->showMessage("Job running...", 2000);
            QVariantMap params = info;
            params.insert("status", "Running");
            params.insert("start_time",
                          QDateTime::currentDateTime().toString(Qt::ISODate));
            active_job_item_->setData(0, PropertyEditor::kParamsRole, params);
            active_job_row_ = append_job_row(name, params);
          });
  connect(job_page, &MoosePanel::job_finished, this,
          [this](const QVariantMap& info) {
            active_ui_context_.job_running = false;
            if (gmsh_panel_) {
              gmsh_panel_->set_external_busy(false);
            }
            update_command_availability();
            if (!active_job_item_) {
              statusBar()->showMessage("Job finished.", 2000);
              return;
            }
            QVariantMap params =
                active_job_item_->data(0, PropertyEditor::kParamsRole).toMap();
            for (auto it = info.begin(); it != info.end(); ++it) {
              params.insert(it.key(), it.value());
            }
            const QString status = info.value("status").toString() == "Normal"
                                       ? "Completed"
                                       : "Failed";
            params.insert("status", status);
            const QString start = params.value("start_time").toString();
            if (!start.isEmpty()) {
              const QDateTime start_dt =
                  QDateTime::fromString(start, Qt::ISODate);
              if (start_dt.isValid()) {
                const qint64 seconds = start_dt.secsTo(
                    QDateTime::currentDateTime());
                params.insert("duration", QString::number(seconds) + "s");
              }
            }
            active_job_item_->setData(0, PropertyEditor::kParamsRole, params);
            const QString job_name = active_job_item_->text(0);
            const QString exodus = info.value("exodus").toString();
            if (!exodus.isEmpty()) {
              upsert_result_item(exodus, job_name);
            }
            if (active_job_row_ >= 0) {
              update_job_row(active_job_row_, job_name, params);
              update_job_detail(active_job_row_);
            }
            active_job_item_ = nullptr;
            active_job_row_ = -1;
            gmp::log_operation(
                "job",
                QString("Job finished: %1 (status=%2, duration=%3, exodus=%4)")
                    .arg(job_name, status,
                         params.value("duration").toString(),
                         exodus.isEmpty() ? QString("(none)") : exodus));
            statusBar()->showMessage("Job finished.", 2000);
          });
  connect(job_page, &MoosePanel::exodus_ready, this,
          [this](const QString& path) { upsert_result_item(path, ""); });

  connect(job_run_btn, &QPushButton::clicked, this, [this]() {
    if (moose_panel_) {
      moose_panel_->run_job();
    }
  });
  connect(job_stop_btn, &QPushButton::clicked, this, [this]() {
    if (moose_panel_) {
      moose_panel_->stop_job();
    }
  });
  connect(job_retry_btn, &QPushButton::clicked, this, [this]() {
    if (moose_panel_) {
      moose_panel_->run_job();
    }
  });
  connect(job_result_btn, &QPushButton::clicked, this, [this]() {
    if (!job_table_ || !viewer_) {
      return;
    }
    const int row = job_table_->currentRow();
    if (row < 0) {
      return;
    }
    auto* item = job_table_->item(row, 0);
    if (!item) {
      return;
    }
    const QVariantMap params = item->data(Qt::UserRole).toMap();
    const QString result = params.value("exodus").toString();
    if (!result.isEmpty()) {
      viewer_->set_exodus_file(result);
      statusBar()->showMessage("Result loaded.", 2000);
    }
  });
  connect(job_log_btn, &QPushButton::clicked, this, [this]() {
    if (!moose_panel_) {
      return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle("Job Log");
    dialog.resize(800, 500);
    auto* layout = new QVBoxLayout(&dialog);
    auto* log_view = new QPlainTextEdit(&dialog);
    log_view->setReadOnly(true);
    log_view->setPlainText(moose_panel_->log_text());
    layout->addWidget(log_view);
    dialog.exec();
  });
  connect(job_table_, &QTableWidget::currentCellChanged, this,
          [this](int row, int, int, int) { apply_job_selection(row); });
  // ---- 作业监控按钮与筛选 ----
  connect(job_refresh_btn, &QPushButton::clicked, this, [this]() {
    if (moose_panel_) {
      moose_panel_->on_refresh_job();
    }
  });
  connect(job_state_filter_,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { refresh_job_table(); });
  connect(detail_refresh_btn, &QPushButton::clicked, this, [this]() {
    if (!selected_job_remote_ || selected_job_id_.isEmpty() ||
        !moose_panel_) {
      return;
    }
    moose_panel_->refresh_job_execution(selected_job_id_);
    moose_panel_->refresh_job_files(selected_job_id_);
  });
  connect(job_cancel_button_, &QPushButton::clicked, this, [this]() {
    if (!selected_job_remote_ || selected_job_id_.isEmpty() ||
        !moose_panel_) {
      return;
    }
    // 危险操作二次确认。
    const auto answer = QMessageBox::question(
        this, "Cancel Remote Job",
        QString("Cancel remote job %1 on the LIMS server?")
            .arg(selected_job_id_));
    if (answer == QMessageBox::Yes) {
      moose_panel_->request_cancel_job(selected_job_id_);
    }
  });
  connect(detail_taskmd_btn, &QPushButton::clicked, this, [this]() {
    if (!selected_job_remote_ || selected_job_id_.isEmpty() ||
        !moose_panel_) {
      return;
    }
    const QString dest =
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) +
        "/gmp_remote/" + selected_job_id_ + "/task.md";
    moose_panel_->download_remote_file(selected_job_id_, "task.md", dest);
    statusBar()->showMessage("Downloading task.md ...", 2000);
  });
  connect(detail_log_btn, &QPushButton::clicked, this, [this]() {
    if (!moose_panel_) {
      return;
    }
    if (selected_job_remote_ && !selected_job_id_.isEmpty()) {
      moose_panel_->request_job_log(selected_job_id_);
      return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle("Job Log");
    dialog.resize(800, 500);
    auto* layout = new QVBoxLayout(&dialog);
    auto* log_view = new QPlainTextEdit(&dialog);
    log_view->setReadOnly(true);
    log_view->setPlainText(moose_panel_->log_text());
    layout->addWidget(log_view);
    dialog.exec();
  });
  connect(detail_result_btn, &QPushButton::clicked, this, [this]() {
    if (!job_table_ || !viewer_ || job_table_->currentRow() < 0) {
      return;
    }
    auto* item = job_table_->item(job_table_->currentRow(), 0);
    if (!item) {
      return;
    }
    const QString result =
        item->data(Qt::UserRole).toMap().value("exodus").toString();
    if (!result.isEmpty()) {
      viewer_->set_exodus_file(result);
      statusBar()->showMessage("Result loaded.", 2000);
    }
  });
  connect(files_refresh_btn, &QPushButton::clicked, this, [this]() {
    if (selected_job_remote_ && !selected_job_id_.isEmpty() &&
        moose_panel_) {
      moose_panel_->refresh_job_files(selected_job_id_);
    }
  });
  connect(files_download_btn, &QPushButton::clicked, this, [this]() {
    if (!selected_job_remote_ || selected_job_id_.isEmpty() ||
        !moose_panel_ || !job_files_table_ ||
        job_files_table_->currentRow() < 0) {
      return;
    }
    auto* item = job_files_table_->item(job_files_table_->currentRow(), 1);
    if (!item) {
      return;
    }
    const QString file_path = item->data(Qt::UserRole).toString();
    if (file_path.isEmpty()) {
      return;
    }
    const QString dest =
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) +
        "/gmp_remote/" + selected_job_id_ + "/" + file_path;
    moose_panel_->download_remote_file(selected_job_id_, file_path, dest);
    statusBar()->showMessage("Downloading " + file_path + " ...", 2000);
  });
  // 自动刷新：仅当 Job 工作窗可见且选中远程运行中作业时触发；
  // 巡览模式不启用，避免依赖外部服务。
  job_auto_refresh_timer_ = new QTimer(this);
  job_auto_refresh_timer_->setInterval(5000);
  connect(job_auto_refresh_timer_, &QTimer::timeout, this, [this]() {
    if (!job_auto_refresh_ || !job_auto_refresh_->isChecked() ||
        !selected_job_remote_ || !selected_job_running_ ||
        selected_job_id_.isEmpty() || !moose_panel_ ||
        !job_work_window_ || !job_work_window_->isVisible() ||
        qEnvironmentVariableIsSet("GMP_SCREENSHOT_DIR")) {
      return;
    }
    moose_panel_->refresh_job_execution(selected_job_id_);
    moose_panel_->on_refresh_job();
  });
  job_auto_refresh_timer_->start();

  connect(model_tree_, &QTreeWidget::itemSelectionChanged, this,
          [this, sketch_tab, part_tab, property_tab, material_tab, section_tab,
           step_tab, interaction_tab, load_tab, mesh_tab, job_tab,
           results_tab, preview_sketch]() {
    auto* item = model_tree_->currentItem();
    property_editor_->set_item(item);
    // 模型树单击驱动选择与模块上下文；草图编辑由双击或显式命令进入。
    // 注: 必须用构造期解析的页签索引, 不能用 module_tab_index/tabText
    // (中文界面下 tabText 已被翻译, 运行期按名查找会失败)
    if (item) {
      const QString kind = item->data(0, PropertyEditor::kKindRole).toString();
      int tab = -1;
      if (kind == "Sketches") {
        tab = sketch_tab;
      } else if (kind == "Parts" || kind == "Datums") {
        tab = part_tab;
      } else if (kind == "Features") {
        // 特征节点是建模历史记录, 点选后看参数 -> 属性页签
        tab = property_tab;
      } else if (kind == "Materials") {
        tab = material_tab;
      } else if (kind == "Sections") {
        tab = section_tab;
      } else if (kind == "Steps") {
        tab = step_tab;
      } else if (kind == "Interactions") {
        tab = interaction_tab;
      } else if (kind == "Loads") {
        tab = load_tab;
      } else if (kind == "Mesh") {
        tab = mesh_tab;
      } else if (kind == "Jobs") {
        tab = job_tab;
      } else if (kind == "Results") {
        tab = results_tab;
      } else if (kind == "BC" || kind == "Functions" || kind == "Variables" ||
                 kind == "Outputs" || kind == "Physics" ||
                 kind == "Constraints" || kind == "Selections" ||
                 kind == "Input Cases") {
        tab = property_tab;
      } else if (kind == "Assembly") {
        tab = item->parent() ? property_tab : -1;
      }
      if (tab >= 0) {
        if (kind == "Sketches" && sketch_panel_ && item->parent()) {
          // 单击草图子节点只同步选择与模块上下文；编辑由双击或显式命令进入。
          if (auto* list = sketch_panel_->sketch_list()) {
            list->setCurrentRow(item->parent()->indexOfChild(item));
          }
          if (tab != module_tabs_->currentIndex()) {
            module_tabs_->setCurrentIndex(tab);
          }
          preview_sketch(item);
        } else if (kind == "Sketches" && !item->parent()) {
          if (tab != module_tabs_->currentIndex()) {
            module_tabs_->setCurrentIndex(tab);
          }
          preview_sketch(nullptr);
        } else if (tab != module_tabs_->currentIndex()) {
          module_tabs_->setCurrentIndex(tab);
        }
      }

      if (item->parent() && viewer_) {
        const QVariantMap params =
            item->data(0, PropertyEditor::kParamsRole).toMap();
        if (kind == "Mesh") {
          const QString path = params.value("path").toString();
          if (!path.isEmpty() && QFileInfo::exists(path)) {
            viewer_->set_mesh_file(path);
          }
        } else if (kind == "Results") {
          const QString path = params.value("path").toString();
          const QString ext = QFileInfo(path).suffix().toLower();
          if (!path.isEmpty() && QFileInfo::exists(path)) {
            if (ext == "e" || ext == "exo" || ext == "exodus") {
              viewer_->set_exodus_file(path);
            } else if (ext == "msh") {
              viewer_->set_mesh_file(path);
            }
          }
        } else if (kind == "Selections") {
          const int dim = params.value("dim", params.value("group_dim", -1))
                              .toInt();
          const int tag = params.value("tag", params.value("group_tag", -1))
                              .toInt();
          if (dim >= 0 && tag >= 0) {
            viewer_->set_mesh_group_filter(dim, tag);
          }
        } else if (kind == "Parts" || kind == "Features") {
          const QString owned_mesh = params.value("mesh").toString();
          if (!owned_mesh.isEmpty() && QFileInfo::exists(owned_mesh)) {
            if (QFileInfo(viewer_->current_file()).absoluteFilePath() !=
                QFileInfo(owned_mesh).absoluteFilePath()) {
              viewer_->set_mesh_file(owned_mesh);
            }
            // 特征即时生成的 MESH 文件归当前 Part/Feature 所有，文件中
            // 可能有多个不相连 Volume；恢复 Part 时必须显示完整文件。
            viewer_->set_mesh_entity_filter(-1, -1);
          } else {
            const QList<int> tags = volume_tags_from_params(params);
            if (tags.size() == 1) {
              viewer_->set_mesh_entity_filter(3, tags.front());
            } else if (tags.size() > 1) {
              // 旧项目可能未保存自有 mesh；至少不能错误收窄为首个实体。
              viewer_->set_mesh_entity_filter(-1, -1);
            }
          }
        }
      }
    }
    refresh_work_context();
    sync_active_ui_context();
    update_command_availability();
    // PropertyEditor 表单是动态重建的, 中文模式下需重新翻译
    if (l10n::current_language() == l10n::Language::Chinese) {
      QTimer::singleShot(0, this, [this]() { l10n::apply(this); });
    }
  });
  connect(model_tree_, &QTreeWidget::itemDoubleClicked, this,
          [this, sketch_tab, part_tab, open_sketch_editor,
           open_part_editor](QTreeWidgetItem* item, int) {
            if (!item || !item->parent()) {
              return;
            }
            const QString kind =
                item->data(0, PropertyEditor::kKindRole).toString();
            if (kind == "Sketches") {
              if (module_tabs_->currentIndex() != sketch_tab) {
                module_tabs_->setCurrentIndex(sketch_tab);
              }
              if (sketch_panel_ && sketch_panel_->sketch_list()) {
                sketch_panel_->sketch_list()->setCurrentRow(
                    item->parent()->indexOfChild(item));
              }
              open_sketch_editor(item);
            } else if (kind == "Parts") {
              if (module_tabs_->currentIndex() != part_tab) {
                module_tabs_->setCurrentIndex(part_tab);
              }
              open_part_editor(item);
            } else if (kind != "Mesh" && kind != "Jobs" &&
                       kind != "Results") {
              open_property_form(item);
            }
          });
  if (action_edit_properties_) {
    connect(action_edit_properties_, &QAction::triggered, this, [this]() {
      auto* item = model_tree_ ? model_tree_->currentItem() : nullptr;
      if (!item || !item->parent()) {
        statusBar()->showMessage("Select an editable model object first.",
                                 2000);
        return;
      }
      model_tree_->itemDoubleClicked(item, 0);
    });
  }
  connect(model_tree_, &QTreeWidget::itemChanged, this,
          [this](QTreeWidgetItem* item, int) {
            if (suppress_dirty_) {
              return;
            }
            if (!item || !item->parent()) {
              return;
            }
            invalidate_downstream_from(
                item->data(0, PropertyEditor::kKindRole).toString());
            set_project_dirty(true);
            refresh_module_pages();
            if (property_editor_) {
              property_editor_->refresh_form_options();
            }
          });

  connect(add_btn, &QPushButton::clicked, this, [this]() {
    auto* item = model_tree_->currentItem();
    if (item && !item->parent()) {
      add_item_under_root(item);
      return;
    }
    if (item && item->parent()) {
      add_item_under_root(item->parent());
      return;
    }
  });
  connect(remove_btn, &QPushButton::clicked, this, [this]() {
    remove_item(model_tree_->currentItem());
  });
  connect(rename_btn, &QPushButton::clicked, this, [this]() {
    auto* item = model_tree_->currentItem();
    if (!item || !item->parent()) {
      return;
    }
    rename_item(item);
  });
  connect(dup_btn, &QPushButton::clicked, this, [this]() {
    auto* item = model_tree_->currentItem();
    duplicate_item(item);
  });

  setCentralWidget(central);
  QSettings layout_settings("gmp-ise", "gmp_ise");
  const QByteArray window_geometry =
      layout_settings.value("ui/layout/v1/main_window_geometry").toByteArray();
  if (!window_geometry.isEmpty()) {
    restoreGeometry(window_geometry);
  }
  const QByteArray main_split_state =
      layout_settings.value("ui/layout/v1/main_split_state").toByteArray();
  if (!main_split_state.isEmpty()) {
    main_split_->restoreState(main_split_state);
  }
  const QByteArray vertical_split_state =
      layout_settings.value("ui/layout/v1/vertical_split_state").toByteArray();
  if (!vertical_split_state.isEmpty()) {
    vertical_split_->restoreState(vertical_split_state);
  }
  QByteArray workspace_geometry =
      layout_settings.value("ui/layout/v6/module_workspace_geometry")
          .toByteArray();
  if (workspace_geometry.isEmpty()) {
    workspace_geometry =
        layout_settings.value("ui/layout/v2/module_workspace_geometry")
            .toByteArray();
  }
  if (!workspace_geometry.isEmpty()) {
    module_work_window_->restoreGeometry(workspace_geometry);
  }
  auto restore_workspace_geometry = [&layout_settings](QDockWidget* workspace,
                                                       const QString& key) {
    const QByteArray geometry = layout_settings.value(key).toByteArray();
    if (workspace && !geometry.isEmpty()) {
      workspace->restoreGeometry(geometry);
    }
  };
  restore_workspace_geometry(job_work_window_,
                             "ui/layout/v3/job_workspace_geometry");
  restore_workspace_geometry(visualization_work_window_,
                             "ui/layout/v2/visualization_workspace_geometry");
  restore_workspace_geometry(results_work_window_,
                             "ui/layout/v2/results_workspace_geometry");
  restore_workspace_geometry(mesh_work_window_,
                             "ui/layout/v1/mesh_workspace_geometry");
  const int tool_layout_version =
      layout_settings.value("ui/layout/v3/version", 0).toInt();
  const QByteArray tool_layout_state =
      layout_settings.value("ui/layout/v3/main_window_state").toByteArray();
  const bool tool_layout_restored =
      tool_layout_version == 3 && !tool_layout_state.isEmpty() &&
      restoreState(tool_layout_state, 3);
  // 防御：恢复历史布局状态可能把工作窗放回 docked 位置（旧版本或损坏
  // 的 saveState blob），QMainWindow 会据此保留右/底部停靠区并在窗口
  // 放大时形成空白。工作窗合同为浮动专用，恢复后强制全部浮动。
  for (QDockWidget* workspace : {module_work_window_, mesh_work_window_,
                                 job_work_window_, visualization_work_window_,
                                 results_work_window_}) {
    if (workspace) {
      workspace->setFloating(true);
    }
  }
  if (!tool_layout_restored) {
    reset_tool_group_layout(false);
  }
  layout_ready_ = true;
  if (layout_settings.value("ui/layout/v2/module_workspace_visible", false)
          .toBool()) {
    module_work_window_->show();
  } else {
    module_work_window_->hide();
  }
  auto restore_workspace_visibility = [&layout_settings](
                                          QDockWidget* workspace,
                                          const QString& key) {
    if (!workspace) {
      return;
    }
    workspace->setVisible(layout_settings.value(key, false).toBool());
  };
  restore_workspace_visibility(job_work_window_,
                               "ui/layout/v3/job_workspace_visible");
  restore_workspace_visibility(
      visualization_work_window_,
      "ui/layout/v2/visualization_workspace_visible");
  restore_workspace_visibility(results_work_window_,
                               "ui/layout/v2/results_workspace_visible");
  restore_workspace_visibility(mesh_work_window_,
                               "ui/layout/v1/mesh_workspace_visible");
  // 启动恢复后统一做一次越界夹取，覆盖第二屏移除或 DPI 变化场景。
  for (QDockWidget* workspace : {mesh_work_window_, job_work_window_,
                                 visualization_work_window_,
                                 results_work_window_}) {
    clamp_window_to_screen(workspace);
  }
  project_status_label_ = new QLabel("Project: Untitled");
  dirty_status_label_ = new QLabel("Saved");
  active_context_status_label_ = new QLabel("Context: Part / Unselected");
  active_context_status_label_->setObjectName("activeContextStatus");
  statusBar()->addPermanentWidget(active_context_status_label_, 1);
  statusBar()->addPermanentWidget(project_status_label_);
  statusBar()->addPermanentWidget(dirty_status_label_);
  update_window_title();
  sync_active_ui_context();
  update_command_availability();
  statusBar()->showMessage("Ready");

  QTimer::singleShot(0, this, [this, tool_layout_restored]() {
    // Display Group 默认保持顶部停靠（make_group 创建即停靠）；
    // 用户手动浮动/拖出后的布局由 saveState 恢复，不强制拉回。
    recover_floating_tool_groups();
  });

  // 启动时恢复语言偏好 (动态重建的页面在模块切换时已另行处理)
  if (l10n::current_language() == l10n::Language::Chinese) {
    QTimer::singleShot(0, this, [this]() { l10n::apply(this); });
  }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  auto belongs_to_tool_group = [](QObject* object) {
    for (QObject* current = object; current; current = current->parent()) {
      if (current->property("gmpToolGroup").toBool()) {
        return true;
      }
    }
    return false;
  };
  if (event && event->type() == QEvent::MouseButtonPress &&
      belongs_to_tool_group(watched)) {
    const auto* mouse_event = static_cast<QMouseEvent*>(event);
    if (mouse_event->button() == Qt::LeftButton) {
      tool_drag_guard_active_ = true;
      // 草图工具有自己独立的互斥状态。点击/拖动悬浮工具组时若临时关闭
      // 再恢复 stage picking，会在释放鼠标时强制调用 Select，覆盖 Move、
      // Line 等刚刚选择的工具。草图会话不需要这层 3D 拾取保护。
      const bool sketch_session = viewer_ && viewer_->sketch_document();
      tool_drag_restore_picking_ =
          !sketch_session && action_stage_pick_ && action_stage_pick_->isChecked();
      if (tool_drag_restore_picking_ && viewer_) {
        viewer_->set_stage_picking(false);
      }
    }
  } else if (event && event->type() == QEvent::MouseButtonRelease &&
             tool_drag_guard_active_) {
    tool_drag_guard_active_ = false;
    if (tool_drag_restore_picking_ && viewer_) {
      viewer_->set_stage_picking(true);
    }
    tool_drag_restore_picking_ = false;
  }
  // ---- 工具组受控拖拽（浮出/磁吸，替代 Qt 原生拖出浮动）----
  auto* group_tb = qobject_cast<QToolBar*>(watched);
  if (group_tb && group_tb->property("gmpToolGroup").toBool() && event) {
    auto* mouse_event = static_cast<QMouseEvent*>(event);
    if (event->type() == QEvent::MouseButtonPress &&
        mouse_event->button() == Qt::LeftButton &&
        !group_tb->isFloating() &&
        !group_tb->actionAt(mouse_event->pos())) {
      // 空白区按下：可能是拖拽起点（按钮上不触发）。
      tool_group_press_target_ = group_tb;
      tool_group_press_global_ = mouse_event->globalPosition().toPoint();
    } else if (event->type() == QEvent::MouseMove &&
               tool_group_press_target_ == group_tb &&
               (mouse_event->buttons() & Qt::LeftButton)) {
      const QPoint current = mouse_event->globalPosition().toPoint();
      if ((current - tool_group_press_global_).manhattanLength() > 12) {
        // 拖拽超阈值：受控浮出，窗口标题栏放到光标下继续拖动。
        tool_group_press_target_ = nullptr;
        // 先补发一次释放，让 Qt 自己的拖拽状态机（QToolBarPrivate 的
        // dragging）在布局尚未变化时正常收尾；否则浮出改变布局后，
        // 释放阶段的 QToolBarPrivate::endDrag → revert 会访问失效 item
        // 而段错误（全屏拖出崩溃即此路径）。
        QMouseEvent end_drag(QEvent::MouseButtonRelease,
                             group_tb->mapFromGlobal(current),
                             group_tb->mapToGlobal(
                                 group_tb->mapFromGlobal(current)),
                             Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(group_tb, &end_drag);
        float_group_at(group_tb, current);
      }
    } else if (event->type() == QEvent::MouseButtonPress &&
               mouse_event->button() == Qt::LeftButton &&
               group_tb->isFloating()) {
      tool_group_float_dragging_ = true;
    } else if (event->type() == QEvent::MouseButtonRelease) {
      tool_group_press_target_ = nullptr;
      tool_group_float_dragging_ = false;
    } else if (event->type() == QEvent::Move &&
               group_tb->isFloating()) {
      try_snap_group(group_tb);
    } else if (event->type() == QEvent::Close && group_tb->isFloating()) {
      // 浮动窗的关闭按钮 = 停回工具条行，而不是隐藏工具组
      // （隐藏会让工具组从行内消失，不符合工具组浮动语义）。
      event->ignore();
      toggle_group_float(group_tb->objectName(), false);
      return true;
    }
  }
  // 浮动工作窗（任意 QDockWidget 顶层窗）拖拽释放后自愈一次工具条渲染。
  if (event && event->type() == QEvent::MouseButtonRelease && watched &&
      watched->isWidgetType() &&
      static_cast<QWidget*>(watched)->isWindow() &&
      qobject_cast<QDockWidget*>(watched)) {
    force_native_relayout();
  }
  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::position_default_display_group() {
  if (!display_tool_group_) {
    return;
  }
  display_tool_group_->adjustSize();
  if (!display_tool_group_->isFloating()) {
    // 保持工具栏仍注册在 QMainWindow 布局中，只切换为 Tool 顶层窗口；
    // 这样 Qt 能同时保存浮动状态，并允许用户拖回其他工具栏的同一行。
    display_tool_group_->setParent(this, Qt::Tool);
    display_tool_group_->setOrientation(Qt::Horizontal);
  }
  const QSize size = display_tool_group_->frameGeometry().size();
  QPoint target;
  if (viewer_) {
    target = viewer_->mapToGlobal(
        QPoint(std::max(12, viewer_->width() - size.width() - 20), 20));
  } else {
    target = mapToGlobal(QPoint(std::max(12, width() - size.width() - 24),
                               menuBar()->height() + 72));
  }
  display_tool_group_->move(target);
  display_tool_group_->show();
  display_tool_group_->raise();
}

void MainWindow::recover_floating_tool_groups() {
  auto recover = [this](QWidget* widget) {
    if (!widget || !widget->isVisible()) {
      return;
    }
    QRect frame = widget->frameGeometry();
    QScreen* target_screen = nullptr;
    for (auto* screen : QGuiApplication::screens()) {
      if (screen && screen->availableGeometry().intersects(frame)) {
        target_screen = screen;
        break;
      }
    }
    if (!target_screen) {
      target_screen = screen();
    }
    if (!target_screen) {
      target_screen = QGuiApplication::primaryScreen();
    }
    if (!target_screen) {
      return;
    }
    const QRect available = target_screen->availableGeometry();
    const int max_x =
        std::max(available.left(), available.right() - frame.width() + 1);
    const int max_y =
        std::max(available.top(), available.bottom() - frame.height() + 1);
    const QPoint clamped(qBound(available.left(), frame.x(), max_x),
                         qBound(available.top(), frame.y(), max_y));
    if (clamped != frame.topLeft()) {
      widget->move(clamped);
    }
  };
  const QStringList toolbar_names = {
      "projectToolGroup", "editToolGroup", "modelToolGroup",
      "meshToolGroup", "jobToolGroup", "displayToolGroup"};
  for (const QString& name : toolbar_names) {
    auto* toolbar = findChild<QToolBar*>(name);
    if (toolbar && toolbar->isFloating()) {
      recover(toolbar);
    }
  }
}

void MainWindow::try_snap_group(QToolBar* group_tb) {
  // 磁吸判定（Move 事件与轮询共用）：按住拖动（自有标志或物理按键）且
  // 窗口中心/顶边进入顶部工具条行磁吸区时吸回行内。
  if (!group_tb || !group_tb->isFloating()) {
    return;
  }
  const bool dragging = tool_group_float_dragging_ ||
                        (QGuiApplication::mouseButtons() & Qt::LeftButton);
  if (!dragging) {
    return;
  }
  int row_top = menuBar() ? menuBar()->frameGeometry().bottom() : 0;
  int row_bottom = row_top + 34;
  if (auto* any_group = findChild<QToolBar*>("projectToolGroup")) {
    if (!any_group->isFloating()) {
      row_top = any_group->frameGeometry().top();
      row_bottom = any_group->frameGeometry().bottom();
    }
  }
  const QRect frame = group_tb->frameGeometry();
  const int win_left = mapToGlobal(QPoint(0, 0)).x();
  const int win_right = win_left + width();
  const bool center_in = frame.center().y() >= row_top - 12 &&
                         frame.center().y() <= row_bottom + 12;
  const bool top_in =
      frame.top() >= row_top - 16 && frame.top() <= row_bottom + 16;
  const bool overlap_x =
      frame.right() >= win_left && frame.left() <= win_right;
  if ((center_in || top_in) && overlap_x) {
    const QString name = group_tb->objectName();
    QTimer::singleShot(0, this,
                       [this, name]() { toggle_group_float(name, false); });
  }
}

void MainWindow::float_group_at(QToolBar* toolbar,
                                const QPoint& global_pos) {
  if (!toolbar) {
    return;
  }
  // 受控浮动：从工具条布局拔出，切为 Tool 顶层窗。此路径与 Qt 原生
  // 拖出浮动不同，重停靠经复位验证可靠。global_pos 有效时把窗口标题栏
  // 放到光标下（拖拽浮出场景），否则层叠摆放在主窗右上方（菜单触发）。
  removeToolBar(toolbar);
  toolbar->setParent(this, Qt::Tool);
  toolbar->setOrientation(Qt::Horizontal);
  toolbar->adjustSize();
  QPoint target;
  if (!global_pos.isNull()) {
    target = global_pos - QPoint(toolbar->frameGeometry().width() / 2, 12);
  } else {
    static const QStringList names = {"projectToolGroup", "editToolGroup",
                                      "modelToolGroup",  "meshToolGroup",
                                      "jobToolGroup",    "displayToolGroup"};
    const int index =
        std::max(0, int(names.indexOf(toolbar->objectName())));
    target = mapToGlobal(
        QPoint(std::max(12, width() - toolbar->frameGeometry().width() - 24),
               96 + index * 28));
  }
  toolbar->move(target);
  toolbar->show();
  toolbar->raise();
  ensure_group_snap_timer();
  // 同步 Float Group 菜单勾选。
  if (auto* menu = findChild<QMenu*>("floatToolGroupMenu")) {
    for (auto* act : menu->actions()) {
      if (act->data().toString() == toolbar->objectName()) {
        const QSignalBlocker blocker(act);
        act->setChecked(true);
      }
    }
  }
  gmp::log_operation(
      "ui", QString("Tool group %1: float").arg(toolbar->objectName()));
}

void MainWindow::toggle_group_float(const QString& object_name,
                                    bool floating) {
  auto* toolbar = findChild<QToolBar*>(object_name);
  if (!toolbar) {
    return;
  }
  if (floating) {
    float_group_at(toolbar, QPoint());
    return;
  }
  {
    removeToolBar(toolbar);
    toolbar->setParent(this, Qt::Widget);
    toolbar->setOrientation(Qt::Horizontal);
    addToolBar(Qt::TopToolBarArea, toolbar);
    toolbar->show();
  }
  // 同步 Float Group 菜单勾选（复位/其他入口可能改变状态）。
  if (auto* menu = findChild<QMenu*>("floatToolGroupMenu")) {
    for (auto* act : menu->actions()) {
      if (act->data().toString() == object_name) {
        const QSignalBlocker blocker(act);
        act->setChecked(toolbar->isFloating());
      }
    }
  }
  gmp::log_operation(
      "ui", QString("Tool group %1: %2 (floating=%3)")
                .arg(object_name, floating ? "float" : "dock")
                .arg(toolbar->isFloating()));
}

void MainWindow::ensure_group_snap_timer() {
  if (group_snap_timer_) {
    return;
  }
  group_snap_timer_ = new QTimer(this);
  group_snap_timer_->setInterval(60);
  connect(group_snap_timer_, &QTimer::timeout, this, [this]() {
    static const QStringList names = {"projectToolGroup", "editToolGroup",
                                      "modelToolGroup",  "meshToolGroup",
                                      "jobToolGroup",    "displayToolGroup"};
    for (const QString& name : names) {
      if (auto* tb = findChild<QToolBar*>(name)) {
        if (tb->isFloating()) {
          try_snap_group(tb);
        }
      }
    }
  });
  group_snap_timer_->start();
}

void MainWindow::reset_tool_group_layout(bool show_feedback) {
  QSettings settings("gmp-ise", "gmp_ise");
  // v2 的 Display Group 是 QDockWidget，不能恢复到当前 QToolBar 类型。
  settings.remove("ui/layout/v2");
  settings.remove("ui/layout/v3");
  // V-03 全量复位：主窗口几何、分割条、各工作窗几何/可见性一并清除，
  // 下次启动也回到预置布局（布局键均带版本号，损坏时 restore 失败即
  // 落回默认，无需逐键校验）。
  settings.remove("ui/layout/v1");
  settings.remove("ui/layout/v6");

  // 逐组复位到顶部区域。被鼠标拖出的工具组是原生浮动顶层窗口：
  // 在 macOS 上没有任何代码路径（addToolBar/removeToolBar/restoreState）
  // 能把它可靠重停靠——只会得到“状态已停靠但渲染仍漂着”的僵尸窗。
  // 正确做法是用同一份动作重建工具组（启动期新工具组渲染已被验证），
  // 并替换 View 菜单的显隐开关与成员指针。
  const QStringList toolbar_names = {
      "projectToolGroup", "editToolGroup", "modelToolGroup",
      "meshToolGroup", "jobToolGroup", "displayToolGroup"};
  for (const QString& name : toolbar_names) {
    auto* toolbar = findChild<QToolBar*>(name);
    if (!toolbar) {
      continue;
    }
    const bool was_floating = toolbar->isFloating();
    if (was_floating) {
      auto* fresh = make_tool_group(toolbar->windowTitle(),
                                    toolbar->objectName());
      fresh->setIconSize(toolbar->iconSize());
      fresh->setToolButtonStyle(toolbar->toolButtonStyle());
      fresh->addActions(toolbar->actions());
      for (QAction* action : fresh->actions()) {
        // addAction 创建的动作归旧工具条所有（父子关系），先挂到主窗口
        // 名下，否则 deleteLater 旧工具条时会被连带销毁成悬空指针。
        action->setParent(this);
      }
      if (auto* menu = findChild<QMenu*>("toolbarVisibilityMenu")) {
        auto* old_toggle = toolbar->toggleViewAction();
        auto* new_toggle = fresh->toggleViewAction();
        new_toggle->setText(old_toggle->text());
        new_toggle->setChecked(old_toggle->isChecked());
        menu->insertAction(old_toggle, new_toggle);
        menu->removeAction(old_toggle);
      }
      if (toolbar == display_tool_group_) {
        display_tool_group_ = fresh;
      }
      toolbar->hide();
      toolbar->deleteLater();
      toolbar = fresh;
    }
    addToolBar(Qt::TopToolBarArea, toolbar);
    toolbar->setOrientation(Qt::Horizontal);
    toolbar->show();
    gmp::log_operation(
        "ui", QString("Layout reset: %1 (was_floating=%2, now area=%3, "
                      "floating=%4, visible=%5)")
                  .arg(name)
                  .arg(was_floating)
                  .arg(int(toolBarArea(toolbar)))
                  .arg(toolbar->isFloating())
                  .arg(toolbar->isVisible()));
  }
  // 左栏与底部区回到预置比例（与启动默认值一致）。
  if (main_split_) {
    const int left_w = qBound(250, int(width() * 0.22), 340);
    const int center_w = std::max(480, width() - left_w);
    main_split_->setSizes({left_w, center_w});
  }
  if (vertical_split_) {
    const int h = std::max(400, height());
    vertical_split_->setSizes({h * 4 / 5, h / 5});
  }
  // 各独立工作窗回到默认尺寸并隐藏；位置经公共越界恢复夹取。
  const QList<QPair<QDockWidget*, QSize>> workspaces = {
      {module_work_window_, QSize(680, 560)},
      {mesh_work_window_, QSize(760, 520)},
      {job_work_window_, QSize(820, 560)},
      {visualization_work_window_, QSize(660, 540)},
      {results_work_window_, QSize(640, 400)}};
  for (const auto& entry : workspaces) {
    if (!entry.first) {
      continue;
    }
    entry.first->resize(entry.second);
    clamp_window_to_screen(entry.first);
    entry.first->hide();
  }
  if (centralWidget()) {
    centralWidget()->updateGeometry();
  }
  QTimer::singleShot(0, this, &MainWindow::recover_floating_tool_groups);
  force_native_relayout();
  // 复位后全部组均为停靠态，同步 Float Group 菜单勾选。
  if (auto* menu = findChild<QMenu*>("floatToolGroupMenu")) {
    for (auto* act : menu->actions()) {
      auto* tb = findChild<QToolBar*>(act->data().toString());
      const QSignalBlocker blocker(act);
      act->setChecked(tb && tb->isFloating());
    }
  }
  if (show_feedback) {
    statusBar()->showMessage("Layout reset to default.", 2500);
  }
  gmp::log_operation("ui", "Layout reset to default.");
}

void MainWindow::force_native_relayout() {
  QTimer::singleShot(80, this, [this]() {
    const QSize current = size();
    resize(current.width(), current.height() + 1);
    resize(current);
  });
}

void MainWindow::resizeEvent(QResizeEvent* event) {
  QMainWindow::resizeEvent(event);
  // 防御：小窗 → 最大化/跨屏缩放时，个别平台（含 VTK 原生 GL 子控件、
  // 多屏 DPI 切换）可能出现中央区域未跟随窗口的几何滞留。
  // 显式触发一次布局重算与重绘，保证舞台与控制台充满新尺寸。
  if (centralWidget()) {
    centralWidget()->updateGeometry();
  }
  if (viewer_) {
    viewer_->update();
  }
}

void MainWindow::closeEvent(QCloseEvent* event) {
  QSettings settings("gmp-ise", "gmp_ise");
  settings.setValue("ui/layout/v3/version", 3);
  settings.setValue("ui/layout/v3/main_window_state", saveState(3));
  settings.setValue("ui/layout/v1/main_window_geometry", saveGeometry());
  if (main_split_) {
    settings.setValue("ui/layout/v1/main_split_state", main_split_->saveState());
  }
  if (vertical_split_) {
    settings.setValue("ui/layout/v1/vertical_split_state",
                      vertical_split_->saveState());
  }
  if (module_work_window_) {
    const QString geometry_key =
        module_workspace_sketch_profile_
            ? "ui/layout/v6/sketch_editor_geometry"
            : "ui/layout/v6/module_workspace_geometry";
    settings.setValue(geometry_key, module_work_window_->saveGeometry());
    if (!module_workspace_sketch_profile_) {
      // 保留旧键供降级版本读取；Sketch 的紧凑尺寸绝不写入通用窗口键。
      settings.setValue("ui/layout/v2/module_workspace_geometry",
                        module_work_window_->saveGeometry());
    }
    settings.setValue("ui/layout/v2/module_workspace_visible",
                      module_work_window_->isVisible());
  }
  auto save_workspace = [&settings](QDockWidget* workspace,
                                    const QString& key_prefix) {
    if (!workspace) {
      return;
    }
    settings.setValue(key_prefix + "_geometry", workspace->saveGeometry());
    settings.setValue(key_prefix + "_visible", workspace->isVisible());
  };
  save_workspace(job_work_window_, "ui/layout/v3/job_workspace");
  save_workspace(visualization_work_window_,
                 "ui/layout/v2/visualization_workspace");
  save_workspace(results_work_window_, "ui/layout/v2/results_workspace");
  save_workspace(mesh_work_window_, "ui/layout/v1/mesh_workspace");
  for (QDockWidget* compare_window : results_compare_windows_) {
    if (compare_window) {
      settings.setValue(compare_window->property("gmpGeometryKey").toString(),
                        compare_window->saveGeometry());
    }
  }
  settings.sync();
  QMainWindow::closeEvent(event);
}

void MainWindow::build_menu() {
  auto* file_menu = menuBar()->addMenu("&File");
  file_menu->setObjectName("fileMenu");
  action_new_ = file_menu->addAction("New Project");
  action_open_ = file_menu->addAction("Open Project...");
  action_save_ = file_menu->addAction("Save Project");
  action_save_as_ = file_menu->addAction("Save Project As...");
  recent_menu_ = file_menu->addMenu("Recent Projects");
  action_export_bundle_ = file_menu->addAction("Export Debug Bundle...");
  action_screenshot_ = file_menu->addAction("Save Screenshot...");
  auto* action_open_logs = file_menu->addAction("Open Operation Log Folder");
  action_new_->setShortcut(QKeySequence::New);
  action_open_->setShortcut(QKeySequence::Open);
  action_save_->setShortcut(QKeySequence::Save);
  action_save_as_->setShortcut(QKeySequence::SaveAs);
  action_screenshot_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));

  auto* model_menu = menuBar()->addMenu("&Model");
  model_menu->setObjectName("modelMenu");
  action_edit_properties_ = model_menu->addAction("Edit Properties...");
  action_edit_properties_->setShortcut(QKeySequence(Qt::Key_Return));
  action_sync_ = model_menu->addAction("Sync Model -> MOOSE Input");
  action_sync_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));

  view_menu_ = menuBar()->addMenu("&View");
  view_menu_->setObjectName("viewMenu");

  auto* mesh_menu = menuBar()->addMenu("&Mesh");
  mesh_menu->setObjectName("meshMenu");
  action_mesh_ = mesh_menu->addAction("Generate Mesh");
  action_preview_mesh_ = mesh_menu->addAction("Preview Mesh...");
  action_mesh_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
  action_preview_mesh_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));

  auto* job_menu = menuBar()->addMenu("&Job");
  job_menu->setObjectName("jobMenu");
  action_run_ = job_menu->addAction("Run");
  action_check_ = job_menu->addAction("Check Input");
  action_stop_ = job_menu->addAction("Stop");
  action_run_->setShortcut(QKeySequence(Qt::Key_F5));
  action_check_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
  action_stop_->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5));

  auto* tools_menu = menuBar()->addMenu("&Tools");
  tools_menu->setObjectName("toolsMenu");
  action_undo_ = tools_menu->addAction("Undo");
  action_redo_ = tools_menu->addAction("Redo");
  action_undo_->setShortcut(QKeySequence::Undo);
  action_redo_->setShortcut(QKeySequence::Redo);
  // Sketch Editor 是浮动顶层窗口；WindowShortcut 在其获得焦点后不会分发给
  // MainWindow 中的 QAction。编辑会话外动作会禁用，因此应用级作用域安全。
  action_undo_->setShortcutContext(Qt::ApplicationShortcut);
  action_redo_->setShortcutContext(Qt::ApplicationShortcut);
  action_undo_->setEnabled(false);
  action_redo_->setEnabled(false);
  tools_menu->addSeparator();
  auto* demo_menu = tools_menu->addMenu("Demos");
  auto* demo_setup_diff =
      demo_menu->addAction("Setup Transient Diffusion");
  auto* demo_run_diff = demo_menu->addAction("Run Transient Diffusion");
  demo_menu->addSeparator();
  auto* demo_setup_tm =
      demo_menu->addAction("Setup Thermo-Mechanics");
  auto* demo_run_tm = demo_menu->addAction("Run Thermo-Mechanics");
  demo_menu->addSeparator();
  auto* demo_setup_nl =
      demo_menu->addAction("Setup Nonlinear Heat");
  auto* demo_run_nl = demo_menu->addAction("Run Nonlinear Heat");

  // 设置菜单: 中英文界面切换 (字典式运行时翻译, 见 L10n)
  auto* settings_menu = menuBar()->addMenu("&Settings");
  settings_menu->setObjectName("settingsMenu");
  auto* lang_menu = settings_menu->addMenu("Language");
  auto* lang_group = new QActionGroup(this);
  lang_group->setExclusive(true);
  auto* lang_en = lang_menu->addAction("English");
  auto* lang_zh = lang_menu->addAction("中文");
  for (auto* a : {lang_en, lang_zh}) {
    a->setCheckable(true);
    lang_group->addAction(a);
  }
  const bool is_zh = l10n::current_language() == l10n::Language::Chinese;
  lang_zh->setChecked(is_zh);
  lang_en->setChecked(!is_zh);
  connect(lang_en, &QAction::triggered, this, [this]() {
    l10n::set_language(l10n::Language::English);
    l10n::apply(this);
    refresh_tree_statuses();
    refresh_results_navigation();
    update_window_title();
  });
  connect(lang_zh, &QAction::triggered, this, [this]() {
    l10n::set_language(l10n::Language::Chinese);
    l10n::apply(this);
    refresh_tree_statuses();
    refresh_results_navigation();
    update_window_title();
  });

  auto* help_menu = menuBar()->addMenu("&Help");
  help_menu->setObjectName("helpMenu");
  auto* about_action = help_menu->addAction("About GMP-ISE");
  connect(about_action, &QAction::triggered, this, [this]() {
    QMessageBox::about(
        this, "About GMP-ISE",
        "GMP-ISE finite-element preprocessing and job submission workspace.");
  });

  connect(action_new_, &QAction::triggered, this, [this]() {
    project_path_.clear();
    schema_version_ = project_schema::kCurrentVersion;
    application_profile_.clear();
    unit_contract_.clear();
    mesh_snapshot_ = PhysicalGroupManifest();
    input_snapshots_.clear();
    clear_model_tree_children();
    refresh_job_table();
    property_editor_->set_item(nullptr);
    refresh_module_pages();
    gmp::log_operation("project", "New project created.");
    statusBar()->showMessage("New project created.", 2000);
    set_project_dirty(false);
    update_project_status();
  });
  connect(action_open_, &QAction::triggered, this, [this]() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Open Project", project_path_,
        "GMP Project (*.gmp.yaml *.yaml)");
    if (path.isEmpty()) {
      return;
    }
    if (load_project(path)) {
      statusBar()->showMessage("Project loaded.", 2000);
      refresh_module_pages();
    }
  });
  connect(action_save_, &QAction::triggered, this, [this]() {
    if (project_path_.isEmpty()) {
      const QString path = QFileDialog::getSaveFileName(
          this, "Save Project", project_path_,
          "GMP Project (*.gmp.yaml *.yaml)");
      if (path.isEmpty()) {
        return;
      }
      project_path_ = path;
      update_project_status();
    }
    if (save_project(project_path_)) {
      gmp::log_operation("project", "Project saved: " + project_path_);
      statusBar()->showMessage("Project saved.", 2000);
      add_recent_project(project_path_);
      set_project_dirty(false);
    }
  });
  connect(action_save_as_, &QAction::triggered, this, [this]() {
    const QString path = QFileDialog::getSaveFileName(
        this, "Save Project As", project_path_,
        "GMP Project (*.gmp.yaml *.yaml)");
    if (path.isEmpty()) {
      return;
    }
    project_path_ = path;
    if (save_project(project_path_)) {
      gmp::log_operation("project", "Project saved: " + project_path_);
      statusBar()->showMessage("Project saved.", 2000);
      add_recent_project(project_path_);
      set_project_dirty(false);
      update_project_status();
    }
  });
  if (action_export_bundle_) {
    connect(action_export_bundle_, &QAction::triggered, this,
            &MainWindow::export_debug_bundle);
  }
  connect(action_open_logs, &QAction::triggered, this, [this]() {
    const QString log_path = gmp::operation_log_path();
    const QString dir = log_path.isEmpty()
                            ? QString()
                            : QFileInfo(log_path).absolutePath();
    if (dir.isEmpty() ||
        !QDesktopServices::openUrl(QUrl::fromLocalFile(dir))) {
      statusBar()->showMessage("Operation log is unavailable.", 3000);
    }
  });
  connect(action_screenshot_, &QAction::triggered, this, [this]() {
    const QString path = QFileDialog::getSaveFileName(
        this, "Save Screenshot", QDir::homePath(),
        "PNG Image (*.png)");
    if (path.isEmpty()) {
      return;
    }
    if (viewer_ && viewer_->save_screenshot(path)) {
      gmp::log_operation("ui", "Screenshot saved: " + path);
      statusBar()->showMessage("Screenshot saved.", 2000);
    } else {
      statusBar()->showMessage("Failed to save screenshot.", 2000);
    }
  });
  connect(action_sync_, &QAction::triggered, this,
          [this]() { sync_model_to_input(); });
  connect(action_mesh_, &QAction::triggered, this, [this]() {
    if (gmsh_panel_) {
      gmsh_panel_->generate_mesh();
      return;
    }
    statusBar()->showMessage("Mesh panel not ready.", 2000);
  });
  connect(action_preview_mesh_, &QAction::triggered, this, [this]() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Open Gmsh Mesh", QDir::homePath(), "Gmsh Mesh (*.msh)");
    if (path.isEmpty()) {
      return;
    }
    if (viewer_) {
      viewer_->set_mesh_file(path);
      statusBar()->showMessage("Mesh loaded.", 2000);
    }
  });
  connect(action_run_, &QAction::triggered, this, [this]() {
    if (moose_panel_) {
      moose_panel_->run_job();
      return;
    }
    statusBar()->showMessage("Job panel not ready.", 2000);
  });
  connect(action_check_, &QAction::triggered, this, [this]() {
    if (moose_panel_) {
      moose_panel_->check_input();
      return;
    }
    statusBar()->showMessage("Job panel not ready.", 2000);
  });
  connect(action_stop_, &QAction::triggered, this, [this]() {
    if (moose_panel_) {
      moose_panel_->stop_job();
      return;
    }
    statusBar()->showMessage("Job panel not ready.", 2000);
  });
  connect(demo_setup_diff, &QAction::triggered, this,
          [this]() { load_demo_diffusion(false); });
  connect(demo_run_diff, &QAction::triggered, this,
          [this]() { load_demo_diffusion(true); });
  connect(demo_setup_tm, &QAction::triggered, this,
          [this]() { load_demo_thermo(false); });
  connect(demo_run_tm, &QAction::triggered, this,
          [this]() { load_demo_thermo(true); });
  connect(demo_setup_nl, &QAction::triggered, this,
          [this]() { load_demo_nonlinear_heat(false); });
  connect(demo_run_nl, &QAction::triggered, this,
          [this]() { load_demo_nonlinear_heat(true); });

  update_recent_menu();
}

QToolBar* MainWindow::make_tool_group(const QString& title,
                                      const QString& object_name) {
  auto* toolbar = new QToolBar(title, this);
  addToolBar(Qt::TopToolBarArea, toolbar);
  toolbar->setObjectName(object_name);
  toolbar->setProperty("gmpToolGroup", true);
  toolbar->setMovable(true);
  // 禁用 Qt 原生拖出浮动：macOS 上 QToolBar 原生浮动窗无法被任何代码
  // 路径可靠重停靠/重绘（多轮僵尸窗/整行空白事故的根因）。工具组仍可
  // 在停靠区间拖动、经 View 菜单显隐；工作窗浮动（QDockWidget）不受影响。
  toolbar->setFloatable(false);
  toolbar->setAllowedAreas(Qt::AllToolBarAreas);
  toolbar->setIconSize(QSize(18, 18));
  toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
  // 受控拖拽（浮出/磁吸）由应用级事件过滤实现。
  toolbar->installEventFilter(this);
  auto update_compact_extent = [toolbar](Qt::Orientation orientation) {
    if (orientation == Qt::Horizontal) {
      toolbar->setMinimumWidth(0);
      toolbar->setMaximumWidth(QWIDGETSIZE_MAX);
      toolbar->setFixedHeight(30);
    } else {
      toolbar->setMinimumHeight(0);
      toolbar->setMaximumHeight(QWIDGETSIZE_MAX);
      toolbar->setFixedWidth(30);
    }
  };
  update_compact_extent(Qt::Horizontal);
  connect(toolbar, &QToolBar::orientationChanged, toolbar,
          update_compact_extent);
  connect(toolbar, &QToolBar::topLevelChanged, this,
          [this, toolbar, update_compact_extent](bool floating) {
            if (floating) {
              toolbar->setOrientation(Qt::Horizontal);
              update_compact_extent(Qt::Horizontal);
              QTimer::singleShot(0, this,
                                 &MainWindow::recover_floating_tool_groups);
            }
          });
  connect(toolbar, &QToolBar::visibilityChanged, this,
          [this, toolbar](bool visible) {
            if (visible && toolbar->isFloating()) {
              QTimer::singleShot(0, this,
                                 &MainWindow::recover_floating_tool_groups);
            }
          });
  return toolbar;
}

void MainWindow::build_toolbar() {
  auto* project_toolbar = make_tool_group("Project", "projectToolGroup");
  auto* edit_toolbar = make_tool_group("Edit", "editToolGroup");
  auto* model_toolbar = make_tool_group("Model", "modelToolGroup");
  auto* mesh_toolbar = make_tool_group("Mesh", "meshToolGroup");
  auto* job_toolbar = make_tool_group("Job", "jobToolGroup");

  if (action_new_) {
    action_new_->setIcon(MakeIcon(IconGlyph::NewFile));
    project_toolbar->addAction(action_new_);
  }
  if (action_open_) {
    action_open_->setIcon(MakeIcon(IconGlyph::OpenFolder));
    project_toolbar->addAction(action_open_);
  }
  if (action_save_) {
    action_save_->setIcon(MakeIcon(IconGlyph::SaveDisk));
    project_toolbar->addAction(action_save_);
  }
  if (action_save_as_) {
    action_save_as_->setIcon(MakeIcon(IconGlyph::SaveDisk));
    project_toolbar->addAction(action_save_as_);
  }
  if (action_screenshot_) {
    action_screenshot_->setIcon(MakeIcon(IconGlyph::Output));
    project_toolbar->addAction(action_screenshot_);
  }
  if (action_undo_) {
    action_undo_->setIcon(MakeIcon(IconGlyph::Undo));
    edit_toolbar->addAction(action_undo_);
  }
  if (action_redo_) {
    action_redo_->setIcon(MakeIcon(IconGlyph::Redo));
    edit_toolbar->addAction(action_redo_);
  }
  if (action_sync_) {
    action_sync_->setIcon(MakeIcon(IconGlyph::Sync));
    model_toolbar->addAction(action_sync_);
  }
  if (action_mesh_) {
    action_mesh_->setIcon(MakeIcon(IconGlyph::Mesh));
    mesh_toolbar->addAction(action_mesh_);
  }
  if (action_preview_mesh_) {
    action_preview_mesh_->setIcon(MakeIcon(IconGlyph::OpenFolder));
    mesh_toolbar->addAction(action_preview_mesh_);
  }
  if (action_run_) {
    action_run_->setIcon(MakeIcon(IconGlyph::Run));
    job_toolbar->addAction(action_run_);
  }
  if (action_check_) {
    action_check_->setIcon(MakeIcon(IconGlyph::Check));
    job_toolbar->addAction(action_check_);
  }
  if (action_stop_) {
    action_stop_->setIcon(MakeIcon(IconGlyph::Stop));
    job_toolbar->addAction(action_stop_);
  }

  // Abaqus 风格的紧凑显示组：默认悬浮于舞台右上角，同时保留 Qt
  // 原生的四向停靠预览和整组拖拽行为。
  display_tool_group_ = make_tool_group("Display Group", "displayToolGroup");
  action_display_mode_ = display_tool_group_->addAction(
      MakeIcon(IconGlyph::Display), "Cycle Display Mode");
  action_stage_pick_ = display_tool_group_->addAction(
      MakeIcon(IconGlyph::Pick), "Pick");
  action_stage_pick_->setCheckable(true);
  action_stage_clear_ = display_tool_group_->addAction(
      MakeIcon(IconGlyph::ClearSelection), "Clear Selection");
  action_stage_slice_ = display_tool_group_->addAction(
      MakeIcon(IconGlyph::Slice), "Slice");
  action_stage_slice_->setCheckable(true);
  action_display_mode_->setToolTip("Cycle Display Mode");
  action_stage_pick_->setToolTip("Pick");
  action_stage_clear_->setToolTip("Clear Selection");
  action_stage_slice_->setToolTip("Slice");
  connect(action_display_mode_, &QAction::triggered, this, [this]() {
    if (viewer_) {
      viewer_->cycle_stage_representation();
    }
  });
  connect(action_stage_pick_, &QAction::toggled, this, [this](bool enabled) {
    if (viewer_) {
      viewer_->set_stage_picking(enabled);
    }
  });
  connect(action_stage_clear_, &QAction::triggered, this, [this]() {
    if (viewer_) {
      viewer_->clear_stage_selection();
    }
    active_ui_context_.stage_selections.clear();
    update_command_availability();
  });
  connect(action_stage_slice_, &QAction::toggled, this, [this](bool enabled) {
    if (viewer_) {
      viewer_->set_stage_slice(enabled);
    }
  });
  // 直接切为顶层 Tool，同时仍保留在 QMainWindow 的工具栏布局注册表中。
  // 与 QDockWidget 不同，重新拖回顶部后可和其他 QToolBar 共用同一行。
  display_tool_group_->setParent(this, Qt::Tool);
  display_tool_group_->setOrientation(Qt::Horizontal);
  display_tool_group_->adjustSize();
  display_tool_group_->hide();

  if (view_menu_) {
    auto* toolbars_menu = view_menu_->addMenu("Toolbars");
    toolbars_menu->setObjectName("toolbarVisibilityMenu");
    for (auto* toolbar : {project_toolbar, edit_toolbar, model_toolbar,
                          mesh_toolbar, job_toolbar}) {
      auto* toggle = toolbar->toggleViewAction();
      toggle->setText(toolbar->windowTitle());
      toolbars_menu->addAction(toggle);
    }
    auto* display_toggle = display_tool_group_->toggleViewAction();
    display_toggle->setText("Display Group");
    toolbars_menu->addAction(display_toggle);
    // 受控浮动开关：六个工具组可切换为独立浮动小窗（Qt::Tool 顶层窗，
    // 非 Qt 原生拖出浮动）。checked 状态与复位/手动停靠同步。
    auto* float_menu = toolbars_menu->addMenu("Float Group");
    float_menu->setObjectName("floatToolGroupMenu");
    for (auto* toolbar : {project_toolbar, edit_toolbar, model_toolbar,
                          mesh_toolbar, job_toolbar, display_tool_group_}) {
      auto* act = float_menu->addAction(toolbar->windowTitle());
      act->setCheckable(true);
      act->setChecked(toolbar->isFloating());
      const QString name = toolbar->objectName();
      act->setData(name);
      connect(act, &QAction::toggled, this,
              [this, name](bool on) { toggle_group_float(name, on); });
    }
    view_menu_->addSeparator();
    action_reset_tool_layout_ =
        view_menu_->addAction("Reset Tool Layout");
    action_reset_tool_layout_->setObjectName("resetToolLayoutAction");
    connect(action_reset_tool_layout_, &QAction::triggered, this,
            [this]() { reset_tool_group_layout(true); });
  }

  // 应用级事件过滤只关注带 gmpToolGroup 标记的工具组；在拖拽开始时
  // 暂停拾取，释放后恢复，其他控件事件原样透传。
  qApp->installEventFilter(this);
  connect(qApp, &QGuiApplication::screenAdded, this, [this](QScreen* screen) {
    if (screen) {
      connect(screen, &QScreen::availableGeometryChanged, this,
              [this]() { recover_floating_tool_groups(); });
      connect(screen, &QScreen::geometryChanged, this,
              [this]() { recover_floating_tool_groups(); });
    }
    QTimer::singleShot(0, this, &MainWindow::recover_floating_tool_groups);
  });
  connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen*) {
    QTimer::singleShot(0, this, &MainWindow::recover_floating_tool_groups);
  });
  for (auto* screen : QGuiApplication::screens()) {
    if (!screen) {
      continue;
    }
    connect(screen, &QScreen::availableGeometryChanged, this,
            [this]() { recover_floating_tool_groups(); });
    connect(screen, &QScreen::geometryChanged, this,
            [this]() { recover_floating_tool_groups(); });
  }
}

void MainWindow::apply_theme() {
  QFont font = QApplication::font();
#if defined(Q_OS_MAC)
  font.setFamily("Helvetica Neue");
#elif defined(Q_OS_WIN)
  font.setFamily("Segoe UI");
#else
  font.setFamily("Noto Sans");
#endif
  font.setPointSize(12);
  QApplication::setFont(font);

  const QString style = R"(
/* ===== 现代浅色主题: 柔和底 + 白色卡片 + 蓝色点缀 ===== */
QMainWindow { background: #eef1f5; }
QWidget { color: #1f2937; }

QWidget#moduleBar {
  background: #e4e8ee;
  border-bottom: 1px solid #cbd2db;
  padding: 2px;
}
QWidget#moduleToolbar {
  background: #f2f4f8;
  border-bottom: 1px solid #cbd2db;
}
QWidget#treePanel,
QWidget#centerPanel,
QWidget#propertyPanel {
  border: 1px solid #d2d8e0;
  border-radius: 4px;
  background: #f7f8fa;
}
QLabel#workflowStatus {
  border: 1px solid #d5dbe3;
  border-radius: 4px;
  background: #eef4ff;
  color: #1e3a5f;
  padding: 5px 8px;
}

QMenuBar {
  background: #f5f6f8;
  border-bottom: 1px solid #d2d8e0;
}
QMenuBar::item { padding: 5px 12px; border-radius: 4px; }
QMenuBar::item:selected { background: #e0e9fb; }
QMenu {
  background: #ffffff;
  border: 1px solid #d2d8e0;
  padding: 4px;
}
QMenu::item { padding: 5px 24px 5px 28px; border-radius: 3px; }
QMenu::item:selected { background: #e0e9fb; }
QMenu::separator { height: 1px; background: #e2e6ec; margin: 4px 8px; }

QTabBar { qproperty-shape: RoundedNorth; border-bottom: 1px solid #cbd2db; }
QTabBar::tab {
  background: #e4e8ee;
  border: 1px solid #cbd2db;
  border-bottom: none;
  padding: 4px 12px;
  min-height: 22px;
  margin-right: 2px;
  border-top-left-radius: 5px;
  border-top-right-radius: 5px;
}
QTabBar::tab:selected {
  background: #ffffff;
  border-bottom: 2px solid #2f6fed;
  color: #1d4ed8;
}
QTabBar::tab:hover:!selected { background: #eef2f8; }
QTabBar::tear { border: 0; }
QTabWidget::pane { border: 1px solid #d2d8e0; border-radius: 3px; }
/* 视口内二级控制页签: 紧凑化, 把空间让给 3D 场景 */
QTabWidget#controlTabs QTabBar::tab {
  padding: 3px 8px;
  min-height: 18px;
}
QTabWidget#controlTabs QTabWidget::tab-bar {
  left: 4px;
}

QTreeWidget, QPlainTextEdit, QLineEdit, QTableWidget, QComboBox, QSpinBox,
QDoubleSpinBox, QListWidget {
  background: #ffffff;
  border: 1px solid #ccd3dc;
  border-radius: 3px;
}
QLineEdit:focus, QPlainTextEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus,
QComboBox:focus {
  border: 1px solid #2f6fed;
}
QTreeWidget::item, QTreeView::item { padding: 3px 6px; }
QTreeView::item:selected, QTreeWidget::item:selected {
  background: #dbe7ff; color: #1e3a5f;
}
QTreeView::item:hover:!selected { background: #eef2f8; }
QTableWidget::item { padding: 2px 4px; }
QHeaderView::section {
  background: #eef1f5;
  padding: 5px;
  border: none;
  border-right: 1px solid #d8dde4;
  border-bottom: 1px solid #cbd2db;
  font-weight: bold;
  color: #374151;
}

QComboBox {
  min-height: 22px;
  min-width: 56px;
  padding: 2px 22px 2px 8px;
  text-align: left;
}
QComboBox::down-arrow {
  image: url(":/icons/down-arrow.png");
  width: 10px;
  height: 8px;
}
QComboBox::down-arrow:on {
  image: url(":/icons/down-arrow-open.png");
  width: 10px;
  height: 8px;
}
QComboBox::drop-down {
  subcontrol-origin: padding;
  subcontrol-position: right center;
  width: 24px;
  border-left: 1px solid #ccd3dc;
}
QComboBox QAbstractItemView {
  background: #ffffff;
  border: 1px solid #ccd3dc;
  selection-background-color: #dbe7ff;
  selection-color: #1e3a5f;
  outline: 0;
}
QComboBox QAbstractItemView::item { min-height: 20px; }
QComboBox QAbstractItemView::item:hover { background: #dbe7ff; color: #1e3a5f; }
QComboBox QAbstractItemView::item:selected { background: #bcd4ff; color: #1e3a5f; }

QGroupBox {
  border: 1px solid #d2d8e0;
  border-radius: 4px;
  margin-top: 8px;
  padding-top: 4px;
  background: #fbfcfd;
}
QGroupBox::title {
  subcontrol-origin: margin;
  left: 8px;
  padding: 0 5px;
  color: #374151;
  font-weight: bold;
}

QPushButton {
  background: #ffffff;
  border: 1px solid #c6cdd7;
  border-radius: 3px;
  padding: 3px 9px;
  min-height: 22px;
}
QPushButton:hover { background: #eef4ff; border-color: #2f6fed; }
QPushButton:pressed { background: #dbe7ff; }
QPushButton:disabled { color: #9aa3af; background: #f3f4f6; }
QPushButton[gmpSketchTool="true"]:checked {
  background: #2f6fed;
  color: #ffffff;
  border-color: #2458bd;
  font-weight: 600;
}
QPushButton[gmpSketchTool="true"]:checked:hover { background: #245fce; }
QPushButton[gmpPrimaryAction="true"] {
  background: #2f6fed;
  color: #ffffff;
  border-color: #2458bd;
  font-weight: 600;
}
QPushButton[gmpPrimaryAction="true"]:hover { background: #245fce; }
QLabel[gmpActiveTool="true"] {
  color: #1f4f99;
  background: #e8f0ff;
  border: 1px solid #b8cdf5;
  border-radius: 4px;
  padding: 5px 8px;
  font-weight: 600;
}
QToolButton { background: transparent; padding: 2px 4px; border-radius: 3px; }
QToolButton:hover { background: #dbe4f0; }
QToolButton:checked { background: #cdd9ee; }

QToolBar { background: #f5f6f8; border-bottom: 1px solid #d2d8e0; spacing: 4px; }
QStatusBar { background: #f5f6f8; border-top: 1px solid #d2d8e0; }

QSplitter::handle { background: #e2e7ed; }
QSplitter::handle:hover { background: #b9c6d6; }
QSplitter::handle:horizontal { width: 4px; }
QSplitter::handle:vertical { height: 4px; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical {
  background: #c3cbd5; border-radius: 4px; min-height: 24px;
}
QScrollBar::handle:vertical:hover { background: #9fabb9; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal {
  background: #c3cbd5; border-radius: 4px; min-width: 24px;
}
QScrollBar::handle:horizontal:hover { background: #9fabb9; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; border: none; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }

QCheckBox, QRadioButton { spacing: 6px; }
QCheckBox::indicator, QRadioButton::indicator {
  width: 14px; height: 14px;
  border: 1px solid #aab4c0;
  border-radius: 3px;
  background: #ffffff;
}
QRadioButton::indicator { border-radius: 7px; }
QCheckBox::indicator:checked {
  background: #2f6fed;
  border: 1px solid #2f6fed;
  image: url(":/icons/check.png");
}
QRadioButton::indicator:checked {
  background: #2f6fed;
  border: 1px solid #2f6fed;
  image: url(":/icons/dot.png");
}
QCheckBox::indicator:hover, QRadioButton::indicator:hover { border-color: #2f6fed; }

QSlider::groove:horizontal {
  height: 4px; background: #d5dbe3; border-radius: 2px;
}
QSlider::handle:horizontal {
  width: 14px; height: 14px; margin: -5px 0;
  border-radius: 7px; background: #2f6fed;
}
QSlider::handle:horizontal:hover { background: #1d4ed8; }

QToolTip {
  background: #1f2937;
  color: #f9fafb;
  border: none;
  padding: 4px 8px;
}
)";
  setStyleSheet(style);
}

void MainWindow::build_model_tree() {
  const QStringList root_nodes = project_schema::model_root_nodes();
  for (const auto& name : root_nodes) {
    auto* item = new QTreeWidgetItem(model_tree_);
    item->setText(0, name);
    item->setExpanded(true);
    item->setData(0, PropertyEditor::kKindRole, name);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    QIcon icon;
    if (name == "Parts") {
      icon = MakeIcon(IconGlyph::Part);
    } else if (name == "Sketches") {
      icon = MakeIcon(IconGlyph::Section);
    } else if (name == "Features") {
      icon = MakeIcon(IconGlyph::Part);
    } else if (name == "Datums") {
      icon = MakeIcon(IconGlyph::Variable);
    } else if (name == "Materials") {
      icon = MakeIcon(IconGlyph::Material);
    } else if (name == "Sections") {
      icon = MakeIcon(IconGlyph::Section);
    } else if (name == "Steps") {
      icon = MakeIcon(IconGlyph::Step);
    } else if (name == "Functions") {
      icon = MakeIcon(IconGlyph::Function);
    } else if (name == "Variables") {
      icon = MakeIcon(IconGlyph::Variable);
    } else if (name == "BC") {
      icon = MakeIcon(IconGlyph::BC);
    } else if (name == "Loads") {
      icon = MakeIcon(IconGlyph::Load);
    } else if (name == "Outputs") {
      icon = MakeIcon(IconGlyph::Output);
    } else if (name == "Interactions") {
      icon = MakeIcon(IconGlyph::Interaction);
    } else if (name == "Assembly") {
      icon = MakeIcon(IconGlyph::Part);
    } else if (name == "Physics") {
      icon = MakeIcon(IconGlyph::Step);
    } else if (name == "Constraints") {
      icon = MakeIcon(IconGlyph::Interaction);
    } else if (name == "Selections") {
      icon = MakeIcon(IconGlyph::Pick);
    } else if (name == "Mesh") {
      icon = MakeIcon(IconGlyph::Mesh);
    } else if (name == "Input Cases") {
      icon = MakeIcon(IconGlyph::Output);
    } else if (name == "Jobs") {
      icon = MakeIcon(IconGlyph::Job);
    } else if (name == "Results") {
      icon = MakeIcon(IconGlyph::Result);
    }
    if (!icon.isNull()) {
      item->setIcon(0, icon);
    }
  }

  model_tree_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(model_tree_, &QTreeWidget::customContextMenuRequested, this,
          [this](const QPoint& pos) {
            auto* item = model_tree_->itemAt(pos);
            if (!item) {
              return;
            }
            model_tree_->setCurrentItem(item);
            QMenu menu(this);
            if (!item->parent()) {
              const QString kind = item->text(0);
              if (kind == "Mesh" || kind == "Jobs" || kind == "Results") {
                auto* open_action = menu.addAction(
                    kind == "Mesh"
                        ? "Open Mesh Workspace"
                        : (kind == "Jobs" ? "Open Job Workspace"
                                           : "Open Results Workspace"));
                connect(open_action, &QAction::triggered, this,
                        [this, kind]() {
                          QDockWidget* workspace =
                              kind == "Jobs"
                                  ? job_work_window_
                                  : (kind == "Results" ? results_work_window_
                                                       : mesh_work_window_);
                          if (workspace) {
                            workspace->show();
                            workspace->raise();
                            workspace->activateWindow();
                          }
                        });
                menu.addSeparator();
              }
              auto* add_action = menu.addAction(QString("Add %1").arg(kind));
              connect(add_action, &QAction::triggered, this,
                      [this, item]() { add_item_under_root(item); });
              menu.addSeparator();
              auto* expand_action = menu.addAction("Expand All");
              auto* collapse_action = menu.addAction("Collapse All");
              connect(expand_action, &QAction::triggered, model_tree_,
                      &QTreeWidget::expandAll);
              connect(collapse_action, &QAction::triggered, model_tree_,
                      &QTreeWidget::collapseAll);
            } else {
              const QString kind =
                  item->data(0, PropertyEditor::kKindRole).toString();
              QString open_label = "Edit Properties...";
              if (kind == "Sketches") {
                open_label = "Open Sketch Editor";
              } else if (kind == "Parts") {
                open_label = "Open Part Editor";
              } else if (kind == "Mesh") {
                open_label = "Open Mesh Workspace";
              } else if (kind == "Jobs") {
                open_label = "Open Job Workspace";
              } else if (kind == "Results") {
                open_label = "Open Result";
              }
              auto* edit_action = menu.addAction(open_label);
              menu.addSeparator();
              auto* duplicate_action = menu.addAction("Duplicate");
              auto* rename_action = menu.addAction("Rename");
              auto* delete_action = menu.addAction("Remove");
              connect(edit_action, &QAction::triggered, this,
                      [this, item, kind]() {
                model_tree_->setCurrentItem(item);
                if (kind == "Mesh") {
                  if (mesh_work_window_) {
                    mesh_work_window_->show();
                    mesh_work_window_->raise();
                  }
                } else if (kind == "Jobs") {
                  if (job_work_window_) {
                    job_work_window_->show();
                    job_work_window_->raise();
                  }
                } else if (kind == "Results") {
                  if (results_work_window_) {
                    results_work_window_->show();
                    results_work_window_->raise();
                  }
                  const QVariantMap params =
                      item->data(0, PropertyEditor::kParamsRole).toMap();
                  const QString path = params.value("path").toString();
                  const QString ext = QFileInfo(path).suffix().toLower();
                  if (viewer_ && !path.isEmpty()) {
                    if (ext == "e" || ext == "exo" || ext == "exodus") {
                      viewer_->set_exodus_file(path);
                    } else if (ext == "msh") {
                      viewer_->set_mesh_file(path);
                    }
                  }
                } else if (action_edit_properties_) {
                  action_edit_properties_->trigger();
                }
              });
              connect(duplicate_action, &QAction::triggered, this,
                      [this, item]() { duplicate_item(item); });
              connect(rename_action, &QAction::triggered, this, [this, item]() {
                rename_item(item);
              });
              connect(delete_action, &QAction::triggered, this,
                      [this, item]() { remove_item(item); });
            }
            l10n::apply(&menu);
            menu.exec(model_tree_->viewport()->mapToGlobal(pos));
          });
}

void MainWindow::open_property_form(QTreeWidgetItem* item) {
  if (!item || !item->parent()) {
    return;
  }
  if (floating_property_form_) {
    floating_property_form_->show();
    floating_property_form_->raise();
    floating_property_form_->activateWindow();
    return;
  }

  const QStringList boundaries =
      property_editor_ ? property_editor_->boundary_groups() : QStringList();
  const QStringList volumes =
      property_editor_ ? property_editor_->volume_groups() : QStringList();
  auto* form = new FloatingPropertyForm(item, boundaries, volumes, this);
  floating_property_form_ = form;
  l10n::apply(form);
  connect(form, &FloatingPropertyForm::committed, this,
          [this](QTreeWidgetItem* committed_item) {
            if (committed_item) {
              invalidate_downstream_from(
                  committed_item->data(0, PropertyEditor::kKindRole)
                      .toString());
            }
            set_project_dirty(true);
            if (model_tree_ && committed_item) {
              model_tree_->setCurrentItem(committed_item);
            }
            if (property_editor_) {
              property_editor_->set_item(committed_item);
            }
            refresh_module_pages();
            refresh_work_context();
            sync_active_ui_context();
            update_command_availability();
            statusBar()->showMessage("Properties updated.", 2000);
          });
  connect(form, &QObject::destroyed, this,
          [this]() {
            floating_property_form_ = nullptr;
            sync_active_ui_context();
            update_command_availability();
          });
  form->open();
  update_command_availability();
  QTimer::singleShot(0, form, [this, form]() {
    if (form) {
      form->place_over_stage(viewer_);
    }
  });
}

void MainWindow::clear_model_tree_children() {
  for (int i = 0; i < model_tree_->topLevelItemCount(); ++i) {
    auto* root = model_tree_->topLevelItem(i);
    if (!root) {
      continue;
    }
    root->takeChildren();
  }
}

void MainWindow::refresh_module_node_list(QListWidget* list,
                                         const QString& root_name,
                                         const QString& empty_text) const {
  if (!list) {
    return;
  }
  list->clear();
  auto* root = find_root_item(root_name);
  if (!root || root->childCount() == 0) {
    list->addItem(empty_text);
    return;
  }
  for (int i = 0; i < root->childCount(); ++i) {
    auto* child = root->child(i);
    if (!child) {
      continue;
    }
    QString label = child->text(0);
    const QVariantMap params =
        child->data(0, PropertyEditor::kParamsRole).toMap();
    const QString type = params.value("type").toString();
    if (!type.isEmpty()) {
      label += QString(" (%1)").arg(type);
    }
    auto* item = new QListWidgetItem(label, list);
    item->setData(Qt::UserRole, i);
    const QString status = params.value("status").toString();
    if (!status.isEmpty()) {
      item->setToolTip(QString("status: %1").arg(status));
    } else if (!params.isEmpty()) {
      item->setToolTip(params.keys().join(", "));
    }
  }
}

QString MainWindow::build_step_sequence_preview() const {
  auto* root = find_root_item("Steps");
  if (!root || root->childCount() == 0) {
    return "No step blocks yet.";
  }
  QStringList lines;
  lines << "Executioner uses the first step only.";
  lines << "Configured sequence:";
  for (int i = 0; i < root->childCount(); ++i) {
    auto* child = root->child(i);
    if (!child) {
      continue;
    }
    const QVariantMap params =
        child->data(0, PropertyEditor::kParamsRole).toMap();
    const QString type = params.value("type", "Transient").toString();
    const QString dt = params.value("dt", "default").toString();
    const QString end_time = params.value("end_time", "default").toString();
    lines << QString("%1) %2 | type=%3 dt=%4 end_time=%5")
                 .arg(i + 1)
                 .arg(child->text(0))
                 .arg(type)
                 .arg(dt)
                 .arg(end_time);
  }
  return lines.join('\n');
}

void MainWindow::refresh_module_pages() {
  refresh_module_node_list(module_part_list_, "Parts", "No parts yet.");
  refresh_module_node_list(module_material_list_, "Materials", "No materials yet.");
  refresh_module_node_list(module_section_list_, "Sections", "No sections yet.");
  refresh_module_node_list(module_assembly_list_, "Parts",
                          "No part entries available for assembly.");
  refresh_module_node_list(module_step_list_, "Steps", "No steps yet.");
  refresh_module_node_list(module_interaction_list_, "Interactions",
                          "No interactions yet.");
  refresh_module_node_list(module_load_list_, "Loads", "No loads yet.");
  if (sketch_panel_) {
    refresh_module_node_list(sketch_panel_->sketch_list(), "Sketches",
                            "No sketches yet.");
  }
  if (part_feature_panel_) {
    QStringList sketch_names;
    if (auto* root = find_root_item("Sketches")) {
      for (int i = 0; i < root->childCount(); ++i) {
        if (auto* child = root->child(i)) {
          sketch_names << child->text(0);
        }
      }
    }
    part_feature_panel_->set_sketch_names(sketch_names);
  }
  if (module_part_list_) {
    const QSignalBlocker blocker(module_part_list_);
    auto* part = active_part_item();
    auto* root = find_root_item("Parts");
    module_part_list_->setCurrentRow(part && root ? root->indexOfChild(part)
                                                  : -1);
  }
  if (step_sequence_preview_) {
    step_sequence_preview_->setPlainText(build_step_sequence_preview());
  }
  refresh_workflow_status();
  refresh_work_context();
  sync_active_ui_context();
  update_command_availability();
}

QString MainWindow::context_root_for_module(int module_index) const {
  // 顺序与构造函数中的隐藏 module_tabs_ 状态机保持一致。
  switch (module_index) {
    case 0:
      return "Sketches";
    case 1:
      return "Parts";
    case 3:
      return "Materials";
    case 4:
      return "Sections";
    case 5:
      return "Parts";  // Assembly 当前以部件实例为工作对象
    case 6:
      return "Steps";
    case 7:
      return "Interactions";
    case 8:
      return "Loads";
    case 9:
      return "Mesh";
    case 10:
      return "Jobs";
    case 11:
    case 12:
      return "Results";
    case 2: {
      // Property 直接跟随当前树对象所属根节点。
      auto* current = model_tree_ ? model_tree_->currentItem() : nullptr;
      return current && current->parent() ? current->parent()->text(0)
                                           : QString();
    }
    default:
      return {};
  }
}

void MainWindow::apply_module_workspace_profile(bool sketch_editor) {
  if (!module_work_window_) {
    return;
  }

  QSettings settings("gmp-ise", "gmp_ise");
  const QString current_key =
      module_workspace_sketch_profile_
          ? "ui/layout/v6/sketch_editor_geometry"
          : "ui/layout/v6/module_workspace_geometry";
  const QString target_key =
      sketch_editor ? "ui/layout/v6/sketch_editor_geometry"
                    : "ui/layout/v6/module_workspace_geometry";

  if (module_workspace_sketch_profile_ != sketch_editor) {
    settings.setValue(current_key, module_work_window_->saveGeometry());
    module_workspace_sketch_profile_ = sketch_editor;
  }

  const QSize minimum = sketch_editor ? QSize(640, 320) : QSize(620, 400);
  const QSize initial = sketch_editor ? QSize(680, 350) : QSize(680, 560);
  module_work_window_->setProperty(
      "gmpWorkspaceProfile", sketch_editor ? "sketch" : "module");
  module_work_window_->setMinimumSize(minimum);

  const QByteArray geometry = settings.value(target_key).toByteArray();
  if (geometry.isEmpty() || !module_work_window_->restoreGeometry(geometry)) {
    module_work_window_->resize(initial);
  }

  // 独立 profile 仍需适应当前屏幕，防止切换显示器后恢复到不可见区域。
  clamp_window_to_screen(module_work_window_);
}

void MainWindow::clamp_window_to_screen(QWidget* window) {
  if (!window) {
    return;
  }
  QScreen* screen = QGuiApplication::screenAt(window->frameGeometry().center());
  if (!screen) {
    screen = QGuiApplication::primaryScreen();
  }
  if (!screen) {
    return;
  }
  const QRect available = screen->availableGeometry().adjusted(16, 16, -16, -16);
  const QSize minimum = window->minimumSize().expandedTo(QSize(320, 240));
  QSize fitted = window->size();
  fitted.setWidth(qBound(minimum.width(), fitted.width(), available.width()));
  fitted.setHeight(qBound(minimum.height(), fitted.height(), available.height()));
  window->resize(fitted);

  QPoint position = window->frameGeometry().topLeft();
  position.setX(qBound(available.left(), position.x(),
                       available.right() - fitted.width() + 1));
  position.setY(qBound(available.top(), position.y(),
                       available.bottom() - fitted.height() + 1));
  window->move(position);
}

void MainWindow::remember_active_object_for_module(int module_index) {
  if (module_index < 0 || !model_tree_) {
    return;
  }
  const QString expected_root = context_root_for_module(module_index);
  auto* current = model_tree_->currentItem();
  if (expected_root.isEmpty() || !current || !current->parent() ||
      current->parent()->text(0) != expected_root) {
    return;
  }
  module_object_memory_.insert(module_index, current->text(0));
}

void MainWindow::restore_active_object_for_module(int module_index) {
  if (!model_tree_ || active_ui_context_.synchronizing || module_index == 2) {
    return;
  }
  const QString root_name = context_root_for_module(module_index);
  auto* root = root_name.isEmpty() ? nullptr : find_root_item(root_name);
  if (!root) {
    return;
  }
  auto* current = model_tree_->currentItem();
  const bool already_in_context =
      current && (current == root ||
                  (current->parent() && current->parent() == root));
  if (already_in_context) {
    remember_active_object_for_module(module_index);
    return;
  }

  QTreeWidgetItem* target = root;
  const QString remembered = module_object_memory_.value(module_index);
  for (int row = 0; !remembered.isEmpty() && row < root->childCount(); ++row) {
    if (root->child(row) && root->child(row)->text(0) == remembered) {
      target = root->child(row);
      break;
    }
  }
  active_ui_context_.synchronizing = true;
  model_tree_->setCurrentItem(target);
  model_tree_->scrollToItem(target);
  active_ui_context_.synchronizing = false;
}

void MainWindow::sync_active_ui_context() {
  if (module_tabs_) {
    active_ui_context_.module_index = module_tabs_->currentIndex();
  }
  auto* current = model_tree_ ? model_tree_->currentItem() : nullptr;
  if (current && current->parent()) {
    active_ui_context_.object_root = current->parent()->text(0);
    active_ui_context_.object_name = current->text(0);
    remember_active_object_for_module(active_ui_context_.module_index);
  } else {
    active_ui_context_.object_root =
        current ? current->text(0)
                : context_root_for_module(active_ui_context_.module_index);
    active_ui_context_.object_name.clear();
  }

  const bool chinese = l10n::current_language() == l10n::Language::Chinese;
  const QString module_name =
      module_tabs_ && active_ui_context_.module_index >= 0
          ? module_tabs_->tabText(active_ui_context_.module_index)
          : (chinese ? QString::fromUtf8("未选择") : QString("Unselected"));
  const QString object_name =
      active_ui_context_.object_name.isEmpty()
          ? (chinese ? QString::fromUtf8("未选择") : QString("Unselected"))
          : active_ui_context_.object_name;
  if (active_context_status_label_) {
    active_context_status_label_->setText(
        chinese ? QString::fromUtf8("上下文：%1 / %2").arg(module_name, object_name)
                : QString("Context: %1 / %2").arg(module_name, object_name));
    active_context_status_label_->setToolTip(
        QString("Active module: %1\nActive object: %2\nStage selections: %3")
            .arg(module_name, object_name)
            .arg(active_ui_context_.stage_selections.size()));
  }

  const QString suffix = active_ui_context_.object_name.isEmpty()
                             ? QString()
                             : QString(" — %1").arg(active_ui_context_.object_name);
  if (active_sketch_doc_ && module_work_window_) {
    module_work_window_->setWindowTitle("Sketch Editor" + suffix);
  } else if (module_work_window_ && module_work_window_->isVisible() &&
             active_ui_context_.module_index != 9 &&
             active_ui_context_.module_index != 10 &&
             active_ui_context_.module_index != 11 &&
             active_ui_context_.module_index != 12) {
    module_work_window_->setWindowTitle(module_name + " Workspace" + suffix);
  }
  if (mesh_work_window_ && active_ui_context_.module_index == 9) {
    mesh_work_window_->setWindowTitle("Mesh Workspace" + suffix);
  }
  if (visualization_work_window_ && active_ui_context_.module_index == 11) {
    visualization_work_window_->setWindowTitle("Visualization Workspace" +
                                               suffix);
  }
  if (job_work_window_ && active_ui_context_.module_index == 10) {
    job_work_window_->setWindowTitle("Job Workspace" + suffix);
  }
  if (results_work_window_ && active_ui_context_.module_index == 12) {
    results_work_window_->setWindowTitle("Results Workspace" + suffix);
  }
}

void MainWindow::update_command_availability() {
  const bool chinese = l10n::current_language() == l10n::Language::Chinese;
  const bool sketch_editing = active_sketch_doc_ != nullptr;
  const bool task_busy =
      active_ui_context_.mesh_running || active_ui_context_.job_running;
  auto* current = model_tree_ ? model_tree_->currentItem() : nullptr;
  const QString kind = current
                           ? current->data(0, PropertyEditor::kKindRole).toString()
                           : QString();
  const bool editable_object =
      current && current->parent() && kind != "Mesh" && kind != "Jobs" &&
      kind != "Results";

  if (module_selector_) {
    module_selector_->setEnabled(!sketch_editing);
    module_selector_->setToolTip(
        sketch_editing
            ? "Finish or close the active Sketch edit before switching modules."
            : "Select the active work module.");
  }
  if (context_object_selector_) {
    const bool has_context =
        !context_root_for_module(active_ui_context_.module_index).isEmpty();
    context_object_selector_->setEnabled(has_context && !sketch_editing);
    if (sketch_editing) {
      context_object_selector_->setToolTip(
          "Finish or close the active Sketch edit before changing objects.");
    }
  }
  if (model_tree_) {
    model_tree_->setEnabled(!sketch_editing);
  }
  if (results_navigation_tree_) {
    results_navigation_tree_->setEnabled(!sketch_editing);
  }
  if (navigation_tabs_) {
    navigation_tabs_->setEnabled(!sketch_editing);
  }

  if (action_edit_properties_) {
    action_edit_properties_->setEnabled(editable_object && !sketch_editing);
    action_edit_properties_->setToolTip(
        editable_object && !sketch_editing
            ? "Edit the active model object."
            : (sketch_editing
                   ? "Finish the active Sketch edit first."
                   : "Select an editable child object; roots, Mesh, Job and Result are not property forms."));
  }
  if (action_sync_) {
    action_sync_->setEnabled(!task_busy && !sketch_editing);
    action_sync_->setToolTip(
        task_busy ? "Wait for the active Mesh/Job task to finish."
                  : (sketch_editing ? "Finish the active Sketch edit first."
                                    : "Generate the active MOOSE .i input case."));
  }
  if (action_mesh_) {
    action_mesh_->setEnabled(!task_busy && !sketch_editing);
    action_mesh_->setText(
        active_ui_context_.mesh_running
            ? (chinese ? QString::fromUtf8("正在生成网格...")
                       : QString("Generating Mesh..."))
            : (chinese ? QString::fromUtf8("生成网格")
                       : QString("Generate Mesh")));
    action_mesh_->setToolTip(
        active_ui_context_.mesh_running
            ? "Mesh generation is running; duplicate submission is disabled."
            : (active_ui_context_.job_running
                   ? "Wait for the active Job to finish before regenerating the mesh."
                   : (sketch_editing ? "Finish the active Sketch edit first."
                                     : "Generate mesh for the active project.")));
  }
  if (action_preview_mesh_) {
    action_preview_mesh_->setEnabled(!active_ui_context_.mesh_running &&
                                     !sketch_editing);
  }
  if (action_run_) {
    action_run_->setEnabled(!task_busy && !sketch_editing);
    action_run_->setText(
        active_ui_context_.job_running
            ? (chinese ? QString::fromUtf8("运行中...") : QString("Running..."))
            : (chinese ? QString::fromUtf8("运行") : QString("Run")));
    action_run_->setToolTip(
        active_ui_context_.job_running
            ? "A Job is already running; duplicate submission is disabled."
            : (active_ui_context_.mesh_running
                   ? "Wait for mesh generation to finish."
                   : (sketch_editing ? "Finish the active Sketch edit first."
                                     : "Run the active MOOSE input case.")));
  }
  if (action_check_) {
    action_check_->setEnabled(!task_busy && !sketch_editing);
  }
  if (action_stop_) {
    action_stop_->setEnabled(active_ui_context_.job_running);
    action_stop_->setToolTip(active_ui_context_.job_running
                                 ? "Stop the active Job."
                                 : "No Job is currently running.");
  }
  if (action_stage_clear_) {
    const bool has_stage_selection =
        !active_ui_context_.stage_selections.isEmpty() ||
        (viewer_ && viewer_->sketch_document() &&
         !viewer_->sketch_selection().isEmpty());
    action_stage_clear_->setEnabled(has_stage_selection);
    action_stage_clear_->setToolTip(
        has_stage_selection ? "Clear the active stage selection."
                            : "No stage selection to clear.");
  }
  if (!active_sketch_doc_) {
    if (action_undo_) {
      action_undo_->setEnabled(false);
    }
    if (action_redo_) {
      action_redo_->setEnabled(false);
    }
  }

  if (job_run_button_) {
    job_run_button_->setEnabled(!task_busy);
    job_run_button_->setText(
        active_ui_context_.job_running
            ? (chinese ? QString::fromUtf8("运行中...") : QString("Running..."))
            : (chinese ? QString::fromUtf8("运行") : QString("Run")));
  }
  if (job_retry_button_) {
    job_retry_button_->setEnabled(!task_busy);
  }
  if (job_stop_button_) {
    job_stop_button_->setEnabled(active_ui_context_.job_running);
  }

  for (auto* button : findChildren<QPushButton*>()) {
    const QString command = button->property("moduleAction").toString();
    if (command.isEmpty()) {
      continue;
    }
    bool enabled = true;
    QString reason;
    if (command.contains("Generate Mesh", Qt::CaseInsensitive)) {
      enabled = !task_busy && !sketch_editing;
      reason = "Mesh generation is unavailable while another task or Sketch edit is active.";
    } else if (command == "Run" || command.contains("Check Input")) {
      enabled = !task_busy && !sketch_editing;
      reason = "Job commands are unavailable while another task or Sketch edit is active.";
    } else if (command.startsWith("Open Selected") || command == "Rename" ||
               command == "Duplicate" || command == "Remove") {
      enabled = editable_object && !sketch_editing;
      reason = "Select an editable object for this command.";
    }
    button->setEnabled(enabled);
    button->setToolTip(enabled ? QString() : reason);
  }
}

void MainWindow::refresh_work_context() {
  const bool chinese =
      l10n::current_language() == l10n::Language::Chinese;
  if (context_project_label_) {
    const QString display = project_path_.isEmpty()
                                ? (chinese ? QString::fromUtf8("未命名")
                                           : QString("Untitled"))
                                : QFileInfo(project_path_).fileName();
    context_project_label_->setText(display);
    context_project_label_->setToolTip(
        project_path_.isEmpty() ? QString("Current project: Untitled")
                                : project_path_);
  }

  if (module_selector_ && module_tabs_) {
    const QSignalBlocker blocker(module_selector_);
    const int combo_index =
        module_selector_->findData(module_tabs_->currentIndex());
    if (combo_index >= 0) {
      module_selector_->setCurrentIndex(combo_index);
    }
  }

  if (!context_object_selector_ || !module_tabs_) {
    return;
  }
  const QString root_name =
      context_root_for_module(module_tabs_->currentIndex());
  auto* root = root_name.isEmpty() ? nullptr : find_root_item(root_name);
  auto* current = model_tree_ ? model_tree_->currentItem() : nullptr;
  int selected_combo_index = 0;
  {
    const QSignalBlocker blocker(context_object_selector_);
    context_object_selector_->clear();
    context_object_selector_->addItem(
        chinese ? QString::fromUtf8("未选择") : QString("Unselected"), -1);
    context_object_selector_->setProperty("contextRoot", root_name);
    for (int row = 0; root && row < root->childCount(); ++row) {
      auto* child = root->child(row);
      if (!child) {
        continue;
      }
      context_object_selector_->addItem(child->text(0), row);
      if (child == current) {
        selected_combo_index = context_object_selector_->count() - 1;
      }
    }
    context_object_selector_->setCurrentIndex(selected_combo_index);
  }
  context_object_selector_->setEnabled(root != nullptr && !active_sketch_doc_);
  context_object_selector_->setToolTip(
      root ? QString("Current %1 object; selecting an entry locates it in the model tree.")
                 .arg(root_name)
           : QString("No object selector is available in this context."));
}

int MainWindow::child_count(const QString& root_name) const {
  const auto* root = find_root_item(root_name);
  return root ? root->childCount() : 0;
}

void MainWindow::apply_model_tree_filter(const QString& text) {
  if (!model_tree_) {
    return;
  }
  const QString needle = text.trimmed();
  for (int i = 0; i < model_tree_->topLevelItemCount(); ++i) {
    apply_name_filter(model_tree_->topLevelItem(i), needle);
  }
}

void MainWindow::apply_results_tree_filter(const QString& text) {
  if (!results_navigation_tree_) {
    return;
  }
  const QString needle = text.trimmed();
  for (int i = 0; i < results_navigation_tree_->topLevelItemCount(); ++i) {
    apply_name_filter(results_navigation_tree_->topLevelItem(i), needle);
  }
}

void MainWindow::refresh_tree_statuses() {
  if (!model_tree_) {
    return;
  }
  const QSignalBlocker tree_blocker(model_tree_);
  const bool chinese = l10n::current_language() == l10n::Language::Chinese;
  const QSet<QString> required_roots = {
      "Parts", "Materials", "Sections", "Steps",
      "BC",    "Loads",     "Mesh",     "Jobs"};

  auto set_status = [](QTreeWidgetItem* item, const QString& text,
                       IconGlyph glyph, const QString& tooltip) {
    if (!item) {
      return;
    }
    item->setText(1, text);
    item->setIcon(1, MakeIcon(glyph, 14));
    item->setToolTip(1, tooltip);
  };

  for (int i = 0; i < model_tree_->topLevelItemCount(); ++i) {
    auto* root = model_tree_->topLevelItem(i);
    if (!root) {
      continue;
    }
    bool has_running = false;
    bool has_failed = false;
    bool has_invalid = false;
    bool has_incomplete = false;
    bool has_generated = false;
    bool all_success = root->childCount() > 0;
    for (int row = 0; row < root->childCount(); ++row) {
      auto* child = root->child(row);
      if (!child) {
        continue;
      }
      const QVariantMap params =
          child->data(0, PropertyEditor::kParamsRole).toMap();
      if (child->icon(0).isNull() && !root->icon(0).isNull()) {
        child->setIcon(0, root->icon(0));
      }
      QString raw =
          child->data(0, PropertyEditor::kStatusRole).toString().trimmed();
      if (raw.isEmpty()) {
        raw = params.value("status").toString().trimmed();
      }
      const QString normalized = raw.toLower();
      const QString path = params.value("path").toString();
      const bool missing_file =
          (root->text(0) == "Mesh" || root->text(0) == "Results") &&
          !path.isEmpty() && !QFileInfo::exists(path);
      if (normalized.contains("fail") || normalized.contains("error")) {
        has_failed = true;
        all_success = false;
        set_status(child, chinese ? "失败" : "Failed", IconGlyph::Stop,
                   raw.isEmpty() ? QString("Failed") : raw);
      } else if (normalized.contains("run") ||
                 normalized.contains("queue") ||
                 normalized.contains("submit") ||
                 normalized.contains("pending")) {
        has_running = true;
        all_success = false;
        set_status(child, chinese ? "运行中" : "Running", IconGlyph::Run,
                   raw.isEmpty() ? QString("Running") : raw);
      } else if (normalized.contains("invalid") ||
                 normalized.contains("stale") ||
                 normalized.contains("outdated") || missing_file) {
        has_invalid = true;
        all_success = false;
        set_status(child, chinese ? "失效" : "Invalid", IconGlyph::Sync,
                   missing_file ? QString("Referenced file is unavailable: %1")
                                      .arg(path)
                                : raw);
      } else if (normalized.contains("complete") ||
                 normalized.contains("success") || normalized == "normal") {
        set_status(child, chinese ? "成功" : "Success", IconGlyph::Check,
                   raw);
      } else if (normalized.contains("generated") ||
                 normalized.contains("written")) {
        has_generated = true;
        all_success = false;
        set_status(child, chinese ? "已生成" : "Generated",
                   IconGlyph::Output, raw);
      } else if (normalized.contains("new") || normalized.contains("idle") ||
                 normalized.contains("missing") ||
                 (!params.isEmpty() &&
                  params.value("type").toString().isEmpty() &&
                  params.value("path").toString().isEmpty())) {
        has_incomplete = true;
        all_success = false;
        set_status(child, chinese ? "不完整" : "Incomplete",
                   IconGlyph::Sync,
                   raw.isEmpty() ? QString("Required configuration is incomplete")
                                 : raw);
      } else if (params.isEmpty()) {
        has_incomplete = true;
        all_success = false;
        set_status(child, chinese ? "未配置" : "Unconfigured",
                   IconGlyph::Result, "No configuration");
      } else {
        all_success = false;
        set_status(child, chinese ? "就绪" : "Ready", IconGlyph::Check,
                   raw.isEmpty() ? QString("Ready") : raw);
      }
    }

    const int count = root->childCount();
    if (has_failed || has_invalid) {
      set_status(root,
                 QString("%1 (%2)")
                     .arg(chinese ? "有问题" : "Issues")
                     .arg(count),
                 IconGlyph::Sync,
                 chinese ? "包含失败、失效或缺少文件的对象"
                         : "Contains failed, invalid, or missing-file objects");
    } else if (has_running) {
      set_status(root,
                 QString("%1 (%2)")
                     .arg(chinese ? "运行中" : "Running")
                     .arg(count),
                 IconGlyph::Run,
                 chinese ? "包含正在运行或排队的对象"
                         : "Contains running or queued objects");
    } else if (has_incomplete) {
      set_status(root,
                 QString("%1 (%2)")
                     .arg(chinese ? "不完整" : "Incomplete")
                     .arg(count),
                 IconGlyph::Sync,
                 chinese ? "包含尚未完成配置的对象"
                         : "Contains incompletely configured objects");
    } else if (all_success) {
      set_status(root,
                 QString("%1 (%2)").arg(chinese ? "成功" : "Success").arg(count),
                 IconGlyph::Check,
                 chinese ? "所有对象均已成功完成"
                         : "All objects completed successfully");
    } else if (has_generated) {
      set_status(root,
                 QString("%1 (%2)")
                     .arg(chinese ? "已生成" : "Generated")
                     .arg(count),
                 IconGlyph::Output,
                 chinese ? "对象已生成" : "Objects generated");
    } else if (count > 0) {
      set_status(root,
                 QString("%1 (%2)")
                     .arg(chinese ? "就绪" : "Ready")
                     .arg(count),
                 IconGlyph::Check, chinese ? "对象已配置" : "Objects configured");
    } else if (required_roots.contains(root->text(0))) {
      set_status(root, chinese ? "缺失 (0)" : "Missing (0)",
                 IconGlyph::Stop,
                 chinese ? "当前流程尚未配置此类对象"
                         : "This workflow object type is not configured");
    } else {
      set_status(root, chinese ? "未配置 (0)" : "Unconfigured (0)",
                 IconGlyph::Result, chinese ? "当前没有对象" : "No objects");
    }
  }
  apply_model_tree_filter(model_tree_filter_ ? model_tree_filter_->text()
                                             : QString());
}

void MainWindow::refresh_results_navigation() {
  if (!results_navigation_tree_ || !model_tree_) {
    return;
  }
  const QString selected_kind =
      results_navigation_tree_->currentItem()
          ? results_navigation_tree_->currentItem()
                ->data(0, kNavigationKindRole)
                .toString()
          : QString();
  const QString selected_name =
      results_navigation_tree_->currentItem()
          ? results_navigation_tree_->currentItem()
                ->data(0, kNavigationNameRole)
                .toString()
          : QString();
  const QSignalBlocker blocker(results_navigation_tree_);
  results_navigation_tree_->clear();

  for (const QString& kind : {QString("Jobs"), QString("Results")}) {
    auto* source_root = find_root_item(kind);
    auto* nav_root = new QTreeWidgetItem(results_navigation_tree_);
    nav_root->setText(0, kind);
    nav_root->setData(0, kNavigationKindRole, kind);
    nav_root->setFlags(nav_root->flags() & ~Qt::ItemIsEditable);
    nav_root->setIcon(0, MakeIcon(kind == "Jobs" ? IconGlyph::Job
                                                 : IconGlyph::Result));
    if (source_root) {
      nav_root->setText(1, source_root->text(1));
      nav_root->setIcon(1, source_root->icon(1));
      nav_root->setToolTip(1, source_root->toolTip(1));
      for (int row = 0; row < source_root->childCount(); ++row) {
        auto* source = source_root->child(row);
        if (!source) {
          continue;
        }
        auto* item = new QTreeWidgetItem(nav_root);
        item->setText(0, source->text(0));
        item->setText(1, source->text(1));
        item->setIcon(0, source->icon(0).isNull()
                             ? MakeIcon(kind == "Jobs" ? IconGlyph::Job
                                                       : IconGlyph::Result)
                             : source->icon(0));
        item->setIcon(1, source->icon(1));
        item->setData(0, kNavigationKindRole, kind);
        item->setData(0, kNavigationNameRole, source->text(0));
        const QVariantMap params =
            source->data(0, PropertyEditor::kParamsRole).toMap();
        item->setData(0, kNavigationPathRole, params.value("path").toString());
        item->setToolTip(0, params.value("path").toString());
        if (kind == selected_kind && source->text(0) == selected_name) {
          results_navigation_tree_->setCurrentItem(item);
        }
      }
    }
    nav_root->setExpanded(true);
  }
  apply_results_tree_filter(results_tree_filter_ ? results_tree_filter_->text()
                                                 : QString());
}

void MainWindow::select_model_item_from_results_navigation(
    QTreeWidgetItem* item) {
  auto* target = model_item_for_navigation(item);
  if (!target || !model_tree_) {
    return;
  }
  model_tree_->setCurrentItem(target);
  model_tree_->scrollToItem(target);
  if (target->parent()) {
    target->parent()->setExpanded(true);
  }
}

QTreeWidgetItem* MainWindow::model_item_for_navigation(
    QTreeWidgetItem* nav_item) const {
  if (!nav_item || !nav_item->parent()) {
    return nullptr;
  }
  const QString kind = nav_item->data(0, kNavigationKindRole).toString();
  const QString name = nav_item->data(0, kNavigationNameRole).toString();
  const QString path = nav_item->data(0, kNavigationPathRole).toString();
  auto* root = find_root_item(kind);
  if (!root) {
    return nullptr;
  }
  for (int row = 0; row < root->childCount(); ++row) {
    auto* candidate = root->child(row);
    if (!candidate) {
      continue;
    }
    const QVariantMap params =
        candidate->data(0, PropertyEditor::kParamsRole).toMap();
    if ((!path.isEmpty() && params.value("path").toString() == path) ||
        (path.isEmpty() && candidate->text(0) == name)) {
      return candidate;
    }
  }
  return nullptr;
}

void MainWindow::build_results_navigation_menu(QMenu* menu,
                                               QTreeWidgetItem* item) {
  if (!menu || !item) {
    return;
  }
  const QString kind = item->data(0, kNavigationKindRole).toString();
  auto show_workspace = [](QDockWidget* workspace) {
    if (!workspace) {
      return;
    }
    workspace->show();
    workspace->raise();
    workspace->activateWindow();
  };
  if (!item->parent()) {
    // 根节点：工作窗入口 + 根级操作 + 展开/折叠。
    if (kind == "Jobs") {
      auto* open_ws = menu->addAction("Open Job Workspace");
      connect(open_ws, &QAction::triggered, this,
              [this, show_workspace]() { show_workspace(job_work_window_); });
      auto* refresh = menu->addAction("Refresh Remote Jobs");
      connect(refresh, &QAction::triggered, this, [this]() {
        if (moose_panel_) {
          moose_panel_->on_refresh_job();
        }
      });
    } else if (kind == "Results") {
      auto* open_ws = menu->addAction("Open Results Workspace");
      connect(open_ws, &QAction::triggered, this, [this, show_workspace]() {
        show_workspace(results_work_window_);
      });
      auto* import = menu->addAction("Import Result File...");
      connect(import, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, "Import Result File", QDir::homePath(),
            "Result Files (*.e *.exo *.exodus *.msh *.csv *.txt *.log *.yaml "
            "*.yml);;All Files (*)");
        if (!path.isEmpty()) {
          import_result_file(path);
        }
      });
    }
    menu->addSeparator();
    auto* expand = menu->addAction("Expand All");
    auto* collapse = menu->addAction("Collapse All");
    connect(expand, &QAction::triggered, results_navigation_tree_,
            &QTreeWidget::expandAll);
    connect(collapse, &QAction::triggered, results_navigation_tree_,
            &QTreeWidget::collapseAll);
    return;
  }

  // 子节点：按 Jobs/Results 分流 + 通用 重命名/删除（映射回模型树条目）。
  auto* model_item = model_item_for_navigation(item);
  const QVariantMap params =
      model_item ? model_item->data(0, PropertyEditor::kParamsRole).toMap()
                 : QVariantMap();
  if (kind == "Jobs") {
    auto* open_ws = menu->addAction("Open Job Workspace");
    const QString job_name = item->text(0);
    connect(open_ws, &QAction::triggered, this,
            [this, show_workspace, job_name]() {
              show_workspace(job_work_window_);
              // 在作业列表中同步选中该作业。
              for (int row = 0; job_table_ && row < job_table_->rowCount();
                   ++row) {
                auto* cell = job_table_->item(row, 0);
                if (cell && cell->text() == job_name) {
                  job_table_->setCurrentCell(row, 0);
                  break;
                }
              }
            });
    if (params.value("remote").toBool()) {
      auto* refresh = menu->addAction("Refresh Status");
      const QString job_id = params.value("job_id").toString();
      connect(refresh, &QAction::triggered, this, [this, job_id]() {
        if (moose_panel_) {
          moose_panel_->refresh_job_execution(job_id);
          moose_panel_->on_refresh_job();
        }
      });
    }
    const QString exodus = params.value("exodus").toString();
    if (!exodus.isEmpty()) {
      auto* open_result = menu->addAction("Open Result");
      connect(open_result, &QAction::triggered, this, [this, exodus]() {
        if (viewer_) {
          viewer_->set_exodus_file(exodus);
        }
      });
    }
  } else if (kind == "Results") {
    const QString path = params.value("path").toString();
    auto* open_view = menu->addAction("Open in Viewer");
    open_view->setEnabled(!path.isEmpty());
    connect(open_view, &QAction::triggered, this, [this, path]() {
      if (!viewer_ || path.isEmpty()) {
        return;
      }
      const QString ext = QFileInfo(path).suffix().toLower();
      if (ext == "e" || ext == "exo" || ext == "exodus") {
        viewer_->set_exodus_file(path);
      } else {
        viewer_->set_mesh_file(path);
      }
    });
    auto* copy_path = menu->addAction("Copy Path");
    copy_path->setEnabled(!path.isEmpty());
    connect(copy_path, &QAction::triggered, this, [path]() {
      QGuiApplication::clipboard()->setText(path);
    });
  }
  if (model_item) {
    menu->addSeparator();
    auto* rename = menu->addAction("Rename");
    auto* remove = menu->addAction("Remove");
    connect(rename, &QAction::triggered, this,
            [this, model_item]() { rename_item(model_item); });
    connect(remove, &QAction::triggered, this,
            [this, model_item]() { remove_item(model_item); });
  }
}

void MainWindow::select_model_item_for_mesh_reference(int dim, int tag,
                                                       bool physical_group) {
  if (!model_tree_) {
    return;
  }
  QTreeWidgetItem* match = nullptr;
  QTreeWidgetItem* matched_root = nullptr;
  const QStringList root_order = physical_group
                                     ? QStringList{"Selections"}
                                     : QStringList{"Parts", "Features",
                                                   "Selections"};
  for (const QString& root_name : root_order) {
    auto* root = find_root_item(root_name);
    for (int row = 0; root && row < root->childCount(); ++row) {
      auto* candidate = root->child(row);
      if (!candidate) {
        continue;
      }
      const QVariantMap params =
          candidate->data(0, PropertyEditor::kParamsRole).toMap();
      const int candidate_dim = root_name == "Parts" || root_name == "Features"
                                    ? 3
                                    : params.value(
                                          "dim", params.value("group_dim", -1))
                                          .toInt();
      const int candidate_tag =
          root_name == "Parts" || root_name == "Features"
              ? params.value("gmsh_volume_tag", -1).toInt()
              : params.value("tag", params.value("group_tag", -1)).toInt();
      const bool candidate_matches =
          (root_name == "Parts" || root_name == "Features")
              ? volume_tags_from_params(params).contains(tag)
              : candidate_tag == tag;
      if (candidate_dim == dim && candidate_matches) {
        match = candidate;
        matched_root = root;
        break;
      }
    }
    if (match) {
      break;
    }
  }
  auto* fallback_root = find_root_item(physical_group ? "Selections" : "Parts");
  if (!matched_root) {
    matched_root = fallback_root;
  }
  if (!matched_root) {
    return;
  }
  matched_root->setExpanded(true);
  model_tree_->setCurrentItem(match ? match : matched_root);
  model_tree_->scrollToItem(match ? match : matched_root);
  if (navigation_tabs_) {
    navigation_tabs_->setCurrentIndex(0);
  }
  statusBar()->showMessage(
      match ? QString("Selection located: %1").arg(match->text(0))
            : QString("Picked mesh entity (%1, %2); no named Selection is bound.")
                  .arg(dim)
                  .arg(tag),
      2500);
}

void MainWindow::invalidate_downstream_from(const QString& source_kind) {
  if (suppress_dirty_ || source_kind.isEmpty() || source_kind == "Jobs" ||
      source_kind == "Results") {
    return;
  }

  QStringList targets;
  if (source_kind == "Mesh") {
    targets = {"Input Cases", "Jobs"};
  } else if (source_kind == "Input Cases") {
    targets = {"Jobs"};
  } else {
    targets = {"Mesh", "Input Cases", "Jobs"};
  }

  const QSignalBlocker blocker(model_tree_);
  for (const QString& target_kind : targets) {
    auto* root = find_root_item(target_kind);
    for (int row = 0; root && row < root->childCount(); ++row) {
      auto* child = root->child(row);
      if (!child) {
        continue;
      }
      const QString current =
          child->data(0, PropertyEditor::kStatusRole).toString().toLower();
      const QString param_status =
          child->data(0, PropertyEditor::kParamsRole)
              .toMap()
              .value("status")
              .toString()
              .toLower();
      const QString effective = current.isEmpty() ? param_status : current;
      if (effective.contains("run") || effective.contains("queue") ||
          effective.contains("submit")) {
        continue;
      }
      child->setData(0, PropertyEditor::kStatusRole, "Stale");
    }
  }
}

void MainWindow::refresh_workflow_status() {
  if (!workflow_status_label_) {
    return;
  }
  const int parts = child_count("Parts");
  const int materials = child_count("Materials");
  const int sections = child_count("Sections");
  const int steps = child_count("Steps");
  const int bcs = child_count("BC");
  const int loads = child_count("Loads");
  const int meshes = child_count("Mesh");
  const int jobs = child_count("Jobs");

  auto format_state = [](int count, const QString& name) -> QString {
    return QString("%1: %2 (%3)")
        .arg(name)
        .arg(count)
        .arg(count > 0 ? "ready" : "missing");
  };

  QStringList segments;
  segments << format_state(parts, "Parts");
  segments << format_state(materials, "Materials");
  segments << format_state(sections, "Sections");
  segments << format_state(steps, "Steps");
  segments << format_state(bcs, "BC");
  segments << format_state(loads, "Loads");
  segments << format_state(meshes, "Mesh");
  segments << format_state(jobs, "Jobs");

  workflow_status_label_->setText(QString("Workflow status: ") + segments.join(" | "));
  refresh_tree_statuses();
  refresh_results_navigation();
}

void MainWindow::ensure_basic_workflow_nodes() {
  auto make_name = [](const QString& base, QTreeWidgetItem* root) -> QString {
    if (!root) {
      return base;
    }
    QString cand = base;
    QSet<QString> existing;
    for (int i = 0; i < root->childCount(); ++i) {
      if (auto* c = root->child(i)) {
        existing.insert(c->text(0));
      }
    }
    if (!existing.contains(cand)) {
      return cand;
    }
    int seq = 1;
    while (true) {
      cand = QString("%1_%2").arg(base).arg(seq++);
      if (!existing.contains(cand)) {
        return cand;
      }
    }
  };

  const bool existed_parts = child_count("Parts") > 0;
  const bool existed_materials = child_count("Materials") > 0;
  const bool existed_sections = child_count("Sections") > 0;
  const bool existed_steps = child_count("Steps") > 0;
  const bool existed_bc = child_count("BC") > 0;
  const bool existed_loads = child_count("Loads") > 0;

  if (!existed_parts) {
    auto* root = find_root_item("Parts");
    if (root) {
      add_child_item(root, make_name("part_1", root), "Parts",
                     {{"type", "Part"}, {"description", "Auto-created for quick submit."}});
    }
  }

  if (!existed_materials) {
    auto* root = find_root_item("Materials");
    if (root) {
      add_child_item(root, make_name("material_1", root), "Materials",
                     {{"type", "GenericConstantMaterial"},
                      {"prop_names", "prop"},
                      {"prop_values", "1.0"}});
    }
  }

  if (!existed_sections) {
    auto* root = find_root_item("Sections");
    if (root) {
      auto* materials = find_root_item("Materials");
      QString material_name = "material_1";
      if (materials && materials->childCount() > 0 && materials->child(0)) {
        material_name = materials->child(0)->text(0);
      }
      add_child_item(root, make_name("section_1", root), "Sections",
                     {{"type", "SolidSection"},
                      {"material", material_name},
                      {"block", "solid"}});
    }
  }

  if (!existed_steps) {
    auto* root = find_root_item("Steps");
    if (root) {
      add_child_item(root, make_name("steady_step", root), "Steps",
                     {{"type", "Steady"}, {"dt", "1.0"}, {"end_time", "1.0"}});
    }
  }

  if (!existed_bc && !existed_loads) {
    auto* root = find_root_item("BC");
    if (root) {
      add_child_item(root, make_name("bc_1", root), "BC",
                     {{"type", "DirichletBC"},
                      {"variable", "u"},
                      {"boundary", "left"},
                      {"value", "0"}});
    }
    auto* loads_root = find_root_item("Loads");
    if (loads_root) {
      add_child_item(loads_root, make_name("load_1", loads_root), "Loads",
                     {{"type", "BodyForce"},
                      {"variable", "u"},
                      {"value", "0"}});
    }
  }

  refresh_module_pages();
  if (!existed_parts || !existed_materials || !existed_sections || !existed_steps ||
      (!existed_bc && !existed_loads)) {
    set_project_dirty(true);
    if (statusBar()) {
      statusBar()->showMessage("Auto-created missing workflow nodes for quick submit.",
                               2000);
    }
  }
}

void MainWindow::start_submit_workflow() {
  if (!gmsh_panel_ || !moose_panel_) {
    if (statusBar()) {
      statusBar()->showMessage("MOOSE/Gmsh panel unavailable.", 2500);
    }
    return;
  }

  ensure_basic_workflow_nodes();

  auto latest_mesh_from_project = [this]() -> QString {
    const auto* root = find_root_item("Mesh");
    if (!root || root->childCount() == 0) {
      return {};
    }
    for (int i = root->childCount() - 1; i >= 0; --i) {
      auto* item = root->child(i);
      if (!item) {
        continue;
      }
      const QVariantMap params =
          item->data(0, PropertyEditor::kParamsRole).toMap();
      const QString path = params.value("path").toString();
      if (!path.isEmpty()) {
        return path;
      }
    }
    return {};
  };

  auto sync_mesh_for_submit = [this, latest_mesh_from_project]() -> QString {
    QString path = moose_panel_->moose_settings().value("mesh_path").toString();
    if (!path.isEmpty()) {
      return path;
    }
    path = latest_mesh_from_project();
    if (!path.isEmpty()) {
      return path;
    }
    gmsh_panel_->set_mesh_generation_dim(3);
    gmsh_panel_->generate_mesh();
    const QString after_mesh =
        moose_panel_->moose_settings().value("mesh_path").toString();
    if (!after_mesh.isEmpty()) {
      return after_mesh;
    }
    return latest_mesh_from_project();
  };

  const QString mesh_path = sync_mesh_for_submit();
  if (mesh_path.isEmpty()) {
    if (statusBar()) {
      statusBar()->showMessage("No mesh found, cannot submit without mesh.", 3000);
    }
    return;
  }

  const QString template_key = moose_panel_->moose_settings().value("template_key").toString();
  if (template_key == "generated") {
    moose_panel_->set_template_by_key("filemesh");
  } else if (template_key == "tm_generated") {
    moose_panel_->set_template_by_key("tm_filemesh");
  }

  sync_model_to_input();
  moose_panel_->set_mesh_path(mesh_path);
  moose_panel_->run_job();
  if (statusBar()) {
    statusBar()->showMessage("Submit workflow started.", 3000);
  }
}

QTreeWidgetItem* MainWindow::find_root_item(const QString& name) const {
  for (int i = 0; i < model_tree_->topLevelItemCount(); ++i) {
    auto* root = model_tree_->topLevelItem(i);
    if (root && root->text(0) == name) {
      return root;
    }
  }
  return nullptr;
}

QTreeWidgetItem* MainWindow::find_child_by_param(QTreeWidgetItem* root,
                                                 const QString& key,
                                                 const QString& value) const {
  if (!root) {
    return nullptr;
  }
  for (int i = 0; i < root->childCount(); ++i) {
    auto* child = root->child(i);
    if (!child) {
      continue;
    }
    const QVariantMap params =
        child->data(0, PropertyEditor::kParamsRole).toMap();
    if (params.value(key).toString() == value) {
      return child;
    }
  }
  return nullptr;
}

bool MainWindow::child_name_exists(QTreeWidgetItem* root, const QString& name,
                                   const QTreeWidgetItem* exclude) const {
  if (!root) {
    return false;
  }
  const QString candidate = name.trimmed();
  if (candidate.isEmpty()) {
    return false;
  }
  for (int row = 0; row < root->childCount(); ++row) {
    const auto* child = root->child(row);
    if (child && child != exclude && child->text(0).trimmed() == candidate) {
      return true;
    }
  }
  return false;
}

QString MainWindow::unique_child_name(QTreeWidgetItem* root,
                                      const QString& preferred,
                                      const QTreeWidgetItem* exclude) const {
  QString base = preferred.trimmed();
  if (base.isEmpty()) {
    base = "item";
  }
  if (!child_name_exists(root, base, exclude)) {
    return base;
  }
  for (int suffix = 2;; ++suffix) {
    const QString candidate = QString("%1_%2").arg(base).arg(suffix);
    if (!child_name_exists(root, candidate, exclude)) {
      return candidate;
    }
  }
}

bool MainWindow::prompt_unique_child_name(QTreeWidgetItem* root,
                                          const QString& title,
                                          const QString& initial_name,
                                          QString* accepted_name,
                                          QTreeWidgetItem* exclude) {
  if (!root || !accepted_name) {
    return false;
  }

  QDialog dialog(this);
  dialog.setObjectName("uniqueObjectNameDialog");
  dialog.setWindowTitle(title);
  dialog.setModal(true);
  dialog.setMinimumWidth(360);
  auto* layout = new QVBoxLayout(&dialog);
  layout->setContentsMargins(16, 14, 16, 14);
  layout->setSpacing(8);
  layout->addWidget(new QLabel("Name:", &dialog));
  auto* editor = new QLineEdit(initial_name, &dialog);
  editor->setObjectName("uniqueObjectNameInput");
  layout->addWidget(editor);
  auto* error = new QLabel(&dialog);
  error->setObjectName("uniqueObjectNameError");
  error->setStyleSheet("color: #b42318;");
  error->setWordWrap(true);
  error->hide();
  layout->addWidget(error);
  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  buttons->setObjectName("uniqueObjectNameButtons");
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, &dialog,
          [&dialog, editor, error, root, exclude, accepted_name, this]() {
            const QString candidate = editor->text().trimmed();
            if (candidate.isEmpty()) {
              error->setText("Name cannot be empty.");
              error->show();
              editor->setFocus();
              editor->selectAll();
              return;
            }
            if (child_name_exists(root, candidate, exclude)) {
              error->setText(
                  QString("'%1' already exists under %2. Enter a different name.")
                      .arg(candidate, root->text(0)));
              error->show();
              editor->setFocus();
              editor->selectAll();
              return;
            }
            *accepted_name = candidate;
            dialog.accept();
          });
  connect(editor, &QLineEdit::returnPressed, buttons,
          [buttons]() {
            if (auto* ok = buttons->button(QDialogButtonBox::Ok)) {
              ok->click();
            }
          });
  l10n::apply(&dialog);
  QTimer::singleShot(0, editor, [editor]() {
    editor->setFocus();
    editor->selectAll();
  });
  return dialog.exec() == QDialog::Accepted;
}

QTreeWidgetItem* MainWindow::add_child_item(QTreeWidgetItem* root,
                                            const QString& name,
                                            const QString& kind,
                                            const QVariantMap& params) {
  if (!root) {
    return nullptr;
  }
  const QString safe_name = unique_child_name(root, name);
  const QVariantMap normalized = normalize_params_for_kind(kind, params);
  auto* item = new QTreeWidgetItem(root);
  item->setText(0, safe_name);
  item->setData(0, PropertyEditor::kKindRole, kind);
  item->setData(0, PropertyEditor::kParamsRole, normalized);
  item->setIcon(0, root->icon(0));
  root->setExpanded(true);
  model_tree_->setCurrentItem(item);
  invalidate_downstream_from(kind);
  set_project_dirty(true);
  refresh_module_pages();
  if (property_editor_) {
    property_editor_->refresh_form_options();
  }
  return item;
}

QTreeWidgetItem* MainWindow::active_part_item() const {
  auto* parts_root = find_root_item("Parts");
  if (!parts_root) {
    return nullptr;
  }

  auto* current = model_tree_ ? model_tree_->currentItem() : nullptr;
  if (current && current->parent() == parts_root &&
      current->data(0, PropertyEditor::kKindRole).toString() == "Parts") {
    return current;
  }

  return nullptr;
}

QTreeWidgetItem* MainWindow::attach_feature_to_part(
    QTreeWidgetItem* part, const QString& type, const QVariantMap& params,
    int gmsh_volume_tag, const QList<int>& gmsh_volume_tags,
    const QString& brep_path) {
  auto* parts_root = find_root_item("Parts");
  auto* features_root = find_root_item("Features");
  if (!part || !parts_root || !features_root || part->parent() != parts_root ||
      part->data(0, PropertyEditor::kKindRole).toString() != "Parts") {
    return nullptr;
  }

  int suffix = features_root->childCount() + 1;
  QString feature_name;
  bool exists = false;
  do {
    feature_name = QString("feature_%1").arg(suffix++);
    exists = false;
    for (int row = 0; row < features_root->childCount(); ++row) {
      if (features_root->child(row) &&
          features_root->child(row)->text(0) == feature_name) {
        exists = true;
        break;
      }
    }
  } while (exists);

  QVariantMap feature_params = params;
  feature_params.insert("type", type);
  feature_params.insert("part", part->text(0));
  feature_params.insert("gmsh_volume_tag", gmsh_volume_tag);
  if (!gmsh_volume_tags.isEmpty()) {
    feature_params.insert("gmsh_volume_tags",
                          volume_tags_to_variant(gmsh_volume_tags));
  }
  if (!brep_path.isEmpty()) {
    feature_params.insert("brep", brep_path);
  }

  auto* feature_item = new QTreeWidgetItem(features_root);
  feature_item->setText(0, feature_name);
  feature_item->setData(0, PropertyEditor::kKindRole, "Features");
  feature_item->setData(
      0, PropertyEditor::kParamsRole,
      normalize_params_for_kind("Features", feature_params));
  feature_item->setIcon(0, features_root->icon(0));
  features_root->setExpanded(true);

  QVariantMap part_params =
      part->data(0, PropertyEditor::kParamsRole).toMap();
  part_params.insert("type", "Part");
  part_params.insert("sketch", params.value("sketch"));
  part_params.insert("feature", feature_name);
  part_params.insert("gmsh_volume_tag", gmsh_volume_tag);
  if (!gmsh_volume_tags.isEmpty()) {
    part_params.insert("gmsh_volume_tags",
                       volume_tags_to_variant(gmsh_volume_tags));
  }
  if (brep_path.isEmpty()) {
    part_params.remove("brep");
  } else {
    part_params.insert("brep", brep_path);
  }
  part->setData(0, PropertyEditor::kParamsRole,
                normalize_params_for_kind("Parts", part_params));

  model_tree_->setCurrentItem(part);
  invalidate_downstream_from("Parts");
  set_project_dirty(true);
  refresh_module_pages();
  if (module_part_list_) {
    module_part_list_->setCurrentRow(parts_root->indexOfChild(part));
  }
  if (property_editor_) {
    property_editor_->refresh_form_options();
  }
  return feature_item;
}

void MainWindow::upsert_mesh_item(const QString& path) {
  if (path.isEmpty()) {
    return;
  }
  auto* root = find_root_item("Mesh");
  if (!root) {
    return;
  }
  auto* item = find_child_by_param(root, "path", path);
  const QString base = QFileInfo(path).baseName();
  const QString name =
      base.isEmpty() ? QString("mesh_%1").arg(root->childCount() + 1) : base;
  QVariantMap params;
  params.insert("path", path);
  params.insert("source", "gmsh");
  if (!item) {
    add_child_item(root, name, "Mesh", params);
  } else {
    item->setText(0, unique_child_name(root, name, item));
    item->setData(0, PropertyEditor::kParamsRole, params);
  }
  item = find_child_by_param(root, "path", path);
  if (item) {
    item->setData(0, PropertyEditor::kStatusRole, "Generated");
  }
  set_project_dirty(true);
}

void MainWindow::upsert_result_item(const QString& path,
                                    const QString& job_name) {
  if (path.isEmpty()) {
    return;
  }
  auto* root = find_root_item("Results");
  if (!root) {
    return;
  }
  auto* item = find_child_by_param(root, "path", path);
  const QString base = QFileInfo(path).baseName();
  const QString name =
      base.isEmpty() ? QString("result_%1").arg(root->childCount() + 1) : base;
  QVariantMap params;
  params.insert("path", path);
  if (!job_name.isEmpty()) {
    params.insert("job", job_name);
  }
  if (!item) {
    add_child_item(root, name, "Results", params);
  } else {
    item->setText(0, unique_child_name(root, name, item));
    item->setData(0, PropertyEditor::kParamsRole, params);
  }
  item = find_child_by_param(root, "path", path);
  if (item) {
    item->setData(0, PropertyEditor::kStatusRole, "Success");
  }
  set_project_dirty(true);
  refresh_results_panel();
}

void MainWindow::import_result_file(const QString& path) {
  if (path.isEmpty() || !QFileInfo::exists(path)) {
    statusBar()->showMessage("Result file is unavailable: " + path, 3000);
    return;
  }
  upsert_result_item(path, QString());
  const QString ext = QFileInfo(path).suffix().toLower();
  if (viewer_ && (ext == "e" || ext == "exo" || ext == "exodus")) {
    viewer_->set_exodus_file(path);
  } else if (viewer_ && ext == "msh") {
    viewer_->set_mesh_file(path);
  }
  gmp::log_operation("results", "Result file imported: " + path);
  statusBar()->showMessage("Result imported: " + QFileInfo(path).fileName(),
                           3000);
}

void MainWindow::refresh_results_panel() {
  if (!results_list_ || !results_preview_) {
    return;
  }
  QString saved_path;
  if (const auto* current = results_list_->currentItem()) {
    saved_path = current->data(Qt::UserRole).toString();
  }
  results_list_->clear();
  results_preview_->clear();
  QString filter_ext = "all";
  if (results_type_filter_) {
    filter_ext = results_type_filter_->currentData().toString();
  }

  auto* root = find_root_item("Results");
  if (!root || root->childCount() == 0) {
    if (results_list_->count() == 0) {
      results_list_->addItem("No results yet.");
    }
    return;
  }

  for (int i = 0; i < root->childCount(); ++i) {
    auto* item = root->child(i);
    if (!item) {
      continue;
    }
    const QString name = item->text(0);
    const QVariantMap params =
        item->data(0, PropertyEditor::kParamsRole).toMap();
    const QString path = params.value("path").toString();
    const QString ext = QFileInfo(path).suffix().toLower();
    bool should_include = true;
    if (filter_ext != "all" && !path.isEmpty()) {
      if (filter_ext == "e") {
        should_include = (ext == "e" || ext == "exo" || ext == "exodus");
      } else if (filter_ext == "msh") {
        should_include = (ext == "msh");
      } else if (filter_ext == "txt") {
        should_include = (ext == "txt" || ext == "csv" || ext == "log" ||
                          ext == "yaml" || ext == "yml");
      }
    }
    if (!should_include) {
      continue;
    }
    const QString status = params.value("status").toString();
    const QString job = params.value("job").toString();
    QString text = name;
    if (!status.isEmpty()) {
      text += QString(" (%1)").arg(status);
    }
    if (!job.isEmpty()) {
      text += QString(" [job:%1]").arg(job);
    }
    auto* row = new QListWidgetItem(text, results_list_);
    row->setData(Qt::UserRole, path);
    if (!job.isEmpty()) {
      row->setData(Qt::UserRole + 1, job);
    }
    if (!path.isEmpty()) {
      row->setToolTip(path);
    }
  }
  if (results_list_->count() == 0) {
    results_list_->addItem("No results yet.");
  }
  if (!saved_path.isEmpty()) {
    for (int i = 0; i < results_list_->count(); ++i) {
      auto* row = results_list_->item(i);
      if (!row) {
        continue;
      }
      if (row->data(Qt::UserRole).toString() == saved_path) {
        results_list_->setCurrentItem(row);
        break;
      }
    }
  } else if (results_list_->count() > 0 &&
             results_list_->item(0)->text() != "No results yet.") {
    results_list_->setCurrentRow(0);
  }
}

void MainWindow::populate_results_compare_list(QListWidget* list) const {
  if (!list) {
    return;
  }
  list->clear();
  auto* root = find_root_item("Results");
  if (!root || root->childCount() == 0) {
    list->addItem("No results yet.");
    return;
  }
  for (int i = 0; i < root->childCount(); ++i) {
    auto* item = root->child(i);
    if (!item) {
      continue;
    }
    const QString name = item->text(0);
    const QVariantMap params =
        item->data(0, PropertyEditor::kParamsRole).toMap();
    const QString path = params.value("path").toString();
    const QString status = params.value("status").toString();
    const QString job = params.value("job").toString();
    QString text = name;
    if (!status.isEmpty()) {
      text += QString(" (%1)").arg(status);
    }
    if (!job.isEmpty()) {
      text += QString(" [job:%1]").arg(job);
    }
    auto* row = new QListWidgetItem(text, list);
    row->setData(Qt::UserRole, path);
    row->setData(Qt::UserRole + 1, job);
    row->setData(Qt::UserRole + 2, name);
    if (!path.isEmpty()) {
      row->setToolTip(path);
    }
  }
  if (list->count() == 0) {
    list->addItem("No results yet.");
  }
}

QDockWidget* MainWindow::create_results_compare_window() {
  ++results_compare_counter_;
  const int index = results_compare_counter_;
  const QString base_title = QString("Results Compare #%1").arg(index);
  const QString geometry_key =
      QString("ui/layout/v1/results_compare_%1_geometry").arg(index);

  auto* window = new QDockWidget(base_title, this);
  window->setObjectName(QString("resultsCompareWindow%1").arg(index));
  window->setProperty("gmpGeometryKey", geometry_key);
  window->setFeatures(QDockWidget::DockWidgetClosable |
                      QDockWidget::DockWidgetMovable |
                      QDockWidget::DockWidgetFloatable);
  window->setMinimumSize(400, 260);
  window->resize(560, 400);
  addDockWidget(Qt::RightDockWidgetArea, window);
  window->setFloating(true);
  window->setAllowedAreas(Qt::NoDockWidgetArea);
  connect(window, &QDockWidget::topLevelChanged, window,
          [window](bool floating) {
            if (!floating) {
              window->setFloating(true);
            }
          });

  auto* content = new QWidget(window);
  auto* layout = new QVBoxLayout(content);
  layout->setContentsMargins(10, 10, 10, 10);
  layout->setSpacing(6);
  auto* head = new QLabel(
      "Result comparison — select an item to preview it, or load it on "
      "stage with Focus Viewport. Closing this window does not unload "
      "stage results or affect other results windows.",
      content);
  head->setWordWrap(true);
  layout->addWidget(head);

  auto* list = new QListWidget(content);
  list->setObjectName("resultsCompareList");
  list->setSelectionMode(QAbstractItemView::SingleSelection);
  list->setMinimumHeight(96);
  layout->addWidget(list);

  auto* preview = new QPlainTextEdit(content);
  preview->setObjectName("resultsComparePreview");
  preview->setReadOnly(true);
  preview->setLineWrapMode(QPlainTextEdit::NoWrap);
  preview->setPlaceholderText("Select a result item for quick preview.");
  layout->addWidget(preview, 1);

  auto* actions = new QHBoxLayout();
  auto* refresh_btn = new QPushButton("Refresh List", content);
  auto* focus_btn = new QPushButton("Focus Viewport", content);
  actions->addWidget(refresh_btn);
  actions->addWidget(focus_btn);
  actions->addStretch(1);
  auto* actions_row = new QWidget(content);
  actions_row->setLayout(actions);
  layout->addWidget(actions_row);
  window->setWidget(content);

  connect(list, &QListWidget::currentItemChanged, this,
          [this, window, base_title, preview](QListWidgetItem* row,
                                              QListWidgetItem*) {
            if (!row) {
              preview->clear();
              return;
            }
            const QString name = row->data(Qt::UserRole + 2).toString();
            if (!name.isEmpty()) {
              window->setWindowTitle(base_title + " — " + name);
            }
            const QString path = row->data(Qt::UserRole).toString();
            const QString job = row->data(Qt::UserRole + 1).toString();
            QString details = "Name: " + name;
            if (!job.isEmpty()) {
              details += "\nJob: " + job;
            }
            if (path.isEmpty()) {
              details += "\nPath: (none)";
            } else {
              const QFileInfo fi(path);
              details += "\nPath: " + path;
              details += QString("\nExists: %1").arg(fi.exists() ? "yes" : "no");
              if (fi.exists()) {
                details += QString("\nSize: %1 bytes").arg(fi.size());
                details += "\nModified: " +
                           fi.lastModified().toString(Qt::ISODate);
              }
              const QString ext = fi.suffix().toLower();
              if (fi.size() > 0 &&
                  (ext == "txt" || ext == "csv" || ext == "log" ||
                   ext == "yaml" || ext == "yml")) {
                QFile f(path);
                if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                  QStringList lines;
                  for (int i = 0; i < 6; ++i) {
                    const QByteArray chunk = f.readLine();
                    if (chunk.isEmpty()) {
                      break;
                    }
                    lines << QString::fromUtf8(chunk).trimmed();
                  }
                  if (!lines.isEmpty()) {
                    details += "\n\nPreview:\n" + lines.join("\n");
                  }
                }
              }
            }
            preview->setPlainText(details);
          });
  connect(refresh_btn, &QPushButton::clicked, this,
          [this, list]() { populate_results_compare_list(list); });
  connect(focus_btn, &QPushButton::clicked, this, [this, list]() {
    auto* row = list->currentItem();
    if (!row || !viewer_) {
      return;
    }
    const QString path = row->data(Qt::UserRole).toString();
    if (path.isEmpty()) {
      statusBar()->showMessage("Selected result has no path.", 2000);
      return;
    }
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "e" || ext == "exo" || ext == "exodus") {
      viewer_->set_exodus_file(path);
    } else {
      viewer_->set_mesh_file(path);
    }
    viewer_->setFocus();
    statusBar()->showMessage("Opened result in viewer.", 1500);
  });
  connect(window, &QDockWidget::visibilityChanged, this,
          [window](bool visible) {
            if (!visible) {
              QSettings settings("gmp-ise", "gmp_ise");
              settings.setValue(window->property("gmpGeometryKey").toString(),
                                window->saveGeometry());
            }
          });

  // 恢复同编号实例的几何记忆；越界时复用公共夹取规则。
  QSettings settings("gmp-ise", "gmp_ise");
  const QByteArray geometry = settings.value(geometry_key).toByteArray();
  if (!geometry.isEmpty()) {
    window->restoreGeometry(geometry);
  }
  clamp_window_to_screen(window);

  populate_results_compare_list(list);
  results_compare_windows_.append(window);
  // 对比窗口按需创建，启动期整窗翻译覆盖不到，单独应用一次。
  l10n::apply(content);
  window->show();
  window->raise();
  window->activateWindow();
  return window;
}

void MainWindow::sync_results_tree_selection(const QListWidgetItem* row) {
  if (!row || !model_tree_) {
    return;
  }
  const QString path = row->data(Qt::UserRole).toString();
  if (path.isEmpty()) {
    return;
  }
  auto* root = find_root_item("Results");
  if (!root) {
    return;
  }
  for (int i = 0; i < root->childCount(); ++i) {
    auto* node = root->child(i);
    if (!node) {
      continue;
    }
    const auto params = node->data(0, PropertyEditor::kParamsRole).toMap();
    if (params.value("path").toString() == path) {
      model_tree_->setCurrentItem(node);
      node->setExpanded(true);
      root->setExpanded(true);
      refresh_results_navigation();
      if (navigation_tabs_) {
        navigation_tabs_->setCurrentIndex(1);
      }
      if (results_navigation_tree_) {
        for (int top = 0;
             top < results_navigation_tree_->topLevelItemCount(); ++top) {
          auto* nav_root = results_navigation_tree_->topLevelItem(top);
          if (!nav_root ||
              nav_root->data(0, kNavigationKindRole).toString() != "Results") {
            continue;
          }
          for (int child = 0; child < nav_root->childCount(); ++child) {
            auto* nav_item = nav_root->child(child);
            if (nav_item &&
                nav_item->data(0, kNavigationPathRole).toString() == path) {
              results_navigation_tree_->setCurrentItem(nav_item);
              results_navigation_tree_->scrollToItem(nav_item);
              break;
            }
          }
        }
      }
      break;
    }
  }
}

QVariantMap MainWindow::default_params_for_kind(const QString& kind) const {
  if (kind == "Functions") {
    return {{"type", "ParsedFunction"}, {"expression", "1.0"}};
  }
  if (kind == "Variables") {
    return {{"order", "FIRST"}, {"family", "LAGRANGE"}};
  }
  if (kind == "Materials") {
    return {{"type", "GenericConstantMaterial"},
            {"prop_names", "prop"},
            {"prop_values", "1.0"}};
  }
  if (kind == "BC") {
    return {{"type", "DirichletBC"},
            {"variable", "u"},
            {"boundary", "left"},
            {"value", "0"}};
  }
  if (kind == "Loads") {
    return {{"type", "BodyForce"}, {"variable", "u"}, {"value", "0"}};
  }
  if (kind == "Outputs") {
    return {{"type", "Exodus"}, {"exodus", "true"}};
  }
  if (kind == "Steps") {
    return {{"type", "Transient"}, {"dt", "0.1"}, {"end_time", "1.0"}};
  }
  if (kind == "Sections") {
    return {{"type", "SolidSection"}, {"material", "material_1"}};
  }
  if (kind == "Parts") {
    return {{"type", "Part"}, {"description", ""}};
  }
  if (kind == "Interactions") {
    return {{"type", "Interaction"}};
  }
  if (kind == "Mesh") {
    return {{"status", "New"}};
  }
  if (kind == "Jobs") {
    return {{"status", "Idle"}};
  }
  if (kind == "Results") {
    return {{"status", "Ready"}};
  }
  if (kind == "Input Cases") {
    return {{"status", "New"}};
  }
  return {};
}

QVariantMap MainWindow::normalize_params_for_kind(
    const QString& kind, const QVariantMap& params) const {
  if (!params.isEmpty()) {
    return params;
  }
  return default_params_for_kind(kind);
}

QString MainWindow::build_block_from_root(QTreeWidgetItem* root,
                                          const QString& block_name,
                                          const QString& default_type,
                                          const QStringList& skip_keys) const {
  if (!root || root->childCount() == 0) {
    return QString();
  }
  QString out;
  out += QString("[%1]\n").arg(block_name);
  for (int i = 0; i < root->childCount(); ++i) {
    auto* child = root->child(i);
    if (!child) {
      continue;
    }
    const QString name = child->text(0);
    out += QString("  [%1]\n").arg(name);
    const QVariantMap params =
        child->data(0, PropertyEditor::kParamsRole).toMap();
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

QString MainWindow::build_variables_block(QTreeWidgetItem* root) const {
  if (!root || root->childCount() == 0) {
    return QString();
  }
  QString out;
  out += "[Variables]\n";
  for (int i = 0; i < root->childCount(); ++i) {
    auto* child = root->child(i);
    if (!child) {
      continue;
    }
    const QString name = child->text(0);
    out += QString("  [%1]\n").arg(name);
    const QVariantMap params =
        child->data(0, PropertyEditor::kParamsRole).toMap();
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

QString MainWindow::build_executioner_block(QTreeWidgetItem* root) const {
  if (!root || root->childCount() == 0) {
    return QString();
  }
  auto* step = root->child(0);
  if (!step) {
    return QString();
  }
  const QVariantMap params =
      step->data(0, PropertyEditor::kParamsRole).toMap();
  QString type = params.value("type").toString();
  if (type.isEmpty()) {
    type = "Transient";
  }
  QString out;
  out += "[Executioner]\n";
  out += QString("  type = %1\n").arg(type);
  for (auto it = params.begin(); it != params.end(); ++it) {
    if (it.key() == "type") {
      continue;
    }
    out += QString("  %1 = %2\n")
               .arg(it.key())
               .arg(it.value().toString());
  }
  out += "[]\n";
  if (root->childCount() > 1) {
    console_->appendPlainText(
        "Warning: multiple Steps found; using the first for [Executioner].");
  }
  return out;
}

void MainWindow::sync_model_to_input() {
  if (!moose_panel_) {
    return;
  }
  const QString functions =
      build_block_from_root(find_root_item("Functions"), "Functions",
                            "ParsedFunction", {});
  const QString variables = build_variables_block(find_root_item("Variables"));
  const QString materials =
      build_block_from_root(find_root_item("Materials"), "Materials",
                            "GenericConstantMaterial", {});
  const QString bcs = build_block_from_root(find_root_item("BC"), "BCs",
                                            "DirichletBC", {});
  const QString kernels =
      build_block_from_root(find_root_item("Loads"), "Kernels", "BodyForce",
                            {"section"});
  const QString outputs =
      build_block_from_root(find_root_item("Outputs"), "Outputs", "Exodus", {});
  const QString executioner =
      build_executioner_block(find_root_item("Steps"));
  moose_panel_->apply_model_blocks(functions, variables, materials, bcs, kernels,
                                   outputs, executioner);
  const QVariantMap settings = moose_panel_->moose_settings();
  const QString input_path = settings.value("input_path").toString();
  auto* input_root = find_root_item("Input Cases");
  if (input_root) {
    auto* input_item = find_child_by_param(input_root, "path", input_path);
    const QString input_name = QFileInfo(input_path).fileName().isEmpty()
                                   ? QString("generated_input.i")
                                   : QFileInfo(input_path).fileName();
    const QVariantMap input_params{
        {"path", input_path},
        {"template_key", settings.value("template_key").toString()},
        {"generated_at",
         QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"status", "Generated"}};
    if (!input_item) {
      input_item = add_child_item(input_root, input_name, "Input Cases",
                                  input_params);
    } else {
      input_item->setText(
          0, unique_child_name(input_root, input_name, input_item));
      input_item->setData(0, PropertyEditor::kParamsRole, input_params);
    }
    if (input_item) {
      input_item->setData(0, PropertyEditor::kStatusRole, "Generated");
    }
  }
  refresh_workflow_status();
  gmp::log_operation("model", "Model tree synced to MOOSE input.");
  statusBar()->showMessage("Model synced to MOOSE input.", 2000);
}

void MainWindow::load_demo_diffusion(bool run) {
  if (!moose_panel_) {
    return;
  }
  clear_model_tree_children();

  auto* functions = find_root_item("Functions");
  add_child_item(functions, "ic_u", "Functions",
                 {{"type", "ParsedFunction"},
                  {"expression", "sin(3.14159*x)*sin(3.14159*y)"}});
  add_child_item(functions, "ic_v", "Functions",
                 {{"type", "ParsedFunction"},
                  {"expression", "0.2*cos(3.14159*x)*cos(3.14159*y)"}});
  add_child_item(functions, "source_u", "Functions",
                 {{"type", "ParsedFunction"},
                  {"expression", "exp(-t)*sin(3.14159*x)*sin(3.14159*y)"}});
  add_child_item(functions, "source_v", "Functions",
                 {{"type", "ParsedFunction"},
                  {"expression", "0.1*exp(-0.5*t)*cos(3.14159*x)"}});
  add_child_item(functions, "bc_left", "Functions",
                 {{"type", "ParsedFunction"},
                  {"expression", "1.0+0.1*sin(6.28318*t)"}});
  add_child_item(functions, "bc_right", "Functions",
                 {{"type", "ParsedFunction"}, {"expression", "0.0"}});

  auto* variables = find_root_item("Variables");
  add_child_item(variables, "u", "Variables",
                 {{"order", "FIRST"}, {"family", "LAGRANGE"}});
  add_child_item(variables, "v", "Variables",
                 {{"order", "FIRST"}, {"family", "LAGRANGE"}});

  auto* materials = find_root_item("Materials");
  add_child_item(materials, "diffusion", "Materials",
                 {{"type", "GenericConstantMaterial"},
                  {"prop_names", "diff_u diff_v"},
                  {"prop_values", "1.0 0.25"}});

  auto* bcs = find_root_item("BC");
  add_child_item(bcs, "u_left", "BC",
                 {{"type", "FunctionDirichletBC"},
                  {"variable", "u"},
                  {"boundary", "left"},
                  {"function", "bc_left"}});
  add_child_item(bcs, "u_right", "BC",
                 {{"type", "FunctionDirichletBC"},
                  {"variable", "u"},
                  {"boundary", "right"},
                  {"function", "bc_right"}});
  add_child_item(bcs, "v_left", "BC",
                 {{"type", "DirichletBC"},
                  {"variable", "v"},
                  {"boundary", "left"},
                  {"value", "0"}});
  add_child_item(bcs, "v_right", "BC",
                 {{"type", "DirichletBC"},
                  {"variable", "v"},
                  {"boundary", "right"},
                  {"value", "0"}});

  auto* loads = find_root_item("Loads");
  add_child_item(loads, "u_dt", "Loads",
                 {{"type", "TimeDerivative"}, {"variable", "u"}});
  add_child_item(loads, "u_diff", "Loads",
                 {{"type", "MatDiffusion"},
                  {"variable", "u"},
                  {"diffusivity", "diff_u"}});
  add_child_item(loads, "u_src", "Loads",
                 {{"type", "BodyForce"}, {"variable", "u"},
                  {"function", "source_u"}});
  add_child_item(loads, "v_dt", "Loads",
                 {{"type", "TimeDerivative"}, {"variable", "v"}});
  add_child_item(loads, "v_diff", "Loads",
                 {{"type", "MatDiffusion"},
                  {"variable", "v"},
                  {"diffusivity", "diff_v"}});
  add_child_item(loads, "v_src", "Loads",
                 {{"type", "BodyForce"}, {"variable", "v"},
                  {"function", "source_v"}});

  auto* outputs = find_root_item("Outputs");
  add_child_item(outputs, "exodus", "Outputs",
                 {{"type", "Exodus"}, {"exodus", "true"}, {"csv", "true"}});

  auto* steps = find_root_item("Steps");
  add_child_item(steps, "transient", "Steps",
                 {{"type", "Transient"},
                  {"solve_type", "NEWTON"},
                  {"scheme", "bdf2"},
                  {"dt", "0.01"},
                  {"end_time", "0.2"}});

  moose_panel_->set_template_by_key("generated", true);
  sync_model_to_input();

  statusBar()->showMessage("Demo loaded: Transient Diffusion", 2000);
  gmp::log_operation("project", "Demo loaded: Transient Diffusion");
  if (run) {
    moose_panel_->run_job();
  }
}

void MainWindow::load_demo_thermo(bool run) {
  if (!moose_panel_) {
    return;
  }
  clear_model_tree_children();

  auto* functions = find_root_item("Functions");
  add_child_item(functions, "heat_src", "Functions",
                 {{"type", "ParsedFunction"},
                  {"expression",
                   "50.0*exp(-t)*sin(3.14159*x)*sin(3.14159*y)"}});

  auto* variables = find_root_item("Variables");
  add_child_item(variables, "T", "Variables",
                 {{"order", "FIRST"},
                  {"family", "LAGRANGE"},
                  {"initial_condition", "300"}});
  add_child_item(variables, "disp_x", "Variables",
                 {{"order", "FIRST"}, {"family", "LAGRANGE"}});
  add_child_item(variables, "disp_y", "Variables",
                 {{"order", "FIRST"}, {"family", "LAGRANGE"}});

  auto* materials = find_root_item("Materials");
  add_child_item(materials, "thcond", "Materials",
                 {{"type", "GenericConstantMaterial"},
                  {"prop_names", "thermal_conductivity"},
                  {"prop_values", "1.0"}});
  add_child_item(materials, "elastic", "Materials",
                 {{"type", "ComputeElasticityTensor"},
                  {"fill_method", "symmetric_isotropic"},
                  {"C_ijkl", "2.1e5 0.8e5"}});
  add_child_item(materials, "strain", "Materials",
                 {{"type", "ComputeSmallStrain"},
                  {"displacements", "disp_x disp_y"},
                  {"eigenstrain_names", "eigenstrain"}});
  add_child_item(materials, "stress", "Materials",
                 {{"type", "ComputeLinearElasticStress"}});
  add_child_item(materials, "thermal_strain", "Materials",
                 {{"type", "ComputeThermalExpansionEigenstrain"},
                  {"thermal_expansion_coeff", "1e-5"},
                  {"temperature", "T"},
                  {"stress_free_temperature", "300"},
                  {"eigenstrain_name", "eigenstrain"}});

  auto* bcs = find_root_item("BC");
  add_child_item(bcs, "temp_left", "BC",
                 {{"type", "DirichletBC"},
                  {"variable", "T"},
                  {"boundary", "left"},
                  {"value", "400"}});
  add_child_item(bcs, "temp_right", "BC",
                 {{"type", "DirichletBC"},
                  {"variable", "T"},
                  {"boundary", "right"},
                  {"value", "300"}});
  add_child_item(bcs, "fix_x", "BC",
                 {{"type", "DirichletBC"},
                  {"variable", "disp_x"},
                  {"boundary", "left"},
                  {"value", "0"}});
  add_child_item(bcs, "fix_y", "BC",
                 {{"type", "DirichletBC"},
                  {"variable", "disp_y"},
                  {"boundary", "bottom"},
                  {"value", "0"}});

  auto* loads = find_root_item("Loads");
  add_child_item(loads, "htcond", "Loads",
                 {{"type", "HeatConduction"}, {"variable", "T"}});
  add_child_item(loads, "TensorMechanics", "Loads",
                 {{"type", "TensorMechanics"},
                  {"displacements", "disp_x disp_y"}});
  add_child_item(loads, "Q_function", "Loads",
                 {{"type", "BodyForce"},
                  {"variable", "T"},
                  {"function", "heat_src"}});

  auto* outputs = find_root_item("Outputs");
  add_child_item(outputs, "exodus", "Outputs",
                 {{"type", "Exodus"}, {"exodus", "true"}, {"csv", "true"}});

  auto* steps = find_root_item("Steps");
  add_child_item(steps, "transient", "Steps",
                 {{"type", "Transient"},
                  {"scheme", "bdf2"},
                  {"dt", "0.05"},
                  {"end_time", "0.5"},
                  {"solve_type", "PJFNK"},
                  {"nl_max_its", "10"},
                  {"l_max_its", "30"},
                  {"nl_abs_tol", "1e-8"},
                  {"l_tol", "1e-4"}});

  moose_panel_->set_template_by_key("tm_generated", true);
  sync_model_to_input();

  statusBar()->showMessage("Demo loaded: Thermo-Mechanics", 2000);
  gmp::log_operation("project", "Demo loaded: Thermo-Mechanics");
  if (run) {
    moose_panel_->run_job();
  }
}

void MainWindow::load_demo_nonlinear_heat(bool run) {
  if (!moose_panel_) {
    return;
  }
  clear_model_tree_children();

  auto* variables = find_root_item("Variables");
  add_child_item(variables, "T", "Variables",
                 {{"order", "FIRST"},
                  {"family", "LAGRANGE"},
                  {"initial_condition", "300"}});

  auto* materials = find_root_item("Materials");
  add_child_item(materials, "k_T", "Materials",
                 {{"type", "ParsedMaterial"},
                  {"property_name", "thermal_conductivity"},
                  {"coupled_variables", "T"},
                  {"expression", "1 + 0.01*T"}});

  auto* bcs = find_root_item("BC");
  add_child_item(bcs, "temp_left", "BC",
                 {{"type", "DirichletBC"},
                  {"variable", "T"},
                  {"boundary", "left"},
                  {"value", "500"}});
  add_child_item(bcs, "temp_right", "BC",
                 {{"type", "DirichletBC"},
                  {"variable", "T"},
                  {"boundary", "right"},
                  {"value", "300"}});

  auto* loads = find_root_item("Loads");
  add_child_item(loads, "T_dt", "Loads",
                 {{"type", "TimeDerivative"}, {"variable", "T"}});
  add_child_item(loads, "T_cond", "Loads",
                 {{"type", "HeatConduction"}, {"variable", "T"}});

  auto* outputs = find_root_item("Outputs");
  add_child_item(outputs, "exodus", "Outputs",
                 {{"type", "Exodus"}, {"exodus", "true"}, {"csv", "true"}});

  auto* steps = find_root_item("Steps");
  add_child_item(steps, "transient", "Steps",
                 {{"type", "Transient"},
                  {"solve_type", "NEWTON"},
                  {"scheme", "bdf2"},
                  {"dt", "0.02"},
                  {"end_time", "0.5"}});

  moose_panel_->set_template_by_key("heat_generated", true);
  sync_model_to_input();

  statusBar()->showMessage("Demo loaded: Nonlinear Heat", 2000);
  gmp::log_operation("project", "Demo loaded: Nonlinear Heat");
  if (run) {
    moose_panel_->run_job();
  }
}

void MainWindow::add_item_under_root(QTreeWidgetItem* root) {
  if (!root) {
    return;
  }
  const QString kind = root->text(0);
  const QString base = kind.left(kind.size() - 1).toLower();
  QString name;
  if (!prompt_unique_child_name(root, QString("Add %1").arg(kind),
                                base + "_1", &name)) {
    return;
  }
  add_child_item(root, name, kind, default_params_for_kind(kind));
}

void MainWindow::remove_item(QTreeWidgetItem* item) {
  if (!item || !item->parent()) {
    return;
  }
  auto* parent = item->parent();
  const QString kind =
      item->data(0, PropertyEditor::kKindRole).toString();
  const QString name = item->text(0);
  const QVariantMap removed_params =
      item->data(0, PropertyEditor::kParamsRole).toMap();
  const QString current_file = viewer_ ? viewer_->current_file() : QString();
  const bool current_is_mesh =
      current_file.endsWith(".msh", Qt::CaseInsensitive);
  bool clear_stage_data = false;

  auto same_file = [](const QString& lhs, const QString& rhs) {
    if (lhs.isEmpty() || rhs.isEmpty()) {
      return false;
    }
    return QFileInfo(lhs).absoluteFilePath() == QFileInfo(rhs).absoluteFilePath();
  };

  if (kind == "Sketches") {
    // 删除当前预览草图时先退出 2D 预览，避免 viewer 继续持有已不存在
    // 对象的快照。引用该草图的 3D Part 会失效，因此当前网格也不再可信。
    if (viewer_ && viewer_->is_sketch_preview()) {
      viewer_->set_sketch_preview(nullptr);
    }
    auto* parts_root = find_root_item("Parts");
    for (int row = 0; parts_root && row < parts_root->childCount(); ++row) {
      const QVariantMap params =
          parts_root->child(row)->data(0, PropertyEditor::kParamsRole).toMap();
      if (params.value("sketch").toString() == name) {
        clear_stage_data = current_is_mesh;
        break;
      }
    }
  } else if (kind == "Parts") {
    // Feature 是 Part 的建模历史。删除 Part 时同步移除其历史节点，避免
    // 留下孤儿 Feature；任一当前网格都可能包含该 Part，必须撤下旧快照。
    auto* features_root = find_root_item("Features");
    for (int row = features_root ? features_root->childCount() - 1 : -1;
         row >= 0; --row) {
      auto* feature = features_root->child(row);
      const QVariantMap params =
          feature->data(0, PropertyEditor::kParamsRole).toMap();
      if (params.value("part").toString() == name) {
        delete features_root->takeChild(row);
      }
    }
    clear_stage_data = current_is_mesh;
  } else if (kind == "Features") {
    // 删除 Feature 后清除所属 Part 上的派生结果引用。
    auto* parts_root = find_root_item("Parts");
    for (int row = 0; parts_root && row < parts_root->childCount(); ++row) {
      auto* part = parts_root->child(row);
      QVariantMap params =
          part->data(0, PropertyEditor::kParamsRole).toMap();
      if (params.value("feature").toString() != name) {
        continue;
      }
      params.remove("feature");
      params.remove("gmsh_volume_tag");
      params.remove("gmsh_volume_tags");
      params.remove("brep");
      params.remove("mesh");
      part->setData(0, PropertyEditor::kParamsRole, params);
    }
    clear_stage_data = current_is_mesh;
  } else if (kind == "Mesh") {
    clear_stage_data = current_is_mesh;
  } else if (kind == "Results") {
    clear_stage_data =
        same_file(current_file, removed_params.value("path").toString());
  }

  parent->removeChild(item);
  delete item;
  if (clear_stage_data && viewer_) {
    viewer_->clear_stage_data();
    active_ui_context_.stage_selections.clear();
  }
  invalidate_downstream_from(kind);
  set_project_dirty(true);
  refresh_module_pages();
  if (property_editor_) {
    property_editor_->refresh_form_options();
  }
}

void MainWindow::duplicate_item(QTreeWidgetItem* item) {
  if (!item || !item->parent()) {
    return;
  }
  auto* parent = item->parent();
  if (!parent) {
    return;
  }
  const QString base = unique_child_name(parent, item->text(0) + "_copy");
  auto* child = new QTreeWidgetItem(parent);
  child->setText(0, base);
  child->setData(0, PropertyEditor::kKindRole,
                 item->data(0, PropertyEditor::kKindRole));
  child->setData(0, PropertyEditor::kParamsRole,
                 item->data(0, PropertyEditor::kParamsRole));
  child->setIcon(0, parent->icon(0));
  parent->setExpanded(true);
  model_tree_->setCurrentItem(child);
  invalidate_downstream_from(
      item->data(0, PropertyEditor::kKindRole).toString());
  set_project_dirty(true);
  refresh_module_pages();
  if (property_editor_) {
    property_editor_->refresh_form_options();
  }
}

void MainWindow::rename_item(QTreeWidgetItem* item) {
  if (!item || !item->parent()) {
    return;
  }
  auto* root = item->parent();
  QString name;
  if (!prompt_unique_child_name(
          root, QString("Rename %1").arg(root->text(0)), item->text(0), &name,
          item) ||
      name == item->text(0)) {
    return;
  }
  item->setText(0, name);
  model_tree_->setCurrentItem(item);
}

void MainWindow::refresh_job_table() {
  if (!job_table_) {
    return;
  }
  // 重建前记录选中作业：刷新（手动/自动）不得丢失用户选中和右侧详情。
  const QString selected_name =
      job_table_->currentRow() >= 0 && job_table_->item(job_table_->currentRow(), 0)
          ? job_table_->item(job_table_->currentRow(), 0)->text()
          : (!selected_job_id_.isEmpty() ? selected_job_id_ : QString());
  job_table_->setRowCount(0);
  if (job_detail_) {
    job_detail_->clear();
  }
  auto* root = find_root_item("Jobs");
  if (!root) {
    return;
  }
  const QString filter = job_state_filter_
                             ? job_state_filter_->currentData().toString()
                             : QString("all");
  for (int i = 0; i < root->childCount(); ++i) {
    auto* child = root->child(i);
    if (!child) {
      continue;
    }
    const QVariantMap params =
        child->data(0, PropertyEditor::kParamsRole).toMap();
    // 作业监控状态筛选：按映射后的状态列匹配。
    if (filter != "all" &&
        params.value("status").toString() != filter) {
      continue;
    }
    append_job_row(child->text(0), params);
  }
  // 恢复选中行：阻塞信号避免重复触发网络拉取，仅同步选中状态与详情文本。
  if (selected_name.isEmpty()) {
    return;
  }
  for (int row = 0; row < job_table_->rowCount(); ++row) {
    auto* cell = job_table_->item(row, 0);
    if (!cell || cell->text() != selected_name) {
      continue;
    }
    {
      const QSignalBlocker blocker(job_table_);
      job_table_->setCurrentCell(row, 0);
    }
    const QVariantMap params = cell->data(Qt::UserRole).toMap();
    selected_job_id_ = params.value("job_id").toString();
    selected_job_remote_ =
        params.value("remote").toBool() && !selected_job_id_.isEmpty();
    const QString status = params.value("status").toString();
    selected_job_running_ = selected_job_remote_ &&
                            (status == "Queued" || status == "Running");
    if (job_cancel_button_) {
      job_cancel_button_->setEnabled(selected_job_running_);
    }
    if (job_detail_stack_ && job_detail_stack_->currentIndex() == 0 &&
        !selected_name.isEmpty()) {
      // 此前因重建被打回占位页的，恢复详情页。
      job_detail_stack_->setCurrentIndex(1);
    }
    if (job_detail_title_) {
      const QString case_name = params.value("case").toString();
      job_detail_title_->setText(
          case_name.isEmpty()
              ? QString("%1 — %2").arg(selected_name, status)
              : QString("%1 · %2 — %3").arg(selected_name, case_name, status));
    }
    update_job_detail(row);
    break;
  }
}

int MainWindow::append_job_row(const QString& name, const QVariantMap& params) {
  if (!job_table_) {
    return -1;
  }
  const int row = job_table_->rowCount();
  job_table_->insertRow(row);
  update_job_row(row, name, params);
  return row;
}

void MainWindow::update_job_row(int row, const QString& name,
                                const QVariantMap& params) {
  if (!job_table_ || row < 0 || row >= job_table_->rowCount()) {
    return;
  }
  auto set_item = [this, row](int col, const QString& text) {
    QTableWidgetItem* item = job_table_->item(row, col);
    if (!item) {
      item = new QTableWidgetItem();
      job_table_->setItem(row, col, item);
    }
    item->setText(text);
  };
  set_item(0, name);
  set_item(1, l10n::tr(params.value("status").toString()));
  set_item(2, params.value("case").toString());
  set_item(3, params.value("progress").toString());
  set_item(4, params.value("start_time").toString());
  set_item(5, params.value("duration").toString());
  set_item(6, l10n::tr(params.value("remote").toBool() ? QString("Remote")
                                                       : QString("Local")));
  set_item(7, params.value("exec").toString());
  set_item(8, params.value("exodus").toString());
  if (auto* item = job_table_->item(row, 0)) {
    item->setData(Qt::UserRole, params);
  }
}

void MainWindow::update_job_detail(int row) {
  if (!job_detail_) {
    return;
  }
  if (!job_table_ || row < 0 || row >= job_table_->rowCount()) {
    job_detail_->clear();
    return;
  }
  auto* item = job_table_->item(row, 0);
  if (!item) {
    job_detail_->clear();
    return;
  }
  const QVariantMap params = item->data(Qt::UserRole).toMap();
  QStringList lines;
  lines << QString("Name: %1").arg(item->text());
  lines << QString("Status: %1").arg(params.value("status").toString());
  lines << QString("Start: %1").arg(params.value("start_time").toString());
  lines << QString("Duration: %1").arg(params.value("duration").toString());
  lines << QString("Mesh: %1").arg(params.value("mesh").toString());
  lines << QString("Exec: %1").arg(params.value("exec").toString());
  lines << QString("Args: %1").arg(params.value("args").toString());
  lines << QString("Workdir: %1").arg(params.value("workdir").toString());
  lines << QString("Result: %1").arg(params.value("exodus").toString());
  lines << QString("Exit: %1").arg(params.value("exit_code").toString());
  if (moose_panel_) {
    const QString tail = moose_panel_->log_tail(30);
    if (!tail.isEmpty()) {
      lines << "" << "Log (latest)" << tail;
    }
  }
  job_detail_->setPlainText(lines.join("\n"));
}

void MainWindow::apply_job_selection(int row) {
  update_job_detail(row);
  if (!job_table_ || row < 0 || row >= job_table_->rowCount()) {
    selected_job_id_.clear();
    selected_job_remote_ = false;
    selected_job_running_ = false;
    if (job_detail_stack_) {
      job_detail_stack_->setCurrentIndex(0);
    }
    return;
  }
  auto* item = job_table_->item(row, 0);
  const QVariantMap params = item ? item->data(Qt::UserRole).toMap()
                                  : QVariantMap();
  selected_job_id_ = params.value("job_id").toString();
  selected_job_remote_ = params.value("remote").toBool() &&
                         !selected_job_id_.isEmpty();
  const QString status = params.value("status").toString();
  selected_job_running_ =
      selected_job_remote_ &&
      (status == "Queued" || status == "Running");
  if (job_detail_stack_) {
    job_detail_stack_->setCurrentIndex(1);
  }
  if (job_cancel_button_) {
    job_cancel_button_->setEnabled(selected_job_running_);
  }
  if (!selected_job_remote_) {
    if (job_detail_title_) {
      job_detail_title_->setText(
          QString("%1 — %2").arg(item->text(), status));
    }
    return;
  }
  if (job_detail_title_) {
    const QString case_name = params.value("case").toString();
    job_detail_title_->setText(
        case_name.isEmpty()
            ? QString("%1 — %2").arg(selected_job_id_, status)
            : QString("%1 · %2 — %3").arg(selected_job_id_, case_name, status));
  }
  // 选中远程作业即拉取实时执行状态与制品清单（LIMS 任务监控交互）。
  // 巡览模式下跳过网络请求，由巡览注入合成数据。
  if (moose_panel_ && !qEnvironmentVariableIsSet("GMP_SCREENSHOT_DIR")) {
    moose_panel_->refresh_job_execution(selected_job_id_);
    moose_panel_->refresh_job_files(selected_job_id_);
  }
}

void MainWindow::update_remote_job_detail(const QVariantMap& status) {
  if (!job_progress_bar_ || !job_progress_text_) {
    return;
  }
  const QVariantMap progress = status.value("progress").toMap();
  const QVariantMap resources = status.value("resources").toMap();
  const QVariantMap timings = status.value("timings").toMap();
  const QVariantMap convergence = status.value("convergence").toMap();

  const double percent = progress.value("percent", -1.0).toDouble();
  job_progress_bar_->setValue(percent >= 0 ? int(percent + 0.5) : 0);
  QStringList progress_parts;
  const int step_cur = progress.value("step_current", -1).toInt();
  if (step_cur >= 0) {
    QString step = QString("step %1").arg(step_cur);
    if (progress.value("step_total", -1).toInt() >= 0) {
      step += QString("/%1").arg(progress.value("step_total").toInt());
    }
    progress_parts << step;
  }
  if (percent >= 0) {
    progress_parts << QString("%1%").arg(percent, 0, 'f', 1);
  }
  if (progress.contains("time_current")) {
    QString time = QString("t=%1").arg(progress.value("time_current").toDouble());
    if (progress.contains("time_total") &&
        !progress.value("time_total").isNull()) {
      time += QString("/%1").arg(progress.value("time_total").toDouble());
    }
    progress_parts << time;
  }
  job_progress_text_->setText(progress_parts.isEmpty()
                                  ? QString("-")
                                  : progress_parts.join("  ·  "));

  auto set_field = [this](const QString& key, const QString& text) {
    QLabel* label = job_detail_fields_.value(key, nullptr);
    if (label) {
      label->setText(text.isEmpty() ? QString("-") : text);
    }
  };
  set_field("input_file", status.value("input_file").toString());
  set_field("pid", status.value("pid").toString());
  const int procs = resources.value("process_count").toInt(0);
  const int cores = resources.value("logical_cores_used").toInt(0);
  set_field("parallel",
            procs > 0
                ? QString("%1 MPI ranks (%2 cores)").arg(procs).arg(cores)
                : QString());
  set_field("cpu", resources.contains("cpu_percent")
                       ? QString("%1%").arg(
                             resources.value("cpu_percent").toDouble(), 0,
                             'f', 1)
                       : QString());
  set_field("memory", resources.contains("memory_mb")
                          ? QString("%1 MB").arg(
                                resources.value("memory_mb").toDouble(), 0,
                                'f', 1)
                          : QString());
  set_field("step", step_cur >= 0 ? QString::number(step_cur) : QString());
  if (progress.contains("current_dt") &&
      !progress.value("current_dt").isNull()) {
    set_field("dt", QString("%1 s%2")
                        .arg(progress.value("current_dt").toDouble())
                        .arg(progress.value("adaptive_dt").toBool()
                                 ? QString(" (adaptive)")
                                 : QString()));
  } else {
    set_field("dt", QString());
  }
  if (progress.contains("time_current")) {
    set_field("physical",
              QString("%1 s").arg(progress.value("time_current").toDouble()));
  } else {
    set_field("physical", QString());
  }
  set_field("converged", convergence.contains("converged_count")
                             ? QString::number(
                                   convergence.value("converged_count").toInt())
                             : QString());
  set_field("avg_step", timings.contains("avg_step_seconds") &&
                                !timings.value("avg_step_seconds").isNull()
                            ? QString("%1 s").arg(
                                  timings.value("avg_step_seconds").toDouble())
                            : QString());
  set_field("elapsed", timings.value("elapsed_human").toString());
  QString eta = timings.value("estimated_remaining_human").toString();
  if (eta.isEmpty() && !timings.value("eta_reason").toString().isEmpty()) {
    eta = timings.value("eta_reason").toString();
  }
  set_field("eta", eta);
  set_field("heartbeat", status.value("heartbeat_at").toString());
  set_field("health", status.value("health").toString());
}

void MainWindow::update_remote_job_files(const QVariantMap& body) {
  if (!job_files_table_) {
    return;
  }
  job_files_table_->setRowCount(0);
  const QVariantList files = body.value("files").toList();
  auto human_size = [](qint64 bytes) {
    if (bytes >= 1024 * 1024) {
      return QString("%1 MB").arg(bytes / 1048576.0, 0, 'f', 1);
    }
    if (bytes >= 1024) {
      return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    }
    return QString("%1 B").arg(bytes);
  };
  for (const QVariant& value : files) {
    const QVariantMap file = value.toMap();
    const int row = job_files_table_->rowCount();
    job_files_table_->insertRow(row);
    const QString path = file.value("path").toString();
    auto make_cell = [](const QString& text) {
      return new QTableWidgetItem(text);
    };
    job_files_table_->setItem(row, 0, make_cell(file.value("kind").toString()));
    auto* name_item = make_cell(file.value("name").toString());
    name_item->setData(Qt::UserRole, path);
    name_item->setToolTip(path);
    job_files_table_->setItem(row, 1, name_item);
    job_files_table_->setItem(
        row, 2, make_cell(human_size(file.value("size").toLongLong())));
    job_files_table_->setItem(row, 3,
                              make_cell(file.value("modified_at").toString()));
    job_files_table_->setItem(
        row, 4,
        make_cell(file.value("snapshot").toBool() ? QString("yes") : QString()));
  }
}

bool MainWindow::load_project(const QString& path) {
  try {
    suppress_dirty_ = true;
    YAML::Node root = YAML::LoadFile(path.toStdString());

    // Phase 0：schema 版本识别与兼容
    int loaded_schema_version = project_schema::kCurrentVersion;
    if (root["schema_version"] && root["schema_version"].IsScalar()) {
      loaded_schema_version = root["schema_version"].as<int>();
    } else if (root["version"] && root["version"].IsScalar()) {
      const int old_version = root["version"].as<int>();
      if (old_version > 2) {
        QMessageBox::warning(this, "Project Load",
                             "Unsupported project version.");
        suppress_dirty_ = false;
        return false;
      }
      loaded_schema_version = project_schema::kCurrentVersion;
    }
    if (loaded_schema_version < 1 ||
        loaded_schema_version > project_schema::kCurrentVersion) {
      QMessageBox::warning(this, "Project Load",
                           QString("Unsupported schema version: %1")
                               .arg(loaded_schema_version));
      suppress_dirty_ = false;
      return false;
    }

    // 读取应用档案与单位合同
    const QVariantMap loaded_application_profile =
        project_schema::yaml_map_to_variant_map(root["application_profile"]);
    const QVariantMap loaded_unit_contract =
        project_schema::yaml_map_to_variant_map(root["unit_contract"]);

    // 读取网格快照
    const PhysicalGroupManifest loaded_mesh_snapshot =
        project_schema::mesh_snapshot_from_yaml(root["mesh_snapshot"]);

    auto parse_map = [](const YAML::Node& node,
                        const QSet<QString>& force_string) {
      QVariantMap map;
      if (!node || !node.IsMap()) {
        return map;
      }
      for (const auto& it : node) {
        const QString key = QString::fromStdString(it.first.as<std::string>());
        const YAML::Node value = it.second;
        if (!value.IsScalar()) {
          continue;
        }
        const QString raw = QString::fromStdString(value.as<std::string>());
        if (force_string.contains(key)) {
          map.insert(key, raw);
          continue;
        }
        const QString lower = raw.toLower();
        if (lower == "true" || lower == "false") {
          map.insert(key, lower == "true");
          continue;
        }
        bool ok_int = false;
        const int int_val = raw.toInt(&ok_int);
        if (ok_int && !raw.contains('.')
            && !raw.contains('e', Qt::CaseInsensitive)) {
          map.insert(key, int_val);
          continue;
        }
        bool ok_double = false;
        const double dbl_val = raw.toDouble(&ok_double);
        if (ok_double) {
          map.insert(key, dbl_val);
          continue;
        }
        map.insert(key, raw);
      }
      return map;
    };
    YAML::Node model = root["model"];
    if (!model || !model.IsMap()) {
      suppress_dirty_ = false;
      QMessageBox::warning(this, "Project Load",
                           "Invalid project file (missing model).");
      return false;
    }
    clear_model_tree_children();
    for (const auto& it : model) {
      const QString kind = QString::fromStdString(it.first.as<std::string>());
      auto* root_item = find_root_item(kind);
      if (!root_item) {
        continue;
      }
      const YAML::Node list = it.second;
      if (!list.IsSequence()) {
        continue;
      }
      for (const auto& entry : list) {
        const QString name =
            QString::fromStdString(entry["name"].as<std::string>(""));
        if (name.isEmpty()) {
          continue;
        }
        auto* child = new QTreeWidgetItem(root_item);
        child->setText(0, unique_child_name(root_item, name));
        child->setData(0, PropertyEditor::kKindRole, kind);
        child->setIcon(0, root_item->icon(0));
        QVariantMap params =
            project_schema::yaml_map_to_variant_map(entry["params"]);
        const QString status = QString::fromStdString(
            entry["status"].as<std::string>(params.value("status").toString()
                                                .toStdString()));
        child->setData(0, PropertyEditor::kStatusRole, status);
        child->setData(0, PropertyEditor::kParamsRole,
                       normalize_params_for_kind(kind, params));
      }
    }
    project_path_ = path;
    gmp::log_operation("project", "Project loaded: " + path);
    YAML::Node gmsh_node = root["gmsh"];
    if (gmsh_node && gmsh_node.IsMap() && gmsh_panel_) {
      const QVariantMap gmsh_settings = parse_map(gmsh_node, {});
      gmsh_panel_->apply_gmsh_settings(gmsh_settings);
    }

    YAML::Node moose_node = root["moose"];
    if (moose_node && moose_node.IsMap() && moose_panel_) {
      const QSet<QString> force_string = {"exec_path", "input_path", "workdir",
                                          "mesh_path", "template_key",
                                          "extra_args", "input_text"};
      const QVariantMap moose_settings = parse_map(moose_node, force_string);
      moose_panel_->apply_moose_settings(moose_settings);
    }
    input_snapshots_.clear();
    if (moose_node && moose_node.IsMap() && moose_node["input_snapshots"] &&
        moose_node["input_snapshots"].IsSequence()) {
      for (const auto& s : moose_node["input_snapshots"]) {
        input_snapshots_.append(
            QString::fromStdString(s.as<std::string>("")));
      }
    }

    YAML::Node viewer_node = root["viewer"];
    if (viewer_node && viewer_node.IsMap() && viewer_) {
      const QSet<QString> force_string = {"current_file", "array_key", "preset",
                                          "output_selected"};
      const QVariantMap viewer_settings = parse_map(viewer_node, force_string);
      viewer_->apply_viewer_settings(viewer_settings);
    }
    schema_version_ = loaded_schema_version;
    application_profile_ = loaded_application_profile;
    unit_contract_ = loaded_unit_contract;
    mesh_snapshot_ = loaded_mesh_snapshot;
    suppress_dirty_ = false;
    refresh_job_table();
    refresh_results_panel();
    refresh_module_pages();
    add_recent_project(path);
    set_project_dirty(false);
    update_project_status();
    return true;
  } catch (const std::exception& e) {
    suppress_dirty_ = false;
    QMessageBox::warning(this, "Project Load",
                         QString("Failed to load: %1").arg(e.what()));
    return false;
  }
}

bool MainWindow::save_project(const QString& path) {
  try {
    YAML::Node root;
    root["schema_version"] = schema_version_;
    root["version"] = 2;  // 保留旧字段以兼容只读 version 的工具
    root["saved_at"] =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();

    // Phase 0：写入应用档案与单位合同
    root["application_profile"] =
        project_schema::variant_map_to_yaml(application_profile_);
    root["unit_contract"] =
        project_schema::variant_map_to_yaml(unit_contract_);

    // Phase 0：写入网格快照
    root["mesh_snapshot"] =
        project_schema::mesh_snapshot_to_yaml(mesh_snapshot_);

    YAML::Node model(YAML::NodeType::Map);
    for (int i = 0; i < model_tree_->topLevelItemCount(); ++i) {
      auto* root_item = model_tree_->topLevelItem(i);
      if (!root_item) {
        continue;
      }
      YAML::Node list(YAML::NodeType::Sequence);
      for (int j = 0; j < root_item->childCount(); ++j) {
        auto* child = root_item->child(j);
        if (!child) {
          continue;
        }
        YAML::Node entry;
        entry["name"] = child->text(0).toStdString();
        entry["kind"] = root_item->text(0).toStdString();
        const QVariantMap map =
            child->data(0, PropertyEditor::kParamsRole).toMap();
        QString status =
            child->data(0, PropertyEditor::kStatusRole).toString();
        if (status.isEmpty()) {
          status = map.value("status").toString();
        }
        entry["status"] = status.toStdString();
        entry["params"] = project_schema::variant_map_to_yaml(map);
        list.push_back(entry);
      }
      model[root_item->text(0).toStdString()] = list;
    }
    root["model"] = model;
    if (gmsh_panel_) {
      YAML::Node gmsh_node(YAML::NodeType::Map);
      const QVariantMap settings = gmsh_panel_->gmsh_settings();
      for (auto it = settings.begin(); it != settings.end(); ++it) {
        const QVariant& val = it.value();
        switch (val.typeId()) {
          case QMetaType::Bool:
            gmsh_node[it.key().toStdString()] = val.toBool();
            break;
          case QMetaType::Int:
            gmsh_node[it.key().toStdString()] = val.toInt();
            break;
          case QMetaType::Double:
            gmsh_node[it.key().toStdString()] = val.toDouble();
            break;
          default:
            gmsh_node[it.key().toStdString()] = val.toString().toStdString();
            break;
        }
      }
      root["gmsh"] = gmsh_node;
    }

    if (moose_panel_) {
      YAML::Node moose_node(YAML::NodeType::Map);
      const QVariantMap settings = moose_panel_->moose_settings();
      for (auto it = settings.begin(); it != settings.end(); ++it) {
        const QVariant& val = it.value();
        switch (val.typeId()) {
          case QMetaType::Bool:
            moose_node[it.key().toStdString()] = val.toBool();
            break;
          case QMetaType::Int:
            moose_node[it.key().toStdString()] = val.toInt();
            break;
          case QMetaType::Double:
            moose_node[it.key().toStdString()] = val.toDouble();
            break;
          default:
            moose_node[it.key().toStdString()] = val.toString().toStdString();
            break;
        }
      }
      // Phase 0：写入历史输入快照列表
      if (!input_snapshots_.isEmpty()) {
        YAML::Node snaps(YAML::NodeType::Sequence);
        for (const QString& s : input_snapshots_) {
          snaps.push_back(s.toStdString());
        }
        moose_node["input_snapshots"] = snaps;
      }
      root["moose"] = moose_node;
    }

    if (viewer_) {
      YAML::Node viewer_node(YAML::NodeType::Map);
      const QVariantMap settings = viewer_->viewer_settings();
      for (auto it = settings.begin(); it != settings.end(); ++it) {
        const QVariant& val = it.value();
        switch (val.typeId()) {
          case QMetaType::Bool:
            viewer_node[it.key().toStdString()] = val.toBool();
            break;
          case QMetaType::Int:
            viewer_node[it.key().toStdString()] = val.toInt();
            break;
          case QMetaType::Double:
            viewer_node[it.key().toStdString()] = val.toDouble();
            break;
          default:
            viewer_node[it.key().toStdString()] = val.toString().toStdString();
            break;
        }
      }
      root["viewer"] = viewer_node;
    }
    std::ofstream out(path.toStdString());
    out << root;
    out.close();
    return true;
  } catch (const std::exception& e) {
    QMessageBox::warning(this, "Project Save",
                         QString("Failed to save: %1").arg(e.what()));
  }
  return false;
}

void MainWindow::set_project_dirty(bool dirty) {
  if (project_dirty_ == dirty) {
    return;
  }
  project_dirty_ = dirty;
  update_window_title();
  update_project_status();
}

void MainWindow::update_window_title() {
  const QString name = project_path_.isEmpty()
                           ? "Untitled"
                           : QFileInfo(project_path_).fileName();
  const QString dirty_mark = project_dirty_ ? " *" : "";
  setWindowTitle(QString("GMP-ISE - %1%2").arg(name, dirty_mark));
}

void MainWindow::update_project_status() {
  if (project_status_label_) {
    const QString label = project_path_.isEmpty()
                              ? "Project: Untitled"
                              : QString("Project: %1").arg(project_path_);
    project_status_label_->setText(label);
  }
  if (dirty_status_label_) {
    dirty_status_label_->setText(project_dirty_ ? "Modified" : "Saved");
  }
  refresh_work_context();
}

void MainWindow::add_recent_project(const QString& path) {
  if (path.isEmpty()) {
    return;
  }
  QSettings settings("gmp-ise", "gmp_ise");
  QStringList list = settings.value("recent_projects").toStringList();
  list.removeAll(path);
  list.prepend(path);
  const int max_items = 10;
  while (list.size() > max_items) {
    list.removeLast();
  }
  settings.setValue("recent_projects", list);
  update_recent_menu();
}

void MainWindow::update_recent_menu() {
  if (!recent_menu_) {
    return;
  }
  recent_menu_->clear();
  QSettings settings("gmp-ise", "gmp_ise");
  const QStringList list = settings.value("recent_projects").toStringList();
  if (list.isEmpty()) {
    auto* empty = recent_menu_->addAction("(None)");
    empty->setEnabled(false);
    return;
  }
  for (const auto& path : list) {
    auto* action = recent_menu_->addAction(path);
    connect(action, &QAction::triggered, this, [this, path]() {
      if (path.isEmpty()) {
        return;
      }
      if (load_project(path)) {
        statusBar()->showMessage("Project loaded.", 2000);
      }
    });
  }
  recent_menu_->addSeparator();
  auto* clear = recent_menu_->addAction("Clear Recent");
  connect(clear, &QAction::triggered, this, [this]() {
    QSettings settings("gmp-ise", "gmp_ise");
    settings.remove("recent_projects");
    update_recent_menu();
  });
}

void MainWindow::export_debug_bundle() {
  const QString base_dir =
      QFileDialog::getExistingDirectory(this, "Export Debug Bundle",
                                        QDir::homePath());
  if (base_dir.isEmpty()) {
    return;
  }
  const QString stamp =
      QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss");
  const QString bundle_dir = QDir(base_dir).filePath("gmp_debug_" + stamp);
  QDir dir(bundle_dir);
  if (!dir.mkpath(".")) {
    QMessageBox::warning(this, "Export Debug Bundle",
                         "Failed to create bundle directory.");
    return;
  }

  const QString project_file = dir.filePath("project.gmp.yaml");
  save_project(project_file);

  if (console_) {
    QFile log_file(dir.filePath("console.log"));
    if (log_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      log_file.write(console_->toPlainText().toUtf8());
    }
  }

  // 操作日志是定位问题的主要依据，调试包必须带上。
  const QString op_log = gmp::operation_log_path();
  if (!op_log.isEmpty() && QFileInfo::exists(op_log)) {
    QFile::copy(op_log, dir.filePath(QFileInfo(op_log).fileName()));
  }

  if (moose_panel_) {
    const QVariantMap settings = moose_panel_->moose_settings();
    const QString input_text = settings.value("input_text").toString();
    if (!input_text.isEmpty()) {
      QFile input_file(dir.filePath("moose_input.i"));
      if (input_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        input_file.write(input_text.toUtf8());
      }
    }
  }

  if (gmsh_panel_) {
    const QVariantMap settings = gmsh_panel_->gmsh_settings();
    const QString mesh_path = settings.value("output_path").toString();
    if (!mesh_path.isEmpty() && QFileInfo::exists(mesh_path)) {
      QFile::copy(mesh_path, dir.filePath(QFileInfo(mesh_path).fileName()));
    }
  }

  if (viewer_) {
    const QVariantMap settings = viewer_->viewer_settings();
    const QString file_path = settings.value("current_file").toString();
    if (!file_path.isEmpty() && QFileInfo::exists(file_path)) {
      QFile::copy(file_path, dir.filePath(QFileInfo(file_path).fileName()));
    }
  }

  QFile info_file(dir.filePath("bundle_info.txt"));
  if (info_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream out(&info_file);
    out << "Bundle created: "
        << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << "\n";
    out << "Project path: " << project_path_ << "\n";
  }

  gmp::log_operation("project", "Debug bundle exported: " + bundle_dir);
  statusBar()->showMessage("Debug bundle exported.", 3000);
  QMessageBox::information(this, "Export Debug Bundle",
                           "Bundle created at:\n" + bundle_dir);
}

void MainWindow::run_screenshot_tour(const QString& dir) {
  QDir().mkpath(dir);
  load_demo_diffusion(false);  // 载入演示模型，让各页面截图有实际内容
  // 可选：GMP_TOUR_MESH=<msh路径> 时先把网格载入视口，便于验证视口显示效果
  const QByteArray tour_mesh = qgetenv("GMP_TOUR_MESH");
  if (!tour_mesh.isEmpty() && viewer_) {
    const QString mesh_path = QString::fromLocal8Bit(tour_mesh);
    // 延迟到窗口完全显示、VTK 初始化就绪后再加载，避免初始化顺序干扰
    QTimer::singleShot(4000, this, [this, mesh_path]() {
      if (viewer_) {
        viewer_->set_mesh_file(mesh_path);
      }
      // 可选: GMP_TOUR_NAV=<index> 切换边栏控制页(0=标量 1=网格 2=视图 3=切片...)
      const QByteArray nav = qgetenv("GMP_TOUR_NAV");
      if (!nav.isEmpty() && viewer_ && viewer_->control_tabs()) {
        if (auto* nav_combo =
                viewer_->control_tabs()->findChild<QComboBox*>()) {
          nav_combo->setCurrentIndex(QString::fromLocal8Bit(nav).toInt());
        }
      }
      // 按名查找 Visualization 页签, 避免页签顺序调整时失效
      for (int i = 0; i < module_tabs_->count(); ++i) {
        if (module_tabs_->tabText(i) == "Visualization") {
          module_tabs_->setCurrentIndex(i);
          break;
        }
      }
    });
    // 直接用 VTK 离屏渲染导出一张视口图（绕过 Qt grab 的时机问题）
    QTimer::singleShot(6000, this, [this, dir]() {
      if (viewer_) {
        viewer_->save_screenshot(dir + "/vtk_viewport.png");
      }
    });
  }

  struct TourStep {
    QString name;
    std::function<void()> activate;
    QWidget* capture = nullptr;
  };
  QList<TourStep> steps;
  const QStringList modules = {"Sketch",   "Part",     "Property",
                               "Material", "Section",  "Assembly",
                               "Step",     "Interaction", "Load",
                               "Mesh",     "Job",      "Visualization",
                               "Results"};
  for (int i = 0; i < modules.size() && i < module_tabs_->count(); ++i) {
    steps.append({QString("module_%1_%2")
                      .arg(i, 2, 10, QLatin1Char('0'))
                      .arg(modules[i]),
                  [this, i]() {
                    module_tabs_->setCurrentIndex(i);
                    // 可选地走与人工点击完全相同的 tabBarClicked 路径，
                    // 用于捕获“切换成功但点击打开工作窗时崩溃”的回归。
                    if (qEnvironmentVariableIsSet("GMP_TOUR_REAL_CLICKS")) {
                      QMetaObject::invokeMethod(module_tabs_, "tabBarClicked",
                                                Qt::DirectConnection,
                                                Q_ARG(int, i));
                    }
                  },
                  this});
  }
  if (qEnvironmentVariableIsSet("GMP_TOUR_REAL_CLICKS")) {
    auto i01_original_name = std::make_shared<QString>();
    auto i01_original_dirty = std::make_shared<bool>(false);
    auto i01_form_size = std::make_shared<QSize>();
    steps.append({"l04_top_context_1280",
                  [this]() {
                    resize(1280, 720);
                    const QStringList groups = {
                        "projectToolGroup", "editToolGroup", "modelToolGroup",
                        "meshToolGroup", "jobToolGroup"};
                    for (const QString& name : groups) {
                      auto* group = findChild<QToolBar*>(name);
                      if (!group || !group->isVisible() || group->height() > 32) {
                        throw std::runtime_error("L-04 compact toolbar group contract failed");
                      }
                    }
                    auto* project_group = findChild<QToolBar*>("projectToolGroup");
                    auto* edit_group = findChild<QToolBar*>("editToolGroup");
                    auto* mesh_group = findChild<QToolBar*>("meshToolGroup");
                    if (!project_group ||
                        !project_group->actions().contains(action_save_) ||
                        !edit_group ||
                        !edit_group->actions().contains(action_undo_) ||
                        !mesh_group ||
                        !mesh_group->actions().contains(action_mesh_)) {
                      throw std::runtime_error("L-04 menu/toolbar QAction sharing contract failed");
                    }
                    const QStringList menus = {
                        "fileMenu", "modelMenu", "viewMenu", "meshMenu",
                        "jobMenu", "toolsMenu", "settingsMenu", "helpMenu"};
                    for (const QString& name : menus) {
                      if (!findChild<QMenu*>(name)) {
                        throw std::runtime_error("L-04 standard menu contract failed");
                      }
                    }
                    if (!module_selector_ || !module_selector_->isVisible() ||
                        !context_project_label_ ||
                        !context_project_label_->isVisible() ||
                        !context_object_selector_ ||
                        !context_object_selector_->isVisible()) {
                      throw std::runtime_error("L-04 work context fields are not visible");
                    }
                    auto* context_bar = findChild<QWidget*>("moduleBar");
                    const int top_height = menuBar()->height() + 30 +
                                           (context_bar ? context_bar->height()
                                                        : 1000);
                    if (top_height > 100) {
                      throw std::runtime_error("L-04 top three-layer height exceeds 100 px");
                    }
                  },
                  this});
    steps.append({"l04_module_selector_part",
                  [this]() {
                    const int combo_index = module_selector_
                                                ? module_selector_->findData(1)
                                                : -1;
                    if (combo_index < 0) {
                      throw std::runtime_error("Part module is missing from work context");
                    }
                    module_selector_->setCurrentIndex(combo_index);
                    if (!module_tabs_ || module_tabs_->currentIndex() != 1) {
                      throw std::runtime_error("Work context module did not switch internal module");
                    }
                  },
                  this});
    steps.append({"l05_tool_group_layout_contract",
                  [this]() {
                    reset_tool_group_layout(false);
                    const QStringList groups = {
                        "projectToolGroup", "editToolGroup", "modelToolGroup",
                        "meshToolGroup", "jobToolGroup"};
                    for (const QString& name : groups) {
                      auto* group = findChild<QToolBar*>(name);
                      // 工具组禁用 Qt 原生拖出浮动（macOS 可靠性决策），
                      // 保留停靠区间拖动（movable）与全区域停靠能力。
                      if (!group || !group->isMovable() ||
                          group->isFloatable() ||
                          group->allowedAreas() != Qt::AllToolBarAreas) {
                        throw std::runtime_error("L-05 movable toolbar contract failed");
                      }
                    }
                    auto* toolbar_menu =
                        findChild<QMenu*>("toolbarVisibilityMenu");
                    if (!display_tool_group_ ||
                        display_tool_group_->isFloating() ||
                        toolBarArea(display_tool_group_) !=
                            Qt::TopToolBarArea ||
                        display_tool_group_->allowedAreas() !=
                            Qt::AllToolBarAreas ||
                        !toolbar_menu || toolbar_menu->actions().size() != 7 ||
                        !action_reset_tool_layout_ ||
                        saveState(3).isEmpty()) {
                      throw std::runtime_error("L-05 display/persistence contract failed");
                    }
                    auto* toggle =
                        findChild<QToolBar*>("modelToolGroup")->toggleViewAction();
                    toggle->trigger();
                    if (toggle->isChecked()) {
                      throw std::runtime_error("L-05 toolbar visibility toggle failed");
                    }
                    toggle->trigger();
                    if (!toggle->isChecked()) {
                      throw std::runtime_error("L-05 toolbar visibility restore failed");
                    }
                    auto* project_group =
                        findChild<QToolBar*>("projectToolGroup");
                    auto* edit_group = findChild<QToolBar*>("editToolGroup");
                    removeToolBar(project_group);
                    addToolBar(Qt::BottomToolBarArea, project_group);
                    edit_group->hide();
                    const QByteArray round_trip_state = saveState(3);
                    reset_tool_group_layout(false);
                    if (!restoreState(round_trip_state, 3) ||
                        toolBarArea(project_group) != Qt::BottomToolBarArea ||
                        edit_group->isVisible()) {
                      throw std::runtime_error("L-05 layout round-trip contract failed");
                    }
                    reset_tool_group_layout(false);
                    auto* job_group = findChild<QToolBar*>("jobToolGroup");
                    removeToolBar(display_tool_group_);
                    display_tool_group_->setParent(this, Qt::Widget);
                    insertToolBar(job_group, display_tool_group_);
                    display_tool_group_->show();
                    if (display_tool_group_->isFloating() ||
                        toolBarArea(display_tool_group_) !=
                            Qt::TopToolBarArea ||
                        display_tool_group_->y() != project_group->y()) {
                      throw std::runtime_error("L-05 display group same-row docking failed");
                    }
                    reset_tool_group_layout(false);
                  },
                  this});
    steps.append({"l05_display_group_default",
                  [this]() {
                    // 默认位置：停靠进顶部工具组区域，不再舞台浮动预置。
                    if (!display_tool_group_ ||
                        !display_tool_group_->isVisible() ||
                        display_tool_group_->isFloating() ||
                        toolBarArea(display_tool_group_) !=
                            Qt::TopToolBarArea) {
                      throw std::runtime_error("L-05 default display group is not docked in the top tool area");
                    }
                  },
                  display_tool_group_});
    steps.append({"p2_workspace_content_layout",
                  [this, dir]() {
                    if (!property_stack_ || !module_work_window_) {
                      throw std::runtime_error("Phase 2 workspace layout fixture is missing");
                    }
                    for (int i = 0; i < property_stack_->count(); ++i) {
                      auto* page = property_stack_->widget(i);
                      if (!page) {
                        throw std::runtime_error("Phase 2 workspace page is missing");
                      }
                      const auto scrolls = page->findChildren<QScrollArea*>();
                      for (auto* scroll : scrolls) {
                        for (QObject* ancestor = scroll->parent(); ancestor &&
                             ancestor != page; ancestor = ancestor->parent()) {
                          if (qobject_cast<QScrollArea*>(ancestor)) {
                            throw std::runtime_error("Phase 2 nested workspace scroll area found");
                          }
                        }
                      }
                    }
                    module_tabs_->setCurrentIndex(3);
                    module_tabs_->tabBarClicked(3);
                    auto* material_page = property_stack_->currentWidget();
                    auto* actions = material_page
                                        ? material_page->findChild<QWidget*>(
                                              "modulePrimaryActions")
                                        : nullptr;
                    auto* content = material_page
                                        ? material_page->findChild<QWidget*>(
                                              "moduleNodeContent")
                                        : nullptr;
                    const auto buttons =
                        actions ? actions->findChildren<QPushButton*>()
                                : QList<QPushButton*>();
                    if (!material_page || !actions || !content ||
                        !material_page->findChildren<QScrollArea*>().isEmpty() ||
                        buttons.size() < 2 ||
                        qAbs(buttons.at(0)->geometry().center().y() -
                             buttons.at(1)->geometry().center().y()) > 2 ||
                        module_work_window_->minimumWidth() < 620 ||
                        !content->isVisible()) {
                      throw std::runtime_error("Phase 2 material workspace layout contract failed");
                    }
                    auto* part_page = property_stack_->widget(1);
                    auto* feature_tabs = part_page
                                             ? part_page->findChild<QTabWidget*>(
                                                   "partFeatureTabs")
                                             : nullptr;
                    if (!feature_tabs || feature_tabs->count() != 4) {
                      throw std::runtime_error("Phase 2 part feature tabs contract failed");
                    }
                    if (!gmsh_panel_ || !mesh_work_window_ ||
                        property_stack_->indexOf(gmsh_panel_) >= 0) {
                      throw std::runtime_error("I-04 mesh workspace ownership contract failed");
                    }
                    auto* mesh_page = gmsh_panel_;
                    auto* gmsh_tabs = mesh_page
                                          ? mesh_page->findChild<QTabWidget*>(
                                                "gmshWorkspaceTabs")
                                          : nullptr;
                    auto* geometry_tabs = mesh_page
                                              ? mesh_page->findChild<QTabWidget*>(
                                                    "gmshGeometryTabs")
                                              : nullptr;
                    auto* groups_tabs = mesh_page
                                            ? mesh_page->findChild<QTabWidget*>(
                                                  "gmshGroupsTabs")
                                            : nullptr;
                    if (!gmsh_tabs || gmsh_tabs->count() != 4 ||
                        !geometry_tabs || geometry_tabs->count() != 3 ||
                        !groups_tabs || groups_tabs->count() != 2) {
                      throw std::runtime_error("Phase 2 mesh workspace tabs contract failed");
                    }
                    mesh_work_window_->show();
                    mesh_work_window_->raise();
                    qApp->processEvents();
                    mesh_page->grab().save(dir +
                                           "/p2_mesh_workspace_layout.png");
                    property_stack_->setCurrentIndex(3);
                  },
                  module_work_window_});
    steps.append({"i01_property_form_cancel_buffer",
                  [this, i01_original_name, i01_original_dirty,
                   i01_form_size, dir]() {
                    auto* root = find_root_item("Materials");
                    if (!root || root->childCount() == 0) {
                      throw std::runtime_error("I-01 material fixture is missing");
                    }
                    auto* item = root->child(0);
                    *i01_original_name = item->text(0);
                    *i01_original_dirty = project_dirty_;
                    model_tree_->setCurrentItem(item);
                    model_tree_->itemDoubleClicked(item, 0);
                    auto* form = findChild<FloatingPropertyForm*>(
                        "floatingPropertyForm");
                    auto* name = form ? form->findChild<QLineEdit*>(
                                            "propertyNameEdit")
                                      : nullptr;
                    auto* cancel = form ? form->findChild<QPushButton*>(
                                              "propertyFormCancel")
                                        : nullptr;
                    const QString title_prefix =
                        l10n::current_language() == l10n::Language::Chinese
                            ? QString::fromUtf8("编辑材料 — ")
                            : QString("Edit Material — ");
                    if (!form || !form->isVisible() || !form->isModal() ||
                        form->windowTitle() != title_prefix + item->text(0) ||
                        !name || !cancel) {
                      throw std::runtime_error("I-01 floating property form contract failed");
                    }
                    const auto property_scrolls =
                        form->findChildren<QScrollArea*>();
                    if (!property_scrolls.isEmpty()) {
                      throw std::runtime_error("I-01 property form contains an outer scroll area");
                    }
                    if (auto* editor_tabs = form->findChild<QTabWidget*>(
                            "propertyEditorTabs");
                        !editor_tabs || editor_tabs->count() != 4) {
                      throw std::runtime_error("I-01 property form tab layout contract failed");
                    }
                    *i01_form_size = form->size();
                    form->grab().save(dir + "/i01_property_form_layout.png");
                    const QRect stage_rect(
                        viewer_->mapToGlobal(QPoint(0, 0)), viewer_->size());
                    if (!stage_rect.contains(form->frameGeometry().center())) {
                      throw std::runtime_error("I-01 property form is outside the stage");
                    }
                    name->setText("i01_discarded_name");
                    if (item->text(0) != *i01_original_name ||
                        project_dirty_ != *i01_original_dirty) {
                      throw std::runtime_error("I-01 uncommitted edit leaked into project state");
                    }
                    cancel->click();
                    if (item->text(0) != *i01_original_name ||
                        project_dirty_ != *i01_original_dirty) {
                      throw std::runtime_error("I-01 cancel changed project state");
                    }
                  },
                  this});
    steps.append({"i01_property_form_validation",
                  [this, i01_original_name, i01_original_dirty,
                   i01_form_size]() {
                    auto* root = find_root_item("Materials");
                    auto* item = root && root->childCount() > 0
                                     ? root->child(0)
                                     : nullptr;
                    if (!item) {
                      throw std::runtime_error("I-01 material fixture disappeared");
                    }
                    model_tree_->setCurrentItem(item);
                    model_tree_->itemDoubleClicked(item, 0);
                    auto* form = findChild<FloatingPropertyForm*>(
                        "floatingPropertyForm");
                    auto* name = form ? form->findChild<QLineEdit*>(
                                            "propertyNameEdit")
                                      : nullptr;
                    auto* ok = form ? form->findChild<QPushButton*>(
                                          "propertyFormOk")
                                    : nullptr;
                    if (!form || !name || !ok ||
                        form->size() != *i01_form_size) {
                      throw std::runtime_error("I-01 property form did not reopen");
                    }
                    name->clear();
                    ok->click();
                    if (!form->isVisible() ||
                        item->text(0) != *i01_original_name ||
                        project_dirty_ != *i01_original_dirty) {
                      throw std::runtime_error("I-01 invalid edit was not blocked and focused");
                    }
                  },
                  this});
    steps.append({"i01_property_form_commit",
                  [this]() {
                    auto* root = find_root_item("Materials");
                    auto* item = root && root->childCount() > 0
                                     ? root->child(0)
                                     : nullptr;
                    auto* form = findChild<FloatingPropertyForm*>(
                        "floatingPropertyForm");
                    auto* name = form ? form->findChild<QLineEdit*>(
                                            "propertyNameEdit")
                                      : nullptr;
                    auto* ok = form ? form->findChild<QPushButton*>(
                                          "propertyFormOk")
                                    : nullptr;
                    if (!item || !form || !name || !ok) {
                      throw std::runtime_error("I-01 commit fixture is missing");
                    }
                    const bool can_assert_system_focus =
                        QGuiApplication::applicationState() ==
                        Qt::ApplicationActive;
                    if (!name->isVisible() || !name->isEnabled() ||
                        (can_assert_system_focus && !name->hasFocus())) {
                      throw std::runtime_error(
                          "I-01 validation did not expose and focus the first field");
                    }
                    name->setText("material_i01_committed");
                    ok->click();
                    if (item->text(0) != "material_i01_committed" ||
                        !project_dirty_) {
                      throw std::runtime_error("I-01 accepted edit was not committed");
                    }
                  },
                  this});
    steps.append({"i02_navigation_status_filter",
                  [this]() {
                    if (!navigation_tabs_ || navigation_tabs_->count() != 2 ||
                        navigation_tabs_->tabText(0) == "Material Library" ||
                        !model_tree_ || model_tree_->columnCount() != 2 ||
                        !results_navigation_tree_ ||
                        results_navigation_tree_->columnCount() != 2 ||
                        model_tree_->header()->sectionResizeMode(0) !=
                            QHeaderView::Interactive ||
                        model_tree_->header()->sectionResizeMode(1) !=
                            QHeaderView::Interactive ||
                        results_navigation_tree_->header()->sectionResizeMode(
                            0) != QHeaderView::Interactive ||
                        results_navigation_tree_->header()->sectionResizeMode(
                            1) != QHeaderView::Interactive ||
                        !find_root_item("Assembly") ||
                        !find_root_item("Physics") ||
                        !find_root_item("Constraints") ||
                        !find_root_item("Selections") ||
                        !find_root_item("Input Cases")) {
                      throw std::runtime_error("I-02 navigation/schema contract failed");
                    }
                    auto* material_root = find_root_item("Materials");
                    auto* parts_root = find_root_item("Parts");
                    auto* selections_root = find_root_item("Selections");
                    auto* mesh_root = find_root_item("Mesh");
                    auto* jobs_root = find_root_item("Jobs");
                    auto* results_root = find_root_item("Results");
                    if (!material_root || !parts_root || !selections_root ||
                        !mesh_root || !jobs_root || !results_root) {
                      throw std::runtime_error("I-02 fixture roots are missing");
                    }
                    if (parts_root->childCount() == 0) {
                      add_child_item(parts_root, "part_i02", "Parts",
                                     {{"type", "Part"},
                                      {"gmsh_volume_tag", 101}});
                    } else {
                      QVariantMap params =
                          parts_root->child(0)
                              ->data(0, PropertyEditor::kParamsRole)
                              .toMap();
                      params.insert("gmsh_volume_tag", 101);
                      parts_root->child(0)->setData(
                          0, PropertyEditor::kParamsRole, params);
                    }
                    add_child_item(selections_root, "selection_i02",
                                   "Selections",
                                   {{"type", "PhysicalGroup"},
                                    {"group_dim", 2},
                                    {"group_tag", 17}});
                    auto* mesh_item = add_child_item(
                        mesh_root, "mesh_i02", "Mesh",
                        {{"status", "Generated"}, {"path", ""}});
                    mesh_item->setData(0, PropertyEditor::kStatusRole,
                                       "Generated");
                    auto* input_root = find_root_item("Input Cases");
                    if (input_root->childCount() == 0) {
                      auto* input_item = add_child_item(
                          input_root, "case_i02.i", "Input Cases",
                          {{"status", "Generated"}, {"path", "case_i02.i"}});
                      input_item->setData(0, PropertyEditor::kStatusRole,
                                          "Generated");
                    }
                    add_child_item(jobs_root, "job_i02", "Jobs",
                                   {{"status", "Completed"}});
                    add_child_item(results_root, "result_i02", "Results",
                                   {{"status", "Success"}, {"path", ""}});
                    refresh_workflow_status();
                    for (int row = 0; row < model_tree_->topLevelItemCount();
                         ++row) {
                      auto* root = model_tree_->topLevelItem(row);
                      if (!root || root->text(1).isEmpty() ||
                          root->icon(1).isNull()) {
                        throw std::runtime_error("I-02 status icon/badge contract failed");
                      }
                    }
                    model_tree_filter_->setText("material_i01_committed");
                    qApp->processEvents();
                    const auto clear_buttons =
                        model_tree_filter_->findChildren<QToolButton*>();
                    if (clear_buttons.isEmpty() ||
                        clear_buttons.first()->height() >
                            model_tree_filter_->height()) {
                      throw std::runtime_error("I-02 embedded filter clear button size failed");
                    }
                    if (material_root->isHidden() || !parts_root->isHidden()) {
                      throw std::runtime_error("I-02 model name filter contract failed");
                    }
                    model_tree_filter_->clear();
                    if (parts_root->isHidden()) {
                      throw std::runtime_error("I-02 clearing model filter did not restore tree");
                    }
                  },
                  this});
    steps.append({"i02_tree_stage_round_trip",
                  [this]() {
                    auto* parts_root = find_root_item("Parts");
                    auto* selections_root = find_root_item("Selections");
                    auto* part = parts_root && parts_root->childCount() > 0
                                     ? parts_root->child(0)
                                     : nullptr;
                    auto* selection =
                        selections_root && selections_root->childCount() > 0
                            ? selections_root->child(
                                  selections_root->childCount() - 1)
                            : nullptr;
                    if (!part || !selection || !viewer_) {
                      throw std::runtime_error("I-02 tree/stage fixture is missing");
                    }
                    model_tree_->setCurrentItem(selection);
                    viewer_->mesh_group_picked(2, 17);
                    if (model_tree_->currentItem() != selection ||
                        navigation_tabs_->currentIndex() != 0) {
                      throw std::runtime_error("I-02 stage group pick did not locate Selection");
                    }
                    viewer_->mesh_entity_picked(3, 101);
                    if (model_tree_->currentItem() != part ||
                        !context_object_selector_ ||
                        context_object_selector_->currentText() !=
                            part->text(0)) {
                      throw std::runtime_error("I-02 stage entity pick did not locate Part");
                    }
                  },
                  this});
    steps.append({"i02_results_navigation",
                  [this]() {
                    refresh_workflow_status();
                    navigation_tabs_->setCurrentIndex(1);
                    QTreeWidgetItem* result_nav = nullptr;
                    for (int top = 0;
                         top < results_navigation_tree_->topLevelItemCount();
                         ++top) {
                      auto* root = results_navigation_tree_->topLevelItem(top);
                      for (int row = 0; root && row < root->childCount(); ++row) {
                        if (root->child(row)->text(0) == "result_i02") {
                          result_nav = root->child(row);
                          break;
                        }
                      }
                    }
                    if (!result_nav) {
                      throw std::runtime_error("I-02 Results navigation did not mirror result");
                    }
                    results_navigation_tree_->setCurrentItem(result_nav);
                    if (!model_tree_->currentItem() ||
                        model_tree_->currentItem()->text(0) != "result_i02") {
                      throw std::runtime_error("I-02 Results navigation did not sync Model tree");
                    }
                    results_tree_filter_->setText("result_i02");
                    qApp->processEvents();
                    QTreeWidgetItem* filtered_result = nullptr;
                    for (int top = 0;
                         top < results_navigation_tree_->topLevelItemCount();
                         ++top) {
                      auto* root = results_navigation_tree_->topLevelItem(top);
                      for (int row = 0; root && row < root->childCount(); ++row) {
                        if (root->child(row)->text(0) == "result_i02") {
                          filtered_result = root->child(row);
                          break;
                        }
                      }
                    }
                    if (!filtered_result || filtered_result->isHidden()) {
                      throw std::runtime_error("I-02 Results name filter hid matching result");
                    }
                    results_tree_filter_->clear();
                    navigation_tabs_->setCurrentIndex(0);
                    if (!model_tree_->currentItem() ||
                        model_tree_->currentItem()->text(0) != "result_i02") {
                      throw std::runtime_error("I-02 tab switch changed project selection");
                    }
                  },
                  this});
    steps.append({"i02_downstream_invalidation",
                  [this]() {
                    auto* material_root = find_root_item("Materials");
                    auto* material = material_root && material_root->childCount() > 0
                                         ? material_root->child(0)
                                         : nullptr;
                    if (!material) {
                      throw std::runtime_error("I-02 invalidation source is missing");
                    }
                    QVariantMap params =
                        material->data(0, PropertyEditor::kParamsRole).toMap();
                    params.insert("i02_revision", 2);
                    material->setData(0, PropertyEditor::kParamsRole, params);
                    refresh_workflow_status();
                    for (const QString& kind :
                         {QString("Mesh"), QString("Input Cases"),
                          QString("Jobs")}) {
                      auto* root = find_root_item(kind);
                      if (!root || root->childCount() == 0 ||
                          root->child(0)
                                  ->data(0, PropertyEditor::kStatusRole)
                                  .toString() != "Stale" ||
                          root->child(0)->text(1).isEmpty()) {
                        throw std::runtime_error("I-02 downstream invalidation contract failed");
                      }
                    }
                  },
                  this});
    steps.append({"i03_context_round_trip",
                  [this]() {
                    auto* parts_root = find_root_item("Parts");
                    auto* materials_root = find_root_item("Materials");
                    auto* part = parts_root && parts_root->childCount() > 0
                                     ? parts_root->child(0)
                                     : nullptr;
                    auto* material =
                        materials_root && materials_root->childCount() > 0
                            ? materials_root->child(0)
                            : nullptr;
                    if (!part || !material || !module_selector_ ||
                        !active_context_status_label_) {
                      throw std::runtime_error("I-03 context round-trip fixture is missing");
                    }
                    model_tree_->setCurrentItem(part);
                    model_tree_->setCurrentItem(material);
                    int combo = module_selector_->findData(1);
                    module_selector_->setCurrentIndex(combo);
                    if (model_tree_->currentItem() != part ||
                        context_object_selector_->currentText() != part->text(0) ||
                        !active_context_status_label_->text().contains(
                            part->text(0))) {
                      throw std::runtime_error("I-03 Part context was not restored consistently");
                    }
                    combo = module_selector_->findData(3);
                    module_selector_->setCurrentIndex(combo);
                    if (model_tree_->currentItem() != material ||
                        context_object_selector_->currentText() !=
                            material->text(0) ||
                        !active_context_status_label_->text().contains(
                            material->text(0))) {
                      throw std::runtime_error("I-03 Material context was not restored consistently");
                    }
                  },
                  this});
    steps.append({"i03_command_availability",
                  [this]() {
                    auto* jobs_root = find_root_item("Jobs");
                    if (!jobs_root || !action_edit_properties_) {
                      throw std::runtime_error("I-03 command availability fixture is missing");
                    }
                    model_tree_->setCurrentItem(jobs_root);
                    if (action_edit_properties_->isEnabled() ||
                        action_edit_properties_->toolTip().isEmpty()) {
                      throw std::runtime_error("I-03 unsupported command lacks disabled reason");
                    }
                    if (!action_stage_clear_ || !viewer_) {
                      throw std::runtime_error("I-03 stage selection fixture is missing");
                    }
                    viewer_->mesh_entity_picked(3, 101);
                    if (!action_stage_clear_->isEnabled()) {
                      throw std::runtime_error("I-03 stage selection collection was not updated");
                    }
                    action_stage_clear_->trigger();
                    if (action_stage_clear_->isEnabled()) {
                      throw std::runtime_error("I-03 stage selection collection was not cleared");
                    }
                  },
                  this});
    steps.append({"i03_mesh_running_guard",
                  [this]() {
                    if (!gmsh_panel_ || !action_mesh_ || !action_run_ ||
                        !job_run_button_) {
                      throw std::runtime_error("I-03 running-state fixture is missing");
                    }
                    auto* panel_generate =
                        gmsh_panel_->findChild<QPushButton*>(
                            "generateMeshButton");
                    auto* panel_run = moose_panel_
                                          ? moose_panel_->findChild<QPushButton*>(
                                                "mooseRunButton")
                                          : nullptr;
                    auto* panel_check =
                        moose_panel_
                            ? moose_panel_->findChild<QPushButton*>(
                                  "mooseCheckButton")
                            : nullptr;
                    if (!panel_generate || !panel_run || !panel_check) {
                      throw std::runtime_error("I-03 panel command fixture is missing");
                    }
                    gmsh_panel_->mesh_generation_started();
                    if (action_mesh_->isEnabled() || action_run_->isEnabled() ||
                        job_run_button_->isEnabled() || panel_run->isEnabled() ||
                        panel_check->isEnabled() ||
                        !action_mesh_->text().contains("...")) {
                      throw std::runtime_error("I-03 mesh running guard did not disable commands");
                    }
                    gmsh_panel_->mesh_generation_finished(true,
                                                          "Mesh generated.");
                    const QString idle_mesh_text =
                        l10n::current_language() == l10n::Language::Chinese
                            ? QString::fromUtf8("生成网格")
                            : QString("Generate Mesh");
                    if (!action_mesh_->isEnabled() || !action_run_->isEnabled() ||
                        !job_run_button_->isEnabled() ||
                        !panel_run->isEnabled() || !panel_check->isEnabled() ||
                        action_mesh_->text() != idle_mesh_text) {
                      throw std::runtime_error("I-03 mesh running guard did not restore commands");
                    }
                    QVariantMap job_info;
                    job_info.insert("input", "i03_running_guard.i");
                    moose_panel_->job_started(job_info);
                    if (panel_generate->isEnabled() || action_mesh_->isEnabled() ||
                        action_run_->isEnabled() || !action_stop_->isEnabled() ||
                        job_run_button_->isEnabled() ||
                        !job_stop_button_->isEnabled()) {
                      throw std::runtime_error("I-03 Job running guard did not disable commands");
                    }
                    QVariantMap job_finish;
                    job_finish.insert("status", "Normal");
                    moose_panel_->job_finished(job_finish);
                    if (!panel_generate->isEnabled() ||
                        !action_mesh_->isEnabled() || !action_run_->isEnabled() ||
                        action_stop_->isEnabled() ||
                        !job_run_button_->isEnabled() ||
                        job_stop_button_->isEnabled()) {
                      throw std::runtime_error("I-03 Job running guard did not restore commands");
                    }
                  },
                  this});
    steps.append({"i03_sketch_switch_guard",
                  [this]() {
                    auto* root = find_root_item("Sketches");
                    if (!root || !module_selector_ || !module_work_window_) {
                      throw std::runtime_error("I-03 Sketch guard fixture is missing");
                    }
                    auto* sketch = add_child_item(
                        root, "sketch_i03_guard", "Sketches",
                        {{"type", "Sketch2D"}, {"plane", "XY"}});
                    model_tree_->setCurrentItem(sketch);
                    model_tree_->itemDoubleClicked(sketch, 0);
                    const int part_combo = module_selector_->findData(1);
                    module_selector_->setCurrentIndex(part_combo);
                    if (!active_sketch_doc_ || module_tabs_->currentIndex() != 0 ||
                        module_selector_->currentData().toInt() != 0 ||
                        module_selector_->isEnabled() || model_tree_->isEnabled()) {
                      throw std::runtime_error("I-03 allowed context switch during Sketch edit");
                    }
                    module_work_window_->close();
                    qApp->processEvents();
                    if (active_sketch_doc_ || !module_selector_->isEnabled() ||
                        !model_tree_->isEnabled()) {
                      throw std::runtime_error("I-03 did not restore context controls after Sketch edit");
                    }
                    const int row = root->indexOfChild(sketch);
                    if (row >= 0) {
                      delete root->takeChild(row);
                    }
                    model_tree_->setCurrentItem(root);
                    if (viewer_) {
                      viewer_->set_sketch_preview(nullptr);
                    }
                  },
                  this});
    steps.append({"module_repeat_Part",
                  [this]() {
                    for (int i = 0; i < module_tabs_->count(); ++i) {
                      if (module_tabs_->tabText(i) == "Part" ||
                          module_tabs_->tabText(i) == QString::fromUtf8("部件")) {
                        module_tabs_->setCurrentIndex(i);
                        QMetaObject::invokeMethod(module_tabs_, "tabBarClicked",
                                                  Qt::DirectConnection,
                                                  Q_ARG(int, i));
                        break;
                      }
                    }
                  },
                  this});
    steps.append({"part_fixture_for_double_click",
                  [this]() {
                    auto* root = find_root_item("Parts");
                    bool found = false;
                    for (int row = 0; root && row < root->childCount(); ++row) {
                      found = found || root->child(row)->text(0) == "part_tour";
                    }
                    if (root && !found) {
                      add_child_item(root, "part_tour", "Parts",
                                     {{"type", "Part"}});
                    }
                  },
                  this});
    steps.append({"l04_object_selector_to_tree",
                  [this]() {
                    refresh_work_context();
                    const int combo_index = context_object_selector_
                                                ? context_object_selector_->findText(
                                                      "part_tour")
                                                : -1;
                    if (combo_index < 0) {
                      throw std::runtime_error("Part fixture is missing from current object selector");
                    }
                    context_object_selector_->setCurrentIndex(combo_index);
                    if (!model_tree_ || !model_tree_->currentItem() ||
                        model_tree_->currentItem()->text(0) != "part_tour") {
                      throw std::runtime_error("Current object selector did not locate model tree item");
                    }
                  },
                  this});
    steps.append({"tree_Parts_root",
                  [this]() {
                    if (auto* root = find_root_item("Parts")) {
                      model_tree_->setCurrentItem(root);
                    }
                  },
                  this});
    steps.append({"tree_Parts_child",
                  [this]() {
                    if (auto* root = find_root_item("Parts");
                        root && root->childCount() > 0) {
                      model_tree_->setCurrentItem(root->child(0));
                    }
                  },
                  this});
    steps.append({"tree_unique_name_guard",
                  [this]() {
                    auto* root = find_root_item("Parts");
                    if (!root) {
                      throw std::runtime_error(
                          "Unique-name regression root is unavailable");
                    }
                    QTreeWidgetItem* existing = nullptr;
                    for (int row = 0; row < root->childCount(); ++row) {
                      if (root->child(row)->text(0) == "part_1") {
                        existing = root->child(row);
                        break;
                      }
                    }
                    bool created_fixture = false;
                    if (!existing) {
                      existing = add_child_item(root, "part_1", "Parts",
                                                {{"type", "Part"}});
                      created_fixture = true;
                    }
                    const QString accepted =
                        unique_child_name(root, "part_name_after_conflict");
                    const int before = root->childCount();
                    struct DialogState {
                      bool opened = false;
                      bool duplicate_rejected = false;
                    };
                    auto state = std::make_shared<DialogState>();
                    QTimer::singleShot(
                        0, this, [state, accepted]() {
                          auto* dialog = qobject_cast<QDialog*>(
                              QApplication::activeModalWidget());
                          auto* editor = dialog
                                             ? dialog->findChild<QLineEdit*>(
                                                   "uniqueObjectNameInput")
                                             : nullptr;
                          auto* error = dialog
                                            ? dialog->findChild<QLabel*>(
                                                  "uniqueObjectNameError")
                                            : nullptr;
                          auto* buttons =
                              dialog ? dialog->findChild<QDialogButtonBox*>(
                                           "uniqueObjectNameButtons")
                                     : nullptr;
                          auto* ok = buttons
                                         ? buttons->button(QDialogButtonBox::Ok)
                                         : nullptr;
                          if (!dialog || !editor || !error || !ok) {
                            if (dialog) {
                              dialog->reject();
                            }
                            return;
                          }
                          state->opened = true;
                          editor->setText("part_1");
                          ok->click();
                          QTimer::singleShot(
                              0, dialog,
                              [state, dialog, editor, error, ok, accepted]() {
                                state->duplicate_rejected =
                                    dialog->isVisible() && error->isVisible() &&
                                    !error->text().isEmpty();
                                editor->setText(accepted);
                                ok->click();
                              });
                        });
                    QTimer watchdog;
                    watchdog.setSingleShot(true);
                    connect(&watchdog, &QTimer::timeout, this, []() {
                      if (auto* dialog = qobject_cast<QDialog*>(
                              QApplication::activeModalWidget())) {
                        dialog->reject();
                      }
                    });
                    watchdog.start(2000);
                    add_item_under_root(root);
                    watchdog.stop();

                    QTreeWidgetItem* added = nullptr;
                    int duplicate_count = 0;
                    for (int row = 0; row < root->childCount(); ++row) {
                      auto* child = root->child(row);
                      duplicate_count += child->text(0) == "part_1" ? 1 : 0;
                      if (child->text(0) == accepted) {
                        added = child;
                      }
                    }
                    if (!state->opened || !state->duplicate_rejected || !added ||
                        root->childCount() != before + 1 ||
                        duplicate_count != 1) {
                      throw std::runtime_error(
                          "Duplicate name was accepted or naming dialog closed early");
                    }

                    // 所有无命名弹窗的内部创建也必须自动避让同名。
                    auto* auto_first = add_child_item(
                        root, "part_programmatic_name_guard", "Parts",
                        {{"type", "Part"}});
                    auto* auto_second = add_child_item(
                        root, "part_programmatic_name_guard", "Parts",
                        {{"type", "Part"}});
                    if (!auto_first || !auto_second ||
                        auto_first->text(0) == auto_second->text(0)) {
                      throw std::runtime_error(
                          "Programmatic tree insertion created a duplicate name");
                    }

                    for (auto* fixture : {added, auto_first, auto_second}) {
                      const int row = root->indexOfChild(fixture);
                      if (row >= 0) {
                        delete root->takeChild(row);
                      }
                    }
                    if (created_fixture) {
                      const int row = root->indexOfChild(existing);
                      if (row >= 0) {
                        delete root->takeChild(row);
                      }
                    }
                    refresh_module_pages();
                  },
                  this});
    steps.append({"part_tree_double_click_opens_editor",
                  [this]() {
                    if (auto* root = find_root_item("Parts");
                        root && root->childCount() > 0) {
                      auto* item = root->child(0);
                      model_tree_->setCurrentItem(item);
                      model_tree_->itemDoubleClicked(item, 0);
                    }
                    if (!module_work_window_ ||
                        !module_work_window_->isVisible() ||
                        !module_work_window_->windowTitle().startsWith(
                            "Part Editor")) {
                      throw std::runtime_error("Part tree double-click did not open editor");
                    }
                  },
                  module_work_window_});
    steps.append({"sketch_new_opens_editor",
                  [this]() {
                    for (int i = 0; i < module_tabs_->count(); ++i) {
                      if (module_tabs_->tabText(i) == "Sketch" ||
                          module_tabs_->tabText(i) == QString::fromUtf8("草图")) {
                        module_tabs_->setCurrentIndex(i);
                        break;
                      }
                    }
                    if (auto* button = sketch_panel_->findChild<QPushButton*>(
                            "newSketchButton")) {
                      button->click();
                    }
                    qApp->processEvents();
                    auto* rectangle = sketch_panel_
                                          ? sketch_panel_->findChild<QPushButton*>(
                                                "sketchTool_5")
                                          : nullptr;
                    auto* tool_status = sketch_panel_
                                            ? sketch_panel_->findChild<QLabel*>(
                                                  "sketchCurrentTool")
                                            : nullptr;
                    auto* finish = sketch_panel_
                                       ? sketch_panel_->findChild<QPushButton*>(
                                             "finishSketchEditButton")
                                       : nullptr;
                    if (!module_work_window_ || !rectangle || !tool_status ||
                        !finish ||
                        module_work_window_->property("gmpWorkspaceProfile")
                                .toString() != "sketch" ||
                        module_work_window_->minimumSize() != QSize(640, 320) ||
                        finish->maximumWidth() > 160) {
                      throw std::runtime_error(
                          "Sketch editor compact window profile is not active");
                    }
                    rectangle->click();
                    qApp->processEvents();
                    if (!rectangle->isChecked() ||
                        !rectangle->property("gmpSketchTool").toBool() ||
                        viewer_->sketch_tool() != SketchToolDrawRectangle ||
                        (!tool_status->text().contains("Rectangle") &&
                         !tool_status->text().contains(
                             QString::fromUtf8("矩形")))) {
                      throw std::runtime_error(
                          "Sketch drawing tool has no persistent visual state");
                    }
                  },
                  module_work_window_});
    steps.append({"sketch_add_preview_fixture",
                  [this]() {
                    if (!viewer_ || !viewer_->sketch_document() ||
                        viewer_->is_sketch_preview()) {
                      throw std::runtime_error("Sketch editor did not enter editable state");
                    }
                    SketchEntity circle;
                    circle.type = SketchEntityType::Circle;
                    circle.center = {0.0, 0.0};
                    circle.radius = 20.0;
                    viewer_->sketch_document()->add_entity(circle);
                    // 走与真实绘制相同的持久化/撤销入栈链路。
                    viewer_->sketch_modified();
                  },
                  this});
    steps.append({"sketch_2d_navigation_and_undo_redo",
                  [this]() {
                    if (!viewer_ || !viewer_->sketch_document() ||
                        viewer_->sketch_document()->entity_count() != 1 ||
                        !action_undo_ || !action_redo_) {
                      throw std::runtime_error(
                          "Sketch fixture or undo actions are unavailable");
                    }
                    if (action_undo_->shortcutContext() !=
                            Qt::ApplicationShortcut ||
                        action_redo_->shortcutContext() !=
                            Qt::ApplicationShortcut ||
                        !action_undo_->isEnabled()) {
                      throw std::runtime_error(
                          "Sketch undo/redo shortcut scope contract failed");
                    }
                    action_undo_->trigger();
                    if (viewer_->sketch_document()->entity_count() != 0 ||
                        !action_redo_->isEnabled()) {
                      throw std::runtime_error("Sketch undo did not restore snapshot");
                    }
                    action_redo_->trigger();
                    if (viewer_->sketch_document()->entity_count() != 1) {
                      throw std::runtime_error("Sketch redo did not restore snapshot");
                    }

                    auto* pan = findChild<QToolButton*>("stageTool_pan");
                    auto* zoom = findChild<QToolButton*>("stageTool_zoom");
                    auto* select =
                        findChild<QToolButton*>("stageTool_sketch-select");
                    auto* pick = findChild<QToolButton*>("stageTool_pick");
                    if (!pan || !zoom || !select || !pick) {
                      throw std::runtime_error("Sketch stage tools are unavailable");
                    }
                    pan->click();
                    if (viewer_->sketch_tool() != SketchToolMove ||
                        viewer_->sketch_navigation_mode() != -1 ||
                        !pan->isChecked() || select->isChecked() ||
                        pick->isVisible() || pick->isChecked() ||
                        (action_stage_pick_ &&
                         action_stage_pick_->isChecked()) ||
                        !pan->toolTip().startsWith(
                            QString::fromUtf8("移动图形"))) {
                      throw std::runtime_error(
                          "Sketch move tool semantics or exclusivity failed");
                    }
                    zoom->click();
                    if (viewer_->sketch_navigation_mode() != 2 ||
                        pan->isChecked() || select->isChecked()) {
                      throw std::runtime_error(
                          "Sketch zoom was not exclusive with entity tools");
                    }
                    select->click();
                    if (viewer_->sketch_navigation_mode() != -1 ||
                        viewer_->sketch_tool() != SketchToolSelect ||
                        pan->isChecked() || zoom->isChecked() ||
                        !select->isChecked() || !action_stage_pick_ ||
                        !action_stage_pick_->isChecked()) {
                      throw std::runtime_error(
                          "Sketch viewport did not return to selection mode");
                    }
                    viewer_->apply_stage_view(0);
                  },
                  this});
    steps.append({"sketch_close_editor",
                  [this]() {
                    if (module_work_window_) {
                      module_work_window_->close();
                    }
                  },
                  this});
    steps.append({"sketch_preview_after_finish",
                  [this]() {
                    if (!viewer_ || !viewer_->is_sketch_preview() ||
                        !viewer_->sketch_document() ||
                        viewer_->sketch_document()->entity_count() == 0) {
                      throw std::runtime_error("Finished sketch was not retained as read-only preview");
                    }
                    if (!context_object_selector_ ||
                        context_object_selector_->currentText() != "sketch_1") {
                      throw std::runtime_error("Model tree selection did not update current object");
                    }
                  },
                  this});
    steps.append({"sketch_preview_move_opens_editor_and_syncs_tools",
                  [this]() {
                    auto* pan = findChild<QToolButton*>("stageTool_pan");
                    auto* select = findChild<QToolButton*>(
                        "stageTool_sketch-select");
                    auto* panel_select = sketch_panel_
                                             ? sketch_panel_->findChild<QPushButton*>(
                                                   "sketchTool_0")
                                             : nullptr;
                    auto* panel_move = sketch_panel_
                                           ? sketch_panel_->findChild<QPushButton*>(
                                                 "sketchTool_6")
                                           : nullptr;
                    if (!pan || !select || !panel_select || !panel_move ||
                        !action_stage_pick_) {
                      throw std::runtime_error(
                          "Sketch tool synchronization controls are unavailable");
                    }

                    // 预览态点击 Move 应自动进入编辑，而不是静默退回 Select。
                    pan->click();
                    if (!active_sketch_doc_ || viewer_->is_sketch_preview() ||
                        viewer_->sketch_tool() != SketchToolMove ||
                        !pan->isChecked() || select->isChecked() ||
                        !panel_move->isChecked() ||
                        action_stage_pick_->isChecked()) {
                      throw std::runtime_error(
                          "Preview Move did not enter editing with synchronized state");
                    }

                    // 顶部 Pick、左侧 Select 和面板 Select 必须同步回同一状态。
                    action_stage_pick_->trigger();
                    if (viewer_->sketch_tool() != SketchToolSelect ||
                        pan->isChecked() || !select->isChecked() ||
                        !panel_select->isChecked() ||
                        !action_stage_pick_->isChecked()) {
                      throw std::runtime_error(
                          "Top Pick did not synchronize all Sketch tool surfaces");
                    }

                    // 再从编辑面板切回 Move，三个入口不能残留双重高亮。
                    panel_move->click();
                    if (viewer_->sketch_tool() != SketchToolMove ||
                        !pan->isChecked() || select->isChecked() ||
                        !panel_move->isChecked() ||
                        action_stage_pick_->isChecked()) {
                      throw std::runtime_error(
                          "Sketch panel Move did not synchronize all tool surfaces");
                    }
                  },
                  module_work_window_});
    steps.append({"sketch_close_after_tool_sync",
                  [this]() {
                    if (module_work_window_) {
                      module_work_window_->close();
                    }
                  },
                  this});
    steps.append({"sketch_tree_double_click_opens_editor",
                  [this]() {
                    if (auto* root = find_root_item("Sketches");
                        root && root->childCount() > 0) {
                      auto* item = root->child(root->childCount() - 1);
                      model_tree_->setCurrentItem(item);
                      model_tree_->itemDoubleClicked(item, 0);
                    }
                  },
                  module_work_window_});
    steps.append({"sketch_finish_editor",
                  [this]() {
                    if (module_work_window_) {
                      module_work_window_->close();
                    }
                  },
                  this});
    steps.append({"part_new_opens_editor",
                  [this]() {
                    for (int i = 0; i < module_tabs_->count(); ++i) {
                      if (module_tabs_->tabText(i) == "Part" ||
                          module_tabs_->tabText(i) == QString::fromUtf8("部件")) {
                        module_tabs_->setCurrentIndex(i);
                        break;
                      }
                    }
                    QTimer::singleShot(100, this, []() {
                      for (QWidget* widget : QApplication::topLevelWidgets()) {
                        if (auto* dialog = qobject_cast<QInputDialog*>(widget);
                            dialog && dialog->isVisible()) {
                          dialog->accept();
                          return;
                        }
                      }
                    });
                    QPushButton* new_part = nullptr;
                    for (auto* button : findChildren<QPushButton*>()) {
                      if (button->property("moduleAction").toString() ==
                          "New Part") {
                        new_part = button;
                        break;
                      }
                    }
                    if (!new_part) {
                      throw std::runtime_error("New Part command button not found");
                    }
                    new_part->click();
                    if (!module_work_window_ ||
                        !module_work_window_->isVisible()) {
                      throw std::runtime_error("New Part did not open editor");
                    }
                  },
                  module_work_window_});
    steps.append({"part_feature_updates_selected_part",
                  [this]() {
                    auto* parts_root = find_root_item("Parts");
                    auto* features_root = find_root_item("Features");
                    if (!parts_root || !features_root) {
                      throw std::runtime_error("Part feature target fixture is missing");
                    }
                    QString target_name = "part_feature_regression";
                    for (int suffix = 2;; ++suffix) {
                      bool exists = false;
                      for (int row = 0; row < parts_root->childCount(); ++row) {
                        exists = exists ||
                                 (parts_root->child(row) &&
                                  parts_root->child(row)->text(0) == target_name);
                      }
                      if (!exists) {
                        break;
                      }
                      target_name = QString("part_feature_regression_%1")
                                        .arg(suffix);
                    }
                    auto* target = add_child_item(
                        parts_root, target_name, "Parts",
                        {{"type", "Part"}, {"sketch", "sketch_1"}});
                    const int part_count = parts_root->childCount();
                    const int feature_count = features_root->childCount();
                    auto* feature = attach_feature_to_part(
                        target, "Extrude",
                        {{"sketch", "sketch_1"}, {"distance", 10.0}}, 777,
                        QList<int>{777}, QString());
                    const QVariantMap part_params =
                        target->data(0, PropertyEditor::kParamsRole).toMap();
                    const QVariantMap feature_params =
                        feature
                            ? feature->data(0, PropertyEditor::kParamsRole)
                                  .toMap()
                            : QVariantMap();
                    if (!feature || parts_root->childCount() != part_count ||
                        features_root->childCount() != feature_count + 1 ||
                        model_tree_->currentItem() != target ||
                        part_params.value("feature").toString() !=
                            feature->text(0) ||
                        part_params.value("gmsh_volume_tag").toInt() != 777 ||
                        feature_params.value("part").toString() !=
                            target->text(0)) {
                      throw std::runtime_error(
                          "Part feature created a duplicate Part or lost its target");
                    }
                    model_tree_->setCurrentItem(parts_root);
                    if (active_part_item()) {
                      throw std::runtime_error(
                          "Part feature retained a hidden target after selecting the Parts root");
                    }
                    model_tree_->setCurrentItem(target);
                  },
                  module_work_window_});
    steps.append({"part_multi_profile_survives_sketch_round_trip",
                  [this]() {
#if defined(GMP_ENABLE_GMSH_GUI) && defined(GMP_ENABLE_VTK_VIEWER)
                    auto* parts_root = find_root_item("Parts");
                    auto* features_root = find_root_item("Features");
                    auto* sketches_root = find_root_item("Sketches");
                    auto* target = model_tree_ ? model_tree_->currentItem() : nullptr;
                    if (!parts_root || !features_root || !sketches_root ||
                        !target || target->parent() != parts_root || !viewer_) {
                      throw std::runtime_error(
                          "Multi-profile Part regression fixture is unavailable");
                    }

                    SketchDocument profiles;
                    SketchEntity left_circle;
                    left_circle.type = SketchEntityType::Circle;
                    left_circle.center = {-12.0, 0.0};
                    left_circle.radius = 5.0;
                    profiles.add_entity(left_circle);
                    SketchEntity right_circle;
                    right_circle.type = SketchEntityType::Circle;
                    right_circle.center = {12.0, 0.0};
                    right_circle.radius = 4.0;
                    profiles.add_entity(right_circle);

                    const QString stem = QDir(QDir::tempPath())
                                             .filePath(
                                                 "gmp_tour_multi_profile_part");
                    const FeatureResult result =
                        extrude_sketch(profiles, 8.0, stem + ".brep");
                    if (!result.ok || result.gmsh_volume_tags.size() != 2) {
                      throw std::runtime_error(
                          "Two-profile extrusion did not retain both Volume tags");
                    }
                    QString mesh_error;
                    const QString mesh_path = stem + ".msh";
                    if (!mesh_current_model(mesh_path, &mesh_error)) {
                      throw std::runtime_error(
                          QString("Two-profile mesh failed: %1")
                              .arg(mesh_error)
                              .toStdString());
                    }
                    QList<int> tags;
                    for (const int tag : result.gmsh_volume_tags) {
                      tags.append(tag);
                    }
                    auto* feature = attach_feature_to_part(
                        target, "Extrude",
                        {{"sketch", "sketch_1"}, {"distance", 8.0}},
                        result.gmsh_volume_tag, tags, result.brep_path);
                    if (!feature) {
                      throw std::runtime_error(
                          "Two-profile Feature could not attach to its Part");
                    }
                    for (auto* item : {target, feature}) {
                      QVariantMap params =
                          item->data(0, PropertyEditor::kParamsRole).toMap();
                      params.insert("mesh", mesh_path);
                      item->setData(0, PropertyEditor::kParamsRole, params);
                    }
                    viewer_->set_mesh_file(mesh_path);
                    if (viewer_->visible_mesh_entity_count(3) != 2) {
                      throw std::runtime_error(
                          "Two-profile Part was incomplete before module switch");
                    }

                    auto* sketch = sketches_root->childCount() > 0
                                       ? sketches_root->child(0)
                                       : nullptr;
                    if (!sketch) {
                      throw std::runtime_error(
                          "Sketch round-trip fixture is unavailable");
                    }
                    model_tree_->setCurrentItem(sketch);
                    if (!viewer_->is_sketch_preview()) {
                      throw std::runtime_error(
                          "Switching to Sketch did not enter preview");
                    }
                    model_tree_->setCurrentItem(target);
                    const QVariantMap part_params =
                        target->data(0, PropertyEditor::kParamsRole).toMap();
                    if (viewer_->is_sketch_preview() ||
                        viewer_->visible_mesh_entity_count(3) != 2 ||
                        volume_tags_from_params(part_params).size() != 2) {
                      throw std::runtime_error(
                          "Sketch/Part round-trip hid one body of a multi-profile Part");
                    }
#endif
                  },
                  this});
    steps.append({"model_delete_clears_dependent_stage_data",
                  [this]() {
                    auto* parts_root = find_root_item("Parts");
                    auto* features_root = find_root_item("Features");
                    auto* sketches_root = find_root_item("Sketches");
                    auto* target = model_tree_ ? model_tree_->currentItem() : nullptr;
                    const QString mesh_path =
                        QDir::current().absoluteFilePath("out/box.msh");
                    if (!parts_root || !features_root || !sketches_root ||
                        !target || target->parent() != parts_root || !viewer_ ||
                        !QFileInfo::exists(mesh_path)) {
                      throw std::runtime_error(
                          "Model delete stage-clear fixture is unavailable");
                    }
                    const QString part_name = target->text(0);
                    QTreeWidgetItem* sketch = nullptr;
                    for (int row = 0; row < sketches_root->childCount(); ++row) {
                      if (sketches_root->child(row)->text(0) == "sketch_1") {
                        sketch = sketches_root->child(row);
                        break;
                      }
                    }
                    if (!sketch) {
                      throw std::runtime_error(
                          "Referenced Sketch fixture is unavailable");
                    }

                    // 模拟特征即时预览，并把产物路径写到 Part/Feature。
                    viewer_->set_mesh_file(mesh_path);
                    QVariantMap part_params =
                        target->data(0, PropertyEditor::kParamsRole).toMap();
                    part_params.insert("sketch", sketch->text(0));
                    part_params.insert("mesh", mesh_path);
                    target->setData(0, PropertyEditor::kParamsRole, part_params);
                    for (int row = 0; row < features_root->childCount(); ++row) {
                      auto* feature = features_root->child(row);
                      QVariantMap params =
                          feature->data(0, PropertyEditor::kParamsRole).toMap();
                      if (params.value("part").toString() == part_name) {
                        params.insert("mesh", mesh_path);
                        feature->setData(0, PropertyEditor::kParamsRole, params);
                      }
                    }
                    if (!viewer_->has_stage_data() ||
                        !viewer_->stage_data_visible()) {
                      throw std::runtime_error(
                          "Mesh fixture was not visible before model deletion");
                    }

                    // 删除被 Part 引用的 Sketch 必须立即撤下失效的 3D 快照。
                    remove_item(sketch);
                    if (viewer_->has_stage_data() ||
                        viewer_->stage_data_visible()) {
                      throw std::runtime_error(
                          "Deleting a referenced Sketch left stale stage data");
                    }

                    // 重新模拟预览，再删除 Part；所属 Feature 应级联删除。
                    viewer_->set_mesh_file(mesh_path);
                    if (!viewer_->has_stage_data() ||
                        !viewer_->stage_data_visible()) {
                      throw std::runtime_error(
                          "Stage data could not be reloaded after clearing");
                    }
                    const int feature_count = features_root->childCount();
                    remove_item(target);
                    bool orphan_feature = false;
                    for (int row = 0; row < features_root->childCount(); ++row) {
                      const QVariantMap params = features_root->child(row)
                                                     ->data(
                                                         0,
                                                         PropertyEditor::kParamsRole)
                                                     .toMap();
                      orphan_feature =
                          orphan_feature ||
                          params.value("part").toString() == part_name;
                    }
                    if (viewer_->has_stage_data() ||
                        viewer_->stage_data_visible() || orphan_feature ||
                        features_root->childCount() >= feature_count) {
                      throw std::runtime_error(
                          "Deleting a Part did not clear stage data and Feature history");
                    }
                  },
                  this});
    const QStringList stage_commands = {"rotate", "pan",   "zoom", "fit",
                                        "front",  "right", "top",  "iso",
                                        "display", "pick", "clear", "slice"};
    for (const QString& command : stage_commands) {
      steps.append({"stage_command_" + command,
                    [this, command]() {
                      if (auto* button = stage_left_toolbar_->findChild<QToolButton*>(
                              "stageTool_" + command)) {
                        button->click();
                      }
                    },
                    this});
    }
  }
  steps.append({"center_Viewport", [this]() {
                  if (viewer_) {
                    viewer_->setFocus();
                  }
                },
                this});
  auto reveal_workspace = [](QDockWidget* workspace) {
    if (!workspace) {
      return;
    }
    workspace->show();
    workspace->raise();
    workspace->activateWindow();
  };
  steps.append({"workspace_Mesh",
                [this, reveal_workspace]() {
                  reveal_workspace(mesh_work_window_);
                  auto* gmsh_tabs = mesh_work_window_
                                        ? mesh_work_window_->findChild<QTabWidget*>(
                                              "gmshWorkspaceTabs")
                                        : nullptr;
                  auto* geometry_tabs =
                      mesh_work_window_
                          ? mesh_work_window_->findChild<QTabWidget*>(
                                "gmshGeometryTabs")
                          : nullptr;
                  auto* groups_tabs = mesh_work_window_
                                          ? mesh_work_window_->findChild<QTabWidget*>(
                                                "gmshGroupsTabs")
                                          : nullptr;
                  const QRect available = mesh_work_window_ &&
                                                   mesh_work_window_->screen()
                                               ? mesh_work_window_->screen()
                                                     ->availableGeometry()
                                               : QRect();
                  if (!mesh_work_window_ || !gmsh_tabs ||
                      gmsh_tabs->count() != 4 || !geometry_tabs ||
                      geometry_tabs->count() != 3 || !groups_tabs ||
                      groups_tabs->count() != 2 ||
                      (!available.isEmpty() &&
                       (mesh_work_window_->height() > available.height() ||
                        mesh_work_window_->width() > available.width()))) {
                    throw std::runtime_error("I-04 mesh workspace layout contract failed");
                  }
                },
                mesh_work_window_});
  steps.append({"workspace_Job",
                [this, reveal_workspace]() {
                  reveal_workspace(job_work_window_);
                  auto* job_tabs = job_work_window_
                                       ? job_work_window_->findChild<QTabWidget*>(
                                             "jobWorkspaceTabs")
                                       : nullptr;
                  auto* moose_tabs = job_work_window_
                                         ? job_work_window_->findChild<QTabWidget*>(
                                               "mooseWorkspaceTabs")
                                         : nullptr;
                  if (job_tabs) {
                    job_tabs->setCurrentIndex(1);
                  }
                  const QRect available = job_work_window_ &&
                                                   job_work_window_->screen()
                                               ? job_work_window_->screen()
                                                     ->availableGeometry()
                                               : QRect();
                  if (!job_tabs || job_tabs->count() != 2 || !moose_tabs ||
                      moose_tabs->count() != 3 ||
                      (!available.isEmpty() &&
                       job_work_window_->height() > available.height())) {
                    throw std::runtime_error("Phase 2 job workspace layout contract failed");
                  }
                },
                job_work_window_});
  steps.append({"workspace_Visualization",
                [this, reveal_workspace]() {
                  reveal_workspace(visualization_work_window_);
                },
                visualization_work_window_});
  const QStringList result_pages = {"Results", "Plot", "Table"};
  for (int i = 0; i < result_pages.size(); ++i) {
    steps.append({QString("workspace_Results_%1").arg(result_pages.at(i)),
                  [this, reveal_workspace, i]() {
                    if (results_work_tabs_) {
                      results_work_tabs_->setCurrentIndex(i);
                    }
                    reveal_workspace(results_work_window_);
                  },
                results_work_window_});
  }
  steps.append({"i04_work_window_contracts",
                [this]() {
                  // 关闭语义：隐藏 Job 窗口不影响作业表与日志内容。
                  if (!job_work_window_ || !job_table_ || !moose_panel_) {
                    throw std::runtime_error("I-04 job workspace fixture is missing");
                  }
                  const int job_rows = job_table_->rowCount();
                  const QString job_log = moose_panel_->log_text();
                  job_work_window_->hide();
                  qApp->processEvents();
                  if (job_table_->rowCount() != job_rows ||
                      moose_panel_->log_text() != job_log) {
                    throw std::runtime_error("I-04 job close semantics contract failed");
                  }
                  // 关闭语义：隐藏 Results 窗口不卸载舞台已加载数据。
                  if (!viewer_) {
                    throw std::runtime_error("I-04 viewer fixture is missing");
                  }
                  const QString mesh_path =
                      QDir::current().absoluteFilePath("out/box.msh");
                  if (QFileInfo::exists(mesh_path)) {
                    viewer_->set_mesh_file(mesh_path);
                    qApp->processEvents();
                  }
                  const bool stage_before = viewer_->stage_data_visible();
                  results_work_window_->hide();
                  qApp->processEvents();
                  if (viewer_->stage_data_visible() != stage_before) {
                    throw std::runtime_error("I-04 results close semantics contract failed");
                  }
                  // 单实例 + 越界恢复：四个独立工作窗同名唯一，
                  // 移出屏幕后通过真实模块入口激活必须回到可视区。
                  const QList<QPair<QString, QDockWidget*>> workspaces = {
                      {"meshWorkspaceWindow", mesh_work_window_},
                      {"jobWorkspaceWindow", job_work_window_},
                      {"visualizationWorkspaceWindow",
                       visualization_work_window_},
                      {"resultsWorkspaceWindow", results_work_window_}};
                  const QList<int> module_indices = {9, 10, 11, 12};
                  for (int i = 0; i < workspaces.size(); ++i) {
                    QDockWidget* workspace = workspaces.at(i).second;
                    if (!workspace) {
                      throw std::runtime_error("I-04 workspace fixture is missing");
                    }
                    if (findChildren<QDockWidget*>(workspaces.at(i).first)
                            .size() != 1) {
                      throw std::runtime_error("I-04 workspace single-instance contract failed");
                    }
                    workspace->move(-10000, -10000);
                    module_tabs_->setCurrentIndex(module_indices.at(i));
                    QMetaObject::invokeMethod(module_tabs_, "tabBarClicked",
                                              Qt::DirectConnection,
                                              Q_ARG(int, module_indices.at(i)));
                    qApp->processEvents();
                    QScreen* screen = workspace->screen();
                    if (!workspace->isVisible() || !screen ||
                        !screen->availableGeometry()
                             .adjusted(16, 16, -16, -16)
                             .contains(workspace->frameGeometry().topLeft())) {
                      throw std::runtime_error("I-04 workspace out-of-bounds recovery contract failed");
                    }
                  }
                  // Module Workspace 同样适用浮动守卫。
                  if (module_work_window_) {
                    module_work_window_->setFloating(false);
                    qApp->processEvents();
                    if (!module_work_window_->isFloating()) {
                      throw std::runtime_error("I-04 module workspace floating guard failed");
                    }
                  }
                },
                results_work_window_});
  steps.append({"i04_results_compare_windows",
                [this]() {
                  if (!results_work_window_ || !viewer_) {
                    throw std::runtime_error("I-04 compare fixture is missing");
                  }
                  results_work_window_->show();
                  results_work_window_->raise();
                  auto* new_compare =
                      results_work_window_->findChild<QPushButton*>(
                          "resultsNewCompareButton");
                  if (!new_compare) {
                    throw std::runtime_error("I-04 compare button fixture is missing");
                  }
                  const int base_count = results_compare_windows_.size();
                  new_compare->click();
                  new_compare->click();
                  qApp->processEvents();
                  if (results_compare_windows_.size() != base_count + 2) {
                    throw std::runtime_error("I-04 compare multi-instance contract failed");
                  }
                  QDockWidget* first = results_compare_windows_.at(base_count);
                  QDockWidget* second =
                      results_compare_windows_.at(base_count + 1);
                  if (!first || !second || !first->isVisible() ||
                      !second->isVisible() ||
                      first->windowTitle() == second->windowTitle() ||
                      !first->windowTitle().contains("Compare") ||
                      !second->windowTitle().contains("Compare")) {
                    throw std::runtime_error("I-04 compare title contract failed");
                  }
                  // 默认 Results 工作窗仍为单实例。
                  if (findChildren<QDockWidget*>("resultsWorkspaceWindow")
                          .size() != 1) {
                    throw std::runtime_error("I-04 results single-instance contract failed");
                  }
                  // 独立几何记忆：两个实例可拥有不同位置。
                  first->move(120, 120);
                  second->move(420, 220);
                  qApp->processEvents();
                  if (first->frameGeometry().topLeft() ==
                      second->frameGeometry().topLeft()) {
                    throw std::runtime_error("I-04 compare geometry memory contract failed");
                  }
                  // 关闭第一个对比窗：不影响主 Results 窗口、另一个对比窗和舞台。
                  const bool stage_before = viewer_->stage_data_visible();
                  first->hide();
                  qApp->processEvents();
                  if (!second->isVisible() ||
                      !results_work_window_->isVisible() ||
                      viewer_->stage_data_visible() != stage_before) {
                    throw std::runtime_error("I-04 compare close semantics contract failed");
                  }
                  // 关闭后写入按实例编号的独立几何键。
                  QSettings settings("gmp-ise", "gmp_ise");
                  if (!settings.contains(
                          first->property("gmpGeometryKey").toString())) {
                    throw std::runtime_error("I-04 compare geometry persistence contract failed");
                  }
                  second->raise();
                },
                results_work_window_});
#ifdef GMP_ENABLE_GMSH_GUI
  steps.append({"i04_geo_import_feedback",
                [this, dir]() {
                  if (!gmsh_panel_) {
                    throw std::runtime_error("I-04 geo import fixture is missing");
                  }
                  // 坏脚本：OCC 内核下混入内置几何命令，导入必须显性失败，
                  // 且不得保留“已加载”的假象（路径/摘要如实清空）。
                  const QString broken = dir + "/tour_broken.geo";
                  {
                    QFile f(broken);
                    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                      throw std::runtime_error("I-04 geo fixture write failed");
                    }
                    f.write("SetFactory(\"OpenCASCADE\");\n"
                            "Rectangle(1) = {0, 0, 0, 1, 1};\n"
                            "Split Curve {1} Point {};\n");
                    f.close();
                  }
                  if (gmsh_panel_->import_geometry(broken, false) ||
                      gmsh_panel_->last_import_error().isEmpty()) {
                    throw std::runtime_error("I-04 broken geo import feedback contract failed");
                  }
                  // 退化几何：xy 平面矩形直接绕 z 轴旋转产生零体积“体”，
                  // 必须在导入阶段被拦截并给出可读原因。
                  const QString degenerate = dir + "/tour_degenerate.geo";
                  {
                    QFile f(degenerate);
                    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                      throw std::runtime_error("I-04 geo fixture write failed");
                    }
                    f.write("SetFactory(\"OpenCASCADE\");\n"
                            "Rectangle(1) = {1.5, 0, -0.5, 1, 1};\n"
                            "out[] = Extrude{{0, 0, 1}, {0, 0, 0}, 2*Pi} { Surface{1}; };\n");
                    f.close();
                  }
                  if (gmsh_panel_->import_geometry(degenerate, false) ||
                      !gmsh_panel_->last_import_error().contains(
                          "Degenerate", Qt::CaseInsensitive)) {
                    throw std::runtime_error("I-04 degenerate geo import contract failed");
                  }
                  // 合法脚本：导入成功并清除错误状态。
                  const QString valid = dir + "/tour_valid.geo";
                  {
                    QFile f(valid);
                    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                      throw std::runtime_error("I-04 geo fixture write failed");
                    }
                    f.write("SetFactory(\"OpenCASCADE\");\n"
                            "Box(1) = {0, 0, 0, 1, 1, 1};\n"
                            "Physical Volume(\"solid\") = {1};\n");
                    f.close();
                  }
                  if (!gmsh_panel_->import_geometry(valid, true) ||
                      !gmsh_panel_->last_import_error().isEmpty()) {
                    throw std::runtime_error("I-04 valid geo import contract failed");
                  }
                  // 自动剖分联动：导入成功后生成的网格必须真实载入舞台，
                  // 不得出现“已生成但舞台为空”。
                  if (!viewer_ || !viewer_->has_stage_data() ||
                      !viewer_->stage_data_visible()) {
                    throw std::runtime_error("I-04 geo import stage linkage contract failed");
                  }
                  if (mesh_work_window_) {
                    mesh_work_window_->show();
                    mesh_work_window_->raise();
                  }
                },
                mesh_work_window_});
#endif
  steps.append({"operation_log_smoke",
                [this]() {
                  // 操作日志链路：埋点写入后文件必须存在且包含对应条目。
                  const QString marker =
                      QString("tour smoke marker %1").arg(QDateTime::currentMSecsSinceEpoch());
                  gmp::log_operation("tour", marker);
                  const QString path = gmp::operation_log_path();
                  QFile f(path);
                  if (path.isEmpty() ||
                      !f.open(QIODevice::ReadOnly | QIODevice::Text) ||
                      !QString::fromUtf8(f.readAll()).contains(marker)) {
                    throw std::runtime_error("Operation log contract failed");
                  }
                },
                this});
  steps.append({"remote_job_registration",
                [this]() {
                  // 远程（LIMS）作业事件必须登记到 Jobs 树与作业列表，
                  // 状态刷新必须更新同一行而不是新建节点。
                  if (!moose_panel_ || !job_table_) {
                    throw std::runtime_error("Remote job fixture is missing");
                  }
                  auto* root = find_root_item("Jobs");
                  if (!root) {
                    throw std::runtime_error("Jobs root fixture is missing");
                  }
                  const int base_children = root->childCount();
                  const QString job_id = "tour_remote_job_1";
                  QVariantMap submit;
                  submit.insert("event", "submitted");
                  submit.insert("job_id", job_id);
                  submit.insert("state", "queued");
                  submit.insert("server", "http://127.0.0.1:8200");
                  submit.insert("snapshot", "/tmp/tour_snapshot");
                  submit.insert("submit_time", "2026-09-06T18:02:41");
                  emit moose_panel_->remote_job_event(submit);
                  auto count_named = [&root](const QString& name) {
                    int count = 0;
                    for (int i = 0; i < root->childCount(); ++i) {
                      if (root->child(i)->text(0) == name) {
                        ++count;
                      }
                    }
                    return count;
                  };
                  bool table_has = false;
                  for (int row = 0; row < job_table_->rowCount(); ++row) {
                    auto* cell = job_table_->item(row, 0);
                    if (cell && cell->text() == job_id) {
                      table_has = true;
                      break;
                    }
                  }
                  if (root->childCount() != base_children + 1 ||
                      count_named(job_id) != 1 || !table_has) {
                    throw std::runtime_error("Remote job registration contract failed");
                  }
                  QVariantMap status;
                  status.insert("event", "status");
                  status.insert("job_id", job_id);
                  status.insert("state", "running");
                  status.insert("progress", "percent=4.9, step=31");
                  emit moose_panel_->remote_job_event(status);
                  if (root->childCount() != base_children + 1 ||
                      count_named(job_id) != 1) {
                    throw std::runtime_error("Remote job status update duplicated the entry");
                  }
                  QTreeWidgetItem* item = nullptr;
                  for (int i = 0; i < root->childCount(); ++i) {
                    if (root->child(i)->text(0) == job_id) {
                      item = root->child(i);
                      break;
                    }
                  }
                  const QVariantMap params =
                      item ? item->data(0, PropertyEditor::kParamsRole).toMap()
                           : QVariantMap();
                  if (!item ||
                      params.value("status").toString() != "Running" ||
                      params.value("progress").toString() !=
                          "percent=4.9, step=31" ||
                      params.value("exec").toString() !=
                          "remote: http://127.0.0.1:8200") {
                    throw std::runtime_error("Remote job status merge contract failed");
                  }
                  // LIMS 列表状态词汇映射：succeeded 必须映射为 Completed。
                  QVariantMap listed;
                  listed.insert("event", "status");
                  listed.insert("job_id", job_id);
                  listed.insert("state", "succeeded");
                  listed.insert("case_name", "tpl-demo");
                  listed.insert("started_at", "2026-09-06T18:02:41");
                  listed.insert("finished_at", "2026-09-06T18:03:21");
                  emit moose_panel_->remote_job_event(listed);
                  const QVariantMap merged =
                      item->data(0, PropertyEditor::kParamsRole).toMap();
                  if (merged.value("status").toString() != "Completed" ||
                      merged.value("duration").toString() != "40s" ||
                      merged.value("case").toString() != "tpl-demo") {
                    throw std::runtime_error("Remote job list state mapping contract failed");
                  }
                },
                job_work_window_});
  steps.append({"remote_job_monitor",
                [this]() {
                  // 作业监控页：注册运行中远程作业并选中，注入合成的
                  // 执行状态与制品清单，断言进度/详情/制品/筛选。
                  if (!moose_panel_ || !job_table_ || !job_state_filter_ ||
                      !job_progress_bar_ || !job_progress_text_ ||
                      !job_files_table_ || !job_detail_stack_) {
                    throw std::runtime_error("Job monitor fixture is missing");
                  }
                  auto* job_tabs = job_work_window_
                                       ? job_work_window_->findChild<QTabWidget*>(
                                             "jobWorkspaceTabs")
                                       : nullptr;
                  if (job_tabs) {
                    job_tabs->setCurrentIndex(0);
                  }
                  auto* root = find_root_item("Jobs");
                  if (!root) {
                    throw std::runtime_error("Jobs root fixture is missing");
                  }
                  const QString job_id = "tour_remote_job_2";
                  QVariantMap submit;
                  submit.insert("event", "submitted");
                  submit.insert("job_id", job_id);
                  submit.insert("state", "running");
                  submit.insert("server", "http://127.0.0.1:8200");
                  submit.insert("case_name", "tour-case");
                  emit moose_panel_->remote_job_event(submit);
                  int target_row = -1;
                  for (int row = 0; row < job_table_->rowCount(); ++row) {
                    auto* cell = job_table_->item(row, 0);
                    if (cell && cell->text() == job_id) {
                      target_row = row;
                      break;
                    }
                  }
                  if (target_row < 0) {
                    throw std::runtime_error("Job monitor selection fixture failed");
                  }
                  job_table_->setCurrentCell(target_row, 0);
                  if (selected_job_id_ != job_id || !selected_job_remote_ ||
                      !selected_job_running_ ||
                      job_detail_stack_->currentIndex() != 1 ||
                      (job_cancel_button_ &&
                       !job_cancel_button_->isEnabled())) {
                    throw std::runtime_error("Job monitor selection contract failed");
                  }
                  QVariantMap status;
                  status.insert("job_id", job_id);
                  status.insert("input_file", "tpl-demo.i");
                  status.insert("pid", 90406);
                  QVariantMap resources;
                  resources.insert("cpu_percent", 398.3);
                  resources.insert("memory_mb", 1060.0);
                  resources.insert("process_count", 4);
                  resources.insert("logical_cores_used", 4);
                  status.insert("resources", resources);
                  QVariantMap progress;
                  progress.insert("step_current", 398);
                  progress.insert("percent", 55.3);
                  progress.insert("time_current", 0.553326);
                  progress.insert("time_total", 1.0);
                  progress.insert("current_dt", 0.00177897);
                  progress.insert("adaptive_dt", true);
                  status.insert("progress", progress);
                  QVariantMap timings;
                  timings.insert("elapsed_human", "31m 0s");
                  timings.insert("avg_step_seconds", 4.7);
                  status.insert("timings", timings);
                  QVariantMap convergence;
                  convergence.insert("converged_count", 397);
                  status.insert("convergence", convergence);
                  status.insert("health", "healthy");
                  emit moose_panel_->remote_execution_status(status);
                  auto field_text = [this](const QString& key) {
                    QLabel* label = job_detail_fields_.value(key, nullptr);
                    return label ? label->text() : QString();
                  };
                  if (job_progress_bar_->value() != 55 ||
                      !job_progress_text_->text().contains("398") ||
                      !job_progress_text_->text().contains("55.3%") ||
                      field_text("cpu") != "398.3%" ||
                      field_text("parallel") != "4 MPI ranks (4 cores)" ||
                      field_text("step") != "398" ||
                      field_text("converged") != "397" ||
                      field_text("elapsed") != "31m 0s" ||
                      field_text("health") != "healthy") {
                    throw std::runtime_error("Job monitor detail contract failed");
                  }
                  QVariantMap body;
                  body.insert("job_id", job_id);
                  QVariantMap f1;
                  f1.insert("path", "task.md");
                  f1.insert("name", "task.md");
                  f1.insert("size", 60928);
                  f1.insert("kind", "task");
                  f1.insert("snapshot", true);
                  f1.insert("modified_at", "2026-09-06 18:34:11");
                  QVariantMap f2;
                  f2.insert("path", "output/result.e");
                  f2.insert("name", "result.e");
                  f2.insert("size", 115000);
                  f2.insert("kind", "exodus");
                  f2.insert("snapshot", false);
                  body.insert("files", QVariantList{f1, f2});
                  emit moose_panel_->remote_files(body);
                  auto* kind_cell = job_files_table_->item(1, 0);
                  auto* name_cell = job_files_table_->item(1, 1);
                  if (job_files_table_->rowCount() != 2 || !kind_cell ||
                      kind_cell->text() != "exodus" || !name_cell ||
                      name_cell->data(Qt::UserRole).toString() !=
                          "output/result.e" ||
                      job_files_table_->item(0, 4)->text() != "yes") {
                    throw std::runtime_error("Job monitor artifacts contract failed");
                  }
                  // 列表刷新（手动/自动）不得丢失选中与右侧详情。
                  refresh_job_table();
                  if (selected_job_id_ != job_id ||
                      job_detail_stack_->currentIndex() != 1 ||
                      job_table_->currentRow() < 0) {
                    throw std::runtime_error("Job monitor refresh selection contract failed");
                  }
                  // 状态筛选：Running 含该作业，Failed 不含。
                  auto has_job = [this, job_id]() {
                    for (int row = 0; row < job_table_->rowCount(); ++row) {
                      auto* cell = job_table_->item(row, 0);
                      if (cell && cell->text() == job_id) {
                        return true;
                      }
                    }
                    return false;
                  };
                  job_state_filter_->setCurrentIndex(2);  // Running
                  if (!has_job()) {
                    throw std::runtime_error("Job monitor filter (Running) failed");
                  }
                  job_state_filter_->setCurrentIndex(4);  // Failed
                  if (has_job()) {
                    throw std::runtime_error("Job monitor filter (Failed) failed");
                  }
                  job_state_filter_->setCurrentIndex(0);  // All
                  if (!has_job()) {
                    throw std::runtime_error("Job monitor filter (All) failed");
                  }
                  // 重新选中，让截图呈现完整详情面板。
                  for (int row = 0; row < job_table_->rowCount(); ++row) {
                    auto* cell = job_table_->item(row, 0);
                    if (cell && cell->text() == job_id) {
                      job_table_->setCurrentCell(row, 0);
                      break;
                    }
                  }
                },
                job_work_window_});
  steps.append({"results_import_file",
                [this]() {
                  // 导入外部结果文件：注册 Results 节点、出现在结果列表、
                  // 网格/Exodus 同步载入舞台。
                  auto* root = find_root_item("Results");
                  if (!root || !results_list_ || !viewer_) {
                    throw std::runtime_error("Results import fixture is missing");
                  }
                  const QString mesh_path =
                      QDir::current().absoluteFilePath("out/box.msh");
                  if (!QFileInfo::exists(mesh_path)) {
                    throw std::runtime_error("Results import mesh fixture is missing");
                  }
                  import_result_file(mesh_path);
                  bool in_tree = false;
                  for (int i = 0; i < root->childCount(); ++i) {
                    const QVariantMap params =
                        root->child(i)
                            ->data(0, PropertyEditor::kParamsRole)
                            .toMap();
                    if (params.value("path").toString() == mesh_path) {
                      in_tree = true;
                      break;
                    }
                  }
                  bool in_list = false;
                  for (int i = 0; i < results_list_->count(); ++i) {
                    auto* row = results_list_->item(i);
                    if (row && row->data(Qt::UserRole).toString() ==
                                   mesh_path) {
                      in_list = true;
                      break;
                    }
                  }
                  if (!in_tree || !in_list ||
                      !viewer_->stage_data_visible()) {
                    throw std::runtime_error("Results import contract failed");
                  }
                },
                results_work_window_});
  steps.append({"results_navigation_context_menu",
                [this]() {
                  // 结果导航树右键菜单：根节点工作窗入口/根级操作，
                  // 子节点打开/复制路径/重命名/删除。
                  if (!results_navigation_tree_) {
                    throw std::runtime_error("Results navigation fixture is missing");
                  }
                  QTreeWidgetItem* jobs_root = nullptr;
                  QTreeWidgetItem* results_root = nullptr;
                  for (int i = 0;
                       i < results_navigation_tree_->topLevelItemCount();
                       ++i) {
                    auto* top = results_navigation_tree_->topLevelItem(i);
                    const QString kind =
                        top->data(0, kNavigationKindRole).toString();
                    if (kind == "Jobs") {
                      jobs_root = top;
                    } else if (kind == "Results") {
                      results_root = top;
                    }
                  }
                  if (!jobs_root || !results_root) {
                    throw std::runtime_error("Navigation roots are missing");
                  }
                  auto action_texts = [](QMenu& menu) {
                    QStringList texts;
                    for (auto* action : menu.actions()) {
                      texts << action->text();
                    }
                    return texts;
                  };
                  {
                    QMenu menu;
                    build_results_navigation_menu(&menu, jobs_root);
                    const QStringList texts = action_texts(menu);
                    if (!texts.contains("Open Job Workspace") ||
                        !texts.contains("Refresh Remote Jobs") ||
                        !texts.contains("Expand All")) {
                      throw std::runtime_error("Jobs root menu contract failed");
                    }
                  }
                  {
                    QMenu menu;
                    build_results_navigation_menu(&menu, results_root);
                    const QStringList texts = action_texts(menu);
                    if (!texts.contains("Open Results Workspace") ||
                        !texts.contains("Import Result File...") ||
                        !texts.contains("Collapse All")) {
                      throw std::runtime_error("Results root menu contract failed");
                    }
                  }
                  if (results_root->childCount() == 0) {
                    throw std::runtime_error("Results child fixture is missing");
                  }
                  {
                    QMenu menu;
                    build_results_navigation_menu(&menu,
                                                  results_root->child(0));
                    const QStringList texts = action_texts(menu);
                    if (!texts.contains("Open in Viewer") ||
                        !texts.contains("Copy Path") ||
                        !texts.contains("Rename") ||
                        !texts.contains("Remove")) {
                      throw std::runtime_error("Results child menu contract failed");
                    }
                  }
                  if (jobs_root->childCount() > 0) {
                    QMenu menu;
                    build_results_navigation_menu(&menu, jobs_root->child(0));
                    const QStringList texts = action_texts(menu);
                    if (!texts.contains("Open Job Workspace") ||
                        !texts.contains("Rename") ||
                        !texts.contains("Remove")) {
                      throw std::runtime_error("Jobs child menu contract failed");
                    }
                  }
                },
                this});
  steps.append({"v01_visual_contracts",
                [this]() {
                  // V-01 视觉合同：舞台左栏与顶部工具组图标按钮
                  // 尺寸/热区/Tooltip；禁止字母占位。
                  if (!stage_left_toolbar_) {
                    throw std::runtime_error("V-01 stage toolbar fixture is missing");
                  }
                  const auto stage_buttons =
                      stage_left_toolbar_->findChildren<QToolButton*>();
                  if (stage_buttons.size() < 4) {
                    throw std::runtime_error("V-01 stage toolbar buttons are missing");
                  }
                  for (auto* button : stage_buttons) {
                    const QSize size = button->size();
                    const QSize icon = button->iconSize();
                    if (size.width() < 28 || size.height() < 28 ||
                        icon.width() < 16 || icon.width() > 20 ||
                        button->toolTip().trimmed().isEmpty()) {
                      throw std::runtime_error("V-01 stage toolbar icon contract failed");
                    }
                  }
                  const QStringList groups = {"projectToolGroup",
                                              "editToolGroup",
                                              "modelToolGroup",
                                              "meshToolGroup",
                                              "jobToolGroup"};
                  int group_buttons = 0;
                  for (const QString& name : groups) {
                    auto* group = findChild<QToolBar*>(name);
                    if (!group) {
                      throw std::runtime_error("V-01 tool group is missing");
                    }
                    const QSize icon = group->iconSize();
                    if (icon.width() < 16 || icon.width() > 20) {
                      throw std::runtime_error("V-01 tool group icon size contract failed");
                    }
                    for (auto* button :
                         group->findChildren<QToolButton*>()) {
                      // 跳过工具栏溢出扩展按钮（Qt 内建，无业务 Tooltip）。
                      if (button->objectName() == "qt_toolbar_ext_button") {
                        continue;
                      }
                      ++group_buttons;
                      // 图标按钮必须带 Tooltip（功能名/快捷键）；
                      // 不得退化为单字母占位。
                      const QString text = button->text().trimmed();
                      if (button->toolTip().trimmed().isEmpty() ||
                          (button->icon().isNull() && text.size() <= 2)) {
                        throw std::runtime_error(
                            QString("V-01 tool button contract failed: "
                                    "group=%1 text='%2' tooltip='%3' "
                                    "objectName=%4 hasIcon=%5")
                                .arg(name, text, button->toolTip(),
                                     button->objectName())
                                .arg(!button->icon().isNull())
                                .toStdString());
                      }
                    }
                  }
                  if (group_buttons < 5) {
                    throw std::runtime_error("V-01 tool group buttons are missing");
                  }
                },
                stage_left_toolbar_});
  steps.append({"v02_l10n_round_trip",
                [this]() {
                  // V-02 国际化：中/英往返切换，既有菜单与本轮新增字符串
                  // 都必须完整跟随，且不破坏数据内容。
                  auto* file_menu = findChild<QMenu*>("fileMenu");
                  auto* import_btn =
                      findChild<QPushButton*>("resultsImportFile");
                  if (!file_menu || !import_btn || !job_state_filter_) {
                    throw std::runtime_error("V-02 l10n fixture is missing");
                  }
                  l10n::set_language(l10n::Language::Chinese);
                  l10n::apply(this);
                  if (!file_menu->title().contains("文件") ||
                      import_btn->text() != "导入结果文件..." ||
                      job_state_filter_->itemText(1) != "排队中" ||
                      job_state_filter_->itemText(2) != "运行中") {
                    throw std::runtime_error("V-02 zh translation contract failed");
                  }
                  l10n::set_language(l10n::Language::English);
                  l10n::apply(this);
                  if (!file_menu->title().contains("File") ||
                      import_btn->text() != "Import Result File..." ||
                      job_state_filter_->itemText(1) != "Queued" ||
                      job_state_filter_->itemText(2) != "Running") {
                    throw std::runtime_error("V-02 en translation contract failed");
                  }
                  l10n::set_language(l10n::Language::Chinese);
                  l10n::apply(this);
                  if (!file_menu->title().contains("文件") ||
                      import_btn->text() != "导入结果文件...") {
                    throw std::runtime_error("V-02 restore translation contract failed");
                  }
                },
                this});
  steps.append({"v03_layout_reset",
                [this]() {
                  // V-03 恢复默认布局：工具组、左栏、底部区与工作窗
                  // 全部回到预置状态。
                  if (!main_split_ || !vertical_split_ ||
                      !action_reset_tool_layout_ || !job_work_window_) {
                    throw std::runtime_error("V-03 layout fixture is missing");
                  }
                  main_split_->setSizes({600, 300});
                  vertical_split_->setSizes({100, 900});
                  job_work_window_->show();
                  action_reset_tool_layout_->trigger();
                  qApp->processEvents();
                  const QList<int> main_sizes = main_split_->sizes();
                  const QList<int> vert_sizes = vertical_split_->sizes();
                  const int left_w = qBound(250, int(width() * 0.22), 340);
                  if (main_sizes.size() != 2 ||
                      qAbs(main_sizes.at(0) - left_w) > 8 ||
                      vert_sizes.size() != 2 ||
                      vert_sizes.at(0) <= vert_sizes.at(1) * 2 ||
                      job_work_window_->isVisible()) {
                    throw std::runtime_error("V-03 layout reset contract failed");
                  }
                  const QStringList groups = {"projectToolGroup",
                                              "editToolGroup",
                                              "modelToolGroup",
                                              "meshToolGroup",
                                              "jobToolGroup"};
                  for (const QString& name : groups) {
                    auto* toolbar = findChild<QToolBar*>(name);
                    if (!toolbar || !toolbar->isVisible() ||
                        toolbar->orientation() != Qt::Horizontal ||
                        toolbar->isFloating()) {
                      throw std::runtime_error("V-03 toolbar reset contract failed");
                    }
                  }
                },
                this});
  steps.append({"console_log_forwarding",
                [this]() {
                  // 弹窗精简合同：Gmsh/MOOSE 日志镜像到主 Console；
                  // Results 预览默认折叠且可切换。
                  if (!console_ || !results_preview_) {
                    throw std::runtime_error("Console forwarding fixture is missing");
                  }
                  if (!console_->toPlainText().contains("[op] gmsh |")) {
                    throw std::runtime_error("Gmsh log forwarding contract failed");
                  }
                  auto* toggle =
                      findChild<QPushButton*>("resultsPreviewToggle");
                  if (!toggle) {
                    throw std::runtime_error("Results preview toggle is missing");
                  }
                  // 用 isHidden() 判定显隐意图，不受祖先窗口可见性影响。
                  if (!results_preview_->isHidden()) {
                    throw std::runtime_error("Results preview should be collapsed by default");
                  }
                  toggle->click();
                  if (results_preview_->isHidden()) {
                    throw std::runtime_error("Results preview expand contract failed");
                  }
                  toggle->click();
                  if (!results_preview_->isHidden()) {
                    throw std::runtime_error("Results preview collapse contract failed");
                  }
                },
                results_work_window_});
  steps.append({"combo_wheel_block",
                [this]() {
                  // 下拉框不响应滚轮：只能通过展开下拉点选切换。
                  if (!job_state_filter_) {
                    throw std::runtime_error("Combo wheel fixture is missing");
                  }
                  const int before = job_state_filter_->currentIndex();
                  QWheelEvent wheel(
                      job_state_filter_->rect().center(),
                      job_state_filter_->mapToGlobal(
                          job_state_filter_->rect().center()),
                      QPoint(0, 0), QPoint(0, -120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
                  QApplication::sendEvent(job_state_filter_, &wheel);
                  if (job_state_filter_->currentIndex() != before) {
                    throw std::runtime_error("Combo wheel block contract failed");
                  }
                  // 数值微调框（QDoubleSpinBox）同样不响应滚轮。
                  auto* spin = gmsh_panel_
                                   ? gmsh_panel_->findChild<QDoubleSpinBox*>()
                                   : nullptr;
                  if (!spin) {
                    throw std::runtime_error("Spin wheel fixture is missing");
                  }
                  const double value_before = spin->value();
                  QWheelEvent spin_wheel(
                      spin->rect().center(),
                      spin->mapToGlobal(spin->rect().center()),
                      QPoint(0, 0), QPoint(0, -120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
                  QApplication::sendEvent(spin, &spin_wheel);
                  if (spin->value() != value_before) {
                    throw std::runtime_error("Spin wheel block contract failed");
                  }
                },
                this});
  steps.append({"main_window_maximize_expands",
                [this]() {
                  // 主窗口最大化后中央区域必须充满：小窗 → 最大化，
                  // 中央控件尺寸应跟随窗口。
                  resize(800, 600);
                  qApp->processEvents();
                  showMaximized();
                  qApp->processEvents();
                  QTimer::singleShot(600, this, [this]() {
                    qApp->processEvents();
                    const QSize cs = centralWidget()->size();
                    const QSize ws = size();
                    qInfo("[tour] maximize check: window=%dx%d central=%dx%d",
                          ws.width(), ws.height(), cs.width(), cs.height());
                    if (cs.width() < ws.width() - 40 ||
                        cs.height() < ws.height() - 120) {
                      qCritical("[tour] FAILED: central does not expand after maximize");
                      QApplication::exit(2);
                      return;
                    }
                    qInfo("[tour] maximize check OK");
                    QApplication::quit();
                  });
                },
                nullptr});
  steps.append({"sketch_nested_loop_hole_extrude",
                [this]() {
                  // 嵌套环拉伸成孔：矩形+圆（对照）与手画多边形+圆
                  // （历史缺陷：线段端点乱序导致内圆不中空）都必须
                  // 拉伸为单个体（外轮廓带内孔），而不是两个独立体。
                  auto make_doc = [](bool concave) {
                    SketchDocument doc;
                    auto add_line = [&doc](double x1, double y1, double x2,
                                           double y2) {
                      SketchEntity e;
                      e.type = SketchEntityType::Line;
                      e.p1 = {x1, y1};
                      e.p2 = {x2, y2};
                      doc.add_entity(e);
                    };
                    if (concave) {
                      // 模拟手画凹多边形：各线段方向故意不一致。
                      add_line(0, 0, 2, 0);
                      add_line(4, 1, 2, 0);
                      add_line(4, 1, 3, 2);
                      add_line(1, 3, 3, 2);
                      add_line(0, 0, 1, 3);
                    } else {
                      add_line(0, 0, 4, 0);
                      add_line(4, 0, 4, 2);
                      add_line(4, 2, 0, 2);
                      add_line(0, 2, 0, 0);
                    }
                    SketchEntity circle;
                    circle.type = SketchEntityType::Circle;
                    circle.center = {2.2, 1.2};
                    circle.radius = 0.4;
                    doc.add_entity(circle);
                    return doc;
                  };
                  for (const bool concave : {false, true}) {
                    SketchDocument doc = make_doc(concave);
                    const FeatureResult result =
                        extrude_sketch(doc, 1.0);
                    if (!result.ok ||
                        result.gmsh_volume_tags.size() != 1) {
                      throw std::runtime_error(
                          concave
                              ? "Concave polygon with inner circle must extrude to one holed solid"
                              : "Rectangle with inner circle must extrude to one holed solid");
                    }
                  }
                },
                this});
  // 该步骤自带退出逻辑，放入独立执行路径
  if (qEnvironmentVariableIsSet("GMP_TOUR_MAXIMIZE_ONLY")) {
    decltype(steps) only;
    for (const auto& st : steps) {
      if (st.name == "main_window_maximize_expands") {
        only.append(st);
      }
    }
    steps = only;
  }
  if (qEnvironmentVariableIsSet("GMP_TOUR_TOOLBAR_SCENARIOS")) {
    // 两个工具栏场景的独立诊断（真实鼠标事件模拟拖动）：
    // S1 显示组拖出 → 恢复默认 → 应回顶部；S2 拖动工作窗 → 工具条不空白。
    decltype(steps) scenarios;
    auto post_mouse = [](QWidget* w, QEvent::Type type, const QPoint& global,
                         Qt::MouseButton button = Qt::LeftButton) {
      QMouseEvent ev(type, w->mapFromGlobal(global),
                     w->mapToGlobal(w->mapFromGlobal(global)), button,
                     type == QEvent::MouseMove ? Qt::LeftButton
                                               : Qt::NoButton,
                     Qt::NoModifier);
      QApplication::sendEvent(w, &ev);
    };
    scenarios.append({"s1_display_group_drag_reset",
                      [this, post_mouse, dir]() {
                        if (!display_tool_group_) {
                          throw std::runtime_error("S1 fixture missing");
                        }
                        auto* tb = display_tool_group_;
                        auto geo_probe = [this](const char* tag) {
                          auto* g = findChild<QToolBar*>("projectToolGroup");
                          auto* d = display_tool_group_;
                          auto* mb = findChild<QWidget*>("moduleBar");
                          qInfo("[S1probe] %s project gy=%d h=%d display gy=%d h=%d moduleBar gy=%d",
                                tag,
                                g ? g->mapToGlobal(QPoint(0, 0)).y() : -1,
                                g ? g->height() : -1,
                                d ? d->mapToGlobal(QPoint(0, 0)).y() : -1,
                                d ? d->height() : -1,
                                mb ? mb->mapToGlobal(QPoint(0, 0)).y() : -1);
                        };
                        geo_probe("startup");
                        grab().save(dir + "/s1_startup.png");
                        // 真实拖出：按住工具条空白处 → 移出主窗口 → 释放。
                        const QPoint grip =
                            tb->mapToGlobal(QPoint(6, tb->height() / 2));
                        post_mouse(tb, QEvent::MouseButtonPress, grip);
                        qApp->processEvents();
                        for (int i = 1; i <= 8; ++i) {
                          post_mouse(tb, QEvent::MouseMove,
                                     grip + QPoint(i * 40, i * 30));
                          qApp->processEvents();
                        }
                        post_mouse(tb, QEvent::MouseButtonRelease,
                                   grip + QPoint(320, 240));
                        qApp->processEvents();
                        qInfo("[S1] after drag-out: floating=%d area=%d visible=%d",
                              tb->isFloating(), int(toolBarArea(tb)),
                              tb->isVisible());
                        geo_probe("after-dragout");
                        grab().save(dir + "/s1_after_dragout.png");
                        reset_tool_group_layout(false);
                        qApp->processEvents();
                        QTimer::singleShot(300, this, [this, dir]() {
                          qApp->processEvents();
                          // 复位可能已重建工具组（旧对象已 deleteLater），
                          // 必须使用成员指针取当前实例。
                          auto* tb = display_tool_group_;
                          qInfo("[S1] after reset: floating=%d area=%d visible=%d",
                                tb->isFloating(), int(toolBarArea(tb)),
                                tb->isVisible());
                          const QStringList gnames = {
                              "projectToolGroup", "editToolGroup",
                              "modelToolGroup", "meshToolGroup",
                              "jobToolGroup", "displayToolGroup"};
                          for (const QString& n : gnames) {
                            auto* g = findChild<QToolBar*>(n);
                            if (g) {
                              qInfo("[S1geo] %s visible=%d pos=(%d,%d) size=(%d,%d) floating=%d parent=(%d,%d,%d,%d)",
                                    qPrintable(n), g->isVisible(), g->x(),
                                    g->y(), g->width(), g->height(),
                                    g->isFloating(),
                                    g->parentWidget() ? g->parentWidget()->x() : -1,
                                    g->parentWidget() ? g->parentWidget()->y() : -1,
                                    g->parentWidget() ? g->parentWidget()->width() : -1,
                                    g->parentWidget() ? g->parentWidget()->height() : -1);
                            }
                          }
                          grab().save(dir + "/s1_after_reset.png");
                          if (tb->isFloating() || !tb->isVisible() ||
                              toolBarArea(tb) != Qt::TopToolBarArea) {
                            qCritical("[S1] FAILED: display group not docked back");
                            QApplication::exit(2);
                            return;
                          }
                          qInfo("[S1] OK");
                          QApplication::quit();
                        });
                      },
                      nullptr});
    scenarios.append({"s2_workspace_drag_toolbar",
                      [this, post_mouse, dir]() {
                        if (!job_work_window_) {
                          throw std::runtime_error("S2 fixture missing");
                        }
                        job_work_window_->show();
                        job_work_window_->raise();
                        qApp->processEvents();
                        // 把工作窗拖到工具条区域上方再移开。
                        QWidget* title = job_work_window_;
                        const QPoint start =
                            title->mapToGlobal(QPoint(title->width() / 2, 12));
                        post_mouse(title, QEvent::MouseButtonPress, start);
                        qApp->processEvents();
                        const QPoint over_toolbar =
                            mapToGlobal(QPoint(width() / 2, 60));
                        for (int i = 1; i <= 6; ++i) {
                          post_mouse(title, QEvent::MouseMove,
                                     start + (over_toolbar - start) * i / 6);
                          qApp->processEvents();
                        }
                        for (int i = 1; i <= 6; ++i) {
                          post_mouse(title, QEvent::MouseMove,
                                     over_toolbar + QPoint(i * 30, i * 40));
                          qApp->processEvents();
                        }
                        post_mouse(title, QEvent::MouseButtonRelease,
                                   over_toolbar + QPoint(180, 240));
                        qApp->processEvents();
                        QTimer::singleShot(400, this, [this, dir]() {
                          qApp->processEvents();
                          // 工具条区域逐个截图并检查非空白。
                          const QStringList names = {
                              "projectToolGroup", "editToolGroup",
                              "modelToolGroup", "meshToolGroup",
                              "jobToolGroup", "displayToolGroup"};
                          bool blank_found = false;
                          for (const QString& n : names) {
                            auto* tb = findChild<QToolBar*>(n);
                            if (!tb) {
                              continue;
                            }
                            qInfo("[S2] %s visible=%d floating=%d area=%d",
                                  qPrintable(n), tb->isVisible(),
                                  tb->isFloating(), int(toolBarArea(tb)));
                            const QImage img = tb->grab().toImage();
                            // 空白检测：整图近似单一颜色。
                            QRgb ref = img.pixel(2, 2);
                            bool uniform = true;
                            for (int x = 0; x < img.width() && uniform; x += 7) {
                              for (int y = 0; y < img.height(); y += 5) {
                                if (qAbs(qRed(img.pixel(x, y)) - qRed(ref)) > 12 ||
                                    qAbs(qGreen(img.pixel(x, y)) - qGreen(ref)) > 12 ||
                                    qAbs(qBlue(img.pixel(x, y)) - qBlue(ref)) > 12) {
                                  uniform = false;
                                  break;
                                }
                              }
                            }
                            qInfo("[S2] %s blank=%d", qPrintable(n), uniform);
                            blank_found = blank_found || uniform;
                          }
                          grab().save(dir + "/s2_after_drag.png");
                          if (blank_found) {
                            qCritical("[S2] FAILED: toolbar area blank after workspace drag");
                            QApplication::exit(2);
                            return;
                          }
                          qInfo("[S2] OK");
                          QApplication::quit();
                        });
                      },
                      nullptr});
    scenarios.append({"s6_drag_float_and_snap",
                      [this, post_mouse, dir]() {
                        // 拖拽浮出 + 磁吸停靠全链路。
                        auto* tb = display_tool_group_;
                        const QPoint grip = tb->mapToGlobal(
                            QPoint(tb->width() - 3, tb->height() / 2));
                        post_mouse(tb, QEvent::MouseButtonPress, grip);
                        qApp->processEvents();
                        for (int i = 1; i <= 6; ++i) {
                          post_mouse(tb, QEvent::MouseMove,
                                     grip + QPoint(i * 30, i * 25));
                          qApp->processEvents();
                        }
                        post_mouse(tb, QEvent::MouseButtonRelease,
                                   grip + QPoint(180, 150));
                        qApp->processEvents();
                        if (!tb->isFloating()) {
                          qCritical("[S6] FAILED: drag did not float the group");
                          QApplication::exit(2);
                          return;
                        }
                        grab().save(dir + "/s6_floated.png");
                        // 按住左键把浮动窗移回工具条行上方：应磁吸停靠。
                        post_mouse(tb, QEvent::MouseButtonPress,
                                   tb->mapToGlobal(QPoint(30, 10)));
                        qApp->processEvents();
                        auto* row = findChild<QToolBar*>("projectToolGroup");
                        tb->move(QPoint(row->frameGeometry().center().x() -
                                            tb->width() / 2,
                                        row->frameGeometry().top() + 4));
                        qApp->processEvents();
                        QTimer::singleShot(200, this, [this, tb, dir]() {
                          qApp->processEvents();
                          grab().save(dir + "/s6_snapped.png");
                          if (tb->isFloating() ||
                              toolBarArea(tb) != Qt::TopToolBarArea ||
                              !tb->isVisible()) {
                            qCritical("[S6] FAILED: magnetic snap did not dock");
                            QApplication::exit(2);
                            return;
                          }
                          qInfo("[S6] OK");
                          QApplication::quit();
                        });
                      },
                      nullptr});
    scenarios.append({"s5_controlled_group_float",
                      [this, dir]() {
                        // 受控浮动：菜单动作浮出两组 → 窗口可见浮动；
                        // 动作停回 → 顶部停靠；再浮出 → 复位 → 回顶部。
                        auto* menu = findChild<QMenu*>("floatToolGroupMenu");
                        auto* display = display_tool_group_;
                        auto* job = findChild<QToolBar*>("jobToolGroup");
                        if (!menu || !display || !job) {
                          throw std::runtime_error("S5 fixture missing");
                        }
                        toggle_group_float("displayToolGroup", true);
                        toggle_group_float("jobToolGroup", true);
                        qApp->processEvents();
                        if (!display_tool_group_->isFloating() ||
                            !findChild<QToolBar*>("jobToolGroup")
                                 ->isFloating() ||
                            !display_tool_group_->isVisible() ||
                            !findChild<QToolBar*>("jobToolGroup")
                                 ->isVisible()) {
                          throw std::runtime_error("S5 float contract failed");
                        }
                        grab().save(dir + "/s5_floating.png");
                        toggle_group_float("displayToolGroup", false);
                        toggle_group_float("jobToolGroup", false);
                        qApp->processEvents();
                        if (display_tool_group_->isFloating() ||
                            findChild<QToolBar*>("jobToolGroup")
                                ->isFloating()) {
                          throw std::runtime_error("S5 dock contract failed");
                        }
                        toggle_group_float("displayToolGroup", true);
                        toggle_group_float("jobToolGroup", true);
                        qApp->processEvents();
                        reset_tool_group_layout(false);
                        qApp->processEvents();
                        QTimer::singleShot(300, this, [this, dir]() {
                          qApp->processEvents();
                          grab().save(dir + "/s5_after_reset.png");
                          if (display_tool_group_->isFloating() ||
                              findChild<QToolBar*>("jobToolGroup")
                                  ->isFloating() ||
                              !display_tool_group_->isVisible() ||
                              !findChild<QToolBar*>("jobToolGroup")
                                   ->isVisible()) {
                            qCritical("[S5] FAILED: reset after controlled float");
                            QApplication::exit(2);
                            return;
                          }
                          qInfo("[S5] OK");
                          QApplication::quit();
                        });
                      },
                      nullptr});
    scenarios.append({"s4_two_groups_float_reset",
                      [this, post_mouse, dir]() {
                        // 用户现场：同时拖出显示组与作业组，再复位。
                        auto drag_out = [this, post_mouse](QToolBar* tb,
                                                           const QPoint& delta) {
                          // 工具条右端必为空白拖拽区（左侧可能命中按钮）。
                          const QPoint grip = tb->mapToGlobal(
                              QPoint(tb->width() - 3, tb->height() / 2));
                          post_mouse(tb, QEvent::MouseButtonPress, grip);
                          qApp->processEvents();
                          for (int i = 1; i <= 6; ++i) {
                            post_mouse(tb, QEvent::MouseMove,
                                       grip + delta * i / 6);
                            qApp->processEvents();
                          }
                          post_mouse(tb, QEvent::MouseButtonRelease,
                                     grip + delta);
                          qApp->processEvents();
                        };
                        drag_out(display_tool_group_, QPoint(320, 240));
                        drag_out(findChild<QToolBar*>("jobToolGroup"),
                                 QPoint(-280, 200));
                        qInfo("[S4] after drag-out: display floating=%d, job floating=%d",
                              display_tool_group_->isFloating(),
                              findChild<QToolBar*>("jobToolGroup")->isFloating());
                        reset_tool_group_layout(false);
                        qApp->processEvents();
                        QTimer::singleShot(500, this, [this, dir]() {
                          qApp->processEvents();
                          const QStringList names = {
                              "projectToolGroup", "editToolGroup",
                              "modelToolGroup", "meshToolGroup",
                              "jobToolGroup", "displayToolGroup"};
                          for (const QString& n : names) {
                            auto* g = findChild<QToolBar*>(n);
                            qInfo("[S4] %s visible=%d floating=%d area=%d pos=(%d,%d) size=(%d,%d)",
                                  qPrintable(n), g ? g->isVisible() : -1,
                                  g ? g->isFloating() : -1,
                                  g ? int(toolBarArea(g)) : -1,
                                  g ? g->x() : -1, g ? g->y() : -1,
                                  g ? g->width() : -1, g ? g->height() : -1);
                          }
                          auto* mb = findChild<QWidget*>("moduleBar");
                          qInfo("[S4] moduleBar gy=%d",
                                mb ? mb->mapToGlobal(QPoint(0, 0)).y() : -1);
                          grab().save(dir + "/s4_after_reset.png");
                          qInfo("[S4] done");
                          QApplication::quit();
                        });
                      },
                      nullptr});
    scenarios.append({"s3_drag_module_workspace_toolbar",
                      [this, post_mouse, dir]() {
                        // 用户现场：拖动浮动的部件工作窗经过顶部工具条区域。
                        module_work_window_->show();
                        module_work_window_->raise();
                        qApp->processEvents();
                        QWidget* win = module_work_window_;
                        const QPoint start =
                            win->mapToGlobal(QPoint(win->width() / 2, 12));
                        post_mouse(win, QEvent::MouseButtonPress, start);
                        qApp->processEvents();
                        const QPoint over_toolbar =
                            mapToGlobal(QPoint(width() / 2, 50));
                        for (int i = 1; i <= 6; ++i) {
                          post_mouse(win, QEvent::MouseMove,
                                     start + (over_toolbar - start) * i / 6);
                          qApp->processEvents();
                        }
                        // 在工具条区域停留几拍再移开。
                        for (int k = 0; k < 3; ++k) {
                          qApp->processEvents();
                        }
                        for (int i = 1; i <= 6; ++i) {
                          post_mouse(win, QEvent::MouseMove,
                                     over_toolbar + QPoint(i * 30, i * 40));
                          qApp->processEvents();
                        }
                        post_mouse(win, QEvent::MouseButtonRelease,
                                   over_toolbar + QPoint(180, 240));
                        qApp->processEvents();
                        reset_tool_group_layout(false);
                        qApp->processEvents();
                        QTimer::singleShot(400, this, [this, dir]() {
                          qApp->processEvents();
                          auto* mb = findChild<QWidget*>("moduleBar");
                          auto* g = findChild<QToolBar*>("projectToolGroup");
                          qInfo("[S3] moduleBar gy=%d, project gy=%d h=%d visible=%d",
                                mb ? mb->mapToGlobal(QPoint(0, 0)).y() : -1,
                                g ? g->mapToGlobal(QPoint(0, 0)).y() : -1,
                                g ? g->height() : -1,
                                g ? g->isVisible() : -1);
                          grab().save(dir + "/s3_after_reset.png");
                          qInfo("[S3] done");
                          QApplication::quit();
                        });
                      },
                      nullptr});
    steps = scenarios;
  }
  const QString step_filter =
      qEnvironmentVariable("GMP_TOUR_STEP_FILTER").trimmed();
  if (!step_filter.isEmpty()) {
    for (int index = steps.size() - 1; index >= 0; --index) {
      if (!steps.at(index).name.contains(step_filter, Qt::CaseInsensitive)) {
        steps.removeAt(index);
      }
    }
  }

  auto state = std::make_shared<int>(-1);
  auto* timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this,
          [this, timer, state, steps, dir]() mutable {
            ++(*state);
            if (*state >= steps.size()) {
              timer->stop();
              if (module_work_window_) {
                module_work_window_->hide();
              }
              if (mesh_work_window_) {
                mesh_work_window_->hide();
              }
              if (job_work_window_) {
                job_work_window_->hide();
              }
              if (visualization_work_window_) {
                visualization_work_window_->hide();
              }
              if (results_work_window_) {
                results_work_window_->hide();
              }
              QApplication::quit();
              return;
            }
            qInfo("[tour] step %d -> %s", *state,
                  qPrintable(steps[*state].name));
            try {
              steps[*state].activate();
            } catch (const std::exception& error) {
              timer->stop();
              qCritical("[tour] FAILED at step %d (%s): %s", *state,
                        qPrintable(steps[*state].name), error.what());
              statusBar()->showMessage(
                  QString("Screenshot tour failed at %1: %2")
                      .arg(steps[*state].name, error.what()),
                  0);
              QApplication::exit(2);
              return;
            } catch (...) {
              timer->stop();
              qCritical("[tour] FAILED at step %d (%s): unknown exception",
                        *state, qPrintable(steps[*state].name));
              QApplication::exit(2);
              return;
            }
            const QString file = dir + "/" + steps[*state].name + ".png";
            QWidget* capture = steps[*state].capture ? steps[*state].capture : this;
            QTimer::singleShot(500, this, [capture, file]() {
              const bool ok = capture && capture->grab().save(file);
              qInfo("[tour] saved %s ok=%d", qPrintable(file), ok);
            });
          });
  timer->start(1500);
}

}  // namespace gmp
