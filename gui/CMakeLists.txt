configure_file(app_info.h.in generated/app_info.h @ONLY)

add_library(jpdf-desk-gui-lib STATIC
    app_identity.cpp
    app_identity.h
    ui/about_dialog.cpp
    ui/about_dialog.h
    ui/dialog_helpers.cpp
    ui/dialog_helpers.h
    ui/fileopen_dialog.cpp
    ui/fileopen_dialog.h
    ui/form_dialog.cpp
    ui/form_dialog.h
    ui/metadata_dialog.cpp
    ui/metadata_dialog.h
    ui/password_dialog.cpp
    ui/password_dialog.h
    main_window.cpp
    main_window.h
    document_view.cpp
    document_view.h
    document_loading.cpp
    document_loading.h
    pdf_task.cpp
    pdf_task.h
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
    ui/print_preview.cpp
    ui/print_preview.h
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
qt6_wrap_cpp(JPDF_DESK_GUI_MOC_SOURCES document_view.h TARGET jpdf-desk-gui-lib)
target_sources(jpdf-desk-gui-lib PRIVATE ${JPDF_DESK_GUI_MOC_SOURCES})
target_include_directories(jpdf-desk-gui-lib
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(jpdf-desk-gui-lib
    PUBLIC jpdf-desk-document jpdf-desk-printing Qt6::Widgets
    PRIVATE jpdf-desk-image
)

set(JPDF_DESK_LOGO "${PROJECT_SOURCE_DIR}/branding/jpdf-desk-logo-256.png")
set_source_files_properties("${JPDF_DESK_LOGO}" PROPERTIES QT_RESOURCE_ALIAS icon.png)
qt6_add_resources(jpdf-desk-gui-lib jpdf_desk_branding
    PREFIX /branding
    FILES "${JPDF_DESK_LOGO}"
)

set(JPDF_DESK_UI_ICONS
    ui/icons/up-light.xpm ui/icons/down-light.xpm
    ui/icons/up-dark.xpm ui/icons/down-dark.xpm
    ui/icons/close-red.xpm
)
foreach(icon IN LISTS JPDF_DESK_UI_ICONS)
    get_filename_component(icon_name "${icon}" NAME)
    set_source_files_properties("${icon}" PROPERTIES QT_RESOURCE_ALIAS "${icon_name}")
endforeach()
qt6_add_resources(jpdf-desk-gui-lib jpdf_desk_ui
    PREFIX /ui
    FILES ${JPDF_DESK_UI_ICONS}
)

qt_add_executable(jpdf-desk WIN32 MANUAL_FINALIZATION main.cpp)
if(APPLE)
    # QtBase's default template path is local to its build subdirectory.
    # Supply our own plist before Qt finalizes the application bundle.
    set_target_properties(jpdf-desk PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "${JPDF_DESK_DISPLAY_NAME}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.juraj8.jpdf-desk"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()
if(WIN32)
    # Own the app manifest rather than depending on Qt's subdirectory-local
    # template path when QtBase is built in the same CMake project.
    configure_file(windows/jpdf-desk.manifest.in jpdf-desk.manifest @ONLY)
    set(JPDF_DESK_WINDOWS_ICON "${PROJECT_SOURCE_DIR}/branding/jpdf-desk.ico")
    configure_file(windows/jpdf-desk.rc.in jpdf-desk.rc @ONLY)
    target_sources(jpdf-desk PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/jpdf-desk.manifest"
        "${CMAKE_CURRENT_BINARY_DIR}/jpdf-desk.rc"
    )
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/jpdf-desk.rc"
        PROPERTIES OBJECT_DEPENDS "${JPDF_DESK_WINDOWS_ICON}")
endif()
target_link_libraries(jpdf-desk PRIVATE jpdf-desk-gui-lib)
if(JPDF_DESK_SINGLE_EXECUTABLE AND MINGW)
    # Include libwinpthread as well as the GCC/C++ runtimes. Third-party
    # dependencies (including OpenSSL) must also be available as static archives.
    target_link_options(jpdf-desk PRIVATE -static)
endif()
if(TARGET jpdf-desk-openssl)
    target_link_libraries(jpdf-desk PRIVATE jpdf-desk-openssl)
    target_compile_definitions(jpdf-desk PRIVATE JPDF_DESK_HAS_OPENSSL)
endif()

set(JPDF_DESK_PLATFORM_PLUGINS)
set(JPDF_DESK_PLATFORM_PLUGIN_OBJECTS)
foreach(_platform_plugin
    QOffscreenIntegrationPlugin
    QXcbIntegrationPlugin
    QWaylandIntegrationPlugin
    QWindowsIntegrationPlugin
    QCocoaIntegrationPlugin
    QCupsPrinterSupportPlugin
)
    if(TARGET Qt6::${_platform_plugin})
        list(APPEND JPDF_DESK_PLATFORM_PLUGINS Qt6::${_platform_plugin})
        list(APPEND JPDF_DESK_PLATFORM_PLUGIN_OBJECTS
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
        "Install the Linux desktop development packages documented in docs/development/build.md, "
        "delete the build directory, and configure again."
    )
endif()

if(UNIX AND NOT APPLE AND NOT TARGET Qt6::QCupsPrinterSupportPlugin)
    message(WARNING "Qt has no CUPS printer backend. Install libcups2-dev and reconfigure with --fresh for physical printer support.")
endif()

add_library(jpdf-desk-qt-platform INTERFACE)
target_link_libraries(jpdf-desk-qt-platform INTERFACE ${JPDF_DESK_PLATFORM_PLUGINS})
target_sources(jpdf-desk-qt-platform INTERFACE ${JPDF_DESK_PLATFORM_PLUGIN_OBJECTS})
target_link_libraries(jpdf-desk PRIVATE jpdf-desk-qt-platform)
qt6_finalize_target(jpdf-desk)

install(
    TARGETS jpdf-desk
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
    BUNDLE DESTINATION . COMPONENT Runtime
)

if(UNIX AND NOT APPLE)
    install(FILES "${PROJECT_SOURCE_DIR}/packaging/jpdf-desk.desktop"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications COMPONENT Runtime)
    install(FILES "${JPDF_DESK_LOGO}"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
        RENAME jpdf-desk.png COMPONENT Runtime)
endif()
