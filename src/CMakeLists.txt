cmake_minimum_required(VERSION 3.10)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Sawadee VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(-DCMAKE)

if(MSVC OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(/W3 /WX- /utf-8)
endif()

foreach(_flag_var
    CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG
    CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELEASE
    CMAKE_C_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_MINSIZEREL
    CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    if(DEFINED ${_flag_var})
        string(REGEX REPLACE "/MD" "/MT" ${_flag_var} "${${_flag_var}}")
    endif()
endforeach()

set(CMAKE_DEBUG_POSTFIX "_mtd")
set(CMAKE_RELEASE_POSTFIX "_mt")
set(CMAKE_MINSIZEREL_POSTFIX "_mt")
set(CMAKE_RELWITHDEBINFO_POSTFIX "_mt")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin)

set(SAWADEE_SRC_DIR "${CMAKE_SOURCE_DIR}/Sawadee")
set(SAWADEE_SKIN_DIR "${CMAKE_SOURCE_DIR}/../bin/skin/Sawadee")
set(SAWADEE_SKIN_ZIP "${CMAKE_CURRENT_BINARY_DIR}/SawadeeSkin.zip")
file(TO_CMAKE_PATH "${SAWADEE_SKIN_ZIP}" SAWADEE_SKIN_ZIP_RC_PATH)
configure_file(
    ${SAWADEE_SRC_DIR}/SawadeeZip.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/SawadeeZip.rc
    @ONLY
)

if(DEFINED ENV{DUIX} AND NOT "$ENV{DUIX}" STREQUAL "")
    set(DUIX_ROOT "$ENV{DUIX}")
else()
    set(DUIX_ROOT "H:/sdk/DuiLib/trunk/3rd/DuiLib_Ultimate-master")
endif()
get_filename_component(DUIX_ROOT "${DUIX_ROOT}" ABSOLUTE)
if(NOT EXISTS "${DUIX_ROOT}/src/DuiLib/UIlib.h")
    message(FATAL_ERROR "DUIX not found (set env DUIX). Tried: ${DUIX_ROOT}")
endif()
message(STATUS "DUIX_ROOT=${DUIX_ROOT}")

set(DUIX_INCLUDE "${DUIX_ROOT}/src/DuiLib")
set(DUIX_BIN "${DUIX_ROOT}/bin")
set(DUIX_WEBVIEW2_ROOT "${DUIX_ROOT}/src/3rd/webview2/pkg/build/native")
set(WEBVIEW2_ROOT "${DUIX_WEBVIEW2_ROOT}")

add_library(DuiLib STATIC IMPORTED GLOBAL)
set_target_properties(DuiLib PROPERTIES
    IMPORTED_LOCATION_DEBUG "${DUIX_BIN}/DuiLib_mtd.lib"
    IMPORTED_LOCATION_RELEASE "${DUIX_BIN}/DuiLib_mt.lib"
    IMPORTED_LOCATION_MINSIZEREL "${DUIX_BIN}/DuiLib_mt.lib"
    IMPORTED_LOCATION_RELWITHDEBINFO "${DUIX_BIN}/DuiLib_mt.lib"
    IMPORTED_LOCATION "${DUIX_BIN}/DuiLib_mtd.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${DUIX_INCLUDE};${WEBVIEW2_ROOT}/include"
    INTERFACE_COMPILE_DEFINITIONS "UILIB_STATIC;DUILIB_RENDER_BACKEND=1;DUILIB_HAS_WEBVIEW2=1"
)

add_library(lunasvg STATIC IMPORTED GLOBAL)
set_target_properties(lunasvg PROPERTIES
    IMPORTED_LOCATION_DEBUG "${DUIX_BIN}/lunasvg_mtd.lib"
    IMPORTED_LOCATION_RELEASE "${DUIX_BIN}/lunasvg_mt.lib"
    IMPORTED_LOCATION_MINSIZEREL "${DUIX_BIN}/lunasvg_mt.lib"
    IMPORTED_LOCATION_RELWITHDEBINFO "${DUIX_BIN}/lunasvg_mt.lib"
    IMPORTED_LOCATION "${DUIX_BIN}/lunasvg_mtd.lib"
)

add_library(plutovg STATIC IMPORTED GLOBAL)
set_target_properties(plutovg PROPERTIES
    IMPORTED_LOCATION_DEBUG "${DUIX_BIN}/plutovg_mtd.lib"
    IMPORTED_LOCATION_RELEASE "${DUIX_BIN}/plutovg_mt.lib"
    IMPORTED_LOCATION_MINSIZEREL "${DUIX_BIN}/plutovg_mt.lib"
    IMPORTED_LOCATION_RELWITHDEBINFO "${DUIX_BIN}/plutovg_mt.lib"
    IMPORTED_LOCATION "${DUIX_BIN}/plutovg_mtd.lib"
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_WV2_ARCH x64)
else()
    set(_WV2_ARCH x86)
endif()
set(_WV2_LOADER "${WEBVIEW2_ROOT}/${_WV2_ARCH}/WebView2LoaderStatic.lib")
if(NOT EXISTS "${_WV2_LOADER}")
    message(FATAL_ERROR "WebView2LoaderStatic.lib not found: ${_WV2_LOADER}")
endif()

set(SAWADEE_SOURCES
    ${SAWADEE_SRC_DIR}/main.cpp
    ${SAWADEE_SRC_DIR}/StdAfx.cpp
    ${SAWADEE_SRC_DIR}/MainWnd.cpp
    ${SAWADEE_SRC_DIR}/ImageWnd.cpp
    ${SAWADEE_SRC_DIR}/WriteGlyph.cpp
    ${SAWADEE_SRC_DIR}/Sawadee.rc
    ${CMAKE_CURRENT_BINARY_DIR}/SawadeeZip.rc
)

file(GLOB_RECURSE SAWADEE_SKIN_FILES CONFIGURE_DEPENDS
    "${SAWADEE_SKIN_DIR}/*"
)

add_custom_target(SawadeeSkinZip ALL
    BYPRODUCTS "${SAWADEE_SKIN_ZIP}"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${SAWADEE_SKIN_ZIP}"
    COMMAND ${CMAKE_COMMAND} -E chdir "${SAWADEE_SKIN_DIR}"
        ${CMAKE_COMMAND} -E tar cf "${SAWADEE_SKIN_ZIP}" --format=zip .
    DEPENDS ${SAWADEE_SKIN_FILES}
    COMMENT "Pack Sawadee skin zip"
    VERBATIM
)

add_executable(Sawadee WIN32 ${SAWADEE_SOURCES})
add_dependencies(Sawadee SawadeeSkinZip)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/SawadeeZip.rc PROPERTIES
    OBJECT_DEPENDS "${SAWADEE_SKIN_ZIP}"
)
set_source_files_properties(${SAWADEE_SRC_DIR}/Sawadee.rc PROPERTIES
    OBJECT_DEPENDS "${SAWADEE_SRC_DIR}/App.ico"
)
target_include_directories(Sawadee PRIVATE ${SAWADEE_SRC_DIR})
target_compile_definitions(Sawadee PRIVATE UNICODE _UNICODE)
target_compile_features(Sawadee PRIVATE cxx_std_17)
target_link_libraries(Sawadee PRIVATE
    DuiLib
    lunasvg
    plutovg
    "${_WV2_LOADER}"
    d2d1 dwrite d3d11 dxgi dcomp windowscodecs
    version
    shell32
    comdlg32
    oleaut32
    gdi32
    user32
    ole32
)

set_target_properties(Sawadee PROPERTIES
    DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}"
    RELEASE_POSTFIX "${CMAKE_RELEASE_POSTFIX}"
    MINSIZEREL_POSTFIX "${CMAKE_MINSIZEREL_POSTFIX}"
    RELWITHDEBINFO_POSTFIX "${CMAKE_RELWITHDEBINFO_POSTFIX}"
)
