cmake_minimum_required(VERSION 3.25.0)

project(jpdf-desk
    VERSION 1.0.0
    DESCRIPTION "A desktop PDF toolkit for viewing, filling, signing, verification, metadata and password management"
    LANGUAGES C CXX
)

set(JPDF_DESK_DISPLAY_NAME "jPDF Desk")
set(JPDF_DESK_AUTHOR "Juraj Giertl" CACHE STRING "Project author shown in the About dialog")
set(JPDF_DESK_AUTHOR_EMAIL "j74a8g@gmail.com" CACHE STRING "Author email used in package contact metadata")
set(JPDF_DESK_PROJECT_URL "${PROJECT_HOMEPAGE_URL}" CACHE STRING "Project homepage shown in the About dialog")
set(JPDF_DESK_ISSUES_URL "https://github.com/juraj8/jPDF-Desk/issues" CACHE STRING "GitHub issue tracker shown in the About dialog")
set(JPDF_DESK_DONATE_URL "https://github.com/sponsors/juraj8" CACHE STRING "Donation page or contact URL")

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Static Qt
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
set(QT_BUILD_EXAMPLES OFF CACHE BOOL "Build Qt examples" FORCE)
set(QT_BUILD_TESTS OFF CACHE BOOL "Build Qt tests" FORCE)

if(UNIX AND NOT APPLE)
    foreach(_xcb_component
        CURSOR GLX ICCCM IMAGE KEYSYMS RANDR RENDER RENDERUTIL
        SHAPE SHM SYNC UTIL XCB XFIXES XKB
    )
        set(USE_XCB_${_xcb_component}_STATIC ON CACHE BOOL
            "Link the XCB ${_xcb_component} component statically" FORCE)
    endforeach()
endif()

option(JPDF_DESK_SINGLE_EXECUTABLE "Link MinGW runtime dependencies into the executable" OFF)
option(JPDF_DESK_BUILD_APPIMAGE "Enable the AppImage packaging target (requires linuxdeploy)" OFF)
include(GNUInstallDirs)
include(CTest)
include(CheckGit)

set(JPDF_DESK_BUILD_TESTING ${BUILD_TESTING})
add_subdirectory(thirdparty/qtbase thirdparty/qtbase-build EXCLUDE_FROM_ALL)

# GCC 14 reports false positives while optimizing Qt and its bundled md4c
# library at -Os. Keep these suppressions inside the vendored QtGui target.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 14
    AND TARGET Gui
)
    target_compile_options(Gui PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Wno-maybe-uninitialized>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-array-bounds>
    )
endif()

set(BUILD_TESTING ${JPDF_DESK_BUILD_TESTING} CACHE BOOL "Build application tests" FORCE)
add_subdirectory(lib)
add_subdirectory(gui)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()

include(cmake/CPackConfig.cmake)
if(JPDF_DESK_BUILD_APPIMAGE)
    include(cmake/AppImage.cmake)
endif()
