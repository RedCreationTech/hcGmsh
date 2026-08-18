# =============================================================================
# Template: tpl-dam-2d-static —— 2D 坝静力预载（静力阶段）
# Source repo : DamSafetyApp @ 69e99d74064a22adbdda0eb17d3c14ce096b7e57
#               (tools/abaqus/gen_dam_two_step_case.py 由 Abaqus dam-2b.inp 转换报告生成;
#                求解器 BlackBear@1c190fd3d2b5f06a3518923f550a0e0a90b015d4 / MOOSE@4bce02d91b56c7ed845a5747df4d24f415592504)
# Source file : .build/cases/abaqus-2d-dam-p0/full-static-dynamic-v1/generated/dam_2d_full_static.i
# Source SHA-256 (input, 未加本头注释/未改相对路径前): ff30af208ad9b95a9adb7eeb88b173ac4b0c3439f3ed13e05bfb14f5b80c08ed
# Mesh        : dam-2b.e  SHA-256: 21a1e7a333b995e33e4bd0bae216b8ab8bbf2530799abc124e560e4d5bce462b
#               (源文件 ../converted/dam-2b.e 已改为同目录 dam-2b.e)
# Extracted   : 2026-08-18
# Status      : prototype —— 不作为工程结论（未经官方回归对照; 线弹性平面应力重解释,
#               证据: DamSafetyApp doc/prototypes/abaqus-2d-static-dynamic-rerun.md）
# =============================================================================
# Generated linear-elastic reinterpretation of the Abaqus static step.
[GlobalParams]
  displacements = 'disp_x disp_y'
  out_of_plane_strain = strain_zz
[]

[Mesh]
  [file]
    type = FileMeshGenerator
    file = dam-2b.e
  []
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
  [strain_zz]
  []
[]

[Physics/SolidMechanics/QuasiStatic]
  [dam]
    planar_formulation = WEAK_PLANE_STRESS
    strain = SMALL
    generate_output = 'stress_xx stress_yy stress_xy vonmises_stress'
  []
[]

[Functions]
  [hydrostatic_pressure]
    type = ParsedFunction
    expression = 'max(0, 592116 * (60.42 - y) / 60.42)'
  []
[]

[Kernels]
  [gravity_y]
    type = Gravity
    variable = disp_y
    value = -9.800000000000001
  []
[]

[BCs]
  [fix_1_1]
    type = DirichletBC
    variable = disp_x
    boundary = _PICKEDSET4__DAM_1
    value = 0
  []
  [fix_2_2]
    type = DirichletBC
    variable = disp_y
    boundary = _PICKEDSET4__DAM_1
    value = 0
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
  [max_vonmises]
    type = ElementExtremeValue
    variable = vonmises_stress
    value_type = max
  []
[]

[Executioner]
  type = Steady
  solve_type = LINEAR
  petsc_options_iname = '-pc_type -pc_hypre_type'
  petsc_options_value = 'hypre boomeramg'
  l_tol = 1e-10
  l_max_its = 300
[]

[Outputs]
  exodus = true
  csv = true
  file_base = dam_2d_full_static
[]
