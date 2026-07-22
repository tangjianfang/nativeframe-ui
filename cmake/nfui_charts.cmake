add_library(nfui_charts STATIC
    src/charts/AreaChartView.cpp
    src/charts/BarChartView.cpp
    src/charts/ChartView.cpp
    src/charts/Charts.cpp
    src/charts/HBarChartView.cpp
    src/charts/LineChartView.cpp
    src/charts/SplineChartView.cpp
    src/charts/internal/ChartsPaint.cpp
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
)
nfui_apply_compiler_options(nfui_charts)