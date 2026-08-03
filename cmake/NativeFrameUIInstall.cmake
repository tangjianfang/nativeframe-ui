# NativeFrameUI SDK packaging — install rules + CMake package export.
#
# This file turns the per-module static libraries and the umbrella
# INTERFACE library into a relocatable CMake package so downstream
# projects can consume NativeFrameUI through the standard SDK paths:
#
#   find_package(NativeFrameUI CONFIG REQUIRED)
#   target_link_libraries(MyApp PRIVATE NativeFrameUI::NativeFrameUI)
#   # or per-component:
#   target_link_libraries(MyApp PRIVATE NativeFrameUI::nfui_button NativeFrameUI::nfui_text)
#
# Consumers must still compile the shared resource script into their
# final executable (static libraries do not carry .rc resources):
#
#   target_sources(MyApp PRIVATE <prefix>/include/nfui/resources/NativeFrameUI.rc)
#   target_include_directories(MyApp PRIVATE <prefix>/include/nfui/resources)
#
# Set NFUI_ENABLE_INSTALL=OFF when embedding this project via
# add_subdirectory() to keep the consumer's install tree clean.

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(NFUI_ENABLE_INSTALL "Generate install rules and the NativeFrameUI CMake package" ON)
if(NOT NFUI_ENABLE_INSTALL)
    return()
endif()

# Per-module targets declared their headers through BUILD_INTERFACE only;
# attach the installed layout so `install(EXPORT)` emits usable
# INTERFACE_INCLUDE_DIRECTORIES for consumers. The resource directory is
# shipped next to the headers because nfui_add_resources() consumers
# include "NativeFrameUIResource.h" from the .rc script.
set(_nfui_exported_targets
    nfui_core
    nfui_command
    nfui_layout
    nfui_theme
    nfui_window
    nfui_dialog
    nfui_control_base
    nfui_button
    nfui_checkbox
    nfui_radio
    nfui_text
    nfui_listbox
    nfui_listview
    nfui_treeview
    nfui_iconview
    nfui_frame
    nfui_menu
    nfui_slider
    nfui_propertygrid
    NativeFrameUI
)
if(NFUI_BUILD_CHARTS)
    list(APPEND _nfui_exported_targets nfui_charts nfui_charts_aa)
endif()

foreach(_target IN LISTS _nfui_exported_targets)
    target_include_directories(${_target} INTERFACE
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nfui/resources>
    )
endforeach()

install(TARGETS ${_nfui_exported_targets}
    EXPORT NativeFrameUITargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Public headers + the resource template that every consumer exe must
# compile (see RESOURCE_GUIDE.md — the explicit .rc strategy).
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/nfui
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nfui/resources
)

install(EXPORT NativeFrameUITargets
    FILE NativeFrameUITargets.cmake
    NAMESPACE NativeFrameUI::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NativeFrameUI
)

# NativeFrameUIConfig.cmake simply includes the exported targets —
# no third-party dependencies to find, no feature options to check.
set(_nfui_config_in "${CMAKE_CURRENT_BINARY_DIR}/NativeFrameUIConfig.cmake.in")
file(WRITE "${_nfui_config_in}" [=[
@PACKAGE_INIT@

include("${CMAKE_CURRENT_LIST_DIR}/NativeFrameUITargets.cmake")

check_required_components(NativeFrameUI)
]=])

configure_package_config_file("${_nfui_config_in}"
    "${CMAKE_CURRENT_BINARY_DIR}/NativeFrameUIConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NativeFrameUI
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/NativeFrameUIConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/NativeFrameUIConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/NativeFrameUIConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NativeFrameUI
)
