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

jpdf_desk_add_test(validation-clock test_validation_clock.cpp
    LIBRARIES jpdf-desk-crypto-support)
jpdf_desk_add_test(validation-clock-ui test_validation_clock_ui.cpp WIDGETS
    LIBRARIES jpdf-desk-mupdf)

if(TARGET jpdf-desk-openssl)
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
    # macOS also ships a system openssl (LibreSSL), whose PKCS#12 defaults
    # may require legacy ciphers unavailable in the linked OpenSSL provider.
    find_program(JPDF_DESK_OPENSSL_EXECUTABLE openssl
        HINTS "${OPENSSL_INCLUDE_DIR}/../bin" "${OPENSSL_ROOT_DIR}/bin" "$ENV{OPENSSL_ROOT_DIR}/bin"
        REQUIRED)
    jpdf_desk_add_test(openssl-support test_openssl_support.cpp
        LIBRARIES jpdf-desk-crypto-support OpenSSL::Crypto)
    jpdf_desk_add_test(signing test_pdf_signing.cpp
        LIBRARIES jpdf-desk-document jpdf-desk-openssl jpdf-desk-mupdf
        ARGUMENTS "${JPDF_DESK_OPENSSL_EXECUTABLE}")
    jpdf_desk_add_test(timestamp-verification test_timestamp_verification.cpp
        LIBRARIES jpdf-desk-openssl OpenSSL::Crypto
        ARGUMENTS "${JPDF_DESK_OPENSSL_EXECUTABLE}")
endif()
if(TARGET jpdf-desk-revocation)
    jpdf_desk_add_test(revocation test_revocation.cpp
        LIBRARIES jpdf-desk-revocation OpenSSL::Crypto)
endif()
if(TARGET jpdf-desk-revocation-http)
    jpdf_desk_add_test(signature-revocation test_signature_revocation.cpp WIDGETS
        LIBRARIES jpdf-desk-openssl
        ARGUMENTS "${JPDF_DESK_OPENSSL_EXECUTABLE}")
    jpdf_desk_add_test(revocation-http test_revocation_http.cpp
        LIBRARIES jpdf-desk-revocation-http)
endif()
jpdf_desk_add_test(signature-services test_signature_services.cpp
    LIBRARIES jpdf-desk-document jpdf-desk-mupdf)
jpdf_desk_add_test(document-annotations test_document_annotations.cpp
    LIBRARIES jpdf-desk-document jpdf-desk-mupdf)
jpdf_desk_add_test(signature-page-content test_signature_page_content.cpp
    LIBRARIES jpdf-desk-document jpdf-desk-mupdf)
jpdf_desk_add_test(mupdf-errors test_mupdf_errors.cpp
    LIBRARIES jpdf-desk-document jpdf-desk-mupdf)
jpdf_desk_add_test(image-loading test_image_loading.cpp
    LIBRARIES jpdf-desk-image jpdf-desk-document-types jpdf-desk-mupdf)
jpdf_desk_add_test(signature-image test_signature_image.cpp WIDGETS)
jpdf_desk_add_test(app-identity test_app_identity.cpp WIDGETS)
# Printing tests exercise only the printing library, not the application window.
jpdf_desk_add_test(printing test_printing.cpp
    LIBRARIES jpdf-desk-printing jpdf-desk-qt-platform jpdf-desk-mupdf)
set_tests_properties(jpdf-desk.printing PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
jpdf_desk_add_test(print-settings test_print_settings.cpp
    LIBRARIES jpdf-desk-printing jpdf-desk-qt-platform)
set_tests_properties(jpdf-desk.print-settings PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
jpdf_desk_add_test(print-preview test_print_preview.cpp WIDGETS)
jpdf_desk_add_test(print-dialog test_print_dialog.cpp WIDGETS)
jpdf_desk_add_test(document-dialogs test_document_dialogs.cpp WIDGETS)
jpdf_desk_add_test(forms test_forms.cpp WIDGETS)
jpdf_desk_add_test(pdf-tasks test_pdf_tasks.cpp WIDGETS LIBRARIES jpdf-desk-mupdf)
jpdf_desk_add_test(outline test_outline.cpp WIDGETS LIBRARIES jpdf-desk-mupdf)
jpdf_desk_add_test(links test_links.cpp WIDGETS LIBRARIES jpdf-desk-mupdf)
jpdf_desk_add_test(search test_search.cpp WIDGETS LIBRARIES jpdf-desk-mupdf)
jpdf_desk_add_test(page-controls test_page_controls.cpp WIDGETS)
jpdf_desk_add_test(password test_password.cpp WIDGETS LIBRARIES jpdf-desk-mupdf)
jpdf_desk_add_test(document-view test_document_view.cpp WIDGETS LIBRARIES jpdf-desk-mupdf)
jpdf_desk_add_test(main-window test_main_window.cpp WIDGETS LIBRARIES jpdf-desk-mupdf)
jpdf_desk_add_test(fileopen test_fileopen.cpp WIDGETS)
jpdf_desk_add_test(pdf-file-diagnostics test_pdf_file_diagnostics.cpp
    LIBRARIES jpdf-desk-document)
jpdf_desk_add_test(signature-store test_signature_store.cpp WIDGETS)
jpdf_desk_add_test(theme test_theme.cpp WIDGETS LIBRARIES jpdf-desk-mupdf)
if(TARGET jpdf-desk-openssl)
    target_link_libraries(jpdf-desk-main-window-test PRIVATE jpdf-desk-openssl)
    target_compile_definitions(jpdf-desk-main-window-test PRIVATE JPDF_DESK_HAS_OPENSSL)
endif()
