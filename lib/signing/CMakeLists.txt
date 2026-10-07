add_library(jpdf-desk-signature-services INTERFACE)
target_include_directories(jpdf-desk-signature-services INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(jpdf-desk-signature-services INTERFACE Qt6::Core)

# PDF signature handling is independent of the selected cryptography backend.
add_library(jpdf-desk-signing STATIC src/pdf_signatures.cpp)
add_library(JPDFDesk::Signing ALIAS jpdf-desk-signing)
target_link_libraries(jpdf-desk-signing
    PUBLIC jpdf-desk-signature-services jpdf-desk-document-types
    PRIVATE jpdf-desk-mupdf)

# The portable backend is selected by the application, never by the document library.
option(JPDF_DESK_WITH_OPENSSL "Build the portable PKCS#12 signing/verification provider" ON)
if(JPDF_DESK_WITH_OPENSSL)
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
    add_library(jpdf-desk-openssl STATIC
        src/openssl_provider.cpp
        "${MUPDF_SOURCE_DIR}/source/helpers/pkcs7/pkcs7-openssl.c")
    add_library(JPDFDesk::OpenSSL ALIAS jpdf-desk-openssl)
    target_compile_definitions(jpdf-desk-openssl PRIVATE HAVE_LIBCRYPTO)
    target_link_libraries(jpdf-desk-openssl
        PUBLIC jpdf-desk-signature-services
        PRIVATE jpdf-desk-mupdf OpenSSL::Crypto)
endif()
