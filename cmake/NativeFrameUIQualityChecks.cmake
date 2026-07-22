function(nfui_add_quality_checks)
    if(NOT BUILD_TESTING)
        return()
    endif()

    find_program(NFUI_POWERSHELL_EXECUTABLE NAMES pwsh powershell)
    if(NOT NFUI_POWERSHELL_EXECUTABLE)
        message(FATAL_ERROR "PowerShell is required for NativeFrameUI quality checks")
    endif()

    set(_boundary_script "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_boundaries.ps1")
    set(_boundary_test_script "${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_boundaries_tests.ps1")

    add_test(NAME NativeFrameUIBoundaryCheck
        COMMAND "${NFUI_POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                -File "${_boundary_script}"
                -Root "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(NAME NativeFrameUIBoundaryCheckRegression
        COMMAND "${NFUI_POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                -File "${_boundary_test_script}"
                -ScriptPath "${_boundary_script}"
    )
    set_tests_properties(
        NativeFrameUIBoundaryCheck
        NativeFrameUIBoundaryCheckRegression
        PROPERTIES
            LABELS "quality;boundary"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endfunction()
