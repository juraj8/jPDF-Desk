# QPrinter delegates to CUPS, Windows, or macOS via Qt's platform backends.
# Printer selection dialogs remain in gui/; this target only runs print jobs.
add_library(pdf-filler-printing STATIC src/pdf_printing.cpp)
add_library(PdfFiller::Printing ALIAS pdf-filler-printing)
target_include_directories(pdf-filler-printing PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(pdf-filler-printing PUBLIC pdf-filler-document Qt6::PrintSupport)
