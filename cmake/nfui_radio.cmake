add_library(nfui_radio STATIC
    src/controls/RadioButton.cpp
)
add_library(NativeFrameUI::nfui_radio ALIAS nfui_radio)
target_include_directories(nfui_radio
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(nfui_radio
    PUBLIC
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
        NativeFrameUI::nfui_controls
)
nfui_apply_compiler_options(nfui_radio)