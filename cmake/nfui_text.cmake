add_library(nfui_text STATIC
    src/controls/StaticText.cpp
    src/controls/Edit.cpp
)
add_library(NativeFrameUI::nfui_text ALIAS nfui_text)
target_include_directories(nfui_text
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(nfui_text
    PUBLIC
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
        NativeFrameUI::nfui_control_base
)
nfui_apply_compiler_options(nfui_text)
