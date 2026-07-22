cmake_minimum_required(VERSION 3.25)

project(NativeFrameUI VERSION 0.1.0 LANGUAGES CXX RC)

include(CTest)
include(cmake/NativeFrameUICompilerOptions.cmake)
include(cmake/NativeFrameUIResources.cmake)

include(cmake/nfui_core.cmake)
include(cmake/nfui_command.cmake)
include(cmake/nfui_layout.cmake)
include(cmake/nfui_theme.cmake)
include(cmake/nfui_window.cmake)
include(cmake/nfui_dialog.cmake)
include(cmake/nfui_control_base.cmake)
include(cmake/nfui_button.cmake)
include(cmake/nfui_checkbox.cmake)
include(cmake/nfui_radio.cmake)
include(cmake/nfui_text.cmake)
include(cmake/nfui_listbox.cmake)
include(cmake/nfui_listview.cmake)
include(cmake/nfui_treeview.cmake)
include(cmake/nfui_iconview.cmake)
include(cmake/nfui_frame.cmake)
include(cmake/nfui_charts.cmake)
include(cmake/nfui_charts_aa.cmake)

add_library(NativeFrameUI INTERFACE)
add_library(NativeFrameUI::NativeFrameUI ALIAS NativeFrameUI)
target_include_directories(NativeFrameUI
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(NativeFrameUI
    INTERFACE
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_command
        NativeFrameUI::nfui_layout
        NativeFrameUI::nfui_theme
        NativeFrameUI::nfui_window
        NativeFrameUI::nfui_dialog
        NativeFrameUI::nfui_control_base
        NativeFrameUI::nfui_button
        NativeFrameUI::nfui_checkbox
        NativeFrameUI::nfui_radio
        NativeFrameUI::nfui_text
        NativeFrameUI::nfui_listbox
        NativeFrameUI::nfui_listview
        NativeFrameUI::nfui_treeview
        NativeFrameUI::nfui_iconview
        NativeFrameUI::nfui_frame
        NativeFrameUI::nfui_charts
)

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

add_executable(NativeFrameUIThemeDemo WIN32
    samples/NativeFrameUIThemeDemo/NativeFrameUIThemeDemo.cpp
)
target_link_libraries(NativeFrameUIThemeDemo PRIVATE NativeFrameUI::NativeFrameUI)
nfui_apply_compiler_options(NativeFrameUIThemeDemo)
nfui_add_resources(NativeFrameUIThemeDemo)

add_executable(NativeFrameUIComponentGallery WIN32
    samples/NativeFrameUIComponentGallery/NativeFrameUIComponentGallery.cpp
)
target_link_libraries(NativeFrameUIComponentGallery PRIVATE NativeFrameUI::NativeFrameUI)
nfui_apply_compiler_options(NativeFrameUIComponentGallery)
nfui_add_resources(NativeFrameUIComponentGallery)

add_executable(NativeFrameUIControls WIN32
    samples/NativeFrameUIControls/NativeFrameUIControls.cpp
)
target_link_libraries(NativeFrameUIControls PRIVATE NativeFrameUI::NativeFrameUI)
nfui_apply_compiler_options(NativeFrameUIControls)
nfui_add_resources(NativeFrameUIControls)

add_executable(NativeFrameUICharts WIN32
    samples/NativeFrameUICharts/NativeFrameUICharts.cpp
)
target_link_libraries(NativeFrameUICharts
    PRIVATE
        NativeFrameUI::NativeFrameUI
        NativeFrameUI::nfui_charts
        NativeFrameUI::nfui_charts_aa
)
nfui_apply_compiler_options(NativeFrameUICharts)
nfui_add_resources(NativeFrameUICharts)

add_executable(NativeFrameUIMinimal WIN32
    samples/NativeFrameUIMinimal/NativeFrameUIMinimal.cpp
)
# Per-component link only — proves the layered architecture.
# nfui_control_base replaces nfui_controls after P2.1 merged.
target_link_libraries(NativeFrameUIMinimal PRIVATE
    NativeFrameUI::nfui_core
    NativeFrameUI::nfui_theme
    NativeFrameUI::nfui_control_base
    NativeFrameUI::nfui_button
)
nfui_apply_compiler_options(NativeFrameUIMinimal)
nfui_add_resources(NativeFrameUIMinimal)

add_executable(NativeFrameUIControlsPlayground WIN32
    samples/NativeFrameUIControlsPlayground/NativeFrameUIControlsPlayground.cpp
)
target_link_libraries(NativeFrameUIControlsPlayground PRIVATE NativeFrameUI::NativeFrameUI)
nfui_apply_compiler_options(NativeFrameUIControlsPlayground)
nfui_add_resources(NativeFrameUIControlsPlayground)

if(BUILD_TESTING)
    add_executable(NativeFrameUISmokeTest
        tests/nativeframeui_smoke.cpp
    )
    target_link_libraries(NativeFrameUISmokeTest PRIVATE NativeFrameUI::NativeFrameUI NativeFrameUI::nfui_charts_aa psapi)
    nfui_apply_compiler_options(NativeFrameUISmokeTest)
    nfui_add_resources(NativeFrameUISmokeTest)
    add_test(NAME NativeFrameUISmokeTest COMMAND NativeFrameUISmokeTest)
    add_test(NAME NativeFrameUIBoundaryCheck
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_boundaries.ps1
                -Root ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
