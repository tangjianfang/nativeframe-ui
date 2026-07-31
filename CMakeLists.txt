cmake_minimum_required(VERSION 3.25)

project(NativeFrameUI VERSION 0.1.0 LANGUAGES CXX RC)

include(CTest)
include(cmake/NativeFrameUICompilerOptions.cmake)
include(cmake/NativeFrameUIResources.cmake)
include(cmake/NativeFrameUIQualityChecks.cmake)

# AutoTerminal integration: skip nfui_charts and nfui_charts_aa by default —
# the charts module has compile errors when built with MSVC that we're not
# ready to fix upstream. Set NFUI_BUILD_CHARTS=ON to opt back in.
option(NFUI_BUILD_CHARTS "Build nfui_charts / nfui_charts_aa modules" OFF)

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
if(NFUI_BUILD_CHARTS)
    include(cmake/nfui_charts.cmake)
    include(cmake/nfui_charts_aa.cmake)
endif()
include(cmake/nfui_menu.cmake)
include(cmake/nfui_slider.cmake)

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
        NativeFrameUI::nfui_menu
        NativeFrameUI::nfui_slider
)
if(NFUI_BUILD_CHARTS)
    target_link_libraries(NativeFrameUI INTERFACE NativeFrameUI::nfui_charts)
endif()

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
# CP11: Showcase paints via foundation helpers only — no framework controls.
# nfui_window is needed because the sample owns a custom : public nfui::Window.
target_link_libraries(NativeFrameUIShowcase PRIVATE
    NativeFrameUI::nfui_core
    NativeFrameUI::nfui_theme
    NativeFrameUI::nfui_window
)
nfui_apply_compiler_options(NativeFrameUIShowcase)
nfui_add_resources(NativeFrameUIShowcase)

add_executable(NativeFrameUIDarkStudio WIN32
    samples/NativeFrameUIDarkStudio/NativeFrameUIDarkStudio.cpp
)
# CP11: DarkStudio uses only StatusBar (a frame-typed control).
# nfui_window is needed because the sample owns a custom : public nfui::Window.
target_link_libraries(NativeFrameUIDarkStudio PRIVATE
    NativeFrameUI::nfui_core
    NativeFrameUI::nfui_theme
    NativeFrameUI::nfui_window
    NativeFrameUI::nfui_frame
)
nfui_apply_compiler_options(NativeFrameUIDarkStudio)
nfui_add_resources(NativeFrameUIDarkStudio)

add_executable(NativeFrameUISettingsDemo WIN32
    samples/NativeFrameUISettingsDemo/NativeFrameUISettingsDemo.cpp
)
# CP11: SettingsDemo uses Button, CheckBox, Edit, ComboBox, ListBox,
# StaticText, and StatusBar. Link only the components it actually uses.
# nfui_window is needed because the sample owns a custom : public nfui::Window.
target_link_libraries(NativeFrameUISettingsDemo PRIVATE
    NativeFrameUI::nfui_core
    NativeFrameUI::nfui_theme
    NativeFrameUI::nfui_window
    NativeFrameUI::nfui_button
    NativeFrameUI::nfui_checkbox
    NativeFrameUI::nfui_text
    NativeFrameUI::nfui_listbox
    NativeFrameUI::nfui_frame
)
nfui_apply_compiler_options(NativeFrameUISettingsDemo)
nfui_add_resources(NativeFrameUISettingsDemo)

add_executable(NativeFrameUIResourceGallery WIN32
    samples/NativeFrameUIResourceGallery/NativeFrameUIResourceGallery.cpp
)
# CP-B6: ResourceGallery now uses nfui::Menu for the themed File/View/Help
# bar (MENUINFO background brush) plus one StatusBar. nfui_window is needed
# because the sample owns a custom : public nfui::Window.
target_link_libraries(NativeFrameUIResourceGallery PRIVATE
    NativeFrameUI::nfui_core
    NativeFrameUI::nfui_theme
    NativeFrameUI::nfui_window
    NativeFrameUI::nfui_button
    NativeFrameUI::nfui_frame
    NativeFrameUI::nfui_menu
)
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

if(NFUI_BUILD_CHARTS)
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

add_executable(NativeFrameUIChartsInteractive WIN32
    samples/NativeFrameUIChartsInteractive/NativeFrameUIChartsInteractive.cpp
)
target_link_libraries(NativeFrameUIChartsInteractive
    PRIVATE
        NativeFrameUI::NativeFrameUI
        NativeFrameUI::nfui_charts
        NativeFrameUI::nfui_charts_aa
        comdlg32
)
nfui_apply_compiler_options(NativeFrameUIChartsInteractive)
nfui_add_resources(NativeFrameUIChartsInteractive)
endif()

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
    NativeFrameUI::nfui_text
)
nfui_apply_compiler_options(NativeFrameUIMinimal)
nfui_add_resources(NativeFrameUIMinimal)

add_executable(NativeFrameUIControlsPlayground WIN32
    samples/NativeFrameUIControlsPlayground/NativeFrameUIControlsPlayground.cpp
)
target_link_libraries(NativeFrameUIControlsPlayground PRIVATE NativeFrameUI::NativeFrameUI)
nfui_apply_compiler_options(NativeFrameUIControlsPlayground)
nfui_add_resources(NativeFrameUIControlsPlayground)

add_executable(NativeFrameUIIconGallery WIN32
    samples/NativeFrameUIIconGallery/NativeFrameUIIconGallery.cpp
)
target_link_libraries(NativeFrameUIIconGallery PRIVATE NativeFrameUI::NativeFrameUI)
nfui_apply_compiler_options(NativeFrameUIIconGallery)
nfui_add_resources(NativeFrameUIIconGallery)

add_executable(NativeFrameUIDialogTour WIN32
    samples/NativeFrameUIDialogTour/NativeFrameUIDialogTour.cpp
)
# CP12: DialogTour uses nfui::Dialog (nfui_dialog) + a custom : public nfui::Window
# + Application. CP22: the launch buttons and status strip now go through
# nfui::Button + nfui::StaticText so they adopt themed chrome; the dialog
# template still owns its own native Edit / CheckBox / ComboBox.
target_link_libraries(NativeFrameUIDialogTour PRIVATE
    NativeFrameUI::nfui_core
    NativeFrameUI::nfui_theme
    NativeFrameUI::nfui_window
    NativeFrameUI::nfui_dialog
    NativeFrameUI::nfui_button
    NativeFrameUI::nfui_text
)
nfui_apply_compiler_options(NativeFrameUIDialogTour)
nfui_add_resources(NativeFrameUIDialogTour)

if(BUILD_TESTING)
    add_subdirectory(tools/visual_audit)

    if(NFUI_BUILD_CHARTS)
        add_executable(NativeFrameUISmokeTest
            tests/nativeframeui_smoke.cpp
        )
        target_link_libraries(NativeFrameUISmokeTest PRIVATE NativeFrameUI::NativeFrameUI NativeFrameUI::nfui_charts_aa psapi)
        nfui_apply_compiler_options(NativeFrameUISmokeTest)
        nfui_add_resources(NativeFrameUISmokeTest)
        add_test(NAME NativeFrameUISmokeTest COMMAND NativeFrameUISmokeTest)
        # CP29: the smoke test's modal-dialog round-trip calls DialogBoxParamW
        # which blocks on its own message pump until the dialog is dismissed.
        # In CI / headless builds nobody dismisses it and the test hangs to
        # the ctest timeout. Set NONFUI_SKIP_DIALOG=1 by default so ctest never
        # invokes the modal round-trip; developers running the test directly
        # can clear the variable to exercise the modal path interactively.
        set_tests_properties(NativeFrameUISmokeTest PROPERTIES
            ENVIRONMENT "NONFUI_SKIP_DIALOG=1"
        )
    endif()
    add_test(NAME NativeFrameUIArchitectureCheck
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_architecture.ps1
                -Root ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # Quality checks (boundary + per-pixel diff) are registered through
    # this single helper so the test list stays in lockstep with the
    # scripts that own them. CP-B20 added the per-pixel diff scorer and
    # its self-test to the helper.
    nfui_add_quality_checks()

    # Pure-logic unit tests (no HWND, no message loop).
    add_executable(NativeFrameUILayoutTest tests/test_layout.cpp)
    target_link_libraries(NativeFrameUILayoutTest PRIVATE NativeFrameUI::nfui_layout)
    nfui_apply_compiler_options(NativeFrameUILayoutTest)
    add_test(NAME NativeFrameUILayoutTest COMMAND NativeFrameUILayoutTest)

    add_executable(NativeFrameUIThemeTest tests/test_theme.cpp)
    target_link_libraries(NativeFrameUIThemeTest PRIVATE NativeFrameUI::nfui_theme)
    nfui_apply_compiler_options(NativeFrameUIThemeTest)
    add_test(NAME NativeFrameUIThemeTest COMMAND NativeFrameUIThemeTest)

    add_executable(NativeFrameUIPersistenceTest tests/test_persistence.cpp)
    target_link_libraries(NativeFrameUIPersistenceTest PRIVATE NativeFrameUI::nfui_core)
    nfui_apply_compiler_options(NativeFrameUIPersistenceTest)
    add_test(NAME NativeFrameUIPersistenceTest COMMAND NativeFrameUIPersistenceTest)

    if(NFUI_BUILD_CHARTS)
        add_executable(NativeFrameUIChartInteractionTest tests/test_chart_interaction.cpp)
        target_link_libraries(NativeFrameUIChartInteractionTest PRIVATE NativeFrameUI::nfui_charts NativeFrameUI::nfui_charts_aa)
        nfui_apply_compiler_options(NativeFrameUIChartInteractionTest)
        add_test(NAME NativeFrameUIChartInteractionTest COMMAND NativeFrameUIChartInteractionTest)
    endif()
endif()
