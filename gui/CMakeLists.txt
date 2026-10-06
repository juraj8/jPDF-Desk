configure_file(app_info.h.in generated/app_info.h @ONLY)

add_library(pdf-filler-gui-lib STATIC
    ui/about_dialog.cpp
    ui/about_dialog.h
    ui/dialog_helpers.cpp
    ui/dialog_helpers.h
    ui/metadata_dialog.cpp
    ui/metadata_dialog.h
    ui/password_dialog.cpp
    ui/password_dialog.h
    main_window.cpp
    main_window.h
    signature_store.cpp
    signature_store.h
    ui/signature_manager.cpp
    ui/signature_manager.h
    ui/buttons.cpp
    ui/buttons.h
    ui/editable_text.cpp
    ui/editable_text.h
    ui/mark_item.cpp
    ui/mark_item.h
    ui/page_annotations.cpp
    ui/page_annotations.h
    ui/outline_panel.cpp
    ui/outline_panel.h
    ui/page_controls.cpp
    ui/page_controls.h
    ui/search_controls.cpp
    ui/search_controls.h
    ui/print_dialog.cpp
    ui/print_dialog.h
    ui/theme.h
    ui/theme.cpp
    ui/sidebar.cpp
    ui/sidebar.h
    ui/signature_image.cpp
    ui/signature_image.h
    ui/signature_item.cpp
    ui/signature_item.h
    ui/text_tools.cpp
    ui/text_tools.h
    ui/window_style.cpp
    ui/window_style.h
    ui/zoom_controls.cpp
    ui/zoom_controls.h
)
target_include_directories(pdf-filler-gui-lib
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(pdf-filler-gui-lib
    PUBLIC pdf-filler-document pdf-filler-printing Qt6::Widgets
    PRIVATE pdf-filler-mupdf
)

set(PDF_FILLER_LOGO "${PROJECT_SOURCE_DIR}/branding/pdf-filler-logo-256.png")
set_source_files_properties("${PDF_FILLER_LOGO}" PROPERTIES QT_RESOURCE_ALIAS icon.png)
qt6_add_resources(pdf-filler-gui-lib pdf_filler_branding
    PREFIX /branding
    FILES "${PDF_FILLER_LOGO}"
)

set(PDF_FILLER_UI_ICONS
    ui/icons/up-light.xpm ui/icons/down-light.xpm
    ui/icons/up-dark.xpm ui/icons/down-dark.xpm
)
foreach(icon IN LISTS PDF_FILLER_UI_ICONS)
    get_filename_component(icon_name "${icon}" NAME)
    set_source_files_properties("${icon}" PROPERTIES QT_RESOURCE_ALIAS "${icon_name}")
endforeach()
qt6_add_resources(pdf-filler-gui-lib pdf_filler_ui
    PREFIX /ui
    FILES ${PDF_FILLER_UI_ICONS}
)

qt_add_executable(pdf-filler WIN32 MANUAL_FINALIZATION main.cpp)
if(WIN32)
    # Own the app manifest rather than depending on Qt's subdirectory-local
    # template path when QtBase is built in the same CMake project.
    configure_file(windows/pdf-filler.manifest.in pdf-filler.manifest @ONLY)
    set(PDF_FILLER_WINDOWS_ICON "${PROJECT_SOURCE_DIR}/branding/pdf-filler.ico")
    configure_file(windows/pdf-filler.rc.in pdf-filler.rc @ONLY)
    target_sources(pdf-filler PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/pdf-filler.manifest"
        "${CMAKE_CURRENT_BINARY_DIR}/pdf-filler.rc"
    )
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/pdf-filler.rc"
        PROPERTIES OBJECT_DEPENDS "${PDF_FILLER_WINDOWS_ICON}")
endif()
target_link_libraries(pdf-filler PRIVATE pdf-filler-gui-lib)
if(PDF_FILLER_SINGLE_EXECUTABLE AND MINGW)
    # Include libwinpthread as well as the GCC/C++ runtimes. Third-party
    # dependencies (including OpenSSL) must also be available as static archives.
    target_link_options(pdf-filler PRIVATE -static)
endif()
if(TARGET pdf-filler-openssl)
    target_link_libraries(pdf-filler PRIVATE pdf-filler-openssl)
    target_compile_definitions(pdf-filler PRIVATE PDF_FILLER_HAS_OPENSSL)
endif()

set(PDF_FILLER_PLATFORM_PLUGINS)
set(PDF_FILLER_PLATFORM_PLUGIN_OBJECTS)
foreach(_platform_plugin
    QOffscreenIntegrationPlugin
    QXcbIntegrationPlugin
    QWaylandIntegrationPlugin
    QWindowsIntegrationPlugin
    QCocoaIntegrationPlugin
    QCupsPrinterSupportPlugin
)
    if(TARGET Qt6::${_platform_plugin})
        list(APPEND PDF_FILLER_PLATFORM_PLUGINS Qt6::${_platform_plugin})
        list(APPEND PDF_FILLER_PLATFORM_PLUGIN_OBJECTS
            $<TARGET_OBJECTS:Qt6::${_platform_plugin}_init>
        )
    endif()
endforeach()

if(UNIX AND NOT APPLE
    AND NOT TARGET Qt6::QXcbIntegrationPlugin
    AND NOT TARGET Qt6::QWaylandIntegrationPlugin
)
    message(FATAL_ERROR
        "Qt was built without an XCB or Wayland desktop platform plugin. "
        "Install the Linux desktop development packages listed in README.md, "
        "delete the build directory, and configure again."
    )
endif()

if(UNIX AND NOT APPLE AND NOT TARGET Qt6::QCupsPrinterSupportPlugin)
    message(WARNING "Qt has no CUPS printer backend. Install libcups2-dev and reconfigure with --fresh for physical printer support.")
endif()

add_library(pdf-filler-qt-platform INTERFACE)
target_link_libraries(pdf-filler-qt-platform INTERFACE ${PDF_FILLER_PLATFORM_PLUGINS})
target_sources(pdf-filler-qt-platform INTERFACE ${PDF_FILLER_PLATFORM_PLUGIN_OBJECTS})
target_link_libraries(pdf-filler PRIVATE pdf-filler-qt-platform)
qt6_finalize_target(pdf-filler)

install(
    TARGETS pdf-filler
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
    BUNDLE DESTINATION . COMPONENT Runtime
)

if(UNIX AND NOT APPLE)
    install(FILES "${PROJECT_SOURCE_DIR}/packaging/pdf-filler.desktop"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications COMPONENT Runtime)
    install(FILES "${PDF_FILLER_LOGO}"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
        RENAME pdf-filler.png COMPONENT Runtime)
endif()
