#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QList>
#include <vector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class QTableWidget;

namespace gmp {

class GmshPanel : public QWidget {
  Q_OBJECT
 public:
 explicit GmshPanel(QWidget* parent = nullptr);
  ~GmshPanel() override;

 public slots:
  void generate_mesh();
  void set_external_busy(bool busy);
  void set_mesh_generation_dim(int dim);
  QVariantMap gmsh_settings() const;
  void apply_gmsh_settings(const QVariantMap& settings);
  void select_physical_group(int dim, int tag);
  void apply_entity_pick(int dim, int tag);

 protected:
  bool eventFilter(QObject* obj, QEvent* event) override;

 signals:
  void mesh_generation_started();
  void mesh_generation_finished(bool success, const QString& message);
  void mesh_written(const QString& path);
  void boundary_groups(const QStringList& names);
  void volume_groups(const QStringList& names);
  void physical_group_selected(int dim, int tag);

 private slots:
  void on_open_geometry();
  void on_clear_model();
  void on_pick_output();
  void on_generate();
  void on_export_geometry();
  void on_entity_size_apply();
  void on_entity_size_clear();
  void on_add_primitive();
  void on_apply_translate();
  void on_apply_rotate();
  void on_apply_scale();
  void on_apply_boolean_fuse();
  void on_apply_boolean_cut();
  void on_apply_boolean_intersect();
  void on_entity_dim_changed(int index);
  void on_physical_group_selected(int index);
  void on_physical_group_add();
  void on_physical_group_update();
  void on_physical_group_delete();
  void on_physical_group_refresh();
  void on_field_apply();
  void on_field_clear();
  void on_field_refresh();

 private:
  void ensure_gmsh();
  bool import_geometry(const QString& path, bool auto_mesh);
  void update_entity_summary();
  void update_entity_list();
  void update_physical_group_list();
  void update_physical_group_table();
  void update_field_list();
  void update_geometry_controls();
  void update_primitive_controls();
  int infer_mesh_dim() const;
  QString pick_entities_dialog(int dim_filter,
                               const QString& title,
                               const QString& current_text);
  std::vector<int> resolve_entity_tags(int dim_filter,
                                       const QString& text) const;
  void populate_transform_entity_templates(int dim_filter);
  void populate_boolean_entity_templates(int dim_filter);
  void refresh_occ_entity_template_lists();
  void validate_entity_input(QLineEdit* input, int dim_filter,
                            bool occ_only = false);
  QStringList invalid_entity_tokens(const QString& text, int dim_filter,
                                   bool occ_only) const;
  void append_entity_template(QLineEdit* target, const QString& token);
  struct DimTagToken {
    int dim = -1;
    int tag = 0;
    bool has_dim = false;
  };
  std::vector<DimTagToken> parse_dim_tag_tokens(const QString& text) const;
  std::vector<std::pair<int, int>> resolve_dim_tags(
      int dim_filter, const std::vector<DimTagToken>& tokens) const;
  std::vector<std::pair<int, int>> resolve_occ_dim_tags(
      int dim_filter, const std::vector<DimTagToken>& tokens) const;
  void append_log(const QString& text);
  void set_mesh_generation_running(bool running);

  QLineEdit* geo_path_ = nullptr;
  QLabel* entity_summary_ = nullptr;
  QPlainTextEdit* entity_list_ = nullptr;
  QComboBox* entity_dim_ = nullptr;
  QCheckBox* auto_mesh_on_import_ = nullptr;
  QCheckBox* auto_reload_geometry_ = nullptr;
  QCheckBox* use_sample_box_ = nullptr;
  QDoubleSpinBox* size_x_ = nullptr;
  QDoubleSpinBox* size_y_ = nullptr;
  QDoubleSpinBox* size_z_ = nullptr;
  QDoubleSpinBox* mesh_size_ = nullptr;
  QComboBox* mesh_dim_ = nullptr;
  QComboBox* elem_order_ = nullptr;
  QComboBox* msh_version_ = nullptr;
  QCheckBox* optimize_ = nullptr;
  QComboBox* high_order_opt_ = nullptr;
  QComboBox* entity_size_dim_ = nullptr;
  QLineEdit* entity_size_ids_ = nullptr;
  QDoubleSpinBox* entity_size_value_ = nullptr;
  QPushButton* entity_size_apply_ = nullptr;
  QPushButton* entity_size_clear_ = nullptr;

  QComboBox* primitive_kind_ = nullptr;
  QDoubleSpinBox* prim_x_ = nullptr;
  QDoubleSpinBox* prim_y_ = nullptr;
  QDoubleSpinBox* prim_z_ = nullptr;
  QDoubleSpinBox* prim_dx_ = nullptr;
  QDoubleSpinBox* prim_dy_ = nullptr;
  QDoubleSpinBox* prim_dz_ = nullptr;
  QDoubleSpinBox* prim_radius_ = nullptr;
  QPushButton* prim_add_btn_ = nullptr;

  QComboBox* transform_dim_ = nullptr;
  QLineEdit* transform_ids_ = nullptr;
  QComboBox* transform_template_ = nullptr;
  QDoubleSpinBox* trans_dx_ = nullptr;
  QDoubleSpinBox* trans_dy_ = nullptr;
  QDoubleSpinBox* trans_dz_ = nullptr;
  QDoubleSpinBox* rot_x_ = nullptr;
  QDoubleSpinBox* rot_y_ = nullptr;
  QDoubleSpinBox* rot_z_ = nullptr;
  QDoubleSpinBox* rot_ax_ = nullptr;
  QDoubleSpinBox* rot_ay_ = nullptr;
  QDoubleSpinBox* rot_az_ = nullptr;
  QDoubleSpinBox* rot_angle_ = nullptr;
  QDoubleSpinBox* scale_cx_ = nullptr;
  QDoubleSpinBox* scale_cy_ = nullptr;
  QDoubleSpinBox* scale_cz_ = nullptr;
  QDoubleSpinBox* scale_x_ = nullptr;
  QDoubleSpinBox* scale_y_ = nullptr;
  QDoubleSpinBox* scale_z_ = nullptr;

  QComboBox* boolean_dim_ = nullptr;
  QLineEdit* boolean_obj_ids_ = nullptr;
  QLineEdit* boolean_tool_ids_ = nullptr;
  QComboBox* boolean_obj_template_ = nullptr;
  QComboBox* boolean_tool_template_ = nullptr;
  QCheckBox* boolean_remove_obj_ = nullptr;
  QCheckBox* boolean_remove_tool_ = nullptr;

  QComboBox* phys_group_list_ = nullptr;
  QComboBox* phys_group_dim_ = nullptr;
  QLineEdit* phys_group_name_ = nullptr;
  QLineEdit* phys_group_entities_ = nullptr;
  QPushButton* phys_group_add_ = nullptr;
  QPushButton* phys_group_update_ = nullptr;
  QPushButton* phys_group_delete_ = nullptr;
  QTableWidget* phys_group_table_ = nullptr;

  QComboBox* field_dim_ = nullptr;
  QLineEdit* field_entities_ = nullptr;
  QDoubleSpinBox* field_dist_min_ = nullptr;
  QDoubleSpinBox* field_dist_max_ = nullptr;
  QDoubleSpinBox* field_size_min_ = nullptr;
  QDoubleSpinBox* field_size_max_ = nullptr;
  QPlainTextEdit* field_list_ = nullptr;
  QList<QPushButton*> mesh_generate_buttons_;
  bool mesh_generation_running_ = false;
  bool mesh_external_busy_ = false;

  QComboBox* algo2d_ = nullptr;
  QComboBox* algo3d_ = nullptr;
  QCheckBox* recombine_ = nullptr;
  QSpinBox* smoothing_ = nullptr;

  QLineEdit* output_path_ = nullptr;
  QPlainTextEdit* log_ = nullptr;
  QSet<QLineEdit*> entity_inputs_;
  QLineEdit* active_entity_input_ = nullptr;
  bool gmsh_ready_ = false;
  bool model_loaded_ = false;
};

}  // namespace gmp
