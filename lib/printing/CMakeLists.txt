# QPrinter delegates to CUPS, Windows, or macOS via Qt's platform backends.
# Printer selection dialogs remain in gui/; job settings and rendering live here.
add_library(jpdf-desk-printing STATIC src/pdf_printing.cpp src/print_settings.cpp)
add_library(JPDFDesk::Printing ALIAS jpdf-desk-printing)
target_include_directories(jpdf-desk-printing PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(jpdf-desk-printing PUBLIC jpdf-desk-document Qt6::PrintSupport)
