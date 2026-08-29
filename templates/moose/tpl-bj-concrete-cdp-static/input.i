# =============================================================================
# Template ID : tpl-bj-concrete-cdp-static
# Status      : prototype / mock data; not for engineering conclusions
# Units       : SI (m, N, s, kg, Pa)
# Source INP  : test.inp
# SHA-256     : b06a04673f6f4db7c22b13c7a86fbb2a0d24efe6cd62b5fcd149c187b5f307ea
# Loading     : U2 ramps from 0 to -0.005 m while MOOSE pseudo-time t=0..1
# Mapping     : Abaqus CDP scalar parameters and four tabular curves are read
#               from test.inp and recorded in cdp-mapping-manifest.json.
# Mapping     : Abaqus kinematic couplings are represented by uniform
#               displacement constraints on the two coupled surfaces.
# Mapping     : C3D8R connectivity is retained as HEX8, but Abaqus reduced
#               integration/hourglass control is not yet reproduced.
# Mapping     : Abaqus viscosity=0.0005 is active in AbaqusCDPStressUpdate.
#               *Static stabilize=0.0002 has no approved MOOSE equivalent and
#               is deliberately not replaced by an uncalibrated force term.
# Validation  : Abaqus ODB/CSV values are comparison evidence, never solver
#               inputs. This template exercises the custom constitutive law.
# Performance : P4 diagnostics expose local Jacobian/factorization/substep
#               costs. Exodus is sampled every 0.01 pseudo-time and history
#               CSV every 0.005 pseudo-time to avoid per-accepted-step I/O.
# =============================================================================

[Mesh]
  [file]
    type = FileMeshGenerator
    file = bj_concrete_mesh.e
  []
[]

[GlobalParams]
  displacements = 'disp_x disp_y disp_z'
[]

[Physics/SolidMechanics/QuasiStatic/concrete]
  add_variables = true
  incremental = true
  block = Part_1__concrete
  strain = SMALL
  generate_output = 'stress_xx stress_xy stress_xz stress_yy stress_yz stress_zz
                     strain_xx strain_xy strain_xz strain_yy strain_yz strain_zz
                     max_principal_stress mid_principal_stress min_principal_stress
                     vonmises_stress'
  save_in = 'resid_x resid_y resid_z'
[]

[AuxVariables]
  [resid_x]
  []
  [resid_y]
  []
  [resid_z]
  []
  [DamageC]
    order = CONSTANT
    family = MONOMIAL
  []
  [DamageT]
    order = CONSTANT
    family = MONOMIAL
  []
  [combined_damage]
    order = CONSTANT
    family = MONOMIAL
  []
  [stiffness_factor]
    order = CONSTANT
    family = MONOMIAL
  []
  [kappa_c]
    order = CONSTANT
    family = MONOMIAL
  []
  [kappa_t]
    order = CONSTANT
    family = MONOMIAL
  []
  [local_iterations]
    order = CONSTANT
    family = MONOMIAL
  []
  [accepted_substeps]
    order = CONSTANT
    family = MONOMIAL
  []
  [jacobian_fallbacks]
    order = CONSTANT
    family = MONOMIAL
  []
  [automatic_jacobian_evaluations]
    order = CONSTANT
    family = MONOMIAL
  []
  [finite_difference_jacobian_evaluations]
    order = CONSTANT
    family = MONOMIAL
  []
  [failed_material_calls]
    order = CONSTANT
    family = MONOMIAL
  []
  [attempted_partitions]
    order = CONSTANT
    family = MONOMIAL
  []
  [maximum_partition_depth]
    order = CONSTANT
    family = MONOMIAL
  []
  [local_factorizations]
    order = CONSTANT
    family = MONOMIAL
  []
  [local_backsolves]
    order = CONSTANT
    family = MONOMIAL
  []
  [integration_microseconds]
    order = CONSTANT
    family = MONOMIAL
  []
[]

[AuxKernels]
  [damage_c]
    type = MaterialRealAux
    variable = DamageC
    property = DamageC
    block = Part_1__concrete
    execute_on = 'initial timestep_end'
  []
  [damage_t]
    type = MaterialRealAux
    variable = DamageT
    property = DamageT
    block = Part_1__concrete
    execute_on = 'initial timestep_end'
  []
  [combined_damage]
    type = MaterialRealAux
    variable = combined_damage
    property = cdp_combined_damage
    block = Part_1__concrete
    execute_on = 'initial timestep_end'
  []
  [stiffness_factor]
    type = MaterialRealAux
    variable = stiffness_factor
    property = cdp_stiffness_factor
    block = Part_1__concrete
    execute_on = 'initial timestep_end'
  []
  [kappa_c]
    type = MaterialRealAux
    variable = kappa_c
    property = cdp_kappa_c
    block = Part_1__concrete
    execute_on = 'initial timestep_end'
  []
  [kappa_t]
    type = MaterialRealAux
    variable = kappa_t
    property = cdp_kappa_t
    block = Part_1__concrete
    execute_on = 'initial timestep_end'
  []
  [local_iterations]
    type = MaterialRealAux
    variable = local_iterations
    property = cdp_local_iterations
    block = Part_1__concrete
    execute_on = 'initial timestep_end'
  []
  [accepted_substeps]
    type = MaterialRealAux
    variable = accepted_substeps
    property = cdp_accepted_substeps
    block = Part_1__concrete
    execute_on = 'initial timestep_end'
  []
  [jacobian_fallbacks]
    type = MaterialRealAux
    variable = jacobian_fallbacks
    property = cdp_jacobian_fallbacks
    block = Part_1__concrete
    execute_on = 'initial timestep_end'
  []
  [automatic_jacobian_evaluations]
    type = MaterialRealAux
    variable = automatic_jacobian_evaluations
    property = cdp_automatic_jacobian_evaluations
    block = Part_1__concrete
    execute_on = 'initial timestep_end'
  []
  [finite_difference_jacobian_evaluations]
    type = MaterialRealAux
    variable = finite_difference_jacobian_evaluations
    property = cdp_finite_difference_jacobian_evaluations
    block = Part_1__concrete
    execute_on = 'initial timestep_end'
  []
  [failed_material_calls]
    type = MaterialRealAux
    variable = failed_material_calls
    property = cdp_failed_material_calls
    block = Part_1__concrete
    execute_on = 'initial timestep_end'
  []
  [attempted_partitions]
    type = MaterialRealAux
    variable = attempted_partitions
    property = cdp_attempted_partitions
    block = Part_1__concrete
    execute_on = 'initial timestep_end'
  []
  [maximum_partition_depth]
    type = MaterialRealAux
    variable = maximum_partition_depth
    property = cdp_maximum_partition_depth
    block = Part_1__concrete
    execute_on = 'initial timestep_end'
  []
  [local_factorizations]
    type = MaterialRealAux
    variable = local_factorizations
    property = cdp_local_factorizations
    block = Part_1__concrete
    execute_on = 'initial timestep_end'
  []
  [local_backsolves]
    type = MaterialRealAux
    variable = local_backsolves
    property = cdp_local_backsolves
    block = Part_1__concrete
    execute_on = 'initial timestep_end'
  []
  [integration_microseconds]
    type = MaterialRealAux
    variable = integration_microseconds
    property = cdp_integration_microseconds
    block = Part_1__concrete
    execute_on = 'initial timestep_end'
  []
[]

[Functions/top_displacement]
  type = ParsedFunction
  expression = '-0.005*t'
[]

[BCs]
  [bottom_x]
    type = DirichletBC
    variable = disp_x
    boundary = SURF__PickedSurf10
    value = 0
  []
  [bottom_y]
    type = DirichletBC
    variable = disp_y
    boundary = SURF__PickedSurf10
    value = 0
  []
  [bottom_z]
    type = DirichletBC
    variable = disp_z
    boundary = SURF__PickedSurf10
    value = 0
  []
  [top_x]
    type = DirichletBC
    variable = disp_x
    boundary = SURF__PickedSurf8
    value = 0
  []
  [top_y]
    type = FunctionDirichletBC
    variable = disp_y
    boundary = SURF__PickedSurf8
    function = top_displacement
  []
  [top_z]
    type = DirichletBC
    variable = disp_z
    boundary = SURF__PickedSurf8
    value = 0
  []
[]

[Materials]
  [elasticity]
    type = ComputeIsotropicElasticityTensor
    block = Part_1__concrete
    youngs_modulus = 3.04e10
    poissons_ratio = 0.2
  []
  [stress]
    type = ComputeMultipleInelasticStress
    block = Part_1__concrete
    inelastic_models = cdp
    perform_finite_strain_rotations = false
  []
  [cdp]
    type = AbaqusCDPStressUpdate
    block = Part_1__concrete
    compression_hardening_file = compression_hardening.csv
    compression_damage_file = compression_damage.csv
    tension_stiffening_file = tension_stiffening.csv
    tension_damage_file = tension_damage.csv
    youngs_modulus = 3.04e10
    poissons_ratio = 0.2
    dilation_angle = 36.31
    eccentricity = 0.1
    biaxial_to_uniaxial_compression_ratio = 1.16
    tensile_meridian_ratio = 0.667
    viscosity = 0.0005
    tension_recovery = 1.0
    compression_recovery = 1.0
    maximum_substeps = 256
    maximum_strain_increment = 2.5e-5
    enable_performance_diagnostics = true
  []
[]

[Postprocessors]
  [RP1_Force]
    type = NodalSum
    variable = resid_y
    boundary = SURF__PickedSurf8
  []
  [RP1_Displacement]
    type = AverageNodalVariableValue
    variable = disp_y
    boundary = SURF__PickedSurf8
  []
  [max_mises]
    type = ElementExtremeValue
    variable = vonmises_stress
    value_type = max
  []
  [max_damagec]
    type = ElementExtremeValue
    variable = DamageC
    value_type = max
  []
  [max_damaget]
    type = ElementExtremeValue
    variable = DamageT
    value_type = max
  []
  [max_local_iterations]
    type = ElementExtremeValue
    variable = local_iterations
    value_type = max
  []
  [max_accepted_substeps]
    type = ElementExtremeValue
    variable = accepted_substeps
    value_type = max
  []
  [max_jacobian_fallbacks]
    type = ElementExtremeValue
    variable = jacobian_fallbacks
    value_type = max
  []
  [max_automatic_jacobian_evaluations]
    type = ElementExtremeValue
    variable = automatic_jacobian_evaluations
    value_type = max
  []
  [max_finite_difference_jacobian_evaluations]
    type = ElementExtremeValue
    variable = finite_difference_jacobian_evaluations
    value_type = max
  []
  [max_failed_material_calls]
    type = ElementExtremeValue
    variable = failed_material_calls
    value_type = max
  []
  [max_attempted_partitions]
    type = ElementExtremeValue
    variable = attempted_partitions
    value_type = max
  []
  [max_partition_depth]
    type = ElementExtremeValue
    variable = maximum_partition_depth
    value_type = max
  []
  [max_local_factorizations]
    type = ElementExtremeValue
    variable = local_factorizations
    value_type = max
  []
  [max_local_backsolves]
    type = ElementExtremeValue
    variable = local_backsolves
    value_type = max
  []
  [max_integration_microseconds]
    type = ElementExtremeValue
    variable = integration_microseconds
    value_type = max
  []
[]

[Preconditioning/smp]
  type = SMP
  full = true
[]

[Executioner]
  type = Transient
  start_time = 0
  end_time = 1
  solve_type = NEWTON
  line_search = bt
  automatic_scaling = true
  nl_rel_tol = 1e-9
  nl_abs_tol = 1e-8
  nl_max_its = 50
  dtmin = 1e-7
  dtmax = 0.002
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_type'
  petsc_options_value = 'lu mumps'

  [TimeStepper]
    type = IterationAdaptiveDT
    dt = 0.001
    optimal_iterations = 8
    iteration_window = 3
    growth_factor = 1.15
    cutback_factor = 0.5
  []
[]

[Times/field_output_times]
  type = TimeIntervalTimes
  start_time = 0
  end_time = 1
  time_interval = 0.01
[]

[Times/history_output_times]
  type = TimeIntervalTimes
  start_time = 0
  end_time = 1
  time_interval = 0.005
[]

[Outputs]
  [field_exodus]
    type = Exodus
    execute_on = 'initial timestep_end'
    sync_times_object = field_output_times
    sync_only = true
    file_base = bj_concrete_cdp_compatible
  []
  [history_csv]
    type = CSV
    execute_on = 'initial timestep_end'
    sync_times_object = history_output_times
    sync_only = true
    file_base = bj_concrete_cdp_compatible
  []
[]
