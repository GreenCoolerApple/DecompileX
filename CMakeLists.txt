cmake_minimum_required(VERSION 3.16)

project(DecompileX VERSION 0.4.0 DESCRIPTION "Binary analysis and archive toolkit" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DECOMPILEX_BUILD_APK_LIB "Build Android JNI library" OFF)
option(DECOMPILEX_BUILD_SETUP "Build setup utility" ON)
option(DECOMPILEX_ENABLE_WARNINGS "Enable compiler warnings" ON)
include(CTest)

set(DECOMPILEX_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(DECOMPILEX_BACKEND_DIR "${DECOMPILEX_SOURCE_DIR}/backend")

set(DECOMPILEX_BACKEND_SOURCES
    "${DECOMPILEX_BACKEND_DIR}/ZipGenerator.cpp"
    "${DECOMPILEX_BACKEND_DIR}/ExeCompiler.cpp"
    "${DECOMPILEX_BACKEND_DIR}/ISOCompiler.cpp"
    "${DECOMPILEX_BACKEND_DIR}/BinCompiler.cpp"
)

set(DECOMPILEX_BACKEND_HEADERS
    "${DECOMPILEX_BACKEND_DIR}/ZipGenerator.hpp"
    "${DECOMPILEX_BACKEND_DIR}/ExeCompiler.hpp"
    "${DECOMPILEX_BACKEND_DIR}/ISOCompiler.hpp"
    "${DECOMPILEX_BACKEND_DIR}/BinCompiler.hpp"
)

foreach(SOURCE IN LISTS DECOMPILEX_BACKEND_SOURCES)
    if(NOT EXISTS "${SOURCE}")
        message(FATAL_ERROR "Missing backend source: ${SOURCE}")
    endif()
endforeach()

if(ANDROID AND DECOMPILEX_BUILD_APK_LIB)
    add_library(DecompileX SHARED
        "${DECOMPILEX_SOURCE_DIR}/android/NativeBridge.cpp"
        ${DECOMPILEX_BACKEND_SOURCES}
        ${DECOMPILEX_BACKEND_HEADERS}
    )
    target_link_libraries(DecompileX PRIVATE log)
else()
    set(DECOMPILEX_RESOURCES)
    if(WIN32)
        enable_language(RC)
        set(DECOMPILEX_RC "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/DecompileX.rc")
        if(EXISTS "${DECOMPILEX_RC}")
            list(APPEND DECOMPILEX_RESOURCES "${DECOMPILEX_RC}")
        endif()
    endif()
    add_executable(DecompileX
        "${DECOMPILEX_SOURCE_DIR}/Main.cpp"
        ${DECOMPILEX_BACKEND_SOURCES}
        ${DECOMPILEX_BACKEND_HEADERS}
        ${DECOMPILEX_RESOURCES}
    )
endif()

target_include_directories(DecompileX PRIVATE
    "${DECOMPILEX_SOURCE_DIR}"
    "${DECOMPILEX_BACKEND_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_definitions(DecompileX PRIVATE
    DECOMPILEX_VERSION="${PROJECT_VERSION}"
    DECOMPILEX_NAME="${PROJECT_NAME}"
)

if(DECOMPILEX_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(DecompileX PRIVATE /W4 /EHsc /utf-8)
    else()
        target_compile_options(DecompileX PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

set_target_properties(DecompileX PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

foreach(CONFIG Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${CONFIG}" CONFIG_UPPER)
    set_target_properties(DecompileX PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER}" "${CMAKE_BINARY_DIR}/bin"
        "LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER}" "${CMAKE_BINARY_DIR}/lib"
        "ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER}" "${CMAKE_BINARY_DIR}/lib"
    )
endforeach()

if(DECOMPILEX_BUILD_SETUP AND NOT ANDROID)
    set(DECOMPILEX_SETUP_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tools/Setup.cpp")
    if(EXISTS "${DECOMPILEX_SETUP_SOURCE}")
        add_executable(DecompileX-Setup "${DECOMPILEX_SETUP_SOURCE}")
        target_compile_features(DecompileX-Setup PRIVATE cxx_std_17)
        set_target_properties(DecompileX-Setup PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )
        install(TARGETS DecompileX-Setup RUNTIME DESTINATION bin)
    endif()
endif()

install(TARGETS DecompileX RUNTIME DESTINATION bin LIBRARY DESTINATION lib)

if(BUILD_TESTING AND NOT (ANDROID AND DECOMPILEX_BUILD_APK_LIB))
    add_test(NAME DecompileX_Help COMMAND $<TARGET_FILE:DecompileX> --help)
    add_test(NAME DecompileX_BinHex COMMAND $<TARGET_FILE:DecompileX> --bin-hex "44 58" "${CMAKE_CURRENT_BINARY_DIR}/decompilex-smoke.bin")
    set_tests_properties(DecompileX_BinHex PROPERTIES DEPENDS DecompileX_Help)
endif()
