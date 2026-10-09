# Non-widget image loading; backend headers and libraries stay private.
add_library(jpdf-desk-image STATIC src/image_loading.cpp)
add_library(JPDFDesk::Image ALIAS jpdf-desk-image)
target_include_directories(jpdf-desk-image PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(jpdf-desk-image
    PUBLIC Qt6::Gui
    PRIVATE jpdf-desk-document-types jpdf-desk-mupdf)
