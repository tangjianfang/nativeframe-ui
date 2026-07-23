add_executable(NativeFrameUIVisualAudit
    NativeFrameUIVisualAudit.cpp
)
set_target_properties(NativeFrameUIVisualAudit PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_compile_features(NativeFrameUIVisualAudit PRIVATE cxx_std_20)
target_compile_definitions(NativeFrameUIVisualAudit PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
)
target_link_libraries(NativeFrameUIVisualAudit PRIVATE
    windowscodecs
    ole32
    gdi32
    user32
    dwmapi
    uxtheme
)

if(MSVC)
    target_compile_options(NativeFrameUIVisualAudit PRIVATE
        /W4 /permissive- /EHsc /utf-8 /FS
    )
endif()
