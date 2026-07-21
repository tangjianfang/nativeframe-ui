add_library(nfui_controls STATIC
    src/controls/Controls.cpp
)
add_library(NativeFrameUI::nfui_controls ALIAS nfui_controls)

target_include_directories(nfui_controls
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(nfui_controls
    PUBLIC
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
)
nfui_apply_compiler_options(nfui_controls)
