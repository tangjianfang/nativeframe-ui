cmake_minimum_required(VERSION 3.25)

project(NativeFrameUI VERSION 0.1.0 LANGUAGES CXX RC)

include(CTest)
include(cmake/NativeFrameUICompilerOptions.cmake)
include(cmake/NativeFrameUIResources.cmake)

add_library(NativeFrameUI STATIC
    src/command/Command.cpp
    src/controls/Controls.cpp
    src/core/Application.cpp
    src/core/Window.cpp
    src/dpi/Dpi.cpp
    src/font/Font.cpp
    src/icon/Icon.cpp
    src/layout/Layout.cpp
    src/paint/Paint.cpp
    src/persistence/Persistence.cpp
    src/resource/ResourceContext.cpp
    src/theme/Theme.cpp
)
add_library(NativeFrameUI::NativeFrameUI ALIAS NativeFrameUI)

target_include_directories(NativeFrameUI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(NativeFrameUI PUBLIC comctl32 uxtheme)
nfui_apply_compiler_options(NativeFrameUI)

add_executable(NativeFrameUIWorkbench WIN32
    samples/NativeFrameUIWorkbench/NativeFrameUIWorkbench.cpp
)
target_link_libraries(NativeFrameUIWorkbench PRIVATE NativeFrameUI::NativeFrameUI)
nfui_apply_compiler_options(NativeFrameUIWorkbench)
nfui_add_resources(NativeFrameUIWorkbench)

add_executable(NativeFrameUIShowcase WIN32
    samples/NativeFrameUIShowcase/NativeFrameUIShowcase.cpp
    samples/NativeFrameUIShowcase/ShowcaseView.cpp
)
target_link_libraries(NativeFrameUIShowcase PRIVATE NativeFrameUI::NativeFrameUI)
nfui_apply_compiler_options(NativeFrameUIShowcase)
nfui_add_resources(NativeFrameUIShowcase)

add_executable(NativeFrameUIDarkStudio WIN32
    samples/NativeFrameUIDarkStudio/NativeFrameUIDarkStudio.cpp
)
target_link_libraries(NativeFrameUIDarkStudio PRIVATE NativeFrameUI::NativeFrameUI)
nfui_apply_compiler_options(NativeFrameUIDarkStudio)
nfui_add_resources(NativeFrameUIDarkStudio)

add_executable(NativeFrameUISettingsDemo WIN32
    samples/NativeFrameUISettingsDemo/NativeFrameUISettingsDemo.cpp
)
target_link_libraries(NativeFrameUISettingsDemo PRIVATE NativeFrameUI::NativeFrameUI)
nfui_apply_compiler_options(NativeFrameUISettingsDemo)
nfui_add_resources(NativeFrameUISettingsDemo)

add_executable(NativeFrameUIResourceGallery WIN32
    samples/NativeFrameUIResourceGallery/NativeFrameUIResourceGallery.cpp
)
target_link_libraries(NativeFrameUIResourceGallery PRIVATE NativeFrameUI::NativeFrameUI)
nfui_apply_compiler_options(NativeFrameUIResourceGallery)
nfui_add_resources(NativeFrameUIResourceGallery)

if(BUILD_TESTING)
    add_executable(NativeFrameUISmokeTest
        tests/nativeframeui_smoke.cpp
    )
    target_link_libraries(NativeFrameUISmokeTest PRIVATE NativeFrameUI::NativeFrameUI psapi)
    nfui_apply_compiler_options(NativeFrameUISmokeTest)
    nfui_add_resources(NativeFrameUISmokeTest)
    add_test(NAME NativeFrameUISmokeTest COMMAND NativeFrameUISmokeTest)
    add_test(NAME NativeFrameUIBoundaryCheck
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_boundaries.ps1
                -Root ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
