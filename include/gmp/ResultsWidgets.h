#pragma once

#include <QList>
#include <QPointF>
#include <QVariantMap>
#include <QWidget>

class QLabel;
class QTableWidget;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QPushButton;

namespace gmp {

class ResultsTableWidget final : public QWidget {
 public:
  explicit ResultsTableWidget(QWidget* parent = nullptr);
  void set_snapshot(const QVariantMap& snapshot);
  void retranslate();

 private:
  void rebuild();
  QString format_value(const QVariant& value) const;

  QVariantMap snapshot_;
  QList<QVariantList> rows_;
  QList<int> visible_rows_;
  QLabel* metadata_ = nullptr;
  QTableWidget* table_ = nullptr;
  QComboBox* column_ = nullptr;
  QLineEdit* entity_min_ = nullptr;
  QLineEdit* entity_max_ = nullptr;
  QLineEdit* value_min_ = nullptr;
  QLineEdit* value_max_ = nullptr;
  QComboBox* special_ = nullptr;
  QComboBox* page_size_ = nullptr;
  QSpinBox* page_ = nullptr;
  QLabel* page_status_ = nullptr;
  QComboBox* number_format_ = nullptr;
  QSpinBox* precision_ = nullptr;
  int sort_column_ = 0;
  Qt::SortOrder sort_order_ = Qt::AscendingOrder;
};

class ResultsPlotWidget final : public QWidget {
 public:
  explicit ResultsPlotWidget(QWidget* parent = nullptr);
  void set_field_snapshot(const QVariantMap& snapshot);
  bool add_csv(const QString& path);
  QVariantList settings() const;
  void restore_settings(const QVariantList& settings);
  void retranslate();

 private:
  class Canvas;
  struct Series {
    QString name;
    QString source;
    QString x_name;
    QString y_name;
    QString x_unit;
    QString y_unit;
    QString component;
    QString color;
    QString note;
    QList<QPointF> points;
    double x_scale = 1.0;
    double y_scale = 1.0;
    double line_width = 2.0;
    int line_style = 1;
    int marker = 0;
    bool preview = false;
    bool pinned = false;
    bool visible = true;
  };

  void refresh();
  void export_png();
  void export_svg();
  void export_csv();
  void compare_curves();
  void edit_selected_curve();

  QList<Series> series_;
  QVariantMap field_snapshot_;
  QLabel* metadata_ = nullptr;
  QLabel* warning_ = nullptr;
  QComboBox* component_ = nullptr;
  Canvas* canvas_ = nullptr;
  QTableWidget* legend_ = nullptr;
  QString status_warning_;
};

}  // namespace gmp
