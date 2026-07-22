add_library(nfui_command STATIC
    src/command/Command.cpp
)
add_library(NativeFrameUI::nfui_command ALIAS nfui_command)

target_include_directories(nfui_command
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(nfui_command
    PUBLIC
        NativeFrameUI::nfui_core
)
nfui_apply_compiler_options(nfui_command)