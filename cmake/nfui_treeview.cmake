add_library(nfui_treeview STATIC src/controls/TreeView.cpp)
add_library(NativeFrameUI::nfui_treeview ALIAS nfui_treeview)
target_include_directories(nfui_treeview PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>)
target_link_libraries(nfui_treeview PUBLIC NativeFrameUI::nfui_core NativeFrameUI::nfui_theme)
nfui_apply_compiler_options(nfui_treeview)