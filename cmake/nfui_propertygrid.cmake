add_library(nfui_propertygrid STATIC
    src/controls/PropertyGrid.cpp
)
add_library(NativeFrameUI::nfui_propertygrid ALIAS nfui_propertygrid)
target_include_directories(nfui_propertygrid
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(nfui_propertygrid
    PUBLIC
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
        NativeFrameUI::nfui_control_base
)
nfui_apply_compiler_options(nfui_propertygrid)
