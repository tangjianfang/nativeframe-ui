add_library(nfui_listbox STATIC
    src/controls/ListBox.cpp
    src/controls/ComboBox.cpp
)
add_library(NativeFrameUI::nfui_listbox ALIAS nfui_listbox)
target_include_directories(nfui_listbox
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(nfui_listbox
    PUBLIC
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
)
nfui_apply_compiler_options(nfui_listbox)
