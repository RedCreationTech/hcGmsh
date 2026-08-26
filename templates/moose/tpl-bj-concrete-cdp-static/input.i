# =============================================================================
# Calibration model: equivalent secant-stiffness damage surrogate
# Data class: mock; status: prototype; SI units (m, N, s, kg, Pa)
# Source RP curve: 参考点RP1力-位移时程.xlsx (101 points)
# This model is intended to obtain a stable full MOOSE result after the native
# BlackBear DamagePlasticityStressUpdate return mapping failed. It is not a
# constitutive-equivalence claim for Abaqus CDP.
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

[Physics]
  [SolidMechanics]
    [QuasiStatic]
      [concrete]
        add_variables = true
        incremental = false
        block = Part_1__concrete
        strain = SMALL
        generate_output = 'stress_xx stress_xy stress_xz stress_yy stress_yz stress_zz
                           strain_xx strain_xy strain_xz strain_yy strain_yz strain_zz
                           max_principal_stress mid_principal_stress min_principal_stress
                           vonmises_stress'
        save_in = 'resid_x resid_y resid_z'
      []
    []
  []
[]

[AuxVariables]
  [resid_x]
  []
  [resid_y]
  []
  [resid_z]
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

[Functions]
  [top_displacement]
    type = ParsedFunction
    expression = '-0.005*t'
  []
  [effective_E]
    type = PiecewiseLinear
    x = '0 0.01 0.02 0.03 0.04 0.05 0.06 0.07 0.08 0.09 0.1 0.11 0.12 0.13 0.14 0.15 0.16 0.17 0.18 0.19 0.2 0.21 0.22 0.23 0.24 0.25 0.26 0.27 0.28 0.29 0.3 0.31 0.32 0.33 0.34 0.35 0.36 0.37 0.38 0.39 0.4 0.41 0.42 0.43 0.44 0.45 0.46 0.47 0.48 0.49 0.5 0.51 0.52 0.53 0.54 0.55 0.56 0.57 0.58 0.59 0.6 0.61 0.62 0.63 0.64 0.65 0.66 0.67 0.68 0.69 0.7 0.71 0.72 0.73 0.74 0.75 0.76 0.77 0.78 0.79 0.8 0.81 0.82 0.83 0.84 0.85 0.86 0.87 0.88 0.89 0.9 0.91 0.92 0.93 0.94 0.95 0.96 0.97 0.98 0.99 1'
    y = '30395412985 30395412985 26724368577.2 23232536496.4 19852128858.8 16770592772.1 13969423742.4 11550622074.2 9550914227.9 7935272400.54 6641423919.15 5607152613.33 4776853159.83 4102946647.3 3553771924.73 3105073906.36 2736506452.71 2431755942.93 2177219509.72 1962751540.88 1780993785.9 1625520488.21 1491834217.29 1376140809.27 1276671806.5 1193154887.36 1127079497.85 1072081627.74 1022428667.8 978399010.743 937887925.101 901048271.651 868228476 837503806.318 808941001.312 781743723.015 756179817.415 732498683.383 709961598.344 688719845.072 668248548.99 648431270.368 629499036.982 611634257.236 595120587.445 580000447.458 566394281.326 553789034.439 542022238.904 531050496.488 520691305.917 510862888.774 501539608.65 492700209.501 484120988.482 475804250.329 467777594.956 460109889.956 452827263.255 445839835.73 439089642.908 432575641.568 426288493.632 420215339.199 414406903.672 408807100.056 403412336.607 398254057.562 393283708.157 388472454.507 383831994.444 379413528.71 375179004.531 371071147.881 367076065.918 363173694.03 359367194.373 355654514.959 352038694.164 348506209.748 345050696.257 341670903.084 338367221.279 335133776.61 331969603.627 328879881.774 325884620.93 322982053.631 320108631.741 317285217.81 314517345.659 311807457.213 309155070.542 306556937.057 304015461.876 301523398.375 299094053.993 296713462.964 294374842.49 292076920.734 289824957.414'
  []
  [damagec_curve]
    type = PiecewiseLinear
    x = '0 0.19 0.38 0.59 0.78 0.97 1'
    y = '0 0.740072776 0.845154798 0.87725556 0.887343171 0.892585616 0.892585616'
  []
  [damaget_curve]
    type = PiecewiseLinear
    x = '0 0.19 0.38 0.59 0.78 0.97 1'
    y = '0 0.0857730153 0.144428147 0.197927393 0.208499082 0.211643244 0.211643244'
  []
[]

[AuxKernels]
  [damagec]
    type = FunctionAux
    variable = DAMAGEC
    function = damagec_curve
    execute_on = 'INITIAL TIMESTEP_END'
  []
  [damaget]
    type = FunctionAux
    variable = DAMAGET
    function = damaget_curve
    execute_on = 'INITIAL TIMESTEP_END'
  []
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
  [effective_modulus]
    type = GenericFunctionMaterial
    prop_names = effective_youngs_modulus
    prop_values = effective_E
  []
  [elasticity]
    type = ComputeVariableIsotropicElasticityTensor
    block = Part_1__concrete
    youngs_modulus = effective_youngs_modulus
    poissons_ratio = 0.2
    args = ''
  []
  [stress]
    type = ComputeLinearElasticStress
    block = Part_1__concrete
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
  start_time = 0
  end_time = 1
  dt = 0.01
  solve_type = NEWTON
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_type'
  petsc_options_value = 'lu mumps'
  nl_rel_tol = 1e-10
  nl_abs_tol = 1e-8
  nl_max_its = 30
[]

[Outputs]
  exodus = true
  csv = true
  execute_on = 'INITIAL TIMESTEP_END'
  file_base = bj_concrete_out
[]
