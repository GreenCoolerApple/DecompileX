cmake_minimum_required(VERSION 3.16)

project(DecompileX VERSION 0.0.1 DESCRIPTION "Binary analysis and archive toolkit" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DECOMPILEX_BUILD_APK_LIB "Build Android JNI shared library" OFF)
option(DECOMPILEX_BUILD_SETUP "Build optional setup utility" ON)
option(DECOMPILEX_ENABLE_WARNINGS "Enable compiler warnings" ON)
option(DECOMPILEX_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(DECOMPILEX_ENABLE_TESTS "Enable CLI smoke tests" ON)

if(DECOMPILEX_BUILD_APK_LIB AND NOT ANDROID)
    message(FATAL_ERROR "DECOMPILEX_BUILD_APK_LIB requires an Android NDK toolchain")
endif()

if(ANDROID AND NOT DECOMPILEX_BUILD_APK_LIB)
    message(FATAL_ERROR "Android builds require -DDECOMPILEX_BUILD_APK_LIB=ON")
endif()

set(DECOMPILEX_SRC "${PROJECT_SOURCE_DIR}/src")
set(DECOMPILEX_BACKEND "${DECOMPILEX_SRC}/backend")

set(DECOMPILEX_BACKEND_SOURCES
    "${DECOMPILEX_BACKEND}/ZipGenerator.cpp"
    "${DECOMPILEX_BACKEND}/ExeCompiler.cpp"
    "${DECOMPILEX_BACKEND}/ISOCompiler.cpp"
    "${DECOMPILEX_BACKEND}/BinCompiler.cpp"
)

set(DECOMPILEX_BACKEND_HEADERS
    "${DECOMPILEX_BACKEND}/ZipGenerator.hpp"
    "${DECOMPILEX_BACKEND}/ExeCompiler.hpp"
    "${DECOMPILEX_BACKEND}/ISOCompiler.hpp"
    "${DECOMPILEX_BACKEND}/BinCompiler.hpp"
)

foreach(DECOMPILEX_REQUIRED IN LISTS DECOMPILEX_BACKEND_SOURCES DECOMPILEX_BACKEND_HEADERS)
    if(NOT EXISTS "${DECOMPILEX_REQUIRED}")
        message(FATAL_ERROR "Required DecompileX file not found: ${DECOMPILEX_REQUIRED}")
    endif()
endforeach()

add_library(DecompileXBackend STATIC
    ${DECOMPILEX_BACKEND_SOURCES}
    ${DECOMPILEX_BACKEND_HEADERS}
)

set_target_properties(DecompileXBackend PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

target_include_directories(DecompileXBackend PUBLIC
    "${DECOMPILEX_SRC}"
    "${DECOMPILEX_BACKEND}"
)

target_compile_features(DecompileXBackend PUBLIC cxx_std_17)

function(decompilex_configure_target target)
    target_compile_definitions(${target} PRIVATE
        DECOMPILEX_VERSION="${PROJECT_VERSION}"
        DECOMPILEX_NAME="${PROJECT_NAME}"
    )

    if(ANDROID)
        target_compile_definitions(${target} PRIVATE DECOMPILEX_ANDROID=1)
    elseif(WIN32)
        target_compile_definitions(${target} PRIVATE DECOMPILEX_WINDOWS=1)
    elseif(APPLE)
        target_compile_definitions(${target} PRIVATE DECOMPILEX_MACOS=1)
    elseif(UNIX)
        target_compile_definitions(${target} PRIVATE DECOMPILEX_LINUX=1)
    endif()

    if(DECOMPILEX_ENABLE_WARNINGS)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /EHsc /utf-8)
            if(DECOMPILEX_WARNINGS_AS_ERRORS)
                target_compile_options(${target} PRIVATE /WX)
            endif()
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
            if(DECOMPILEX_WARNINGS_AS_ERRORS)
                target_compile_options(${target} PRIVATE -Werror)
            endif()
        endif()
    endif()
endfunction()

decompilex_configure_target(DecompileXBackend)

if(ANDROID)
    set(DECOMPILEX_JNI_SOURCE "${DECOMPILEX_SRC}/android/NativeBridge.cpp")
    if(NOT EXISTS "${DECOMPILEX_JNI_SOURCE}")
        message(FATAL_ERROR "Android JNI source missing: ${DECOMPILEX_JNI_SOURCE}")
    endif()

    add_library(DecompileX SHARED "${DECOMPILEX_JNI_SOURCE}")
    target_link_libraries(DecompileX PRIVATE DecompileXBackend log)
    decompilex_configure_target(DecompileX)
else()
    set(DECOMPILEX_MAIN "${DECOMPILEX_SRC}/Main.cpp")
    if(NOT EXISTS "${DECOMPILEX_MAIN}")
        message(FATAL_ERROR "Desktop entry point missing: ${DECOMPILEX_MAIN}")
    endif()

    set(DECOMPILEX_RESOURCES)
    if(WIN32)
        set(DECOMPILEX_ICON_RC "${PROJECT_SOURCE_DIR}/assets/icons/DecompileX.rc")
        if(EXISTS "${DECOMPILEX_ICON_RC}")
            enable_language(RC)
            list(APPEND DECOMPILEX_RESOURCES "${DECOMPILEX_ICON_RC}")
        endif()
    endif()

    add_executable(DecompileX "${DECOMPILEX_MAIN}" ${DECOMPILEX_RESOURCES})
    target_link_libraries(DecompileX PRIVATE DecompileXBackend)
    decompilex_configure_target(DecompileX)

    if(DECOMPILEX_BUILD_SETUP)
        set(DECOMPILEX_SETUP "${PROJECT_SOURCE_DIR}/tools/Setup.cpp")
        if(EXISTS "${DECOMPILEX_SETUP}")
            add_executable(DecompileX-Setup "${DECOMPILEX_SETUP}")
            decompilex_configure_target(DecompileX-Setup)
            set_target_properties(DecompileX-Setup PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            )
            install(TARGETS DecompileX-Setup RUNTIME DESTINATION bin)
        else()
            message(STATUS "Optional Setup.cpp not found; skipping DecompileX-Setup")
        endif()
    endif()
endif()

set_target_properties(DecompileX PROPERTIES
    OUTPUT_NAME "DecompileX"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

foreach(DECOMPILEX_CONFIG Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${DECOMPILEX_CONFIG}" DECOMPILEX_CONFIG_UPPER)
    set_target_properties(DecompileX PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${DECOMPILEX_CONFIG_UPPER}" "${CMAKE_BINARY_DIR}/bin"
        "LIBRARY_OUTPUT_DIRECTORY_${DECOMPILEX_CONFIG_UPPER}" "${CMAKE_BINARY_DIR}/lib"
    )
endforeach()

install(TARGETS DecompileX
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

if(NOT ANDROID AND DECOMPILEX_ENABLE_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        add_test(NAME DecompileX_Help COMMAND $<TARGET_FILE:DecompileX> --help)
        add_test(NAME DecompileX_BinHex
            COMMAND $<TARGET_FILE:DecompileX> --bin-hex "44 58" "${CMAKE_CURRENT_BINARY_DIR}/decompilex-smoke.bin")
        set_tests_properties(DecompileX_BinHex PROPERTIES DEPENDS DecompileX_Help)
    endif()
endif()

message(STATUS "DecompileX ${PROJECT_VERSION}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Android JNI library: ${DECOMPILEX_BUILD_APK_LIB}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
