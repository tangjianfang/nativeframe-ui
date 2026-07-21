add_library(nfui_layout STATIC
    src/layout/Layout.cpp
)
add_library(NativeFrameUI::nfui_layout ALIAS nfui_layout)

target_include_directories(nfui_layout
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
nfui_apply_compiler_options(nfui_layout)
