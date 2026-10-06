add_library(pdf-filler-document STATIC src/pdf_document.cpp)
add_library(PdfFiller::Document ALIAS pdf-filler-document)
target_link_libraries(pdf-filler-document
    PUBLIC pdf-filler-document-types pdf-filler-signature-services Qt6::Gui
    PRIVATE pdf-filler-signing pdf-filler-mupdf)
