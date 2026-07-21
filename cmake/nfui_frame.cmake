add_library(nfui_frame STATIC
    src/controls/StatusBar.cpp
    src/controls/TabControl.cpp
    src/controls/Tooltip.cpp
    src/controls/ProgressBar.cpp
    src/controls/Panel.cpp
    src/controls/Splitter.cpp
)
add_library(NativeFrameUI::nfui_frame ALIAS nfui_frame)
target_include_directories(nfui_frame
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(nfui_frame
    PUBLIC
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
        NativeFrameUI::nfui_control_base
)
nfui_apply_compiler_options(nfui_frame)