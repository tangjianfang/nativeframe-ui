add_library(nfui_window STATIC
    src/core/Window.cpp
)
add_library(NativeFrameUI::nfui_window ALIAS nfui_window)

target_include_directories(nfui_window
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(nfui_window
    PUBLIC
        comctl32
)
nfui_apply_compiler_options(nfui_window)