#include "gmp/ResultsWidgets.h"

#include "gmp/IconFactory.h"

#include <QApplication>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QSplitter>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextStream>
#include <QToolTip>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <limits>

#include "gmp/ResultData.h"
#include "gmp/L10n.h"

namespace gmp {
namespace {

bool number(const QVariant& value, double* result) {
  bool ok = false;
  *result = value.toDouble(&ok);
  return ok;
}

QString unit_for(const CsvData& csv, int column) {
  return column >= 0 && column < csv.units.size() ? csv.units.at(column)
                                                   : QString();
}

const QList<QColor>& chart_palette() {
  static const QList<QColor> colors = {QColor("#0072B2"), QColor("#D55E00"),
                                       QColor("#009E73"), QColor("#CC79A7"),
                                       QColor("#E69F00"), QColor("#56B4E9")};
  return colors;
}

QString normalized_field_name(QString name) {
  name.remove(QRegularExpression("[\\s_]+"));
  return name.toLower();
}

void sort_points(QList<QPointF>* points) {
  std::stable_sort(points->begin(), points->end(),
                   [](const QPointF& lhs, const QPointF& rhs) {
                     return lhs.x() < rhs.x();
                   });
}

QString translated_preview_name(const QString& name) {
  for (const QString& suffix : {QString("min"), QString("mean"),
                                QString("max")}) {
    if (name.endsWith(" " + suffix))
      return name.left(name.size() - suffix.size()) +
             l10n::tr(suffix == "min" ? "Min"
                                      : (suffix == "max" ? "Max" : "Mean"));
  }
  return name;
}

}  // namespace

ResultsTableWidget::ResultsTableWidget(QWidget* parent) : QWidget(parent) {
  setObjectName("resultsDataTablePanel");
  auto* layout = new QVBoxLayout(this);
  metadata_ = new QLabel("No data", this);
  metadata_->setObjectName("resultsTableMetadata");
  metadata_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(metadata_);
  column_ = new QComboBox(this);
  column_->setObjectName("resultsTableColumn");
  entity_min_ = new QLineEdit(this);
  entity_min_->setObjectName("resultsTableEntityMin");
  entity_max_ = new QLineEdit(this);
  entity_max_->setObjectName("resultsTableEntityMax");
  value_min_ = new QLineEdit(this);
  value_min_->setObjectName("resultsTableValueMin");
  value_max_ = new QLineEdit(this);
  value_max_->setObjectName("resultsTableValueMax");
  special_ = new QComboBox(this);
  special_->setObjectName("resultsTableSpecial");
  special_->addItems({"All values", "Finite", "NaN", "+Inf", "-Inf"});
  for (auto* edit : {entity_min_, entity_max_, value_min_, value_max_}) {
    edit->setMaximumWidth(92);
    edit->setPlaceholderText("0");
  }
  // 筛选区：单行「标签在上、控件在下」的字段对排列（列/实体#从/到/
  // 数值从/到/全部数值 + 清除筛选），整行顶部与底部对齐，视觉整齐。
  const bool zh = l10n::current_language() == l10n::Language::Chinese;
  auto* filters = new QHBoxLayout();
  filters->setContentsMargins(0, 0, 0, 0);
  filters->setSpacing(8);
  auto make_field = [this, zh, filters](const QString& label,
                                        QWidget* control, int stretch = 0) {
    auto* box = new QVBoxLayout();
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(2);
    auto* tag = new QLabel(label, this);
    tag->setStyleSheet("color: #555;");
    box->addWidget(tag);
    box->addWidget(control);
    auto* holder = new QWidget(this);
    holder->setLayout(box);
    filters->addWidget(holder, stretch);
  };
  make_field(l10n::tr("Column"), column_, 1);
  make_field(zh ? QString::fromUtf8("实体#从") : "Entity # from", entity_min_);
  make_field(zh ? QString::fromUtf8("实体#到") : "Entity # to", entity_max_);
  make_field(zh ? QString::fromUtf8("数值从") : "Value from", value_min_);
  make_field(zh ? QString::fromUtf8("数值到") : "Value to", value_max_);
  make_field(zh ? QString::fromUtf8("全部数值") : "All values", special_);
  auto* clear = new QPushButton("Clear filters", this);
  clear->setIcon(gmp::icons::get("clear_filters"));
  clear->setObjectName("gmpIcon_clear_filters");
  {
    auto* box = new QVBoxLayout();
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(2);
    box->addWidget(new QWidget(this));  // 与标签行同高的占位
    box->addWidget(clear);
    auto* holder = new QWidget(this);
    holder->setLayout(box);
    filters->addWidget(holder);
  }
  layout->addLayout(filters);

  table_ = new QTableWidget(this);
  table_->setObjectName("resultsDataTable");
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->setSortingEnabled(false);
  table_->horizontalHeader()->setSortIndicatorShown(true);
  // 列宽策略：全列按比例分摊容器宽度（rebuild() 每次重建列后重设，
  // 保证重进弹窗/切换数据后依然生效）。
  table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  layout->addWidget(table_, 1);

  auto* pager = new QHBoxLayout();
  auto* previous = new QPushButton("Previous", this);
  previous->setIcon(gmp::icons::get("prev_page"));
  previous->setObjectName("gmpIcon_prev_page");
  auto* next = new QPushButton("Next", this);
  next->setIcon(gmp::icons::get("next_page"));
  next->setObjectName("gmpIcon_next_page");
  page_size_ = new QComboBox(this);
  page_size_->setObjectName("resultsTablePageSize");
  page_size_->addItems({"50", "100", "500", "1000"});
  page_size_->setCurrentText("100");
  page_ = new QSpinBox(this);
  page_->setObjectName("resultsTablePage");
  page_->setMinimum(1);
  page_status_ = new QLabel("0 / 0", this);
  number_format_ = new QComboBox(this);
  number_format_->setObjectName("resultsTableNumberFormat");
  number_format_->addItems({"Auto", "Scientific", "Fixed"});
  precision_ = new QSpinBox(this);
  precision_->setObjectName("resultsTablePrecision");
  precision_->setRange(2, 16);
  precision_->setValue(8);
  pager->addWidget(previous);
  pager->addWidget(next);
  pager->addWidget(new QLabel("Page", this));
  pager->addWidget(page_);
  pager->addWidget(page_status_);
  pager->addStretch(1);
  pager->addWidget(new QLabel("Rows", this));
  pager->addWidget(page_size_);
  pager->addWidget(number_format_);
  pager->addWidget(new QLabel("Digits", this));
  pager->addWidget(precision_);
  layout->addLayout(pager);

  const auto update = [this]() { rebuild(); };
  for (auto* edit : {entity_min_, entity_max_, value_min_, value_max_}) {
    connect(edit, &QLineEdit::textChanged, this, update);
  }
  connect(column_, &QComboBox::currentIndexChanged, this, update);
  connect(special_, &QComboBox::currentIndexChanged, this, update);
  connect(page_size_, &QComboBox::currentIndexChanged, this, [this]() {
    page_->setValue(1);
    rebuild();
  });
  connect(page_, &QSpinBox::valueChanged, this, update);
  connect(number_format_, &QComboBox::currentIndexChanged, this, update);
  connect(precision_, &QSpinBox::valueChanged, this, update);
  connect(previous, &QPushButton::clicked, page_, [this]() {
    page_->setValue(qMax(1, page_->value() - 1));
  });
  connect(next, &QPushButton::clicked, page_, [this]() {
    page_->setValue(qMin(page_->maximum(), page_->value() + 1));
  });
  connect(clear, &QPushButton::clicked, this, [this]() {
    entity_min_->clear(); entity_max_->clear(); value_min_->clear();
    value_max_->clear(); special_->setCurrentIndex(0); page_->setValue(1);
  });
  connect(table_->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this,
          [this](int column, Qt::SortOrder order) {
            sort_column_ = column;
            sort_order_ = order;
            page_->setValue(1);
            rebuild();
          });
}

void ResultsTableWidget::set_snapshot(const QVariantMap& snapshot) {
  const QString old_field = snapshot_.value("field").toString();
  const QString old_source = snapshot_.value("source").toString();
  const QVariant old_time = snapshot_.value("time");
  snapshot_ = snapshot;
  rows_.clear();
  for (const QVariant& row : snapshot.value("rows").toList()) {
    rows_ << row.toList();
  }
  const QStringList columns = snapshot.value("columns").toStringList();
  const QString selected = column_->currentData().toString();
  column_->blockSignals(true);
  column_->clear();
  for (const QString& name : columns) column_->addItem(l10n::tr(name), name);
  const int same = column_->findData(selected);
  column_->setCurrentIndex(same >= 0 ? same : qMin(1, columns.size() - 1));
  column_->blockSignals(false);
  if (old_source != snapshot.value("source").toString() ||
      old_field != snapshot.value("field").toString() ||
      old_time != snapshot.value("time"))
    page_->setValue(1);
  rebuild();
}

void ResultsTableWidget::retranslate() {
  l10n::apply(this);
  rebuild();
}

QString ResultsTableWidget::format_value(const QVariant& value) const {
  double numeric = 0.0;
  if (!number(value, &numeric)) return value.toString();
  if (std::isnan(numeric)) return "NaN";
  if (std::isinf(numeric)) return numeric > 0 ? "+Inf" : "-Inf";
  const char format = number_format_->currentIndex() == 1
                          ? 'e'
                          : (number_format_->currentIndex() == 2 ? 'f' : 'g');
  return QString::number(numeric, format, precision_->value());
}

void ResultsTableWidget::rebuild() {
  const QStringList columns = snapshot_.value("columns").toStringList();
  table_->clear();
  table_->setColumnCount(columns.size());
  // 全列 Stretch：所有列按比例分摊容器宽度（用户反馈只拉伸末列会让
  // 最后一列巨大留白、重进弹窗又被 resizeColumnsToContents 打回内容宽）。
  table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  QStringList displayed_columns;
  for (const QString& name : columns) displayed_columns << l10n::tr(name);
  table_->setHorizontalHeaderLabels(displayed_columns);
  visible_rows_.clear();
  const int value_column = qBound(0, column_->currentIndex(),
                                  qMax(0, columns.size() - 1));
  sort_column_ = qBound(0, sort_column_, qMax(0, columns.size() - 1));
  bool entity_min_ok = false, entity_max_ok = false, value_min_ok = false,
       value_max_ok = false;
  const qlonglong entity_min = entity_min_->text().toLongLong(&entity_min_ok);
  const qlonglong entity_max = entity_max_->text().toLongLong(&entity_max_ok);
  const double value_min = value_min_->text().toDouble(&value_min_ok);
  const double value_max = value_max_->text().toDouble(&value_max_ok);
  for (int index = 0; index < rows_.size(); ++index) {
    const QVariantList& row = rows_.at(index);
    const qlonglong entity = row.value(0).toLongLong();
    if ((entity_min_ok && entity < entity_min) ||
        (entity_max_ok && entity > entity_max)) continue;
    double value = 0.0;
    const bool numeric = number(row.value(value_column), &value);
    if ((value_min_ok && (!numeric || value < value_min)) ||
        (value_max_ok && (!numeric || value > value_max))) continue;
    const int special = special_->currentIndex();
    if (special == 1 && (!numeric || !std::isfinite(value))) continue;
    if (special == 2 && (!numeric || !std::isnan(value))) continue;
    if (special == 3 && (!numeric || !std::isinf(value) || value < 0)) continue;
    if (special == 4 && (!numeric || !std::isinf(value) || value > 0)) continue;
    visible_rows_ << index;
  }
  std::stable_sort(visible_rows_.begin(), visible_rows_.end(), [this](int a, int b) {
    const QVariant left = rows_.at(a).value(sort_column_);
    const QVariant right = rows_.at(b).value(sort_column_);
    double ln = 0.0, rn = 0.0;
    const bool numeric = number(left, &ln) && number(right, &rn);
    if (numeric && (std::isnan(ln) || std::isnan(rn))) {
      if (std::isnan(ln) && std::isnan(rn)) return false;
      return !std::isnan(ln);
    }
    return sort_order_ == Qt::AscendingOrder
               ? (numeric ? ln < rn : left.toString() < right.toString())
               : (numeric ? ln > rn : left.toString() > right.toString());
  });
  const int page_size = page_size_->currentText().toInt();
  const int pages = qMax(1, (visible_rows_.size() + page_size - 1) / page_size);
  page_->blockSignals(true);
  page_->setMaximum(pages);
  if (page_->value() > pages) page_->setValue(pages);
  page_->blockSignals(false);
  const int begin = (page_->value() - 1) * page_size;
  const int end = qMin(begin + page_size, visible_rows_.size());
  table_->setRowCount(qMax(0, end - begin));
  for (int display = begin; display < end; ++display) {
    const QVariantList& row = rows_.at(visible_rows_.at(display));
    for (int column = 0; column < columns.size(); ++column) {
      auto* item = new QTableWidgetItem(format_value(row.value(column)));
      double raw = 0.0;
      if (number(row.value(column), &raw) && !std::isfinite(raw)) {
        item->setBackground(QColor("#fff2cc"));
      }
      table_->setItem(display - begin, column, item);
    }
  }
  // 信息行：收集非空片段后用 " | " 拼接，association 为空时不再出现
  // 连续双竖线。
  QStringList segments;
  segments << QString("%1: %2")
                  .arg(l10n::tr("Source"), snapshot_.value("source").toString());
  segments << QString("%1: %2")
                  .arg(l10n::tr("Field"), snapshot_.value("field").toString());
  const QString association = snapshot_.value("association").toString();
  if (!association.isEmpty()) segments << l10n::tr(association);
  segments << QString("%1: %2")
                  .arg(l10n::tr("Time"), snapshot_.value("time").toString());
  segments << QString("%1 / %2 %3")
                  .arg(visible_rows_.size())
                  .arg(rows_.size())
                  .arg(l10n::tr("rows"));
  metadata_->setText(segments.join(" | "));
  page_status_->setText(QString("/ %1").arg(pages));
  // Stretch 模式下内容定宽无意义，去掉以免覆盖列宽策略。
}

class ResultsPlotWidget::Canvas final : public QWidget {
 public:
  explicit Canvas(QWidget* parent = nullptr) : QWidget(parent) {
    setMinimumSize(480, 260);
    setMouseTracking(true);
  }
  QList<Series> series;
  QString x_label;
  QString y_label;
  QString warning;

  void paint(QPainter& painter, const QRect& target) const {
    painter.fillRect(target, Qt::white);
    const QRectF plot = target.adjusted(72, 24, -24, -58);
    painter.setPen(QPen(QColor("#333333"), 1));
    painter.drawLine(plot.bottomLeft(), plot.bottomRight());
    painter.drawLine(plot.bottomLeft(), plot.topLeft());
    QList<const Series*> visible;
    double xmin = std::numeric_limits<double>::infinity();
    double xmax = -xmin, ymin = xmin, ymax = -xmin;
    for (const Series& item : series) {
      if (!item.visible || item.points.isEmpty()) continue;
      visible << &item;
      for (const QPointF& point : item.points) {
        if (!std::isfinite(point.x()) || !std::isfinite(point.y())) continue;
        xmin = qMin(xmin, point.x()); xmax = qMax(xmax, point.x());
        ymin = qMin(ymin, point.y()); ymax = qMax(ymax, point.y());
      }
    }
    if (visible.isEmpty() || !std::isfinite(xmin) || !std::isfinite(ymin)) {
      painter.drawText(plot, Qt::AlignCenter, l10n::tr("No curve selected"));
      return;
    }
    if (xmax == xmin) { xmax += 0.5; xmin -= 0.5; }
    if (ymax == ymin) { ymax += 0.5; ymin -= 0.5; }
    painter.setPen(QPen(QColor("#dddddd"), 1));
    for (int i = 1; i < 5; ++i) {
      const double x = plot.left() + plot.width() * i / 5.0;
      const double y = plot.top() + plot.height() * i / 5.0;
      painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
      painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }
    auto map = [&](const QPointF& point) {
      return QPointF(plot.left() + (point.x() - xmin) / (xmax - xmin) * plot.width(),
                     plot.bottom() - (point.y() - ymin) / (ymax - ymin) * plot.height());
    };
    int color = 0;
    for (const Series* item : visible) {
      const QColor curve_color = item->color.isEmpty()
                                     ? chart_palette().at(
                                           color % chart_palette().size())
                                     : QColor(item->color);
      QPen pen(curve_color, item->line_width,
               static_cast<Qt::PenStyle>(item->line_style));
      painter.setPen(pen);
      QPainterPath path;
      bool first = true;
      for (const QPointF& point : item->points) {
        if (!std::isfinite(point.x()) || !std::isfinite(point.y())) continue;
        const QPointF mapped = map(point);
        first ? path.moveTo(mapped) : path.lineTo(mapped);
        first = false;
      }
      painter.drawPath(path);
      if (item->marker > 0) {
        painter.setBrush(curve_color);
        const int stride = qMax(1, item->points.size() / 80);
        for (int point_index = 0; point_index < item->points.size();
             point_index += stride) {
          const QPointF& point = item->points.at(point_index);
          if (!std::isfinite(point.x()) || !std::isfinite(point.y())) continue;
          const QPointF mapped = map(point);
          if (item->marker == 1)
            painter.drawEllipse(mapped, 3.0, 3.0);
          else
            painter.drawRect(QRectF(mapped.x() - 3.0, mapped.y() - 3.0,
                                    6.0, 6.0));
        }
        painter.setBrush(Qt::NoBrush);
      }
      ++color;
    }
    painter.setPen(QColor("#333333"));
    painter.drawText(QRectF(plot.left(), plot.bottom() + 28, plot.width(), 24),
                     Qt::AlignCenter, x_label);
    painter.save();
    painter.translate(18, plot.center().y()); painter.rotate(-90);
    painter.drawText(QRectF(-plot.height() / 2, -12, plot.height(), 24),
                     Qt::AlignCenter, y_label);
    painter.restore();
    painter.drawText(QRectF(4, plot.bottom() - 10, 64, 20), Qt::AlignRight,
                     QString::number(ymin, 'g', 6));
    painter.drawText(QRectF(4, plot.top() - 10, 64, 20), Qt::AlignRight,
                     QString::number(ymax, 'g', 6));
    painter.drawText(QRectF(plot.left(), plot.bottom() + 4, 90, 20), Qt::AlignLeft,
                     QString::number(xmin, 'g', 6));
    painter.drawText(QRectF(plot.right() - 90, plot.bottom() + 4, 90, 20), Qt::AlignRight,
                     QString::number(xmax, 'g', 6));
    const qreal legend_width = qMin<qreal>(260.0, plot.width() * 0.42);
    const qreal legend_height = visible.size() * 18.0 + 8.0;
    const QRectF legend_rect(plot.right() - legend_width - 6.0,
                             plot.top() + 6.0, legend_width, legend_height);
    painter.fillRect(legend_rect, QColor(255, 255, 255, 225));
    painter.setPen(QColor("#bbbbbb"));
    painter.drawRect(legend_rect);
    for (int row = 0; row < visible.size(); ++row) {
      const Series* item = visible.at(row);
      const QColor curve_color = item->color.isEmpty()
                                     ? chart_palette().at(
                                           row % chart_palette().size())
                                     : QColor(item->color);
      const qreal y = legend_rect.top() + 13.0 + row * 18.0;
      painter.setPen(QPen(curve_color, item->line_width,
                         static_cast<Qt::PenStyle>(item->line_style)));
      painter.drawLine(QPointF(legend_rect.left() + 8.0, y),
                       QPointF(legend_rect.left() + 36.0, y));
      painter.setPen(QColor("#333333"));
      painter.drawText(
          QRectF(legend_rect.left() + 42.0, y - 9.0,
                 legend_rect.width() - 48.0, 18.0),
          Qt::AlignVCenter | Qt::AlignLeft, item->name);
    }
    if (!warning.isEmpty()) {
      painter.setPen(QColor("#9c5b00"));
      painter.drawText(QRectF(plot.left(), 2.0, plot.width(), 20.0),
                       Qt::AlignLeft | Qt::AlignVCenter, warning);
    }
  }

 protected:
  void paintEvent(QPaintEvent*) override { QPainter painter(this); paint(painter, rect()); }
  void mouseMoveEvent(QMouseEvent* event) override {
    const QRectF plot = rect().adjusted(72, 24, -24, -58);
    double xmin = INFINITY, xmax = -INFINITY, ymin = INFINITY, ymax = -INFINITY;
    for (const Series& item : series)
      if (item.visible)
        for (const QPointF& point : item.points) {
          if (!std::isfinite(point.x()) || !std::isfinite(point.y())) continue;
          xmin = qMin(xmin, point.x()); xmax = qMax(xmax, point.x());
          ymin = qMin(ymin, point.y()); ymax = qMax(ymax, point.y());
        }
    if (!std::isfinite(xmin) || !std::isfinite(ymin)) return;
    if (xmax == xmin) { xmax += .5; xmin -= .5; }
    if (ymax == ymin) { ymax += .5; ymin -= .5; }
    const QPointF cursor = event->position();
    const Series* nearest_series = nullptr;
    QPointF nearest_value;
    double nearest_distance = 100.0;
    for (const Series& item : series) {
      if (!item.visible) continue;
      for (const QPointF& point : item.points) {
        if (!std::isfinite(point.x()) || !std::isfinite(point.y())) continue;
        const QPointF mapped(
            plot.left() + (point.x() - xmin) / (xmax - xmin) * plot.width(),
            plot.bottom() - (point.y() - ymin) / (ymax - ymin) * plot.height());
        const QPointF delta = mapped - cursor;
        const double distance = delta.x() * delta.x() + delta.y() * delta.y();
        if (distance < nearest_distance) {
          nearest_distance = distance;
          nearest_series = &item;
          nearest_value = point;
        }
      }
    }
    if (nearest_series) {
      QToolTip::showText(
          event->globalPosition().toPoint(),
          QString("%1\nX: %2\nY: %3")
              .arg(nearest_series->name,
                   QString::number(nearest_value.x(), 'g', 12),
                   QString::number(nearest_value.y(), 'g', 12)),
          this);
    } else {
      QToolTip::hideText();
    }
  }
};

ResultsPlotWidget::ResultsPlotWidget(QWidget* parent) : QWidget(parent) {
  setObjectName("resultsPlotPanel");
  auto* layout = new QVBoxLayout(this);
  auto* actions = new QHBoxLayout();
  auto* pin = new QPushButton("Pin preview", this);
  pin->setObjectName("resultsPinPreview");
  pin->setIcon(gmp::icons::get("pin_preview"));
  auto* import = new QPushButton("Import CSV...", this);
  import->setIcon(gmp::icons::get("import_csv"));
  import->setObjectName("gmpIcon_import_csv");
  auto* export_menu = new QMenu(this);
  auto* export_button = new QToolButton(this);
  export_button->setObjectName("resultsExportMenuButton");
  export_button->setText("Export \xe2\x96\xbe");
  export_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  export_button->setIcon(gmp::icons::get("export_png"));
  export_button->setPopupMode(QToolButton::InstantPopup);
  export_button->setMenu(export_menu);
  auto* png = export_menu->addAction(gmp::icons::get("export_png"), "PNG");
  png->setObjectName("gmpIcon_export_png");
  auto* svg = export_menu->addAction(gmp::icons::get("export_svg"), "SVG");
  svg->setObjectName("gmpIcon_export_svg");
  auto* csv = export_menu->addAction(gmp::icons::get("export_csv"), "CSV");
  csv->setObjectName("gmpIcon_export_csv");
  auto* copy =
      export_menu->addAction(gmp::icons::get("copy_image"), "Copy image");
  copy->setObjectName("gmpIcon_copy_image");
  auto* curve_menu = new QMenu(this);
  auto* curve_button = new QToolButton(this);
  curve_button->setObjectName("resultsCurveMenuButton");
  curve_button->setText("Curve \xe2\x96\xbe");
  curve_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  curve_button->setIcon(gmp::icons::get("edit_selected"));
  curve_button->setPopupMode(QToolButton::InstantPopup);
  curve_button->setMenu(curve_menu);
  auto* remove =
      curve_menu->addAction(gmp::icons::get("remove_selected"), "Remove selected");
  remove->setObjectName("gmpIcon_remove_selected");
  auto* edit =
      curve_menu->addAction(gmp::icons::get("edit_selected"), "Edit selected");
  edit->setObjectName("gmpIcon_edit_selected");
  auto* compare = curve_menu->addAction(gmp::icons::get("compare_curves"),
                                        "Compare 2 curves");
  compare->setObjectName("gmpIcon_compare_curves");
  for (QWidget* button : {static_cast<QWidget*>(pin),
                          static_cast<QWidget*>(import),
                          static_cast<QWidget*>(export_button),
                          static_cast<QWidget*>(curve_button)})
    actions->addWidget(button);
  actions->addStretch(1);
  actions->addWidget(new QLabel("Component", this));
  component_ = new QComboBox(this);
  component_->setObjectName("resultsComponentSelector");
  component_->addItem(l10n::tr("Value"), "Value");
  actions->addWidget(component_);
  layout->addLayout(actions);
  metadata_ = new QLabel("Follow viewport: no field selected", this);
  metadata_->setObjectName("resultsPlotMetadata");
  warning_ = new QLabel(this);
  warning_->setStyleSheet("color:#9c5b00;");
  layout->addWidget(metadata_);
  layout->addWidget(warning_);
  canvas_ = new Canvas(this);
  canvas_->setObjectName("resultsPlotCanvas");
  legend_ = new QTableWidget(this);
  legend_->setObjectName("resultsCurveLegend");
  legend_->setColumnCount(4);
  legend_->setHorizontalHeaderLabels({"Visible", "Pinned", "Name", "Source"});
  legend_->setSelectionBehavior(QAbstractItemView::SelectRows);
  legend_->setMinimumWidth(220);
  legend_->setMaximumWidth(640);
  // 横向滚动条关闭：列宽由内容定宽+末列拉伸管理（见 refresh() 的列宽
  // 策略）。否则横向滚动条挤占高度触发纵向滚动条，两者互相反馈，
  // 在 可见/固定 列旁残留滚动条残影。
  legend_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  // 曲线区与清单表之间可拖动调整比例（用户反馈原固定布局）。
  auto* body_splitter = new QSplitter(Qt::Horizontal, this);
  body_splitter->setObjectName("resultsPlotSplitter");
  body_splitter->setChildrenCollapsible(false);
  body_splitter->addWidget(canvas_);
  body_splitter->addWidget(legend_);
  body_splitter->setStretchFactor(0, 1);
  body_splitter->setStretchFactor(1, 0);
  layout->addWidget(body_splitter, 1);
  connect(pin, &QPushButton::clicked, this, [this]() {
    for (Series& item : series_) {
      if (!item.preview) continue;
      item.preview = false;
      item.pinned = true;
    }
    refresh();
  });
  connect(import, &QPushButton::clicked, this, [this]() {
    const QString path = QFileDialog::getOpenFileName(this, l10n::tr("Import curve CSV"), {},
                                                     "CSV (*.csv *.txt);;All (*)");
    if (!path.isEmpty()) add_csv(path);
  });
  connect(remove, &QAction::triggered, this, [this]() {
    const int row = legend_->currentRow();
    if (row >= 0 && row < series_.size()) series_.removeAt(row);
    refresh();
  });
  connect(edit, &QAction::triggered, this,
          &ResultsPlotWidget::edit_selected_curve);
  connect(compare, &QAction::triggered, this, &ResultsPlotWidget::compare_curves);
  connect(png, &QAction::triggered, this, &ResultsPlotWidget::export_png);
  connect(svg, &QAction::triggered, this, &ResultsPlotWidget::export_svg);
  connect(csv, &QAction::triggered, this, &ResultsPlotWidget::export_csv);
  connect(copy, &QAction::triggered, this, [this]() {
    QImage image(canvas_->size() * 2, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(2); QPainter painter(&image); canvas_->paint(painter, canvas_->rect());
    QApplication::clipboard()->setImage(image);
  });
  connect(legend_, &QTableWidget::itemChanged, this,
          [this](QTableWidgetItem* item) {
            if (!item || item->row() >= series_.size()) return;
            if (item->column() == 0) {
              series_[item->row()].visible =
                  item->checkState() == Qt::Checked;
              canvas_->series = series_;
              canvas_->update();
            } else if (item->column() == 1 &&
                       (series_[item->row()].preview ||
                        series_[item->row()].pinned)) {
              const int row = item->row();
              if (item->checkState() == Qt::Checked) {
                series_[row].preview = false;
                series_[row].pinned = true;
                refresh();
              } else if (series_[row].pinned) {
                series_.removeAt(row);
                set_field_snapshot(field_snapshot_);
              }
            }
          });
  connect(component_, &QComboBox::currentTextChanged, this,
          [this]() { set_field_snapshot(field_snapshot_); });
}

void ResultsPlotWidget::set_field_snapshot(const QVariantMap& snapshot) {
  field_snapshot_ = snapshot;
  const QString previous_component = component_->currentData().toString();
  const QStringList components = snapshot.value("components").toStringList();
  {
    const QSignalBlocker blocker(component_);
    component_->clear();
    const QStringList available =
        components.isEmpty() ? QStringList{"Value"} : components;
    for (const QString& name : available)
      component_->addItem(l10n::tr(name), name);
    const int previous = component_->findData(previous_component);
    component_->setCurrentIndex(previous >= 0 ? previous : 0);
  }
  for (int i = series_.size() - 1; i >= 0; --i)
    if (series_.at(i).preview) series_.removeAt(i);
  for (const QVariant& value : snapshot.value("series").toList()) {
    const QVariantMap raw = value.toMap();
    const QString component = raw.value("component", "Value").toString();
    if (component != component_->currentData().toString()) continue;
    Series item;
    item.name = translated_preview_name(raw.value("name").toString());
    item.source = snapshot.value("source").toString();
    item.x_name = snapshot.value("x_label").toString();
    item.y_name = raw.value("y_label", snapshot.value("y_label")).toString();
    item.component = component;
    item.preview = true;
    for (const QVariant& point : raw.value("points").toList()) {
      const QVariantList xy = point.toList();
      if (xy.size() >= 2) item.points << QPointF(xy.at(0).toDouble(), xy.at(1).toDouble());
    }
    series_ << item;
  }
  metadata_->setText(
      QString("%1 · %2 · %3 · %4")
          .arg(l10n::tr("Follow viewport"),
               snapshot.value("field").toString(),
               l10n::tr(snapshot.value("association").toString()),
               l10n::tr(snapshot.value("mode").toString())));
  refresh();
}

void ResultsPlotWidget::retranslate() {
  l10n::apply(this);
  set_field_snapshot(field_snapshot_);
}

bool ResultsPlotWidget::add_csv(const QString& path) {
  CsvData csv = read_csv_data(path);
  if (!csv.valid()) {
    const auto answer = QMessageBox::question(
        this, l10n::tr("CSV data errors"), csv.errors.join("\n") +
              "\n\n" + l10n::tr("Ignore bad rows and continue?"));
    if (answer != QMessageBox::Yes) return false;
    csv = read_csv_data(path, true);
  }
  QDialog dialog(this);
  dialog.setWindowTitle(l10n::tr("Select CSV curve columns"));
  auto* form = new QFormLayout(&dialog);
  auto* x = new QComboBox(&dialog);
  auto* y = new QListWidget(&dialog);
  auto* display_name = new QLineEdit(&dialog);
  display_name->setPlaceholderText(l10n::tr("Optional curve name or prefix"));
  auto* x_unit = new QLineEdit(&dialog);
  auto* y_unit = new QLineEdit(&dialog);
  x_unit->setPlaceholderText(l10n::tr("Unit unspecified — enter one if known"));
  y_unit->setPlaceholderText(l10n::tr("Unit unspecified — enter one if known"));
  auto* x_scale = new QDoubleSpinBox(&dialog);
  auto* y_scale = new QDoubleSpinBox(&dialog);
  for (auto* scale : {x_scale, y_scale}) {
    scale->setDecimals(12);
    scale->setRange(-1.0e12, 1.0e12);
    scale->setValue(1.0);
  }
  y->setSelectionMode(QAbstractItemView::ExtendedSelection);
  for (int i = 0; i < csv.headers.size(); ++i) {
    if (!csv.numeric_columns.value(i)) continue;
    x->addItem(csv.headers.at(i), i);
    auto* item = new QListWidgetItem(csv.headers.at(i), y);
    item->setData(Qt::UserRole, i);
  }
  for (int i = 0; i < x->count(); ++i)
    if (x->itemText(i).compare("time", Qt::CaseInsensitive) == 0 ||
        x->itemText(i).startsWith("time ", Qt::CaseInsensitive)) x->setCurrentIndex(i);
  auto* quality = new QLabel(
      QString("%1 · delimiter %2 · %3 rows%4")
          .arg(csv.encoding, csv.delimiter).arg(csv.rows.size())
          .arg(csv.ignored_bad_rows
                   ? QString(" · ignored %1 bad rows: %2")
                         .arg(csv.ignored_bad_rows)
                         .arg(csv.ignored_row_errors.join("; "))
                                    : QString()), &dialog);
  quality->setWordWrap(true);
  auto* sample = new QTableWidget(qMin(5, csv.rows.size()), csv.headers.size(),
                                  &dialog);
  sample->setHorizontalHeaderLabels(csv.headers);
  sample->setEditTriggers(QAbstractItemView::NoEditTriggers);
  for (int row = 0; row < sample->rowCount(); ++row)
    for (int column = 0; column < sample->columnCount(); ++column)
      sample->setItem(row, column,
                      new QTableWidgetItem(csv.rows.at(row).value(column).toString()));
  sample->setMaximumHeight(150);
  form->addRow(l10n::tr("Detected"), quality);
  form->addRow(l10n::tr("Sample"), sample);
  form->addRow("X", x);
  form->addRow("Y", y);
  form->addRow(l10n::tr("Name"), display_name);
  form->addRow(l10n::tr("X unit"), x_unit);
  form->addRow(l10n::tr("Y unit"), y_unit);
  form->addRow(l10n::tr("X multiplier"), x_scale);
  form->addRow(l10n::tr("Y multiplier / sign"), y_scale);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  l10n::apply(&dialog);
  if (dialog.exec() != QDialog::Accepted || y->selectedItems().isEmpty()) return false;
  const int x_column = x->currentData().toInt();
  status_warning_.clear();
  for (QListWidgetItem* selected : y->selectedItems()) {
    const int y_column = selected->data(Qt::UserRole).toInt();
    if (y_column == x_column) continue;
    Series item;
    const QString custom_name = display_name->text().trimmed();
    item.name = custom_name.isEmpty()
                    ? QFileInfo(path).completeBaseName() + " · " + selected->text()
                    : (y->selectedItems().size() == 1
                           ? custom_name
                           : custom_name + " · " + selected->text());
    item.source = path; item.x_name = csv.headers.value(x_column);
    item.y_name = csv.headers.value(y_column);
    item.x_unit = x_unit->text().trimmed().isEmpty()
                      ? unit_for(csv, x_column)
                      : x_unit->text().trimmed();
    item.y_unit = y_unit->text().trimmed().isEmpty()
                      ? unit_for(csv, y_column)
                      : y_unit->text().trimmed();
    item.x_scale = x_scale->value();
    item.y_scale = y_scale->value();
    if (csv.ignored_bad_rows > 0)
      item.note = QString("%1 ignored %2 malformed CSV rows: %3")
                      .arg(item.name)
                      .arg(csv.ignored_bad_rows)
                      .arg(csv.ignored_row_errors.join("; "));
    QString conflict;
    for (const Series& existing : series_) {
      if (!existing.visible) continue;
      if (!existing.x_unit.isEmpty() && !item.x_unit.isEmpty() &&
          existing.x_unit != item.x_unit) conflict = "X units: " + existing.x_unit + " vs " + item.x_unit;
      if (!existing.y_unit.isEmpty() && !item.y_unit.isEmpty() &&
          existing.y_unit != item.y_unit) conflict = "Y units: " + existing.y_unit + " vs " + item.y_unit;
    }
    if (!conflict.isEmpty()) {
      status_warning_ = "Cannot share axes for " + item.name + " — " + conflict +
                        ". Use a separate comparison window or apply an explicit conversion.";
      continue;
    }
    bool mapping_confirmation = false;
    for (const Series& existing : series_) {
      if (!existing.visible || existing.points.isEmpty()) continue;
      if (normalized_field_name(existing.x_name) !=
              normalized_field_name(item.x_name) ||
          normalized_field_name(existing.y_name) !=
              normalized_field_name(item.y_name)) {
        mapping_confirmation = true;
        break;
      }
    }
    if (mapping_confirmation &&
        QMessageBox::question(
            this, l10n::tr("Confirm axis mapping"),
            l10n::tr("The selected X/Y field names differ from existing curves. Add this curve using the explicit mapping shown in the dialog?")) !=
            QMessageBox::Yes)
      continue;
    for (const QVariantList& row : csv.rows) {
      bool x_ok = false, y_ok = false;
      const double xv = row.value(x_column).toDouble(&x_ok);
      const double yv = row.value(y_column).toDouble(&y_ok);
      if (x_ok && y_ok)
        item.points << QPointF(xv * item.x_scale, yv * item.y_scale);
    }
    sort_points(&item.points);
    series_ << item;
  }
  refresh();
  return true;
}

QVariantList ResultsPlotWidget::settings() const {
  QVariantList saved;
  for (const Series& item : series_) {
    if (item.preview || item.source.startsWith("derived:")) continue;
    QVariantMap entry{{"name", item.name},
                      {"source", item.source},
                      {"x_name", item.x_name},
                      {"y_name", item.y_name},
                      {"x_unit", item.x_unit},
                      {"y_unit", item.y_unit},
                      {"component", item.component},
                      {"x_scale", item.x_scale},
                      {"y_scale", item.y_scale},
                      {"color", item.color},
                      {"line_width", item.line_width},
                      {"line_style", item.line_style},
                      {"marker", item.marker},
                      {"note", item.note},
                      {"pinned", item.pinned},
                      {"visible", item.visible}};
    if (QFileInfo(item.source).suffix().compare("csv", Qt::CaseInsensitive) !=
        0) {
      QVariantList points;
      for (const QPointF& point : item.points)
        points << QVariant(QVariantList{point.x(), point.y()});
      entry.insert("points", points);
    }
    saved << QVariant(entry);
  }
  return saved;
}

void ResultsPlotWidget::restore_settings(const QVariantList& settings) {
  for (int i = series_.size() - 1; i >= 0; --i)
    if (!series_.at(i).preview) series_.removeAt(i);
  QStringList unavailable;
  for (const QVariant& value : settings) {
    const QVariantMap saved = value.toMap();
    Series item;
    item.name = saved.value("name").toString();
    item.source = saved.value("source").toString();
    item.x_name = saved.value("x_name").toString();
    item.y_name = saved.value("y_name").toString();
    item.x_unit = saved.value("x_unit").toString();
    item.y_unit = saved.value("y_unit").toString();
    item.component = saved.value("component").toString();
    item.x_scale = saved.value("x_scale", 1.0).toDouble();
    item.y_scale = saved.value("y_scale", 1.0).toDouble();
    item.color = saved.value("color").toString();
    item.line_width = saved.value("line_width", 2.0).toDouble();
    item.line_style = saved.value("line_style", 1).toInt();
    item.marker = saved.value("marker", 0).toInt();
    item.note = saved.value("note").toString();
    item.pinned = saved.contains("pinned")
                      ? saved.value("pinned").toBool()
                      : !saved.value("points").toList().isEmpty();
    item.visible = saved.value("visible", true).toBool();
    for (const QVariant& point : saved.value("points").toList()) {
      const QVariantList xy = point.toList();
      if (xy.size() >= 2)
        item.points << QPointF(xy.at(0).toDouble(), xy.at(1).toDouble());
    }
    const CsvData csv = item.points.isEmpty() ? read_csv_data(item.source)
                                               : CsvData();
    const int x = csv.headers.indexOf(item.x_name);
    const int y = csv.headers.indexOf(item.y_name);
    if (!item.points.isEmpty()) {
      // Pinned Exodus preview stores only its compact time series, never the
      // field dataset, so it remains available after reopening the project.
    } else if (csv.valid() && x >= 0 && y >= 0) {
      for (const QVariantList& row : csv.rows) {
        bool x_ok = false, y_ok = false;
        const double xv = row.value(x).toDouble(&x_ok);
        const double yv = row.value(y).toDouble(&y_ok);
        if (x_ok && y_ok)
          item.points << QPointF(xv * item.x_scale, yv * item.y_scale);
      }
    } else {
      item.visible = false;
      item.note = "Unavailable saved curve: " + item.name +
                  ". Relink or re-import its CSV source.";
      unavailable << item.name;
    }
    sort_points(&item.points);
    series_ << item;
  }
  status_warning_ = unavailable.isEmpty()
                        ? QString()
                        : "Unavailable saved curves: " + unavailable.join(", ") +
                              ". Relink or re-import their CSV source.";
  refresh();
}

void ResultsPlotWidget::refresh() {
  const QSignalBlocker blocker(legend_);
  legend_->setHorizontalHeaderLabels(
      {l10n::tr("Visible"), l10n::tr("Pinned"), l10n::tr("Name"),
       l10n::tr("Source")});
  legend_->setRowCount(series_.size());
  for (int row = 0; row < series_.size(); ++row) {
    auto* visible = new QTableWidgetItem();
    visible->setCheckState(series_.at(row).visible ? Qt::Checked : Qt::Unchecked);
    legend_->setItem(row, 0, visible);
    auto* pinned = new QTableWidgetItem();
    if (series_.at(row).preview || series_.at(row).pinned) {
      pinned->setCheckState(series_.at(row).pinned ? Qt::Checked
                                                   : Qt::Unchecked);
    } else {
      pinned->setText(QString::fromUtf8("—"));
      pinned->setFlags(pinned->flags() & ~Qt::ItemIsUserCheckable);
    }
    legend_->setItem(row, 1, pinned);
    legend_->setItem(row, 2, new QTableWidgetItem(series_.at(row).name));
    legend_->setItem(row, 3, new QTableWidgetItem(
                                  QFileInfo(series_.at(row).source).fileName()));
  }
  canvas_->series = series_;
  const Series* first = nullptr;
  for (const Series& item : series_) if (item.visible) { first = &item; break; }
  canvas_->x_label = first ? l10n::tr(first->x_name) + " [" +
                                  (first->x_unit.isEmpty()
                                       ? l10n::tr("unit unspecified")
                                       : first->x_unit) +
                                  "]"
                           : "X";
  canvas_->y_label = first ? l10n::tr(first->y_name) + " [" +
                                  (first->y_unit.isEmpty()
                                       ? l10n::tr("unit unspecified")
                                       : first->y_unit) +
                                  "]"
                           : "Y";
  // 列宽策略：无数据时全列等分占满清单宽；有数据时按内容定宽、
  // 末列拉伸填满余量（无横向滚动条），各列保持可手动拖动调整。
  if (series_.isEmpty()) {
    legend_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  } else {
    legend_->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Interactive);
    legend_->resizeColumnsToContents();
    legend_->horizontalHeader()->setStretchLastSection(true);
  }
  QStringList warnings;
  if (!status_warning_.isEmpty()) warnings << status_warning_;
  for (const Series& item : series_)
    if (!item.note.isEmpty() && !warnings.contains(item.note)) warnings << item.note;
  warning_->setText(warnings.join(" · "));
  canvas_->warning = warning_->text();
  canvas_->update();
  if (auto* export_button =
          findChild<QToolButton*>("resultsExportMenuButton")) {
    bool has_visible = false;
    for (const Series& item : series_)
      if (item.visible) {
        has_visible = true;
        break;
      }
    export_button->setEnabled(has_visible);
  }
  if (auto* curve_button = findChild<QToolButton*>("resultsCurveMenuButton"))
    curve_button->setEnabled(!series_.isEmpty());
}

void ResultsPlotWidget::edit_selected_curve() {
  const int row = legend_->currentRow();
  if (row < 0 || row >= series_.size()) return;
  Series& item = series_[row];
  QDialog dialog(this);
  dialog.setWindowTitle(l10n::tr("Edit curve appearance"));
  auto* form = new QFormLayout(&dialog);
  auto* name = new QLineEdit(item.name, &dialog);
  auto* color = new QPushButton(
      item.color.isEmpty() ? l10n::tr("Automatic") : item.color, &dialog);
  auto* style = new QComboBox(&dialog);
  style->addItem("Solid", int(Qt::SolidLine));
  style->addItem("Dash", int(Qt::DashLine));
  style->addItem("Dot", int(Qt::DotLine));
  style->addItem("Dash-dot", int(Qt::DashDotLine));
  style->setCurrentIndex(qMax(0, style->findData(item.line_style)));
  auto* width = new QDoubleSpinBox(&dialog);
  width->setRange(0.5, 8.0);
  width->setValue(item.line_width);
  auto* marker = new QComboBox(&dialog);
  marker->addItems({"None", "Circle", "Square"});
  marker->setCurrentIndex(qBound(0, item.marker, 2));
  connect(color, &QPushButton::clicked, &dialog, [&dialog, color, &item]() {
    const QColor selected = QColorDialog::getColor(
        item.color.isEmpty() ? QColor("#0072B2") : QColor(item.color),
        &dialog, l10n::tr("Curve color"));
    if (selected.isValid()) color->setText(selected.name());
  });
  form->addRow(l10n::tr("Name"), name);
  form->addRow(l10n::tr("Color"), color);
  form->addRow(l10n::tr("Line style"), style);
  form->addRow(l10n::tr("Width"), width);
  form->addRow(l10n::tr("Marker"), marker);
  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  l10n::apply(&dialog);
  if (dialog.exec() != QDialog::Accepted) return;
  if (!name->text().trimmed().isEmpty()) item.name = name->text().trimmed();
  item.color = color->text() == l10n::tr("Automatic") ? QString() : color->text();
  item.line_style = style->currentData().toInt();
  item.line_width = width->value();
  item.marker = marker->currentIndex();
  refresh();
}

void ResultsPlotWidget::export_png() {
  const QString path = QFileDialog::getSaveFileName(this, l10n::tr("Export chart PNG"), {}, "PNG (*.png)");
  if (path.isEmpty()) return;
  bool ok = false;
  const QString scale_text = QInputDialog::getItem(
      this, l10n::tr("PNG resolution"), l10n::tr("Scale"), {"1×", "2×", "4×"}, 1, false, &ok);
  if (!ok) return;
  const int scale = scale_text.left(1).toInt();
  QImage image(canvas_->size() * scale, QImage::Format_ARGB32_Premultiplied);
  image.setDevicePixelRatio(scale); QPainter painter(&image); canvas_->paint(painter, canvas_->rect());
  image.save(path);
}

void ResultsPlotWidget::export_svg() {
  const QString path = QFileDialog::getSaveFileName(this, l10n::tr("Export chart SVG"), {}, "SVG (*.svg)");
  if (path.isEmpty()) return;
  QFile file(path); if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
  QTextStream out(&file);
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1000\" "
         "height=\"600\" viewBox=\"0 0 1000 600\">"
         "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>";
  double xmin = INFINITY, xmax = -INFINITY, ymin = INFINITY, ymax = -INFINITY;
  QList<const Series*> visible;
  for (const Series& item : series_) {
    if (!item.visible || item.points.isEmpty()) continue;
    visible << &item;
    for (const QPointF& point : item.points) {
      if (!std::isfinite(point.x()) || !std::isfinite(point.y())) continue;
      xmin = qMin(xmin, point.x()); xmax = qMax(xmax, point.x());
      ymin = qMin(ymin, point.y()); ymax = qMax(ymax, point.y());
    }
  }
  if (!std::isfinite(xmin) || !std::isfinite(ymin)) {
    out << "<text x=\"500\" y=\"300\" text-anchor=\"middle\">"
           "No curve selected</text></svg>";
    return;
  }
  if (xmax == xmin) { xmax += .5; xmin -= .5; }
  if (ymax == ymin) { ymax += .5; ymin -= .5; }
  out << "<g stroke=\"#dddddd\" stroke-width=\"1\">";
  for (int i = 1; i < 5; ++i)
    out << "<path d=\"M" << 80 + i * 176 << " 50V520 M80 "
        << 50 + i * 94 << "H960\"/>";
  out << "</g><path d=\"M80 50V520H960\" fill=\"none\" "
         "stroke=\"#333333\"/>";
  int index = 0;
  for (const Series* item : visible) {
    const QString color = item->color.isEmpty()
                              ? chart_palette().at(index % chart_palette().size()).name()
                              : item->color;
    const QString dash = item->line_style == int(Qt::DashLine)
                             ? " stroke-dasharray=\"8 5\""
                             : (item->line_style == int(Qt::DotLine)
                                    ? " stroke-dasharray=\"2 4\""
                                    : (item->line_style == int(Qt::DashDotLine)
                                           ? " stroke-dasharray=\"8 4 2 4\""
                                           : QString()));
    out << "<polyline fill=\"none\" stroke=\"" << color
        << "\" stroke-width=\"" << item->line_width << "\"" << dash
        << " points=\"";
    for (const QPointF& point : item->points) {
      if (!std::isfinite(point.x()) || !std::isfinite(point.y())) continue;
      out << 80 + (point.x() - xmin) / (xmax - xmin) * 880 << ','
          << 520 - (point.y() - ymin) / (ymax - ymin) * 470 << ' ';
    }
    out << "\"/>";
    const int legend_y = 68 + index * 22;
    out << "<line x1=\"700\" x2=\"735\" y1=\"" << legend_y
        << "\" y2=\"" << legend_y << "\" stroke=\"" << color
        << "\" stroke-width=\"" << item->line_width << "\"" << dash
        << "/><text x=\"742\" y=\"" << legend_y + 5
        << "\" font-size=\"13\" fill=\"#333333\">"
        << item->name.toHtmlEscaped() << "</text>";
    ++index;
  }
  out << "<text x=\"520\" y=\"570\" text-anchor=\"middle\">"
      << canvas_->x_label.toHtmlEscaped()
      << "</text><text x=\"20\" y=\"285\" text-anchor=\"middle\" "
         "transform=\"rotate(-90 20 285)\">"
      << canvas_->y_label.toHtmlEscaped() << "</text>";
  if (!canvas_->warning.isEmpty())
    out << "<text x=\"80\" y=\"25\" fill=\"#9c5b00\">"
        << canvas_->warning.toHtmlEscaped() << "</text>";
  out << "</svg>";
}

void ResultsPlotWidget::export_csv() {
  const QString path = QFileDialog::getSaveFileName(this, l10n::tr("Export visible curves"), {}, "CSV (*.csv)");
  if (path.isEmpty()) return;
  QFile file(path); if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
  QTextStream out(&file);
  out << "curve,source,x_field,y_field,x_unit,y_unit,x_multiplier,"
         "y_multiplier,processing,note,x,y\n";
  for(const Series&s:series_) if(s.visible) for(const QPointF&p:s.points)
    out << '"' << s.name << "\",\"" << s.source << "\",\"" << s.x_name
        << "\",\"" << s.y_name << "\",\"" << s.x_unit << "\",\""
        << s.y_unit << "\"," << QString::number(s.x_scale, 'g', 17) << ','
        << QString::number(s.y_scale, 'g', 17) << ",\""
        << (s.source.startsWith("derived:")
                ? s.source
                : ((s.x_scale == 1.0 && s.y_scale == 1.0)
                       ? "raw source samples"
                       : "scaled source samples; multipliers recorded"))
        << "\",\"" << s.note << "\"," << QString::number(p.x(),'g',17)
        << ',' << QString::number(p.y(),'g',17) << '\n';
}

void ResultsPlotWidget::compare_curves() {
  QList<int> available;
  for (int i = 0; i < series_.size(); ++i)
    if (series_.at(i).visible && series_.at(i).points.size() > 1) available << i;
  if (available.size() < 2) {
    QMessageBox::information(this, l10n::tr("Compare curves"),
                             l10n::tr("Select at least two visible curves."));
    return;
  }
  QDialog dialog(this);
  dialog.setWindowTitle(l10n::tr("Compare curves"));
  auto* form = new QFormLayout(&dialog);
  auto* reference = new QComboBox(&dialog);
  auto* candidate = new QComboBox(&dialog);
  for (int index : available) {
    reference->addItem(series_.at(index).name, index);
    candidate->addItem(series_.at(index).name, index);
  }
  candidate->setCurrentIndex(1);
  form->addRow(l10n::tr("Reference"), reference);
  form->addRow(l10n::tr("Candidate"), candidate);
  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  l10n::apply(&dialog);
  if (dialog.exec() != QDialog::Accepted) return;
  const int reference_index = reference->currentData().toInt();
  const int candidate_index = candidate->currentData().toInt();
  if (reference_index == candidate_index) {
    QMessageBox::warning(this, l10n::tr("Compare curves"),
                         l10n::tr("Reference and candidate must be different curves."));
    return;
  }
  const Series& ref = series_.at(reference_index);
  const Series& cmp = series_.at(candidate_index);
  if ((!ref.x_unit.isEmpty() && !cmp.x_unit.isEmpty() &&
       ref.x_unit != cmp.x_unit) ||
      (!ref.y_unit.isEmpty() && !cmp.y_unit.isEmpty() &&
       ref.y_unit != cmp.y_unit)) {
    QMessageBox::warning(
        this, l10n::tr("Incompatible curves"),
        l10n::tr("Cannot compare curves with incompatible X or Y units."));
    return;
  }
  QList<QPointF> candidate_points = cmp.points;
  sort_points(&candidate_points);
  auto interpolate = [&candidate_points](double x, double* y) {
    auto upper = std::lower_bound(
        candidate_points.begin(), candidate_points.end(), x,
        [](const QPointF& point, double value) { return point.x() < value; });
    if (upper != candidate_points.end() && upper->x() == x) {
      *y = upper->y();
      return std::isfinite(*y);
    }
    if (upper == candidate_points.begin() || upper == candidate_points.end())
      return false;
    const QPointF& right = *upper;
    const QPointF& left = *(upper - 1);
    if (right.x() == left.x()) return false;
    *y = left.y() + (x - left.x()) / (right.x() - left.x()) *
                         (right.y() - left.y());
    return std::isfinite(*y);
  };
  Series diff;
  diff.name = cmp.name + " − " + ref.name;
  diff.source = "derived: linear interpolation, no extrapolation";
  diff.x_name = ref.x_name;
  diff.y_name = "Difference";
  diff.x_unit = ref.x_unit;
  diff.y_unit = ref.y_unit;
  diff.line_style = int(Qt::DashLine);
  double sum_abs = 0.0, sum_sq = 0.0, max_abs = 0.0;
  double ref_min = INFINITY, ref_max = -INFINITY;
  int excluded = 0;
  for (const QPointF& point : ref.points) {
    double candidate_y = 0.0;
    if (!std::isfinite(point.x()) || !std::isfinite(point.y()) ||
        !interpolate(point.x(), &candidate_y)) {
      ++excluded;
      continue;
    }
    const double error = candidate_y - point.y();
    diff.points << QPointF(point.x(), error);
    sum_abs += std::abs(error);
    sum_sq += error * error;
    max_abs = qMax(max_abs, std::abs(error));
    ref_min = qMin(ref_min, point.y());
    ref_max = qMax(ref_max, point.y());
  }
  if (diff.points.isEmpty()) {
    QMessageBox::warning(
        this, l10n::tr("No overlap"),
        l10n::tr("Curves have no usable overlapping monotonic X interval."));
    return;
  }
  const double count = diff.points.size();
  const double rmse = std::sqrt(sum_sq / count);
  const double range = ref_max - ref_min;
  diff.note = QString("reference=%1; candidate=%2; points=%3; excluded=%4; "
                      "linear interpolation; no extrapolation")
                  .arg(ref.name, cmp.name)
                  .arg(diff.points.size())
                  .arg(excluded);
  series_ << diff;
  QMessageBox::information(
      this, l10n::tr("Comparison metrics"),
      QString("Overlap: %1 to %2\nEffective points: %3\nExcluded: %4\n"
              "Interpolation: linear, no extrapolation\nMAE: %5\nRMSE: %6\n"
              "Max |error|: %7\nNRMSE: %8")
          .arg(diff.points.first().x(), 0, 'g', 8)
          .arg(diff.points.last().x(), 0, 'g', 8)
          .arg(diff.points.size())
          .arg(excluded)
          .arg(sum_abs / count, 0, 'g', 8)
          .arg(rmse, 0, 'g', 8)
          .arg(max_abs, 0, 'g', 8)
          .arg(range > 0 ? QString::number(rmse / range, 'g', 8)
                         : "disabled (reference range is zero)"));
  refresh();
}

}  // namespace gmp
