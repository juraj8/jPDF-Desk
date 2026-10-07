# Register application tests consistently; widget tests use the offscreen plugin.
function(jpdf_desk_add_test name source)
    cmake_parse_arguments(TEST "WIDGETS" "" "LIBRARIES;ARGUMENTS" ${ARGN})
    set(target jpdf-desk-${name}-test)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE ${TEST_LIBRARIES})
    add_test(NAME jpdf-desk.${name} COMMAND ${target} ${TEST_ARGUMENTS})
    if(TEST_WIDGETS)
        target_link_libraries(${target} PRIVATE jpdf-desk-gui-lib jpdf-desk-qt-platform)
        set_tests_properties(jpdf-desk.${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endfunction()

if(TARGET jpdf-desk-openssl)
    find_program(JPDF_DESK_OPENSSL_EXECUTABLE openssl REQUIRED)
    jpdf_desk_add_test(signing test_pdf_signing.cpp
        LIBRARIES jpdf-desk-document jpdf-desk-openssl jpdf-desk-mupdf
        ARGUMENTS "${JPDF_DESK_OPENSSL_EXECUTABLE}")
endif()
jpdf_desk_add_test(signature-services test_signature_services.cpp
    LIBRARIES jpdf-desk-document jpdf-desk-mupdf)
jpdf_desk_add_test(legacy-document test_legacy_document.cpp
    LIBRARIES jpdf-desk-document jpdf-desk-mupdf)
jpdf_desk_add_test(app-identity test_app_identity.cpp WIDGETS)
# Printing tests exercise only the printing library, not the application window.
jpdf_desk_add_test(printing test_printing.cpp
    LIBRARIES jpdf-desk-printing jpdf-desk-qt-platform jpdf-desk-mupdf)
set_tests_properties(jpdf-desk.printing PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
jpdf_desk_add_test(print-dialog test_print_dialog.cpp WIDGETS)
jpdf_desk_add_test(document-dialogs test_document_dialogs.cpp WIDGETS)
jpdf_desk_add_test(outline test_outline.cpp WIDGETS LIBRARIES jpdf-desk-mupdf)
jpdf_desk_add_test(search test_search.cpp WIDGETS LIBRARIES jpdf-desk-mupdf)
jpdf_desk_add_test(page-controls test_page_controls.cpp WIDGETS)
jpdf_desk_add_test(password test_password.cpp WIDGETS LIBRARIES jpdf-desk-mupdf)
jpdf_desk_add_test(main-window test_main_window.cpp WIDGETS LIBRARIES jpdf-desk-mupdf)
jpdf_desk_add_test(signature-store test_signature_store.cpp WIDGETS)
jpdf_desk_add_test(theme test_theme.cpp WIDGETS LIBRARIES jpdf-desk-mupdf)
if(TARGET jpdf-desk-openssl)
    target_link_libraries(jpdf-desk-main-window-test PRIVATE jpdf-desk-openssl)
    target_compile_definitions(jpdf-desk-main-window-test PRIVATE JPDF_DESK_HAS_OPENSSL)
endif()
