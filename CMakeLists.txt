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

find_package(Qt6 REQUIRED COMPONENTS Widgets)

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
endif()

add_executable(gmp_ise
  src/main.cpp
  src/MainWindow.cpp
  src/GmshPanel.cpp
  src/L10n.cpp
  src/MoosePanel.cpp
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
target_link_libraries(gmp_ise PRIVATE Qt6::Widgets)

add_subdirectory(external/yaml-cpp)
target_link_libraries(gmp_ise PRIVATE yaml-cpp)

if(GMP_ENABLE_GMSH_GUI)
  target_compile_definitions(gmp_ise PRIVATE GMP_ENABLE_GMSH_GUI)
  if(TARGET gmsh::shared)
    target_link_libraries(gmp_ise PRIVATE gmsh::shared)
  endif()
endif()

if(GMP_ENABLE_VTK_VIEWER)
  target_compile_definitions(gmp_ise PRIVATE GMP_ENABLE_VTK_VIEWER)
  target_link_libraries(gmp_ise PRIVATE ${VTK_LIBRARIES})
  if(VTK_VERSION VERSION_GREATER_EQUAL "9.0")
    vtk_module_autoinit(TARGETS gmp_ise MODULES ${VTK_LIBRARIES})
  endif()
endif()
