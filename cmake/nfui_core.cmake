add_library(nfui_core STATIC
    src/command/Command.cpp
    src/core/Application.cpp
    src/core/HoverState.cpp
    src/dpi/Dpi.cpp
    src/font/Font.cpp
    src/icon/Icon.cpp
    src/paint/Paint.cpp
    src/persistence/Persistence.cpp
    src/resource/ResourceContext.cpp
)
add_library(NativeFrameUI::nfui_core ALIAS nfui_core)

target_include_directories(nfui_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(nfui_core PUBLIC comctl32)
nfui_apply_compiler_options(nfui_core)
