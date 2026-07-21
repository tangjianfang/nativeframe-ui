add_library(nfui_theme STATIC
    src/theme/Theme.cpp
)
add_library(NativeFrameUI::nfui_theme ALIAS nfui_theme)

target_include_directories(nfui_theme
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(nfui_theme PUBLIC NativeFrameUI::nfui_core)
nfui_apply_compiler_options(nfui_theme)
