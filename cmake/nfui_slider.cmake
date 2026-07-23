add_library(nfui_slider STATIC
    src/controls/Slider.cpp
)
add_library(NativeFrameUI::nfui_slider ALIAS nfui_slider)
target_include_directories(nfui_slider
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(nfui_slider
    PUBLIC
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
        NativeFrameUI::nfui_control_base
)
nfui_apply_compiler_options(nfui_slider)
