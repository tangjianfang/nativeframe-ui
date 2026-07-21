add_library(nfui_button STATIC
    src/controls/Button.cpp
)
add_library(NativeFrameUI::nfui_button ALIAS nfui_button)
target_include_directories(nfui_button
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(nfui_button
    PUBLIC
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
        NativeFrameUI::nfui_control_base
)
nfui_apply_compiler_options(nfui_button)
