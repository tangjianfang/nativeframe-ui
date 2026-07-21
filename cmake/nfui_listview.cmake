add_library(nfui_listview STATIC
    src/controls/ListView.cpp
)
add_library(NativeFrameUI::nfui_listview ALIAS nfui_listview)
target_include_directories(nfui_listview
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(nfui_listview
    PUBLIC
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
        NativeFrameUI::nfui_control_base
)
nfui_apply_compiler_options(nfui_listview)
