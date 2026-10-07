# MuPDF uses GNU Make, not CMake. Build only the two static libraries needed
# by the PDF editor; keep all objects out of the source submodule.
include(ExternalProject)
find_program(MUPDF_MAKE NAMES gmake make REQUIRED)

set(MUPDF_SOURCE_DIR "${PROJECT_SOURCE_DIR}/thirdparty/mupdf")
set(MUPDF_OUTPUT_DIR "${PROJECT_BINARY_DIR}/mupdf")
if(NOT EXISTS "${MUPDF_SOURCE_DIR}/include/mupdf/fitz.h")
    message(FATAL_ERROR "MuPDF source is missing. Run: git submodule update --init thirdparty/mupdf")
endif()
foreach(_header
    freetype/include/freetype/freetype.h
    jbig2dec/jbig2.h
    libjpeg/jpeglib.h
    lcms2/include/lcms2mt.h
    openjpeg/src/lib/openjp2/openjpeg.h
    zlib/zlib.h
)
    if(NOT EXISTS "${MUPDF_SOURCE_DIR}/thirdparty/${_header}")
        message(FATAL_ERROR
            "MuPDF dependency '${_header}' is missing. Run: "
            "git -C thirdparty/mupdf submodule update --init "
            "thirdparty/freetype thirdparty/jbig2dec thirdparty/libjpeg "
            "thirdparty/lcms2 thirdparty/openjpeg thirdparty/zlib"
        )
    endif()
endforeach()

# MuPDF enables SSE4.1 deskew code for Windows even when MinGW's default
# target only guarantees SSE2. Use its portable cores rather than requiring
# SSE4.1 for every Windows CPU (the app does not use deskew).
set(_mupdf_platform_args)
if(WIN32)
    # Do not let uname select Linux's host linker for embedded font objects
    # during cross-compilation. Generated C data works with the target compiler.
    list(APPEND _mupdf_platform_args "OS=Windows" "HAVE_OBJCOPY=no")
endif()
if(MINGW)
    list(APPEND _mupdf_platform_args "XCFLAGS=-DARCH_HAS_SSE=0")
endif()

# MuPDF's optional HTML, JS, XPS, SVG, Brotli and document-export engines
# require more submodules. The editor only opens and writes PDF files.
ExternalProject_Add(jpdf-desk-mupdf-build
    SOURCE_DIR "${MUPDF_SOURCE_DIR}"
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE OFF
    BUILD_COMMAND "${MUPDF_MAKE}" -C "${MUPDF_SOURCE_DIR}" -j4 libs
        OUT=${MUPDF_OUTPUT_DIR} build=release
        html=no svg=no xps=no extract=no mujs=no brotli=no
        CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER}
        AR=${CMAKE_AR} RANLIB=${CMAKE_RANLIB}
        ${_mupdf_platform_args}
    INSTALL_COMMAND ""
    BUILD_ALWAYS TRUE
    BUILD_BYPRODUCTS
        "${MUPDF_OUTPUT_DIR}/libmupdf.a"
        "${MUPDF_OUTPUT_DIR}/libmupdf-third.a"
)

add_library(jpdf-desk-mupdf INTERFACE)
add_dependencies(jpdf-desk-mupdf jpdf-desk-mupdf-build)
target_include_directories(jpdf-desk-mupdf INTERFACE "${MUPDF_SOURCE_DIR}/include")
# Library order matters for static archives. All dependencies are bundled
# into libmupdf-third.a, so no libmupdf.so is needed at runtime.
target_link_libraries(jpdf-desk-mupdf INTERFACE
    "${MUPDF_OUTPUT_DIR}/libmupdf.a"
    "${MUPDF_OUTPUT_DIR}/libmupdf-third.a"
)
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(jpdf-desk-mupdf INTERFACE Threads::Threads ${CMAKE_DL_LIBS} m)
endif()
