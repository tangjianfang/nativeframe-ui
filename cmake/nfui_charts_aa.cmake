# nfui_charts_aa — opt-in antialiasing renderer for the chart module.
#
# Sibling of nfui_charts. Holds every GDI+ translation unit so the
# `gdiplus` link dependency stays scoped to this library. Consumers that
# want antialiased line + spline + circle markers must explicitly link
# NativeFrameUI::nfui_charts_aa on top of NativeFrameUI::nfui_charts;
# consumers that don't want AA stay on nfui_charts alone and get the
# existing GDI fallback path.
#
# The public antialiasing entry points (nfui::initialize_chart_aa /
# nfui::shutdown_chart_aa) live here so nfui_charts doesn't need to
# resolve GdiplusContext symbols at link time. Declarations still live
# in <nfui/Charts.hpp> — the consumer-facing header stays put.
add_library(nfui_charts_aa STATIC
    src/charts/ChartsPaint.cpp
    src/charts/internal/GdiplusContext.cpp
)
add_library(NativeFrameUI::nfui_charts_aa ALIAS nfui_charts_aa)

target_include_directories(nfui_charts_aa
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(nfui_charts_aa
    PUBLIC
        NativeFrameUI::nfui_charts
    PRIVATE
        gdiplus
)
nfui_apply_compiler_options(nfui_charts_aa)