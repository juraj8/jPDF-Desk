# Internal helpers; no PDF or network dependencies. Clock checks also work without OpenSSL.
set(validation_clock_header "${CMAKE_CURRENT_BINARY_DIR}/generated/validation_clock_build.h")
# Capture the actual build invocation, not merely the initial configuration time.
# configure_file in the script avoids rewriting the header when its contents are unchanged.
add_custom_target(jpdf-desk-validation-clock-build
    COMMAND "${CMAKE_COMMAND}" "-DOUTPUT_FILE=${validation_clock_header}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/generate_validation_clock.cmake"
    BYPRODUCTS "${validation_clock_header}"
    VERBATIM)
add_library(jpdf-desk-crypto-support STATIC src/validation_clock.cpp)
add_dependencies(jpdf-desk-crypto-support jpdf-desk-validation-clock-build)
target_include_directories(jpdf-desk-crypto-support
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(jpdf-desk-crypto-support PUBLIC Qt6::Core)
