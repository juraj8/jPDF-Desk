add_library(jpdf-desk-document STATIC src/pdf_document.cpp)
add_library(JPDFDesk::Document ALIAS jpdf-desk-document)
target_link_libraries(jpdf-desk-document
    PUBLIC jpdf-desk-document-types jpdf-desk-signature-services Qt6::Gui
    PRIVATE jpdf-desk-signing jpdf-desk-mupdf)
