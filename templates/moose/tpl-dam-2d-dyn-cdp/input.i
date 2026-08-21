# =============================================================================
# Template: tpl-dam-2d-dyn-cdp —— 2D 坝 CDP 动力损伤两阶段（动力阶段）
# Source repo : DamSafetyApp
# Source file : .build/cases/abaqus-2d-dam-p0/full-static-dynamic-v1/generated/dam_2d_full_dynamic_cdp_adaptive.i
# Mesh        : dam_2d_full_static_cdp.e  SHA-256: c60cd1a12ebd7a4d9562d2365f701f987a1a70cf5355da9082a38a03d3091640
#               (tpl-dam-2d-static-cdp 的 CDP 静力求解结果, 作 Exodus restart 网格与初值来源)
# Extra files : dam_2d_full_acceleration.csv  SHA-256: 91c34fe2d8c3f6830270a9d05e3a541383b328ee3979b03aced1959a0f39f2ad (基底加速度时程)
#               added-mass_x.csv              SHA-256: b50db5df3bcf01bda10ac09ad60b30d21c1c54f72f5bb6bd757b74bc369bf42a (X 向节点附加质量)
#               added-mass_y.csv              SHA-256: 99d140392fcef85315169bb4364b057b4acb5a9762b92c54d8ee888f8a235719 (Y 向节点附加质量)
# Status      : prototype —— 不作为工程结论（CDP 参数由 Abaqus dam-2b.inp 映射,
#               尚未与客户提供的 Abaqus ODB 对标; 采用 IterationAdaptiveDT + 正则化
#               促进收敛, 按客户反馈粘性系数方向已用 smoothing_tol/yield_function_tol 近似）
# Extracted   : 2026-08-21
# =============================================================================
# Generated full dynamic step with BlackBear DamagePlasticityStressUpdate.
# Initialized from the static CDP result.
[GlobalParams]
  displacements = 'disp_x disp_y'
  out_of_plane_strain = strain_zz
[]

[Mesh]
  [file]
    type = FileMeshGenerator
    file = dam_2d_full_static_cdp.e
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
  [DAMAGEC]
    order = CONSTANT
    family = MONOMIAL
  []
  [DAMAGET]
    order = CONSTANT
    family = MONOMIAL
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
    incremental = true
    planar_formulation = WEAK_PLANE_STRESS
    density = density
    generate_output = 'stress_xx stress_yy stress_xy vonmises_stress'
  []
[]

[AuxKernels]
  [damagec_aux]
    type = MaterialRealAux
    property = compression_damage
    variable = DAMAGEC
  []
  [damaget_aux]
    type = MaterialRealAux
    property = tensile_damage
    variable = DAMAGET
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
  [damage_plasticity_model]
    type = DamagePlasticityStressUpdate
    biaxial_uniaxial_compressive_stress_factor = 0.121
    dilatancy_factor = 0.59
    stiff_recovery_factor = 0.667
    yield_strength_in_tension = 2.55e6
    ft_ep_slope_factor_at_zero_ep = 0.5
    tensile_damage_at_half_tensile_strength = 0.5
    fracture_energy_in_tension = 150
    yield_strength_in_compression = 8.6e6
    maximum_strength_in_compression = 2.13e7
    compressive_damage_at_max_compressive_strength = 0.314
    fracture_energy_in_compression = 5000
    yield_function_tol = 1e-4
    smoothing_tol = 1e-2
  []
  [stress]
    type = ComputeMultipleInelasticDamageStress
    inelastic_models = 'damage_plasticity_model'
    perform_finite_strain_rotations = false
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
  [max_damagec]
    type = ElementExtremeValue
    variable = DAMAGEC
    value_type = max
  []
  [max_damaget]
    type = ElementExtremeValue
    variable = DAMAGET
    value_type = max
  []
[]

[Executioner]
  type = Transient
  start_time = 1
  end_time = 51
  dt = 0.05
  dtmin = 1e-6
  dtmax = 0.05
  line_search = 'bt'
  solve_type = PJFNK
  petsc_options = '-snes_ksp_ew'
  petsc_options_iname = '-pc_type -pc_hypre_type'
  petsc_options_value = 'hypre boomeramg'
  nl_rel_tol = 1e-10
  nl_abs_tol = 1e-14
  nl_max_its = 100

  [TimeStepper]
    type = IterationAdaptiveDT
    dt = 0.05
    optimal_iterations = 8
    iteration_window = 2
    linear_iteration_ratio = 1e6
    growth_factor = 1.2
    cutback_factor = 0.5
  []
[]

[Outputs]
  exodus = true
  csv = true
  time_step_interval = 1
  file_base = results/dam_2d_full_dynamic_cdp
[]
