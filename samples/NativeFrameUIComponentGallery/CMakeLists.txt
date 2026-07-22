cmake_minimum_required(VERSION 3.25)

# Standalone CMake fragment — wired into the root CMakeLists via
# add_executable(... samples/NativeFrameUIComponentGallery/NativeFrameUIComponentGallery.cpp).
# This file exists so the sample can be opened and built in isolation
# (e.g. via `cmake -S samples/NativeFrameUIComponentGallery -B build`).

project(NativeFrameUIComponentGallery LANGUAGES CXX RC)

# Locate the NativeFrameUI umbrella + its cmake helpers from the parent tree.
set(NFUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
include("${NFUI_ROOT}/cmake/NativeFrameUICompilerOptions.cmake")
include("${NFUI_ROOT}/cmake/NativeFrameUIResources.cmake")

add_executable(NativeFrameUIComponentGallery WIN32
    NativeFrameUIComponentGallery.cpp
)
target_include_directories(NativeFrameUIComponentGallery PRIVATE
    "${NFUI_ROOT}/include"
    "${NFUI_ROOT}/resources"
    "${NFUI_ROOT}/samples"
)
target_link_libraries(NativeFrameUIComponentGallery PRIVATE NativeFrameUI::NativeFrameUI)
nfui_apply_compiler_options(NativeFrameUIComponentGallery)
nfui_add_resources(NativeFrameUIComponentGallery)