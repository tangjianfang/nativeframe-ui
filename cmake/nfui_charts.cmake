add_library(nfui_charts STATIC
    src/charts/AreaChartView.cpp
    src/charts/BarChartView.cpp
    src/charts/ChartGroup.cpp
    src/charts/ChartImageExport.cpp
    src/charts/ChartInteraction.cpp
    src/charts/ChartView.cpp
    src/charts/Charts.cpp
    src/charts/HBarChartView.cpp
    src/charts/KpiTile.cpp
    src/charts/LineChartView.cpp
    src/charts/SplineChartView.cpp
    src/charts/Sparkline.cpp
    src/charts/internal/ChartsPaint.cpp
    src/charts/internal/HitTest.cpp
    src/charts/internal/InteractionState.cpp
)
add_library(NativeFrameUI::nfui_charts ALIAS nfui_charts)

target_include_directories(nfui_charts
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)
target_link_libraries(nfui_charts
    PUBLIC
        NativeFrameUI::nfui_core
        NativeFrameUI::nfui_theme
        NativeFrameUI::nfui_window
    PRIVATE
        windowscodecs
        ole32
)
nfui_apply_compiler_options(nfui_charts)
# Gated so only the AA library pulls in wincodec.h's macros through the AA TU;
# base consumers linking nfui_charts alone never see the GDI+ runtime.
target_compile_definitions(nfui_charts PRIVATE NFUI_CHARTS_HAS_WIC=1)