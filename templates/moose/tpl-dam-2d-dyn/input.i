# =============================================================================
# Template: tpl-dam-2d-dyn —— 2D 坝静力—动力两阶段（动力阶段, Newmark γ/β 可配）
# Source repo : DamSafetyApp @ 69e99d74064a22adbdda0eb17d3c14ce096b7e57
#               (tools/abaqus/gen_dam_two_step_case.py 由 Abaqus dam-2b.inp 转换报告生成;
#                Newmark γ/β 参数化见 c58229f085b05ff7d4868bb2a54f8a0c5f54f6fb —— 该提交面向
#                beam_field_solver.py 外部求解器案例; 本模板为 MOOSE 输入, γ/β 直接改
#                Physics/NodalKernels/BCs 中的 newmark_beta/newmark_gamma/beta/gamma 参数;
#                求解器 BlackBear@1c190fd3d2b5f06a3518923f550a0e0a90b015d4 / MOOSE@4bce02d91b56c7ed845a5747df4d24f415592504)
# Source file : .build/cases/abaqus-2d-dam-p0/full-static-dynamic-v1/generated/dam_2d_full_dynamic.i
# Source SHA-256 (input, 未加本头注释/未改相对路径前): e5553fd2b230013c2df11f1028585a04baf0f5e47fe3c0d3c7e6da0d4f30ff8c
# Mesh        : dam_2d_full_static.e  SHA-256: 0f95890caa226d35a28318623b1de473f0c3e0f66e56d4021284b883a7679c26
#               (tpl-dam-2d-static 的静力求解结果, 作 Exodus restart 网格与初值来源;
#                若修改静力参数, 需先重跑静力模板并替换本文件)
# Extra files : dam_2d_full_acceleration.csv  SHA-256: 91c34fe2d8c3f6830270a9d05e3a541383b328ee3979b03aced1959a0f39f2ad (基底加速度时程)
#               added-mass_x.csv              SHA-256: b50db5df3bcf01bda10ac09ad60b30d21c1c54f72f5bb6bd757b74bc369bf42a (X 向节点附加质量)
#               added-mass_y.csv              SHA-256: 99d140392fcef85315169bb4364b057b4acb5a9762b92c54d8ee888f8a235719 (Y 向节点附加质量)
# Extracted   : 2026-08-18
# Status      : prototype —— 不作为工程结论（未迁移 CDP/Lanczos 模态步; 地震幅值末值非零导致
#               位移漂移待 Abaqus ODB 对标; Newmark 阻尼参数按客户反馈标定中,
#               证据: DamSafetyApp doc/prototypes/abaqus-2d-static-dynamic-rerun.md）
# =============================================================================
# Generated full linear-elastic dynamic step initialized from the static result.
# Known limitation: Abaqus CDP and the preceding Lanczos frequency step are not migrated.
[GlobalParams]
  displacements = 'disp_x disp_y'
  out_of_plane_strain = strain_zz
[]

[Mesh]
  [file]
    type = FileMeshGenerator
    file = dam_2d_full_static.e
    use_for_exodus_restart = true
  []
[]

[Variables]
  [disp_x]
    initial_from_file_var = disp_x
  []
  [disp_y]
    initial_from_file_var = disp_y
  []
  [strain_zz]
    initial_from_file_var = strain_zz
  []
[]

[AuxVariables]
  [vel_x]
  []
  [accel_x]
  []
  [vel_y]
  []
  [accel_y]
  []
[]

[Physics/SolidMechanics/Dynamic]
  [dam]
    add_variables = false
    velocities = 'vel_x vel_y'
    accelerations = 'accel_x accel_y'
    newmark_beta = 0.25
    newmark_gamma = 0.5
    hht_alpha = 0.0
    mass_damping_coefficient = 1.95
    stiffness_damping_coefficient = 0.00113
    strain = SMALL
    incremental = false
    planar_formulation = WEAK_PLANE_STRESS
    density = density
    generate_output = 'stress_xx stress_yy stress_xy vonmises_stress'
  []
[]

[Functions]
  [hydrostatic_pressure]
    type = ParsedFunction
    expression = 'max(0, 592116 * (60.42 - y) / 60.42)'
  []
  [base_acceleration]
    type = PiecewiseLinear
    data_file = dam_2d_full_acceleration.csv
    format = columns
  []
[]

[Kernels]
  [gravity_y]
    type = Gravity
    variable = disp_y
    value = -9.800000000000001
  []
[]

[NodalKernels]
  [added_mass_x]
    type = NodalTranslationalInertia
    variable = disp_x
    velocity = vel_x
    acceleration = accel_x
    beta = 0.25
    gamma = 0.5
    boundary = POINT_MASS
    nodal_mass_file = added-mass_x.csv
  []
  [added_mass_y]
    type = NodalTranslationalInertia
    variable = disp_y
    velocity = vel_y
    acceleration = accel_y
    beta = 0.25
    gamma = 0.5
    boundary = POINT_MASS
    nodal_mass_file = added-mass_y.csv
  []
[]

[BCs]
  [fix_2_2]
    type = DirichletBC
    variable = disp_y
    boundary = _PICKEDSET4__DAM_1
    value = 0
  []
  [base_acceleration_x]
    type = PresetAcceleration
    variable = disp_x
    boundary = _PickedSet8__DAM_1
    function = base_acceleration
    beta = 0.25
    velocity = vel_x
    acceleration = accel_x
  []
  [water_pressure_x]
    type = Pressure
    variable = disp_x
    boundary = _PickedSurf7
    function = hydrostatic_pressure
  []
  [water_pressure_y]
    type = Pressure
    variable = disp_y
    boundary = _PickedSurf7
    function = hydrostatic_pressure
  []
[]

[Materials]
  [elasticity]
    type = ComputeIsotropicElasticityTensor
    youngs_modulus = 30400000000
    poissons_ratio = 0.2
  []
  [stress]
    type = ComputeLinearElasticStress
  []
  [density]
    type = GenericConstantMaterial
    prop_names = density
    prop_values = 2260.74
  []
[]

[Postprocessors]
  [max_abs_disp_x]
    type = NodalExtremeValue
    variable = disp_x
    value_type = max_abs
  []
  [max_abs_disp_y]
    type = NodalExtremeValue
    variable = disp_y
    value_type = max_abs
  []
  [max_abs_accel_x]
    type = NodalExtremeValue
    variable = accel_x
    value_type = max_abs
  []
  [max_vonmises]
    type = ElementExtremeValue
    variable = vonmises_stress
    value_type = max
  []
[]

[Executioner]
  type = Transient
  start_time = 1
  end_time = 51
  dt = 0.01
  solve_type = LINEAR
  petsc_options_iname = '-pc_type -pc_hypre_type'
  petsc_options_value = 'hypre boomeramg'
  l_tol = 1e-9
  l_max_its = 300
[]

[Outputs]
  exodus = true
  csv = true
  time_step_interval = 10
  file_base = dam_2d_full_dynamic
[]
