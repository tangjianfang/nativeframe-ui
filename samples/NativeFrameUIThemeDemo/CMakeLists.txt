cmake_minimum_required(VERSION 3.25)

# Standalone CMake fragment — wired into the root CMakeLists via
# add_executable(... samples/NativeFrameUIThemeDemo/NativeFrameUIThemeDemo.cpp).
# This file exists so the sample can be opened and built in isolation
# (e.g. via `cmake -S samples/NativeFrameUIThemeDemo -B build`).

project(NativeFrameUIThemeDemo LANGUAGES CXX RC)

# Locate the NativeFrameUI umbrella + its cmake helpers from the parent tree.
set(NFUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
include("${NFUI_ROOT}/cmake/NativeFrameUICompilerOptions.cmake")
include("${NFUI_ROOT}/cmake/NativeFrameUIResources.cmake")

add_executable(NativeFrameUIThemeDemo WIN32
    NativeFrameUIThemeDemo.cpp
)
target_include_directories(NativeFrameUIThemeDemo PRIVATE
    "${NFUI_ROOT}/include"
    "${NFUI_ROOT}/resources"
    "${NFUI_ROOT}/samples"
)
target_link_libraries(NativeFrameUIThemeDemo PRIVATE NativeFrameUI::NativeFrameUI)
nfui_apply_compiler_options(NativeFrameUIThemeDemo)
nfui_add_resources(NativeFrameUIThemeDemo)