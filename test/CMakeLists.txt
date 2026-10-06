# Register application tests consistently; widget tests use the offscreen plugin.
function(pdf_filler_add_test name source)
    cmake_parse_arguments(TEST "WIDGETS" "" "LIBRARIES;ARGUMENTS" ${ARGN})
    set(target pdf-filler-${name}-test)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE ${TEST_LIBRARIES})
    add_test(NAME pdf-filler.${name} COMMAND ${target} ${TEST_ARGUMENTS})
    if(TEST_WIDGETS)
        target_link_libraries(${target} PRIVATE pdf-filler-gui-lib pdf-filler-qt-platform)
        set_tests_properties(pdf-filler.${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endfunction()

if(TARGET pdf-filler-openssl)
    find_program(PDF_FILLER_OPENSSL_EXECUTABLE openssl REQUIRED)
    pdf_filler_add_test(signing test_pdf_signing.cpp
        LIBRARIES pdf-filler-document pdf-filler-openssl pdf-filler-mupdf
        ARGUMENTS "${PDF_FILLER_OPENSSL_EXECUTABLE}")
endif()
pdf_filler_add_test(signature-services test_signature_services.cpp
    LIBRARIES pdf-filler-document pdf-filler-mupdf)
# Printing tests exercise only the printing library, not the application window.
pdf_filler_add_test(printing test_printing.cpp
    LIBRARIES pdf-filler-printing pdf-filler-qt-platform pdf-filler-mupdf)
set_tests_properties(pdf-filler.printing PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
pdf_filler_add_test(print-dialog test_print_dialog.cpp WIDGETS)
pdf_filler_add_test(document-dialogs test_document_dialogs.cpp WIDGETS)
pdf_filler_add_test(outline test_outline.cpp WIDGETS LIBRARIES pdf-filler-mupdf)
pdf_filler_add_test(search test_search.cpp WIDGETS LIBRARIES pdf-filler-mupdf)
pdf_filler_add_test(page-controls test_page_controls.cpp WIDGETS)
pdf_filler_add_test(password test_password.cpp WIDGETS LIBRARIES pdf-filler-mupdf)
pdf_filler_add_test(main-window test_main_window.cpp WIDGETS LIBRARIES pdf-filler-mupdf)
pdf_filler_add_test(signature-store test_signature_store.cpp WIDGETS)
pdf_filler_add_test(theme test_theme.cpp WIDGETS LIBRARIES pdf-filler-mupdf)
if(TARGET pdf-filler-openssl)
    target_link_libraries(pdf-filler-main-window-test PRIVATE pdf-filler-openssl)
    target_compile_definitions(pdf-filler-main-window-test PRIVATE PDF_FILLER_HAS_OPENSSL)
endif()
