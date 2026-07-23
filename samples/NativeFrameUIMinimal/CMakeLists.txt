add_executable(NativeFrameUIMinimal WIN32
    NativeFrameUIMinimal.cpp
)
# Per-component link only -- proves the layered architecture.
# NativeFrameUI::nfui_control_base replaces nfui_controls after P2.1 merged.
# CP30: nfui_text is no longer needed -- the previous hidden StaticText
# status row was removed in favour of a process-local std::wstring so the
# parent WM_PAINT can draw the status line without a child control
# clobbering it.
target_link_libraries(NativeFrameUIMinimal PRIVATE
    NativeFrameUI::nfui_core
    NativeFrameUI::nfui_theme
    NativeFrameUI::nfui_control_base
    NativeFrameUI::nfui_button
)
nfui_apply_compiler_options(NativeFrameUIMinimal)
nfui_add_resources(NativeFrameUIMinimal)