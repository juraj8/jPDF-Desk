# Third-party PDF support belongs to the reusable libraries, not the GUI.
include(BuildMuPDF)

# Shared value types and provider contracts avoid document/signing dependency cycles.
add_library(jpdf-desk-document-types INTERFACE)
target_include_directories(jpdf-desk-document-types INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/document/include")
target_link_libraries(jpdf-desk-document-types INTERFACE Qt6::Core)

add_subdirectory(signing)
add_subdirectory(document)
add_subdirectory(printing)
