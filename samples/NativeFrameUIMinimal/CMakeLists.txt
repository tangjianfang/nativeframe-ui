add_executable(NativeFrameUIMinimal WIN32
    NativeFrameUIMinimal.cpp
)
# Per-component link only -- proves the layered architecture.
# TODO(P2.1): switch NativeFrameUI::nfui_controls to
# NativeFrameUI::nfui_control_base once the base-class extraction lands.
target_link_libraries(NativeFrameUIMinimal PRIVATE
    NativeFrameUI::nfui_core
    NativeFrameUI::nfui_theme
    NativeFrameUI::nfui_controls
    NativeFrameUI::nfui_button
)
nfui_apply_compiler_options(NativeFrameUIMinimal)
nfui_add_resources(NativeFrameUIMinimal)