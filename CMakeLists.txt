cmake_minimum_required(VERSION 3.16)

project(DecompileX VERSION 0.0.1 DESCRIPTION "Binary analysis and archive toolkit" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

option(DECOMPILEX_BUILD_APK_LIB "Build the Android JNI library" OFF)
option(DECOMPILEX_BUILD_SETUP "Build the optional setup utility" ON)
option(DECOMPILEX_BUILD_TERMINAL "Build the desktop terminal service" ON)
option(DECOMPILEX_BUILD_TERMINAL_DEMO "Build the terminal demonstration" OFF)
option(DECOMPILEX_ENABLE_TESTS "Enable desktop smoke tests" ON)
option(DECOMPILEX_ENABLE_WARNINGS "Enable compiler warnings" ON)
option(DECOMPILEX_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

if(DECOMPILEX_BUILD_APK_LIB AND NOT ANDROID)
    message(FATAL_ERROR "DECOMPILEX_BUILD_APK_LIB requires the Android NDK toolchain")
endif()
if(ANDROID AND NOT DECOMPILEX_BUILD_APK_LIB)
    message(FATAL_ERROR "Android builds require -DDECOMPILEX_BUILD_APK_LIB=ON")
endif()

set(DECOMPILEX_SRC_DIR "${PROJECT_SOURCE_DIR}/src")
set(DECOMPILEX_BACKEND_DIR "${DECOMPILEX_SRC_DIR}/backend")
set(DECOMPILEX_TOOLS_DIR "${PROJECT_SOURCE_DIR}/tools")
set(DECOMPILEX_TERMINAL_DIR "${DECOMPILEX_TOOLS_DIR}/service")

set(DECOMPILEX_BACKEND_SOURCES
    "${DECOMPILEX_BACKEND_DIR}/ZipGenerator.cpp"
    "${DECOMPILEX_BACKEND_DIR}/ExeCompiler.cpp"
    "${DECOMPILEX_BACKEND_DIR}/ISOCompiler.cpp"
    "${DECOMPILEX_BACKEND_DIR}/BinCompiler.cpp"
    "${DECOMPILEX_BACKEND_DIR}/PspAnalyzer.cpp"
)
set(DECOMPILEX_BACKEND_HEADERS
    "${DECOMPILEX_BACKEND_DIR}/ZipGenerator.hpp"
    "${DECOMPILEX_BACKEND_DIR}/ExeCompiler.hpp"
    "${DECOMPILEX_BACKEND_DIR}/ISOCompiler.hpp"
    "${DECOMPILEX_BACKEND_DIR}/BinCompiler.hpp"
    "${DECOMPILEX_BACKEND_DIR}/PspAnalyzer.hpp"
)

function(decompilex_require_files)
    foreach(file IN LISTS ARGN)
        if(NOT EXISTS "${file}")
            message(FATAL_ERROR "Required DecompileX file missing: ${file}")
        endif()
    endforeach()
endfunction()

decompilex_require_files(${DECOMPILEX_BACKEND_SOURCES} ${DECOMPILEX_BACKEND_HEADERS})

function(decompilex_configure_target target)
    target_compile_features(${target} PUBLIC cxx_std_17)
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

function(decompilex_set_output_dirs target)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
    foreach(config Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER "${config}" upper)
        set_target_properties(${target} PROPERTIES
            "RUNTIME_OUTPUT_DIRECTORY_${upper}" "${CMAKE_BINARY_DIR}/bin"
            "LIBRARY_OUTPUT_DIRECTORY_${upper}" "${CMAKE_BINARY_DIR}/lib"
            "ARCHIVE_OUTPUT_DIRECTORY_${upper}" "${CMAKE_BINARY_DIR}/lib"
        )
    endforeach()
endfunction()

add_library(DecompileXBackend STATIC
    ${DECOMPILEX_BACKEND_SOURCES}
    ${DECOMPILEX_BACKEND_HEADERS}
)
set_target_properties(DecompileXBackend PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(DecompileXBackend PUBLIC
    "${DECOMPILEX_SRC_DIR}"
    "${DECOMPILEX_BACKEND_DIR}"
)
decompilex_configure_target(DecompileXBackend)
decompilex_set_output_dirs(DecompileXBackend)

if(ANDROID)
    set(jni_source "${DECOMPILEX_SRC_DIR}/android/NativeBridge.cpp")
    decompilex_require_files("${jni_source}")
    add_library(DecompileX SHARED "${jni_source}")
    target_link_libraries(DecompileX PRIVATE DecompileXBackend log)
    decompilex_configure_target(DecompileX)
else()
    set(main_source "${DECOMPILEX_SRC_DIR}/Main.cpp")
    decompilex_require_files("${main_source}")

    if(DECOMPILEX_BUILD_TERMINAL)
        set(terminal_cpp "${DECOMPILEX_TERMINAL_DIR}/Terminal.cpp")
        set(terminal_hpp "${DECOMPILEX_TERMINAL_DIR}/Terminal.hpp")
        decompilex_require_files("${terminal_cpp}" "${terminal_hpp}")
        add_library(DecompileXTerminal STATIC "${terminal_cpp}" "${terminal_hpp}")
        target_include_directories(DecompileXTerminal PUBLIC "${DECOMPILEX_TERMINAL_DIR}")
        decompilex_configure_target(DecompileXTerminal)
        decompilex_set_output_dirs(DecompileXTerminal)
    endif()

    set(desktop_resources)
    if(WIN32)
        set(icon_rc "${PROJECT_SOURCE_DIR}/assets/icons/DecompileX.rc")
        if(EXISTS "${icon_rc}")
            enable_language(RC)
            list(APPEND desktop_resources "${icon_rc}")
        else()
            message(STATUS "Windows icon resource not found; building without icon")
        endif()
    endif()

    add_executable(DecompileX "${main_source}" ${desktop_resources})
    target_link_libraries(DecompileX PRIVATE DecompileXBackend)
    if(TARGET DecompileXTerminal)
        target_link_libraries(DecompileX PRIVATE DecompileXTerminal)
    endif()
    decompilex_configure_target(DecompileX)

    if(DECOMPILEX_BUILD_SETUP)
        set(setup_source "${DECOMPILEX_TOOLS_DIR}/Setup.cpp")
        if(EXISTS "${setup_source}")
            add_executable(DecompileX-Setup "${setup_source}")
            if(TARGET DecompileXTerminal)
                target_link_libraries(DecompileX-Setup PRIVATE DecompileXTerminal)
            endif()
            decompilex_configure_target(DecompileX-Setup)
            decompilex_set_output_dirs(DecompileX-Setup)
            install(TARGETS DecompileX-Setup RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
        else()
            message(STATUS "Setup.cpp not found; skipping optional setup executable")
        endif()
    endif()

    if(DECOMPILEX_BUILD_TERMINAL_DEMO)
        if(NOT TARGET DecompileXTerminal)
            message(FATAL_ERROR "Terminal demo requires DECOMPILEX_BUILD_TERMINAL=ON")
        endif()
        set(demo_source "${DECOMPILEX_TERMINAL_DIR}/TerminalDemo.cpp")
        decompilex_require_files("${demo_source}")
        add_executable(DecompileX-TerminalDemo "${demo_source}")
        target_link_libraries(DecompileX-TerminalDemo PRIVATE DecompileXTerminal)
        decompilex_configure_target(DecompileX-TerminalDemo)
        decompilex_set_output_dirs(DecompileX-TerminalDemo)
    endif()
endif()

decompilex_set_output_dirs(DecompileX)
set_target_properties(DecompileX PROPERTIES OUTPUT_NAME DecompileX)
install(TARGETS DecompileX
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

if(NOT ANDROID AND DECOMPILEX_ENABLE_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        add_test(NAME DecompileX_Help COMMAND $<TARGET_FILE:DecompileX> --help)
        add_test(NAME DecompileX_BinHex
            COMMAND $<TARGET_FILE:DecompileX> --bin-hex "44 58" "${CMAKE_CURRENT_BINARY_DIR}/decompilex-smoke.bin")
        set_tests_properties(DecompileX_BinHex PROPERTIES DEPENDS DecompileX_Help)
        if(TARGET DecompileX-TerminalDemo)
            add_test(NAME DecompileX_TerminalDemo COMMAND $<TARGET_FILE:DecompileX-TerminalDemo>)
        endif()
    endif()
endif()

message(STATUS "DecompileX version: ${PROJECT_VERSION}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Android JNI: ${DECOMPILEX_BUILD_APK_LIB}")
message(STATUS "Desktop terminal: ${DECOMPILEX_BUILD_TERMINAL}")
message(STATUS "Tests enabled: ${DECOMPILEX_ENABLE_TESTS}")
