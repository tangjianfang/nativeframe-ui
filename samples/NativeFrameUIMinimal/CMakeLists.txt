add_executable(NativeFrameUIMinimal WIN32
    NativeFrameUIMinimal.cpp
)
# Per-component link only -- proves the layered architecture.
# NativeFrameUI::nfui_control_base replaces nfui_controls after P2.1 merged.
target_link_libraries(NativeFrameUIMinimal PRIVATE
    NativeFrameUI::nfui_core
    NativeFrameUI::nfui_theme
    NativeFrameUI::nfui_control_base
    NativeFrameUI::nfui_text
    NativeFrameUI::nfui_button
)
nfui_apply_compiler_options(NativeFrameUIMinimal)
nfui_add_resources(NativeFrameUIMinimal)