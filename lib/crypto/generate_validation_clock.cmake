# Source-maintained lower bound. Advance deliberately; all values are UTC.
set(JPDF_EMBEDDED_TIMESTAMP "2026-10-14T13:02:05Z")
string(TIMESTAMP JPDF_BUILD_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ" UTC)
if(NOT JPDF_BUILD_TIMESTAMP STRGREATER JPDF_EMBEDDED_TIMESTAMP)
    message(FATAL_ERROR
        "Build timestamp (${JPDF_BUILD_TIMESTAMP}) must be later than the embedded timestamp (${JPDF_EMBEDDED_TIMESTAMP}). Check the build clock and SOURCE_DATE_EPOCH.")
endif()
configure_file("${CMAKE_CURRENT_LIST_DIR}/validation_clock_build.h.in"
    "${OUTPUT_FILE}" @ONLY)
