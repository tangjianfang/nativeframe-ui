function(nfui_add_resources target)
    target_sources(${target} PRIVATE
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../resources/NativeFrameUI.rc
    )
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../resources
    )
endfunction()
