#include "gmp/MainWindow.h"
#include "gmp/L10n.h"

#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QInputDialog>
#include <QLabel>
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
#include <QStackedWidget>
#include <QFrame>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QAbstractItemView>
#include <QVBoxLayout>
#include <QStyle>
#include <QStyleFactory>
#include <QKeySequence>
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
#include "gmp/MoosePanel.h"
#include "gmp/PropertyEditor.h"
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
      p.drawRect(r);
      p.drawLine(s / 2, m + 2, s / 2, s - m - 2);
      p.drawLine(m + 2, s / 2, s - m - 2, s / 2);
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
  }

  return QIcon(pix);
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("GMP-ISE");
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
  auto* module_bar_layout = new QVBoxLayout(module_bar);
  module_bar_layout->setContentsMargins(0, 0, 0, 0);
  module_bar_layout->setSpacing(2);

  module_tabs_ = new QTabBar(module_bar);
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
  module_tabs_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  module_tabs_->setMinimumHeight(30);
  module_tabs_->setMaximumHeight(34);
  module_tabs_->setExpanding(true);
  module_tabs_->setUsesScrollButtons(false);
  module_bar_layout->addWidget(module_tabs_);

  auto* module_toolbar = new QWidget(module_bar);
  module_toolbar->setObjectName("moduleToolbar");
  module_toolbar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  module_toolbar->setFixedHeight(34);
  auto* module_toolbar_layout = new QHBoxLayout(module_toolbar);
  module_toolbar_layout->setContentsMargins(4, 0, 4, 2);
  module_toolbar_layout->setSpacing(6);
  auto* module_toolbar_title = new QLabel("Part", module_toolbar);
  QFont module_toolbar_font = module_toolbar_title->font();
  module_toolbar_font.setBold(true);
  module_toolbar_font.setPointSize(module_toolbar_font.pointSize() + 1);
  module_toolbar_title->setFont(module_toolbar_font);
  auto* command_host = new QWidget(module_toolbar);
  auto* command_layout = new QHBoxLayout(command_host);
  command_layout->setContentsMargins(0, 0, 0, 0);
  command_layout->setSpacing(6);
  command_layout->addStretch(1);
  module_toolbar_layout->addWidget(module_toolbar_title);
  module_toolbar_layout->addWidget(command_host, 1);
  module_bar_layout->addWidget(module_toolbar);
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
      module_toolbar_actions(12);

  const auto part_tab = module_tab_index("Part");
  const auto property_tab = module_tab_index("Property");
  const auto material_tab = module_tab_index("Material");
  const auto section_tab = module_tab_index("Section");
  const auto assembly_tab = module_tab_index("Assembly");
  const auto step_tab = module_tab_index("Step");
  const auto interaction_tab = module_tab_index("Interaction");
  const auto load_tab = module_tab_index("Load");
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
    auto* outer = new QVBoxLayout(container);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(6);

    auto* scroll = new QScrollArea(container);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* panel = new QWidget(scroll);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* heading = new QLabel(title, panel);
    QFont hfont = heading->font();
    hfont.setPointSize(hfont.pointSize() + 3);
    hfont.setBold(true);
    heading->setFont(hfont);
    layout->addWidget(heading);

    auto* desc = new QLabel(description, panel);
    desc->setWordWrap(true);
    layout->addWidget(desc);

    if (!buttons.empty()) {
      auto* actions = new QWidget(panel);
      auto* actions_layout = new QVBoxLayout(actions);
      actions_layout->setContentsMargins(0, 4, 0, 4);
      actions_layout->setSpacing(6);
      for (const auto& button : buttons) {
        auto* btn = new QPushButton(button.first, actions);
        // 不设最小宽度, 由右栏滚动区在过窄时承载横向滚动
        const auto action = button.second;
        connect(btn, &QPushButton::clicked, container,
                [action]() { action(); });
        actions_layout->addWidget(btn);
      }
      actions_layout->addStretch(1);
      layout->addWidget(actions);
    }

    layout->addStretch(1);
    scroll->setWidget(panel);
    outer->addWidget(scroll, 1);
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
    list_layout->addWidget(list);
    list_layout->addWidget(list_action_bar);
    auto* page = qobject_cast<QVBoxLayout*>(container->layout());
    if (page) {
      page->addWidget(panel);
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

  auto* vertical_split = new QSplitter(Qt::Vertical, central);
  vertical_split->setChildrenCollapsible(false);
  main_layout->addWidget(vertical_split, 1);

  // 不允许整栏折叠消失; 内容最小宽度已通过紧凑样式压小, 可拖到很窄
  auto* main_split = new QSplitter(Qt::Horizontal, vertical_split);
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
  auto* plot_page = new QWidget(center_tabs);
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

  auto* table_page = new QWidget(center_tabs);
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
  center_tabs->addTab(plot_page, "Plot");
  center_tabs->addTab(table_page, "Table");
  // 页签条与边栏按钮(打开视口/曲线/表格)重复, 隐藏页签条,
  // 切换完全由边栏按钮驱动; 标题跟随当前页
  center_tabs->tabBar()->hide();
  center_tabs->setCurrentIndex(0);
  connect(center_tabs, &QTabWidget::currentChanged, this,
          [center_title](int index) {
            const QStringList titles = {"Viewport", "Plot Preview", "Table Preview"};
            if (index >= 0 && index < titles.size()) {
              center_title->setText(titles.at(index));
            }
          });
  // 中栏包滚动区: 视口控制行最宽可达 1300+px, 不允许它撑死布局;
  // 窗口较窄时由内部横向滚动条承载, 分割条保持可拖动
  auto* center_scroll = new QScrollArea(center_panel);
  center_scroll->setWidgetResizable(true);
  center_scroll->setFrameShape(QFrame::NoFrame);
  center_scroll->setMinimumWidth(0);
  center_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  center_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  center_scroll->setWidget(center_tabs);
  center_layout->addWidget(center_scroll);

  auto* property_panel = new QFrame(main_split);
  property_panel->setObjectName("propertyPanel");
  property_panel->setFrameShape(QFrame::StyledPanel);
  property_panel->setFrameShadow(QFrame::Sunken);
  auto* property_layout = new QVBoxLayout(property_panel);
  property_layout->setContentsMargins(0, 0, 0, 0);
  property_layout->setSpacing(3);
  auto* property_title = new QLabel("Current Module", property_panel);
  QFont property_title_font = property_title->font();
  property_title_font.setBold(true);
  property_title->setFont(property_title_font);
  property_layout->addWidget(property_title);

  property_stack_ = new QStackedWidget();
  // 右栏包一层滚动区: 不设最小/最大宽度, 分割条可自由拖动;
  // 内容显示不开时由内部滚动条承载
  auto* property_scroll = new QScrollArea(property_panel);
  property_scroll->setWidgetResizable(true);
  property_scroll->setFrameShape(QFrame::NoFrame);
  property_scroll->setMinimumWidth(0);
  // 竖向滚动由各页面内部滚动区承担, 外层只保留横向兜底, 避免双竖向滚动条
  property_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  property_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  property_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  property_scroll->setWidget(property_stack_);
  property_layout->addWidget(property_scroll, 1);

  main_split->addWidget(tree_panel);
  main_split->addWidget(center_panel);
  main_split->addWidget(property_panel);

  std::function<void(int)> apply_toolbar_actions;

  property_editor_ = new PropertyEditor(property_stack_);
  auto* mesh_page = new GmshPanel(property_stack_);
  auto* job_page = new MoosePanel(property_stack_);
  moose_panel_ = job_page;
  gmsh_panel_ = mesh_page;

  auto* job_container = new QWidget(property_stack_);
  auto* job_layout = new QVBoxLayout(job_container);
  job_layout->setContentsMargins(0, 0, 0, 0);
  job_layout->setSpacing(6);

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
  auto* job_actions_container = new QWidget(job_container);
  job_actions_container->setLayout(job_actions);
  job_layout->addWidget(job_actions_container);

  auto* job_split = new QSplitter(Qt::Vertical, job_container);
  job_split->setChildrenCollapsible(false);
  auto* job_info_panel = new QWidget(job_split);
  auto* job_info_layout = new QVBoxLayout(job_info_panel);
  job_info_layout->setContentsMargins(0, 0, 0, 0);
  job_table_ = new QTableWidget(job_info_panel);
  job_table_->setColumnCount(7);
  job_table_->setHorizontalHeaderLabels(
      {"Name", "Status", "Start", "Duration", "Mesh", "Exec", "Result"});
  job_table_->horizontalHeader()->setStretchLastSection(true);
  job_table_->verticalHeader()->setVisible(false);
  job_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  job_table_->setSelectionMode(QAbstractItemView::SingleSelection);
  job_table_->setMinimumHeight(58);
  job_info_layout->addWidget(job_table_);
  job_detail_ = new QPlainTextEdit(job_info_panel);
  job_detail_->setReadOnly(true);
  job_detail_->setPlaceholderText("Select a job to view details.");
  job_info_layout->addWidget(job_detail_);
  job_split->addWidget(job_info_panel);
  job_split->addWidget(job_page);
  job_split->setStretchFactor(0, 0);
  job_split->setStretchFactor(1, 1);
  job_layout->addWidget(job_split, 1);

  auto* part_page = make_module_node_page(
      "Part",
      "Define geometric primitives and manage part-level entities. Parts are a user-facing grouping for your geometry and mesh assignments.",
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
            [this]() {
             if (auto* root = find_root_item("Parts")) {
               add_item_under_root(root);
             }
           }},
          {"Open Gmsh Panel",
           [this, module_tab_index]() {
             const int mesh_tab = module_tab_index("Mesh");
             if (mesh_tab >= 0) {
               module_tabs_->setCurrentIndex(mesh_tab);
             }
           }},
      });

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
          {"Open Visualization Tab", [center_tabs]() { center_tabs->setCurrentIndex(0); }},
          {"Show Plot Preview", [center_tabs]() { center_tabs->setCurrentIndex(1); }},
          {"Show Table Preview", [center_tabs]() { center_tabs->setCurrentIndex(2); }},
      });

  // 视口控制页签(Scalar/Mesh/View/...)迁移到右侧边栏,
  // 让中栏黑色 3D 场景占据全部高度。
  // make_module_page 的结构是 container > QScrollArea > panel,
  // 需要插入到滚动区内的 panel 布局底部 stretch 之前
  if (auto* ct = viewer_ ? viewer_->control_tabs() : nullptr) {
    if (auto* scroll = visualization_page->findChild<QScrollArea*>()) {
      if (QWidget* panel = scroll->widget()) {
        if (auto* panel_layout =
                qobject_cast<QVBoxLayout*>(panel->layout())) {
          const int ct_pos = qMax(0, panel_layout->count() - 1);
          ct->setParent(panel);
          panel_layout->insertWidget(ct_pos, ct, 1);
          ct->show();  // setParent 会隐藏控件, 需重新显示
          // 视口顶部的文件/输出操作区也迁入边栏, 位于控制页之前
          if (auto* tb = viewer_->top_bar()) {
            tb->setParent(panel);
            panel_layout->insertWidget(ct_pos, tb);
            tb->show();
          }
        }
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

  auto open_result_in_viewer = [this, center_tabs](const QListWidgetItem* row) {
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
    center_tabs->setCurrentIndex(0);
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
          [this, center_tabs, open_result_in_viewer]() {
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

  assign_module_actions(part_tab,
                       {
                           {"Open Part Root", [this]() {
                             if (auto* root = find_root_item("Parts")) {
                               model_tree_->setCurrentItem(root);
                               root->setExpanded(true);
                             }
                           }},
                           {"New Part", [this]() {
                             if (auto* root = find_root_item("Parts")) {
                               add_item_under_root(root);
                             }
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
                           {"Open Job Module", [this, module_tab_index]() {
                             const int target = module_tab_index("Job");
                             if (target >= 0) {
                               module_tabs_->setCurrentIndex(target);
                             }
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
                           {"Open Viewport", [center_tabs]() { center_tabs->setCurrentIndex(0); }},
                           {"Open Plot", [center_tabs]() { center_tabs->setCurrentIndex(1); }},
                           {"Open Table", [center_tabs]() { center_tabs->setCurrentIndex(2); }},
                       });

  auto open_selected_result_in_viewer = [this, center_tabs]() {
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
    center_tabs->setCurrentIndex(0);
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
                          module_toolbar_title,
                          property_title,
                          command_host,
                          toolbar_actions = std::move(module_toolbar_actions)](int index) {
    const QString title =
        (index >= 0) ? module_tabs_->tabText(index) : QString("Modules");
    module_toolbar_title->setText(title + " Module");
    if (property_title) {
      property_title->setText(title + " Module");
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
      action_btn->setMinimumHeight(24);
      connect(action_btn, &QPushButton::clicked, action_btn,
              [action]() { action.second(); });
      command_layout->addWidget(action_btn);
    }
    command_layout->addStretch(1);
  };

  // 无内部滚动区的页面统一包一层, 竖向滚动只保留一层
  // (否则外层 property_scroll 与页面内滚动区会出现双竖向滚动条)
  auto wrap_scroll = [](QWidget* w) {
    auto* s = new QScrollArea();
    s->setWidgetResizable(true);
    s->setFrameShape(QFrame::NoFrame);
    s->setMinimumWidth(0);
    s->setWidget(w);
    return s;
  };
  property_stack_->addWidget(wrap_scroll(property_editor_));
  property_stack_->addWidget(part_page);
  property_stack_->addWidget(material_page);
  property_stack_->addWidget(section_page);
  property_stack_->addWidget(assembly_page);
  property_stack_->addWidget(step_page);
  property_stack_->addWidget(interaction_page);
  property_stack_->addWidget(load_page);
  property_stack_->addWidget(mesh_page);
  property_stack_->addWidget(wrap_scroll(job_container));
  property_stack_->addWidget(visualization_page);
  property_stack_->addWidget(wrap_scroll(results_page));
  // QStackedWidget 的最小宽度默认取所有页面的最大值(如 Job 页的7列表格),
  // 会把整个右栏撑宽并迫使每页都出现横向滚动条。
  // 各页改为 Ignored, 栈只按当前页内容计算宽度。
  for (int i = 0; i < property_stack_->count(); ++i) {
    property_stack_->widget(i)->setSizePolicy(QSizePolicy::Ignored,
                                              QSizePolicy::Preferred);
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
  main_split->setStretchFactor(2, 0);
  const int left_w = qBound(180, int(width() * 0.16), 300);
  const int right_w = qBound(280, int(width() * 0.21), 400);
  const int center_w = std::max(480, width() - left_w - right_w);
  main_split->setSizes({left_w, center_w, right_w});

  connect(module_tabs_, &QTabBar::currentChanged, this,
          [this, apply_toolbar_actions, results_tab](
              int index) {
            static constexpr int module_to_property[] = {1, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
            constexpr int module_count = 12;
            const int target =
                (index >= 0 && index < module_count) ? module_to_property[index] : 0;
            refresh_module_pages();
            if (target >= 0 && target < property_stack_->count()) {
              property_stack_->setCurrentIndex(target);
            }
            apply_toolbar_actions(index);
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
  connect(center_tabs, &QTabWidget::currentChanged, this,
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
          [this, center_tabs]() { center_tabs->setCurrentIndex(0); });
  connect(table_open_btn, &QPushButton::clicked, this,
          [this, center_tabs]() { center_tabs->setCurrentIndex(2); });

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

  connect(model_tree_, &QTreeWidget::itemSelectionChanged, this, [this]() {
    auto* item = model_tree_->currentItem();
    property_editor_->set_item(item);
    // PropertyEditor 表单是动态重建的, 中文模式下需重新翻译
    if (l10n::current_language() == l10n::Language::Chinese) {
      QTimer::singleShot(0, this, [this]() { l10n::apply(this); });
    }
  });
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
  project_status_label_ = new QLabel("Project: Untitled");
  dirty_status_label_ = new QLabel("Saved");
  statusBar()->addPermanentWidget(project_status_label_);
  statusBar()->addPermanentWidget(dirty_status_label_);
  update_window_title();
  statusBar()->showMessage("Ready");

  // 启动时恢复语言偏好 (动态重建的页面在模块切换时已另行处理)
  if (l10n::current_language() == l10n::Language::Chinese) {
    QTimer::singleShot(0, this, [this]() { l10n::apply(this); });
  }
}

void MainWindow::build_menu() {
  auto* file_menu = menuBar()->addMenu("&File");
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
  action_sync_ = model_menu->addAction("Sync Model -> MOOSE Input");
  action_sync_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));

  auto* mesh_menu = menuBar()->addMenu("&Mesh");
  action_mesh_ = mesh_menu->addAction("Generate Mesh");
  action_preview_mesh_ = mesh_menu->addAction("Preview Mesh...");
  action_mesh_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
  action_preview_mesh_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));

  auto* job_menu = menuBar()->addMenu("&Job");
  action_run_ = job_menu->addAction("Run");
  action_check_ = job_menu->addAction("Check Input");
  action_stop_ = job_menu->addAction("Stop");
  action_run_->setShortcut(QKeySequence(Qt::Key_F5));
  action_check_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
  action_stop_->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5));

  auto* demo_menu = menuBar()->addMenu("&Demos");
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

  connect(action_new_, &QAction::triggered, this, [this]() {
    project_path_.clear();
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
    load_project(path);
    statusBar()->showMessage("Project loaded.", 2000);
    refresh_module_pages();
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
  auto* toolbar = addToolBar("Main");
  toolbar->setMovable(false);
  toolbar->setIconSize(QSize(18, 18));

  if (action_new_) {
    action_new_->setIcon(MakeIcon(IconGlyph::NewFile));
    toolbar->addAction(action_new_);
  }
  if (action_open_) {
    action_open_->setIcon(MakeIcon(IconGlyph::OpenFolder));
    toolbar->addAction(action_open_);
  }
  if (action_save_) {
    action_save_->setIcon(MakeIcon(IconGlyph::SaveDisk));
    toolbar->addAction(action_save_);
  }
  if (action_save_as_) {
    action_save_as_->setIcon(MakeIcon(IconGlyph::SaveDisk));
    toolbar->addAction(action_save_as_);
  }
  if (action_screenshot_) {
    action_screenshot_->setIcon(MakeIcon(IconGlyph::Output));
    toolbar->addAction(action_screenshot_);
  }
  if (action_sync_) {
    action_sync_->setIcon(MakeIcon(IconGlyph::Sync));
    toolbar->addAction(action_sync_);
  }
  toolbar->addSeparator();
  if (action_mesh_) {
    action_mesh_->setIcon(MakeIcon(IconGlyph::Mesh));
    toolbar->addAction(action_mesh_);
  }
  if (action_preview_mesh_) {
    action_preview_mesh_->setIcon(MakeIcon(IconGlyph::OpenFolder));
    toolbar->addAction(action_preview_mesh_);
  }
  if (action_run_) {
    action_run_->setIcon(MakeIcon(IconGlyph::Run));
    toolbar->addAction(action_run_);
  }
  if (action_check_) {
    action_check_->setIcon(MakeIcon(IconGlyph::Check));
    toolbar->addAction(action_check_);
  }
  if (action_stop_) {
    action_stop_->setIcon(MakeIcon(IconGlyph::Stop));
    toolbar->addAction(action_stop_);
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
  const QStringList root_nodes = {
      "Parts",
      "Materials",
      "Sections",
      "Steps",
      "BC",
      "Loads",
      "Interactions",
      "Functions",
      "Variables",
      "Outputs",
      "Mesh",
      "Jobs",
      "Results"};
  for (const auto& name : root_nodes) {
    auto* item = new QTreeWidgetItem(model_tree_);
    item->setText(0, name);
    item->setExpanded(true);
    item->setData(0, PropertyEditor::kKindRole, name);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    QIcon icon;
    if (name == "Parts") {
      icon = MakeIcon(IconGlyph::Part);
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
              auto* add_action = menu.addAction("Add");
              auto* duplicate_action = menu.addAction("Duplicate");
              auto* rename_action = menu.addAction("Rename");
              auto* delete_action = menu.addAction("Remove");
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
  if (step_sequence_preview_) {
    step_sequence_preview_->setPlainText(build_step_sequence_preview());
  }
  refresh_workflow_status();
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

void MainWindow::load_project(const QString& path) {
  try {
    suppress_dirty_ = true;
    YAML::Node root = YAML::LoadFile(path.toStdString());
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
      return;
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
        QVariantMap params;
        const YAML::Node param_node = entry["params"];
        if (param_node && param_node.IsMap()) {
          for (const auto& p : param_node) {
            const QString key =
                QString::fromStdString(p.first.as<std::string>());
            const QString value =
                QString::fromStdString(p.second.as<std::string>());
            params.insert(key, value);
          }
        }
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

    YAML::Node viewer_node = root["viewer"];
    if (viewer_node && viewer_node.IsMap() && viewer_) {
      const QSet<QString> force_string = {"current_file", "array_key", "preset",
                                          "output_selected"};
      const QVariantMap viewer_settings = parse_map(viewer_node, force_string);
      viewer_->apply_viewer_settings(viewer_settings);
    }
    suppress_dirty_ = false;
    refresh_job_table();
    refresh_results_panel();
    refresh_module_pages();
    add_recent_project(path);
    set_project_dirty(false);
    update_project_status();
  } catch (const std::exception& e) {
    suppress_dirty_ = false;
    QMessageBox::warning(this, "Project Load",
                         QString("Failed to load: %1").arg(e.what()));
  }
}

bool MainWindow::save_project(const QString& path) {
  try {
    YAML::Node root;
    root["version"] = 1;
    root["saved_at"] =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
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
        YAML::Node params(YAML::NodeType::Map);
        const QVariantMap map =
            child->data(0, PropertyEditor::kParamsRole).toMap();
        for (auto it = map.begin(); it != map.end(); ++it) {
          params[it.key().toStdString()] =
              it.value().toString().toStdString();
        }
        entry["params"] = params;
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
      load_project(path);
      statusBar()->showMessage("Project loaded.", 2000);
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
      module_tabs_->setCurrentIndex(10);  // Visualization
    });
    // 直接用 VTK 离屏渲染导出一张视口图（绕过 Qt grab 的时机问题）
    QTimer::singleShot(6000, this, [this, dir]() {
      if (viewer_) {
        viewer_->save_screenshot(dir + "/vtk_viewport.png");
      }
    });
  }

  QList<QPair<QString, std::function<void()>>> steps;
  const QStringList modules = {"Part",     "Property", "Material",
                               "Section",  "Assembly", "Step",
                               "Interaction", "Load",   "Mesh",
                               "Job",      "Visualization", "Results"};
  for (int i = 0; i < modules.size() && i < module_tabs_->count(); ++i) {
    steps.append({QString("module_%1_%2")
                      .arg(i, 2, 10, QLatin1Char('0'))
                      .arg(modules[i]),
                  [this, i]() { module_tabs_->setCurrentIndex(i); }});
  }
  // viewer_ 作为 QTabWidget 的页，其直接父对象是内部 QStackedWidget，需向上找 QTabWidget
  QTabWidget* center_tabs = nullptr;
  for (QWidget* p = viewer_ ? viewer_->parentWidget() : nullptr; p;
       p = p->parentWidget()) {
    if ((center_tabs = qobject_cast<QTabWidget*>(p))) {
      break;
    }
  }
  if (center_tabs) {
    const QStringList pages = {"Viewport", "Plot", "Table"};
    const int vis_index = modules.indexOf("Visualization");
    for (int i = 0; i < pages.size() && i < center_tabs->count(); ++i) {
      steps.append({QString("center_%1").arg(pages[i]),
                    [this, center_tabs, i, vis_index]() {
                      module_tabs_->setCurrentIndex(vis_index);
                      center_tabs->setCurrentIndex(i);
                    }});
    }
  }

  auto state = std::make_shared<int>(-1);
  auto* timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this,
          [this, timer, state, steps, dir]() mutable {
            ++(*state);
            if (*state >= steps.size()) {
              timer->stop();
              QApplication::quit();
              return;
            }
            qInfo("[tour] step %d -> %s", *state,
                  qPrintable(steps[*state].first));
            steps[*state].second();
            const QString file = dir + "/" + steps[*state].first + ".png";
            QTimer::singleShot(500, this, [this, file]() {
              const bool ok = grab().save(file);
              qInfo("[tour] saved %s ok=%d", qPrintable(file), ok);
            });
          });
  timer->start(1500);
}

}  // namespace gmp
