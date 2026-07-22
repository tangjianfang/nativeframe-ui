add_library(nfui_dialog STATIC
    src/core/Dialog.cpp
)
add_library(NativeFrameUI::nfui_dialog ALIAS nfui_dialog)

target_include_directories(nfui_dialog
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(nfui_dialog
    PUBLIC
        NativeFrameUI::nfui_core
)
nfui_apply_compiler_options(nfui_dialog)