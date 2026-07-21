add_library(nfui_iconview STATIC
    src/controls/IconView.cpp
)
add_library(NativeFrameUI::nfui_iconview ALIAS nfui_iconview)
target_include_directories(nfui_iconview
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(nfui_iconview
    PUBLIC
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
)
nfui_apply_compiler_options(nfui_iconview)