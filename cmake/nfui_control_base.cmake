add_library(nfui_control_base STATIC
    src/controls/Controls.cpp
)
add_library(NativeFrameUI::nfui_control_base ALIAS nfui_control_base)

target_include_directories(nfui_control_base
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(nfui_control_base
    PUBLIC
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
)
nfui_apply_compiler_options(nfui_control_base)
