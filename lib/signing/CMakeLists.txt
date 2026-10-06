add_library(pdf-filler-signature-services INTERFACE)
target_include_directories(pdf-filler-signature-services INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(pdf-filler-signature-services INTERFACE Qt6::Core)

# PDF signature handling is independent of the selected cryptography backend.
add_library(pdf-filler-signing STATIC src/pdf_signatures.cpp)
add_library(PdfFiller::Signing ALIAS pdf-filler-signing)
target_link_libraries(pdf-filler-signing
    PUBLIC pdf-filler-signature-services pdf-filler-document-types
    PRIVATE pdf-filler-mupdf)

# The portable backend is selected by the application, never by the document library.
option(PDF_FILLER_WITH_OPENSSL "Build the portable PKCS#12 signing/verification provider" ON)
if(PDF_FILLER_WITH_OPENSSL)
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
    add_library(pdf-filler-openssl STATIC
        src/openssl_provider.cpp
        "${MUPDF_SOURCE_DIR}/source/helpers/pkcs7/pkcs7-openssl.c")
    add_library(PdfFiller::OpenSSL ALIAS pdf-filler-openssl)
    target_compile_definitions(pdf-filler-openssl PRIVATE HAVE_LIBCRYPTO)
    target_link_libraries(pdf-filler-openssl
        PUBLIC pdf-filler-signature-services
        PRIVATE pdf-filler-mupdf OpenSSL::Crypto)
endif()
