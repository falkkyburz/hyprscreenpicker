cmake_minimum_required(VERSION 3.19)

project(hyprscreenpicker VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic)

find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED COMPONENTS GLES3)

pkg_check_modules(hyprtoolkit REQUIRED IMPORTED_TARGET hyprtoolkit)
pkg_check_modules(
  hyprtoolkit_deps
  REQUIRED
  IMPORTED_TARGET
  wayland-client
  wayland-protocols
  egl
  hyprutils>=0.11.0
  hyprlang>=0.6.0
  pixman-1
  libdrm
  gbm
  xkbcommon
  pango
  cairo
  pangocairo
  iniparser
  hyprgraphics>=0.3.0
  aquamarine>=0.10.0
  absl_flat_hash_map)

add_executable(hyprscreenpicker
  src/hyprscreenpicker.cpp
)
target_link_libraries(hyprscreenpicker PRIVATE PkgConfig::hyprtoolkit PkgConfig::hyprtoolkit_deps OpenGL::EGL OpenGL::OpenGL)

install(TARGETS hyprscreenpicker RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
