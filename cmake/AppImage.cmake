if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "AppImage packaging is supported only for Linux targets")
endif()
find_program(PDF_FILLER_LINUXDEPLOY_EXECUTABLE NAMES linuxdeploy REQUIRED)

add_custom_target(appimage
    COMMAND "${CMAKE_COMMAND}"
        "-DBUILD_DIR=${PROJECT_BINARY_DIR}"
        "-DLINUXDEPLOY=${PDF_FILLER_LINUXDEPLOY_EXECUTABLE}"
        "-DAPPIMAGE_NAME=${PROJECT_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.AppImage"
        -P "${PROJECT_SOURCE_DIR}/cmake/BuildAppImage.cmake"
    DEPENDS pdf-filler
    COMMENT "Bundling the application and runtime libraries into an AppImage"
    VERBATIM
)
