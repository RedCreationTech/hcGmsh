#include "gmp/MainWindow.h"
#include "gmp/L10n.h"

#include <QFileDialog>
#include <QFile>
#include <QDir>
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
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QVariantMap>
#include <QMetaType>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QLabel>
#include <QTextStream>
#include <QDateTime>
#include <QTimer>
#include <memory>
#include <functional>
#include <vector>

#include <fstream>
#include <QFileInfo>
#include <yaml-cpp/yaml.h>

#include "gmp/GmshPanel.h"
#include "gmp/FloatingPropertyForm.h"
#include "gmp/MoosePanel.h"
#include "gmp/OccBridge.h"
#include "gmp/PartFeaturePanel.h"
#include "gmp/PropertyEditor.h"
#include "gmp/ProjectSchema.h"
#include "gmp/SketchDocument.h"
#include "gmp/StageLeftToolbar.h"
#include "gmp/SketchPanel.h"
#include "gmp/VtkViewer.h"

namespace gmp {

namespace {

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
    list->setMinimumHeight(180);
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
    connect(rename_btn, &QPushButton::clicked, this, [resolve_selected_item]() {
      if (auto* target = resolve_selected_item()) {
        target->setFlags(target->flags() | Qt::ItemIsEditable);
        if (auto* itemView = target->treeWidget()) {
          itemView->editItem(target, 0);
        }
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
  // 左栏包滚动区: 与中/右栏一致, 允许分割条拖到很窄, 内容滚动承载
  auto* tree_scroll = new QScrollArea(tree_panel);
  tree_scroll->setWidgetResizable(true);
  tree_scroll->setFrameShape(QFrame::NoFrame);
  tree_scroll->setMinimumWidth(0);
  tree_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  tree_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  tree_outer->addWidget(tree_scroll);
  auto* tree_content = new QWidget();
  tree_scroll->setWidget(tree_content);
  auto* tree_layout = new QVBoxLayout(tree_content);
  tree_layout->setContentsMargins(0, 0, 0, 0);
  tree_layout->setSpacing(3);
  auto* model_tree_title = new QLabel("Model Tree", tree_panel);
  QFont tree_title_font = model_tree_title->font();
  tree_title_font.setBold(true);
  model_tree_title->setFont(tree_title_font);
  tree_layout->addWidget(model_tree_title);

  workflow_status_label_ = new QLabel("Workflow: Part [0], Material [0], Section [0], "
                                     "Steps [0], BC [0], Loads [0], Mesh [0]",
                                     tree_panel);
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
  auto* tree_actions_container = new QWidget(tree_panel);
  tree_actions_container->setLayout(tree_actions);
  tree_layout->addWidget(tree_actions_container);

  model_tree_ = new QTreeWidget(tree_panel);
  model_tree_->setHeaderLabel("Model Tree");
  // 不设最小宽度: 允许分割条自由拖动, 过窄时树内部出现横向滚动条
  model_tree_->setEditTriggers(QAbstractItemView::SelectedClicked |
                               QAbstractItemView::EditKeyPressed);
  tree_layout->addWidget(model_tree_, 1);
  build_model_tree();

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
  module_work_window_->setMinimumSize(620, 540);
  module_work_window_->resize(760, 700);
  addDockWidget(Qt::RightDockWidgetArea, module_work_window_);
  module_work_window_->setFloating(true);
  module_work_window_->setAllowedAreas(Qt::NoDockWidgetArea);

  auto* property_panel = new QFrame(module_work_window_);
  property_panel->setObjectName("propertyPanel");
  property_panel->setFrameShape(QFrame::StyledPanel);
  property_panel->setFrameShadow(QFrame::Sunken);
  auto* property_layout = new QVBoxLayout(property_panel);
  property_layout->setContentsMargins(0, 0, 0, 0);
  property_layout->setSpacing(0);

  property_stack_ = new QStackedWidget(property_panel);
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
    workspace->setMinimumSize(560, 500);
    workspace->resize(initial_size);
    addDockWidget(Qt::RightDockWidgetArea, workspace);
    workspace->setFloating(true);
    workspace->setAllowedAreas(Qt::NoDockWidgetArea);
    workspace->hide();
    if (view_menu_) {
      auto* toggle = workspace->toggleViewAction();
      toggle->setText(title);
      view_menu_->addAction(toggle);
    }
    return workspace;
  };
  job_work_window_ = make_floating_workspace(
      "Job Workspace", "jobWorkspaceWindow", QSize(900, 720));
  visualization_work_window_ = make_floating_workspace(
      "Visualization Workspace", "visualizationWorkspaceWindow",
      QSize(720, 740));
  results_work_window_ = make_floating_workspace(
      "Results Workspace", "resultsWorkspaceWindow", QSize(900, 720));

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
  auto* job_log_btn = new QPushButton("Open Log");
  auto* job_result_btn = new QPushButton("Open Result");
  job_actions->addWidget(job_run_btn);
  job_actions->addWidget(job_stop_btn);
  job_actions->addWidget(job_retry_btn);
  job_actions->addWidget(job_log_btn);
  job_actions->addWidget(job_result_btn);
  job_actions->addStretch(1);
  auto* job_actions_container = new QWidget(job_manager_page);
  job_actions_container->setLayout(job_actions);
  job_manager_layout->addWidget(job_actions_container);

  auto* job_info_split = new QSplitter(Qt::Vertical, job_manager_page);
  job_info_split->setChildrenCollapsible(false);
  job_table_ = new QTableWidget(job_info_split);
  job_table_->setColumnCount(7);
  job_table_->setHorizontalHeaderLabels(
      {"Name", "Status", "Start", "Duration", "Mesh", "Exec", "Result"});
  job_table_->horizontalHeader()->setStretchLastSection(true);
  job_table_->verticalHeader()->setVisible(false);
  job_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  job_table_->setSelectionMode(QAbstractItemView::SingleSelection);
  job_table_->setMinimumHeight(58);
  job_detail_ = new QPlainTextEdit(job_info_split);
  job_detail_->setReadOnly(true);
  job_detail_->setPlaceholderText("Select a job to view details.");
  job_info_split->addWidget(job_table_);
  job_info_split->addWidget(job_detail_);
  job_info_split->setStretchFactor(0, 1);
  job_info_split->setStretchFactor(1, 1);
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

  // 特征结果统一处理: 失败弹框; 成功挂 Features 根 + 自动划分网格显示 + 控制台提示
  auto handle_feature_result = [this, feature_out_dir](
                                   const QString& type,
                                   const QVariantMap& params,
                                   const FeatureResult& res) {
    if (!res.ok) {
      QMessageBox::warning(this, type, res.error);
      return;
    }
    if (auto* root = find_root_item("Features")) {
      QVariantMap p = params;
      p.insert("type", type);
      p.insert("gmsh_volume_tag", res.gmsh_volume_tag);
      if (!res.brep_path.isEmpty()) {
        p.insert("brep", res.brep_path);
      }
      const QString feature_name =
          QString("feature_%1").arg(root->childCount() + 1);
      add_child_item(root, feature_name, "Features", p);
      // 语义: Feature = 建模历史 (操作+参数), Part = 最终 3D 部件产物。
      // 特征成功后自动产出关联的 Part 条目, 两者通过 feature 参数关联
      if (auto* parts_root = find_root_item("Parts")) {
        QVariantMap pp{{"type", "Part"},
                       {"sketch", params.value("sketch")},
                       {"feature", feature_name}};
        pp.insert("gmsh_volume_tag", res.gmsh_volume_tag);
        if (!res.brep_path.isEmpty()) {
          pp.insert("brep", res.brep_path);
        }
        add_child_item(parts_root,
                       QString("part_%1").arg(parts_root->childCount() + 1),
                       "Parts", pp);
      }
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
    QString mesh_err;
    QString msg =
        QString("%1 ok: imported to gmsh as volume %2 (brep: %3).")
            .arg(type)
            .arg(res.gmsh_volume_tag)
            .arg(res.brep_path.isEmpty() ? "-" : res.brep_path);
    if (mesh_current_model(msh, &mesh_err)) {
      if (viewer_) {
        viewer_->set_mesh_file(msh);
      }
      msg += QString(" Meshed and shown in viewport (mesh: %1).").arg(msh);
    } else {
      msg += QString(" Auto mesh failed (%1); mesh it manually in the Mesh "
                     "module to visualize.")
                 .arg(mesh_err);
    }
    console_->appendPlainText(msg);
    statusBar()->showMessage(msg, 5000);
  };

#ifdef GMP_ENABLE_GMSH_GUI
  connect(part_feature_panel_, &PartFeaturePanel::extrude_requested, this,
          [this, load_sketch_doc, feature_out_dir,
           handle_feature_result](const QString& sketch, double distance) {
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
                "Extrude", {{"sketch", sketch}, {"distance", distance}},
                extrude_sketch(doc, distance, brep));
          });
  connect(part_feature_panel_, &PartFeaturePanel::revolve_requested, this,
          [this, load_sketch_doc, feature_out_dir,
           handle_feature_result](const QString& sketch, double angle_deg) {
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
                "Revolve", {{"sketch", sketch}, {"angle_deg", angle_deg}},
                revolve_sketch(doc, angle_deg, brep));
          });
  connect(part_feature_panel_, &PartFeaturePanel::loft_requested, this,
          [this, load_sketch_doc, feature_out_dir,
           handle_feature_result](const QStringList& sketches) {
            if (sketches.size() != 2) {
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
                "Loft",
                {{"sketch", sketches.at(0)},
                 {"sketch2", sketches.at(1)},
                 {"z2", z2}},
                loft_sketches(sections, /*solid=*/true, brep));
          });
  connect(part_feature_panel_, &PartFeaturePanel::sweep_requested, this,
          [this, load_sketch_doc, feature_out_dir,
           handle_feature_result](const QString& profile, const QString& path) {
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
                "Sweep", {{"sketch", profile}, {"path", path}},
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
    }
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
      module_work_window_->setWindowTitle(
          QString("Sketch Editor — %1").arg(target->text(0)));
      module_work_window_->show();
      module_work_window_->raise();
      module_work_window_->activateWindow();
    }
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
          [resolve_selected_sketch]() {
            if (auto* target = resolve_selected_sketch()) {
              target->setFlags(target->flags() | Qt::ItemIsEditable);
              if (auto* view = target->treeWidget()) {
                view->editItem(target, 0);
              }
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
    if (stage_left_toolbar_) {
      stage_left_toolbar_->set_sketch_tool_checked(tool);
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
  auto* results_open_view = new QPushButton("Open in Viewer", results_page);
  auto* results_open_text = new QPushButton("Open as Text", results_page);
  auto* results_filter_label = new QLabel("Type", results_page);
  results_type_filter_ = new QComboBox(results_page);
  results_type_filter_->addItem("All", "all");
  results_type_filter_->addItem("Solver (.e/.exo)", "e");
  results_type_filter_->addItem("Mesh (.msh)", "msh");
  results_type_filter_->addItem("Text (.txt/.csv/.log/.yaml/.yml)", "txt");
  results_actions->addWidget(results_open_root);
  results_actions->addWidget(results_refresh);
  results_actions->addWidget(results_open_view);
  results_actions->addWidget(results_open_text);
  results_actions->addStretch(1);
  results_actions->addWidget(results_filter_label);
  results_actions->addWidget(results_type_filter_);
  results_actions->addStretch(1);
  auto* results_actions_row = new QWidget(results_page);
  results_actions_row->setLayout(results_actions);
  results_layout->addWidget(results_actions_row);

  results_list_ = new QListWidget(results_page);
  results_list_->setSelectionMode(QAbstractItemView::SingleSelection);
  results_list_->setMinimumHeight(140);
  results_layout->addWidget(results_list_);

  results_preview_ = new QPlainTextEdit(results_page);
  results_preview_->setReadOnly(true);
  results_preview_->setPlaceholderText("Select a result item for quick preview.");
  results_preview_->setLineWrapMode(QPlainTextEdit::NoWrap);
  results_layout->addWidget(results_preview_, 1);

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
    sync_results_tree_selection(row);
    const QString path = row->data(Qt::UserRole).toString();
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
            sync_results_tree_selection(current);
            const QString path = current->data(Qt::UserRole).toString();
            if (path.isEmpty()) {
              results_preview_->setPlainText(
                  QString("No file attached for: %1")
                      .arg(current->text()));
              return;
            }
            const QString job = current->data(Qt::UserRole + 1).toString();
            const QString ext = QFileInfo(path).suffix().toLower();
            const QFileInfo fi(path);
            QString details;
            details += QString("Result: %1").arg(current->text());
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
  job_work_window_->resize(900, 720);
  visualization_work_window_->resize(720, 740);
  results_work_window_->resize(900, 720);
  plot_open_btn->setText("Focus Viewport");
  table_open_btn->setText("Focus Viewport");

  auto show_workspace = [](QDockWidget* workspace) {
    if (!workspace) {
      return;
    }
    workspace->show();
    workspace->raise();
    workspace->activateWindow();
  };

  connect(stage_left_toolbar_, &StageLeftToolbar::interaction_mode_requested,
          viewer_, &VtkViewer::set_stage_interaction_mode);
  connect(stage_left_toolbar_, &StageLeftToolbar::view_preset_requested,
          viewer_, &VtkViewer::apply_stage_view);
  connect(stage_left_toolbar_, &StageLeftToolbar::picking_toggled,
          viewer_, &VtkViewer::set_stage_picking);
  connect(stage_left_toolbar_, &StageLeftToolbar::clear_selection_requested,
          viewer_, &VtkViewer::clear_stage_selection);
  connect(stage_left_toolbar_, &StageLeftToolbar::slice_toggled,
          viewer_, &VtkViewer::set_stage_slice);
  connect(stage_left_toolbar_,
          &StageLeftToolbar::representation_cycle_requested,
          viewer_, &VtkViewer::cycle_stage_representation);
  connect(stage_left_toolbar_, &StageLeftToolbar::sketch_tool_requested,
          viewer_, &VtkViewer::set_sketch_tool);
  connect(viewer_, &VtkViewer::stage_picking_changed, stage_left_toolbar_,
          &StageLeftToolbar::set_picking_checked);
  connect(viewer_, &VtkViewer::stage_slice_changed, stage_left_toolbar_,
          &StageLeftToolbar::set_slice_checked);
  connect(viewer_, &VtkViewer::stage_picking_changed, this,
          [this](bool enabled) {
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
            show_workspace(module_work_window_);
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
                          toolbar_actions = std::move(module_toolbar_actions)](int index) {
    const QString title =
        (index >= 0) ? module_tabs_->tabText(index) : QString("Modules");
    if (module_work_window_) {
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
  property_stack_->addWidget(property_editor_);
  property_stack_->addWidget(part_page);
  property_stack_->addWidget(material_page);
  property_stack_->addWidget(section_page);
  property_stack_->addWidget(assembly_page);
  property_stack_->addWidget(step_page);
  property_stack_->addWidget(interaction_page);
  property_stack_->addWidget(load_page);
  property_stack_->addWidget(sketch_panel_);
  property_stack_->addWidget(mesh_page);
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

  vertical_split->setStretchFactor(0, 4);
  vertical_split->setStretchFactor(1, 1);
  main_split->setStretchFactor(0, 0);
  main_split->setStretchFactor(1, 1);
  const int left_w = qBound(180, int(width() * 0.16), 300);
  const int center_w = std::max(480, width() - left_w);
  main_split->setSizes({left_w, center_w});
  vertical_split->setSizes({std::max(480, height() - 90), 80});

  connect(module_tabs_, &QTabBar::currentChanged, this,
          [this, apply_toolbar_actions, results_tab, sketch_tab, mesh_tab, viz_tab,
           close_sketch_editor, preview_sketch](
              int index) {
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
            refresh_work_context();
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
          [this, job_tab, viz_tab, results_tab, show_workspace](int index) {
            if (!layout_ready_) {
              return;
            }
            if (index == job_tab) {
              show_workspace(job_work_window_);
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
  connect(mesh_page, &GmshPanel::mesh_written, this,
          [this](const QString& path) {
            upsert_mesh_item(path);
            statusBar()->showMessage("Mesh generated.", 2000);
          });
  connect(job_page, &MoosePanel::exodus_ready, viewer_,
          &VtkViewer::set_exodus_file);
  connect(job_page, &MoosePanel::exodus_history, viewer_,
          &VtkViewer::set_exodus_history);
  connect(job_page, &MoosePanel::job_started, this,
          [this](const QVariantMap& info) {
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
            if (!active_job_item_) {
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
            const QString exodus = info.value("exodus").toString();
            if (!exodus.isEmpty()) {
              upsert_result_item(exodus, active_job_item_->text(0));
            }
            if (active_job_row_ >= 0) {
              update_job_row(active_job_row_, active_job_item_->text(0), params);
              update_job_detail(active_job_row_);
            }
            active_job_item_ = nullptr;
            active_job_row_ = -1;
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
          [this](int row, int, int, int) { update_job_detail(row); });

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
                 kind == "Outputs") {
        tab = property_tab;
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
    }
    refresh_work_context();
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
    model_tree_->editItem(item, 0);
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
  const QByteArray workspace_geometry =
      layout_settings.value("ui/layout/v2/module_workspace_geometry")
          .toByteArray();
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
  const int tool_layout_version =
      layout_settings.value("ui/layout/v3/version", 0).toInt();
  const QByteArray tool_layout_state =
      layout_settings.value("ui/layout/v3/main_window_state").toByteArray();
  const bool tool_layout_restored =
      tool_layout_version == 3 && !tool_layout_state.isEmpty() &&
      restoreState(tool_layout_state, 3);
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
  project_status_label_ = new QLabel("Project: Untitled");
  dirty_status_label_ = new QLabel("Saved");
  statusBar()->addPermanentWidget(project_status_label_);
  statusBar()->addPermanentWidget(dirty_status_label_);
  update_window_title();
  statusBar()->showMessage("Ready");

  QTimer::singleShot(0, this, [this, tool_layout_restored]() {
    if (!tool_layout_restored) {
      position_default_display_group();
    }
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
      tool_drag_restore_picking_ =
          action_stage_pick_ && action_stage_pick_->isChecked();
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

void MainWindow::reset_tool_group_layout(bool show_feedback) {
  QSettings settings("gmp-ise", "gmp_ise");
  // v2 的 Display Group 是 QDockWidget，不能恢复到当前 QToolBar 类型。
  settings.remove("ui/layout/v2");
  settings.remove("ui/layout/v3");

  const QStringList toolbar_names = {
      "projectToolGroup", "editToolGroup", "modelToolGroup",
      "meshToolGroup", "jobToolGroup"};
  for (const QString& name : toolbar_names) {
    auto* toolbar = findChild<QToolBar*>(name);
    if (!toolbar) {
      continue;
    }
    removeToolBar(toolbar);
    toolbar->setParent(this, Qt::Widget);
    toolbar->setOrientation(Qt::Horizontal);
    addToolBar(Qt::TopToolBarArea, toolbar);
    toolbar->show();
  }
  if (display_tool_group_) {
    display_tool_group_->setParent(this, Qt::Tool);
    display_tool_group_->setOrientation(Qt::Horizontal);
    display_tool_group_->show();
    QTimer::singleShot(0, this,
                       &MainWindow::position_default_display_group);
  }
  QTimer::singleShot(0, this, &MainWindow::recover_floating_tool_groups);
  if (show_feedback) {
    statusBar()->showMessage("Tool layout reset to default.", 2500);
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
    settings.setValue("ui/layout/v2/module_workspace_geometry",
                      module_work_window_->saveGeometry());
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
    update_window_title();
  });
  connect(lang_zh, &QAction::triggered, this, [this]() {
    l10n::set_language(l10n::Language::Chinese);
    l10n::apply(this);
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
    console_->appendPlainText("New project created.");
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
      console_->appendPlainText("Project saved: " + project_path_);
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
      console_->appendPlainText("Project saved: " + project_path_);
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
  connect(action_screenshot_, &QAction::triggered, this, [this]() {
    const QString path = QFileDialog::getSaveFileName(
        this, "Save Screenshot", QDir::homePath(),
        "PNG Image (*.png)");
    if (path.isEmpty()) {
      return;
    }
    if (viewer_ && viewer_->save_screenshot(path)) {
      console_->appendPlainText("Screenshot saved: " + path);
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

void MainWindow::build_toolbar() {
  auto make_group = [this](const QString& title, const QString& object_name) {
    auto* toolbar = new QToolBar(title, this);
    addToolBar(Qt::TopToolBarArea, toolbar);
    toolbar->setObjectName(object_name);
    toolbar->setProperty("gmpToolGroup", true);
    toolbar->setMovable(true);
    toolbar->setFloatable(true);
    toolbar->setAllowedAreas(Qt::AllToolBarAreas);
    toolbar->setIconSize(QSize(18, 18));
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
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
  };

  auto* project_toolbar = make_group("Project", "projectToolGroup");
  auto* edit_toolbar = make_group("Edit", "editToolGroup");
  auto* model_toolbar = make_group("Model", "modelToolGroup");
  auto* mesh_toolbar = make_group("Mesh", "meshToolGroup");
  auto* job_toolbar = make_group("Job", "jobToolGroup");

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
  display_tool_group_ = make_group("Display Group", "displayToolGroup");
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
  padding: 7px 14px;
  min-height: 26px;
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
  padding: 4px 10px;
  min-height: 20px;
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
QTreeWidget::item, QTreeView::item { padding: 4px 6px; }
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
  min-height: 24px;
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
  border-radius: 5px;
  margin-top: 10px;
  padding-top: 6px;
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
  border-radius: 4px;
  padding: 5px 10px;
  min-height: 24px;
}
QPushButton:hover { background: #eef4ff; border-color: #2f6fed; }
QPushButton:pressed { background: #dbe7ff; }
QPushButton:disabled { color: #9aa3af; background: #f3f4f6; }
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
    } else if (name == "Mesh") {
      icon = MakeIcon(IconGlyph::Mesh);
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
            QMenu menu(this);
            if (!item->parent()) {
              auto* add_action =
                  menu.addAction(QString("Add %1").arg(item->text(0)));
              connect(add_action, &QAction::triggered, this,
                      [this, item]() { add_item_under_root(item); });
            } else {
              auto* edit_action = menu.addAction("Edit Properties...");
              auto* add_action = menu.addAction("Add");
              auto* duplicate_action = menu.addAction("Duplicate");
              auto* rename_action = menu.addAction("Rename");
              auto* delete_action = menu.addAction("Remove");
              connect(edit_action, &QAction::triggered, this, [this, item]() {
                model_tree_->setCurrentItem(item);
                if (action_edit_properties_) {
                  action_edit_properties_->trigger();
                }
              });
              connect(add_action, &QAction::triggered, this, [this, item]() {
                if (item->parent()) {
                  add_item_under_root(item->parent());
                }
              });
              connect(duplicate_action, &QAction::triggered, this,
                      [this, item]() { duplicate_item(item); });
              connect(rename_action, &QAction::triggered, this, [this, item]() {
                model_tree_->editItem(item, 0);
              });
              connect(delete_action, &QAction::triggered, this,
                      [this, item]() { remove_item(item); });
            }
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
            set_project_dirty(true);
            if (model_tree_ && committed_item) {
              model_tree_->setCurrentItem(committed_item);
            }
            if (property_editor_) {
              property_editor_->set_item(committed_item);
            }
            refresh_module_pages();
            refresh_work_context();
            statusBar()->showMessage("Properties updated.", 2000);
          });
  connect(form, &QObject::destroyed, this,
          [this]() { floating_property_form_ = nullptr; });
  form->open();
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
  if (step_sequence_preview_) {
    step_sequence_preview_->setPlainText(build_step_sequence_preview());
  }
  refresh_workflow_status();
  refresh_work_context();
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
  context_object_selector_->setEnabled(root != nullptr);
  context_object_selector_->setToolTip(
      root ? QString("Current %1 object; selecting an entry locates it in the model tree.")
                 .arg(root_name)
           : QString("No object selector is available in this context."));
}

int MainWindow::child_count(const QString& root_name) const {
  const auto* root = find_root_item(root_name);
  return root ? root->childCount() : 0;
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

QTreeWidgetItem* MainWindow::add_child_item(QTreeWidgetItem* root,
                                            const QString& name,
                                            const QString& kind,
                                            const QVariantMap& params) {
  if (!root) {
    return nullptr;
  }
  const QVariantMap normalized = normalize_params_for_kind(kind, params);
  auto* item = new QTreeWidgetItem(root);
  item->setText(0, name);
  item->setData(0, PropertyEditor::kKindRole, kind);
  item->setData(0, PropertyEditor::kParamsRole, normalized);
  root->setExpanded(true);
  model_tree_->setCurrentItem(item);
  refresh_module_pages();
  if (property_editor_) {
    property_editor_->refresh_form_options();
  }
  return item;
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
    item->setText(0, name);
    item->setData(0, PropertyEditor::kParamsRole, params);
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
    item->setText(0, name);
    item->setData(0, PropertyEditor::kParamsRole, params);
  }
  set_project_dirty(true);
  refresh_results_panel();
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
  console_->appendPlainText("Model tree synced to MOOSE input.");
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
  console_->appendPlainText("Demo loaded: Transient Diffusion");
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
  console_->appendPlainText("Demo loaded: Thermo-Mechanics");
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
  console_->appendPlainText("Demo loaded: Nonlinear Heat");
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
  const QString name = QInputDialog::getText(
      this, QString("Add %1").arg(kind), "Name:", QLineEdit::Normal,
      base + "_1");
  if (name.isEmpty()) {
    return;
  }
  auto* item = new QTreeWidgetItem(root);
  item->setText(0, name);
  item->setData(0, PropertyEditor::kKindRole, kind);
  item->setData(0, PropertyEditor::kParamsRole,
                default_params_for_kind(kind));
  root->setExpanded(true);
  model_tree_->setCurrentItem(item);
  set_project_dirty(true);
  refresh_module_pages();
  if (property_editor_) {
    property_editor_->refresh_form_options();
  }
}

void MainWindow::remove_item(QTreeWidgetItem* item) {
  if (!item || !item->parent()) {
    return;
  }
  auto* parent = item->parent();
  parent->removeChild(item);
  delete item;
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
  const QString base = item->text(0) + "_copy";
  auto* child = new QTreeWidgetItem(parent);
  child->setText(0, base);
  child->setData(0, PropertyEditor::kKindRole,
                 item->data(0, PropertyEditor::kKindRole));
  child->setData(0, PropertyEditor::kParamsRole,
                 item->data(0, PropertyEditor::kParamsRole));
  parent->setExpanded(true);
  model_tree_->setCurrentItem(child);
  set_project_dirty(true);
  refresh_module_pages();
  if (property_editor_) {
    property_editor_->refresh_form_options();
  }
}

void MainWindow::refresh_job_table() {
  if (!job_table_) {
    return;
  }
  job_table_->setRowCount(0);
  if (job_detail_) {
    job_detail_->clear();
  }
  auto* root = find_root_item("Jobs");
  if (!root) {
    return;
  }
  for (int i = 0; i < root->childCount(); ++i) {
    auto* child = root->child(i);
    if (!child) {
      continue;
    }
    const QVariantMap params =
        child->data(0, PropertyEditor::kParamsRole).toMap();
    append_job_row(child->text(0), params);
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
  set_item(1, params.value("status").toString());
  set_item(2, params.value("start_time").toString());
  set_item(3, params.value("duration").toString());
  set_item(4, params.value("mesh").toString());
  set_item(5, params.value("exec").toString());
  set_item(6, params.value("exodus").toString());
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
        child->setText(0, name);
        child->setData(0, PropertyEditor::kKindRole, kind);
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
    console_->appendPlainText("Project loaded: " + path);
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
                        qFatal("L-04 compact toolbar group contract failed");
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
                      qFatal("L-04 menu/toolbar QAction sharing contract failed");
                    }
                    const QStringList menus = {
                        "fileMenu", "modelMenu", "viewMenu", "meshMenu",
                        "jobMenu", "toolsMenu", "settingsMenu", "helpMenu"};
                    for (const QString& name : menus) {
                      if (!findChild<QMenu*>(name)) {
                        qFatal("L-04 standard menu contract failed");
                      }
                    }
                    if (!module_selector_ || !module_selector_->isVisible() ||
                        !context_project_label_ ||
                        !context_project_label_->isVisible() ||
                        !context_object_selector_ ||
                        !context_object_selector_->isVisible()) {
                      qFatal("L-04 work context fields are not visible");
                    }
                    auto* context_bar = findChild<QWidget*>("moduleBar");
                    const int top_height = menuBar()->height() + 30 +
                                           (context_bar ? context_bar->height()
                                                        : 1000);
                    if (top_height > 100) {
                      qFatal("L-04 top three-layer height exceeds 100 px");
                    }
                  },
                  this});
    steps.append({"l04_module_selector_part",
                  [this]() {
                    const int combo_index = module_selector_
                                                ? module_selector_->findData(1)
                                                : -1;
                    if (combo_index < 0) {
                      qFatal("Part module is missing from work context");
                    }
                    module_selector_->setCurrentIndex(combo_index);
                    if (!module_tabs_ || module_tabs_->currentIndex() != 1) {
                      qFatal("Work context module did not switch internal module");
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
                      if (!group || !group->isMovable() ||
                          !group->isFloatable() ||
                          group->allowedAreas() != Qt::AllToolBarAreas) {
                        qFatal("L-05 movable toolbar contract failed");
                      }
                    }
                    auto* toolbar_menu =
                        findChild<QMenu*>("toolbarVisibilityMenu");
                    if (!display_tool_group_ ||
                        !display_tool_group_->isFloating() ||
                        display_tool_group_->allowedAreas() !=
                            Qt::AllToolBarAreas ||
                        !toolbar_menu || toolbar_menu->actions().size() != 6 ||
                        !action_reset_tool_layout_ ||
                        saveState(3).isEmpty()) {
                      qFatal("L-05 display/persistence contract failed");
                    }
                    auto* toggle =
                        findChild<QToolBar*>("modelToolGroup")->toggleViewAction();
                    toggle->trigger();
                    if (toggle->isChecked()) {
                      qFatal("L-05 toolbar visibility toggle failed");
                    }
                    toggle->trigger();
                    if (!toggle->isChecked()) {
                      qFatal("L-05 toolbar visibility restore failed");
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
                      qFatal("L-05 layout round-trip contract failed");
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
                      qFatal("L-05 display group same-row docking failed");
                    }
                    reset_tool_group_layout(false);
                  },
                  this});
    steps.append({"l05_display_group_default",
                  [this]() {
                    if (!display_tool_group_ || !viewer_ ||
                        !display_tool_group_->isVisible() ||
                        !display_tool_group_->isFloating()) {
                      qFatal("L-05 default display group is not floating");
                    }
                    const QRect viewer_rect(viewer_->mapToGlobal(QPoint(0, 0)),
                                            viewer_->size());
                    const QRect group_rect = display_tool_group_->frameGeometry();
                    if (group_rect.center().x() < viewer_rect.center().x() ||
                        group_rect.top() > viewer_rect.top() +
                                               viewer_rect.height() / 3) {
                      qFatal("L-05 display group is not in the stage top-right preset");
                    }
                  },
                  display_tool_group_});
    steps.append({"p2_workspace_content_layout",
                  [this, dir]() {
                    if (!property_stack_ || !module_work_window_) {
                      qFatal("Phase 2 workspace layout fixture is missing");
                    }
                    for (int i = 0; i < property_stack_->count(); ++i) {
                      auto* page = property_stack_->widget(i);
                      if (!page) {
                        qFatal("Phase 2 workspace page is missing");
                      }
                      const auto scrolls = page->findChildren<QScrollArea*>();
                      for (auto* scroll : scrolls) {
                        for (QObject* ancestor = scroll->parent(); ancestor &&
                             ancestor != page; ancestor = ancestor->parent()) {
                          if (qobject_cast<QScrollArea*>(ancestor)) {
                            qFatal("Phase 2 nested workspace scroll area found");
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
                      qFatal("Phase 2 material workspace layout contract failed");
                    }
                    auto* part_page = property_stack_->widget(1);
                    auto* feature_tabs = part_page
                                             ? part_page->findChild<QTabWidget*>(
                                                   "partFeatureTabs")
                                             : nullptr;
                    if (!feature_tabs || feature_tabs->count() != 4) {
                      qFatal("Phase 2 part feature tabs contract failed");
                    }
                    auto* mesh_page = property_stack_->widget(9);
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
                    if (!gmsh_tabs || gmsh_tabs->count() != 5 ||
                        !geometry_tabs || geometry_tabs->count() != 3 ||
                        !groups_tabs || groups_tabs->count() != 2) {
                      qFatal("Phase 2 mesh workspace tabs contract failed");
                    }
                    property_stack_->setCurrentIndex(9);
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
                      qFatal("I-01 material fixture is missing");
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
                      qFatal("I-01 floating property form contract failed");
                    }
                    const auto property_scrolls =
                        form->findChildren<QScrollArea*>();
                    if (!property_scrolls.isEmpty()) {
                      qFatal("I-01 property form contains an outer scroll area");
                    }
                    if (auto* editor_tabs = form->findChild<QTabWidget*>(
                            "propertyEditorTabs");
                        !editor_tabs || editor_tabs->count() != 4) {
                      qFatal("I-01 property form tab layout contract failed");
                    }
                    *i01_form_size = form->size();
                    form->grab().save(dir + "/i01_property_form_layout.png");
                    const QRect stage_rect(
                        viewer_->mapToGlobal(QPoint(0, 0)), viewer_->size());
                    if (!stage_rect.contains(form->frameGeometry().center())) {
                      qFatal("I-01 property form is outside the stage");
                    }
                    name->setText("i01_discarded_name");
                    if (item->text(0) != *i01_original_name ||
                        project_dirty_ != *i01_original_dirty) {
                      qFatal("I-01 uncommitted edit leaked into project state");
                    }
                    cancel->click();
                    if (item->text(0) != *i01_original_name ||
                        project_dirty_ != *i01_original_dirty) {
                      qFatal("I-01 cancel changed project state");
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
                      qFatal("I-01 material fixture disappeared");
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
                      qFatal("I-01 property form did not reopen");
                    }
                    name->clear();
                    ok->click();
                    if (!form->isVisible() ||
                        item->text(0) != *i01_original_name ||
                        project_dirty_ != *i01_original_dirty) {
                      qFatal("I-01 invalid edit was not blocked and focused");
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
                      qFatal("I-01 commit fixture is missing");
                    }
                    if (!name->hasFocus()) {
                      qFatal("I-01 validation did not focus the first field");
                    }
                    name->setText("material_i01_committed");
                    ok->click();
                    if (item->text(0) != "material_i01_committed" ||
                        !project_dirty_) {
                      qFatal("I-01 accepted edit was not committed");
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
                    if (auto* root = find_root_item("Parts");
                        root && root->childCount() == 0) {
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
                      qFatal("Part fixture is missing from current object selector");
                    }
                    context_object_selector_->setCurrentIndex(combo_index);
                    if (!model_tree_ || !model_tree_->currentItem() ||
                        model_tree_->currentItem()->text(0) != "part_tour") {
                      qFatal("Current object selector did not locate model tree item");
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
                      qFatal("Part tree double-click did not open editor");
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
                  },
                  module_work_window_});
    steps.append({"sketch_add_preview_fixture",
                  [this]() {
                    if (!viewer_ || !viewer_->sketch_document() ||
                        viewer_->is_sketch_preview()) {
                      qFatal("Sketch editor did not enter editable state");
                    }
                    SketchEntity circle;
                    circle.type = SketchEntityType::Circle;
                    circle.center = {0.0, 0.0};
                    circle.radius = 20.0;
                    viewer_->sketch_document()->add_entity(circle);
                    viewer_->refresh_sketch();
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
                      qFatal("Finished sketch was not retained as read-only preview");
                    }
                    if (!context_object_selector_ ||
                        context_object_selector_->currentText() != "sketch_1") {
                      qFatal("Model tree selection did not update current object");
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
                      qFatal("New Part command button not found");
                    }
                    new_part->click();
                    if (!module_work_window_ ||
                        !module_work_window_->isVisible()) {
                      qFatal("New Part did not open editor");
                    }
                  },
                  module_work_window_});
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
                      moose_tabs->count() != 4 ||
                      (!available.isEmpty() &&
                       job_work_window_->height() > available.height())) {
                    qFatal("Phase 2 job workspace layout contract failed");
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
            steps[*state].activate();
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
