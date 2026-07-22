add_library(nfui_menu STATIC
    src/menu/Menu.cpp
)
add_library(NativeFrameUI::nfui_menu ALIAS nfui_menu)
target_include_directories(nfui_menu
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(nfui_menu
    PUBLIC
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
)
nfui_apply_compiler_options(nfui_menu)