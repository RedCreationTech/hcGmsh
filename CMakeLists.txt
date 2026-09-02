cmake_minimum_required(VERSION 3.21)

project(gmp_ise LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(GMP_ENABLE_MPI "Enable MPI related features" ON)
option(GMP_ENABLE_CATALYST "Enable Catalyst in-situ" OFF)
option(GMP_ENABLE_PARAVIEW "Enable ParaView SDK" OFF)
option(GMP_ENABLE_GMSH_GUI "Enable embedded Gmsh GUI" OFF)
option(GMP_ENABLE_WSL_RUNNER "Enable WSL runner (Windows only)" OFF)
option(GMP_ENABLE_VTK_VIEWER "Enable VTK viewer" OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

if(GMP_ENABLE_MPI)
  find_package(MPI)
endif()

if(GMP_ENABLE_PARAVIEW OR GMP_ENABLE_CATALYST)
  find_package(ParaView REQUIRED)
endif()

if(GMP_ENABLE_VTK_VIEWER)
  find_package(VTK REQUIRED)
endif()

if(GMP_ENABLE_GMSH_GUI)
  find_package(gmsh CONFIG REQUIRED)
  if(TARGET gmsh::shared)
    # conda-forge 的 gmsh 包在 INTERFACE_LINK_LIBRARIES 中残留了构建机绝对路径
    # (D:/bld/.../*.lib), 链接共享库无需传递依赖, 直接清空规避
    set_target_properties(gmsh::shared PROPERTIES INTERFACE_LINK_LIBRARIES "")
  endif()

  # --- WS0-A: OpenCASCADE 直调通道 ---
  # brew 的 cmake 配置位于 $(brew --prefix opencascade)/lib/cmake/opencascade;
  # /usr/local 在 CMake 默认搜索前缀内, 通常可直接找到。若找不到, 配置时显式加:
  #   -DOpenCASCADE_DIR="$(brew --prefix opencascade)/lib/cmake/opencascade"
  find_package(OpenCASCADE REQUIRED)

  # --- WS0-A: planegcs 约束求解器 (FetchContent) ---
  # 注: 原定的 https://github.com/FreeCAD/planegcs 已不存在 (404),
  # 改用等价的独立抽取库 marcuswu/libplanegcs (PlaneGCS from FreeCAD as a
  # standalone library)。该仓库无 release tag, 仅有 master 分支, 固定到当前
  # commit 以保证可复现。依赖 Eigen3 (brew eigen) 与 Boost graph (brew boost)。
  # 若网络拉取失败, 可回退为: git submodule add <url> external/planegcs,
  # 并改用 add_subdirectory(external/planegcs)。
  #
  # 不引入上游 CMakeLists (SOURCE_SUBDIR 指向不存在目录即跳过): 上游强制
  # add_library(SHARED) 且无任何 __declspec(dllexport) 导出宏, Windows/MSVC
  # 下 DLL 零导出 -> 不生成导入库 planegcs.lib -> LNK1181。改为自建 STATIC
  # 目标 (源文件清单同上游 CMakeLists), 也免去 Windows 打包再部署一个 DLL。
  include(FetchContent)
  FetchContent_Declare(planegcs
    GIT_REPOSITORY https://github.com/marcuswu/libplanegcs.git
    GIT_TAG        e6e749d0771a9b36604bf56a15e675d164ed4a7c # master
    SOURCE_SUBDIR  _skip_upstream_cmake_
  )
  FetchContent_MakeAvailable(planegcs)
  find_package(Eigen3 REQUIRED)
  find_package(Boost REQUIRED)  # planegcs 仅用 boost::graph, 头文件即可
  add_library(planegcs STATIC
    ${planegcs_SOURCE_DIR}/src/GCS.cpp
    ${planegcs_SOURCE_DIR}/src/Geo.cpp
    ${planegcs_SOURCE_DIR}/src/Constraints.cpp
    ${planegcs_SOURCE_DIR}/src/SubSystem.cpp
    ${planegcs_SOURCE_DIR}/src/qp_eq.cpp
  )
  target_include_directories(planegcs PUBLIC ${planegcs_SOURCE_DIR}/src)
  target_link_libraries(planegcs PUBLIC Eigen3::Eigen Boost::headers)
endif()

add_executable(gmp_ise
  src/main.cpp
  src/Env.cpp
  include/gmp/Env.h
  resources/icons.qrc
  src/MainWindow.cpp
  src/GmshPanel.cpp
  src/SketchPanel.cpp
  include/gmp/SketchPanel.h
  src/SketchDocument.cpp
  include/gmp/SketchDocument.h
  src/PartFeaturePanel.cpp
  include/gmp/PartFeaturePanel.h
  src/L10n.cpp
  src/MoosePanel.cpp
  src/MooseTemplates.cpp
  src/MooseSnapshot.cpp
  src/MooseMappingRegistry.cpp
  include/gmp/MooseMappingRegistry.h
  src/ApplicationProfile.cpp
  include/gmp/ApplicationProfile.h
  src/PhysicalGroupManifest.cpp
  include/gmp/PhysicalGroupManifest.h
  src/SimClient.cpp
  src/ArtifactDialog.cpp
  include/gmp/MooseTemplates.h
  include/gmp/MooseSnapshot.h
  include/gmp/SimClient.h
  include/gmp/ArtifactDialog.h
  src/VtkViewer.cpp
  src/PropertyEditor.cpp
  src/LocalRunner.cpp
  src/ComboPopupFix.cpp
  src/ProcessRunner.cpp
  src/RemoteRunner.cpp
  src/RunnerFactory.cpp
  src/WslRunner.cpp
  src/ProcessRunner.h
  include/gmp/Runner.h
  include/gmp/L10n.h
  include/gmp/MainWindow.h
  include/gmp/GmshPanel.h
  include/gmp/MoosePanel.h
  include/gmp/VtkViewer.h
  include/gmp/PropertyEditor.h
)

target_include_directories(gmp_ise PRIVATE include)
target_link_libraries(gmp_ise PRIVATE Qt6::Widgets Qt6::Network)

# TASK-E2E-10: MOOSE 模板库根目录（templates/moose/），运行期可被
# GMP_MOOSE_TEMPLATES 环境变量覆盖
target_compile_definitions(gmp_ise PRIVATE
  GMP_TEMPLATE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/templates/moose"
  GMP_PROFILE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/templates/moose/profiles")

add_subdirectory(external/yaml-cpp)
target_link_libraries(gmp_ise PRIVATE yaml-cpp)

if(GMP_ENABLE_GMSH_GUI)
  target_compile_definitions(gmp_ise PRIVATE GMP_ENABLE_GMSH_GUI)
  target_sources(gmp_ise PRIVATE src/OccBridge.cpp include/gmp/OccBridge.h)
  # SketchSolver 依赖 planegcs 头文件 (见下方 include_directories), 一并归入此块
  target_sources(gmp_ise PRIVATE src/SketchSolver.cpp include/gmp/SketchSolver.h)
  if(TARGET gmsh::shared)
    target_link_libraries(gmp_ise PRIVATE gmsh::shared)
  endif()
  # WS0-A: OCC 直调链接 TK 系列目标 (由 OpenCASCADEConfig 导出);
  # TKOffset 提供 BRepOffsetAPI_ThruSections, 其余传递依赖由目标自动带出
  target_link_libraries(gmp_ise PRIVATE
    TKernel TKMath TKBRep TKPrim TKBO TKTopAlgo TKGeomAlgo TKFillet TKOffset)
  # WS0-A: planegcs (上方自建的 STATIC 目标, PUBLIC 传递头文件目录)
  target_link_libraries(gmp_ise PRIVATE planegcs)
endif()

if(GMP_ENABLE_VTK_VIEWER)
  target_compile_definitions(gmp_ise PRIVATE GMP_ENABLE_VTK_VIEWER)
  target_link_libraries(gmp_ise PRIVATE ${VTK_LIBRARIES})
  if(VTK_VERSION VERSION_GREATER_EQUAL "9.0")
    vtk_module_autoinit(TARGETS gmp_ise MODULES ${VTK_LIBRARIES})
  endif()
endif()

# Phase 0 自测程序：验证 ApplicationProfile、MooseMappingRegistry、PhysicalGroupManifest、Snapshot v2
add_executable(gmp_ise_phase0_test tests/test_phase0.cpp
  src/ApplicationProfile.cpp include/gmp/ApplicationProfile.h
  src/MooseMappingRegistry.cpp include/gmp/MooseMappingRegistry.h
  src/MooseSnapshot.cpp include/gmp/MooseSnapshot.h
  src/MooseTemplates.cpp include/gmp/MooseTemplates.h
  src/PhysicalGroupManifest.cpp include/gmp/PhysicalGroupManifest.h
  src/Env.cpp include/gmp/Env.h
)
target_include_directories(gmp_ise_phase0_test PRIVATE include)
target_link_libraries(gmp_ise_phase0_test PRIVATE Qt6::Core Qt6::Network yaml-cpp)
target_compile_definitions(gmp_ise_phase0_test PRIVATE
  GMP_TEMPLATE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/templates/moose"
  GMP_PROFILE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/templates/moose/profiles")
