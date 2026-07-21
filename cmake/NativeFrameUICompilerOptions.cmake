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
        # /FS forces synchronous PDB writes so parallel cl.exe invocations
        # targeting the same .pdb don't collide (C1041). Required because
        # our per-component libraries compile multiple TUs into one static
        # lib with a shared target PDB.
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc /utf-8 /FS /MP)
    endif()
endfunction()
