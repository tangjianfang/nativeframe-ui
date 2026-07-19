function(nfui_apply_compiler_options target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_compile_definitions(${target}
        PUBLIC
            UNICODE
            _UNICODE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            _WIN32_WINNT=0x0A00
    )

    if(MSVC)
        set_property(TARGET ${target} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc /utf-8)
    endif()
endfunction()
