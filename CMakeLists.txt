cmake_minimum_required(VERSION 3.16)

project(DecompileX
    VERSION 0.2.0
    DESCRIPTION "Advanced Game Binary Analysis Toolkit"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DECOMPILEX_BUILD_APK_LIB "Build Android JNI library" OFF)
option(DECOMPILEX_ENABLE_WARNINGS "Enable compiler warnings" ON)

set(DECOMPILEX_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

set(DECOMPILEX_BACKEND_DIR
    "${DECOMPILEX_SOURCE_DIR}/backend"
)

set(DECOMPILEX_MAIN_SOURCES
    "${DECOMPILEX_SOURCE_DIR}/Main.cpp"
    "${DECOMPILEX_BACKEND_DIR}/ZipGenerator.cpp"
    "${DECOMPILEX_BACKEND_DIR}/ExeCompiler.cpp"
)

set(DECOMPILEX_BACKEND_HEADERS
    "${DECOMPILEX_BACKEND_DIR}/ZipGenerator.hpp"
    "${DECOMPILEX_BACKEND_DIR}/ExeCompiler.hpp"
)

set(DECOMPILEX_ANDROID_BRIDGE
    "${DECOMPILEX_SOURCE_DIR}/android/NativeBridge.cpp"
)

if(ANDROID AND DECOMPILEX_BUILD_APK_LIB)
    add_library(DecompileX SHARED
        "${DECOMPILEX_ANDROID_BRIDGE}"
        "${DECOMPILEX_BACKEND_DIR}/ZipGenerator.cpp"
        "${DECOMPILEX_BACKEND_DIR}/ExeCompiler.cpp"
        ${DECOMPILEX_BACKEND_HEADERS}
    )

    target_link_libraries(DecompileX PRIVATE
        log
    )
else()
    set(DECOMPILEX_PLATFORM_SOURCES)

    if(WIN32)
        enable_language(RC)

        set(DECOMPILEX_ICON_RC
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/DecompileX.rc"
        )

        if(EXISTS "${DECOMPILEX_ICON_RC}")
            list(APPEND DECOMPILEX_PLATFORM_SOURCES
                "${DECOMPILEX_ICON_RC}"
            )
        endif()
    endif()

    add_executable(DecompileX
        ${DECOMPILEX_MAIN_SOURCES}
        ${DECOMPILEX_BACKEND_HEADERS}
        ${DECOMPILEX_PLATFORM_SOURCES}
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

if(ANDROID)
    target_compile_definitions(DecompileX PRIVATE
        DECOMPILEX_ANDROID=1
    )
elseif(WIN32)
    target_compile_definitions(DecompileX PRIVATE
        DECOMPILEX_WINDOWS=1
    )
elseif(APPLE)
    target_compile_definitions(DecompileX PRIVATE
        DECOMPILEX_MACOS=1
    )
elseif(UNIX)
    target_compile_definitions(DecompileX PRIVATE
        DECOMPILEX_LINUX=1
    )
endif()

if(DECOMPILEX_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(DecompileX PRIVATE
            /W4
            /EHsc
            /utf-8
        )
    else()
        target_compile_options(DecompileX PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()
endif()

set_target_properties(DecompileX PROPERTIES
    OUTPUT_NAME "DecompileX"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

foreach(CONFIG Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${CONFIG}" CONFIG_UPPER)

    set_target_properties(DecompileX PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER}"
        "${CMAKE_BINARY_DIR}/bin"
        "LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER}"
        "${CMAKE_BINARY_DIR}/lib"
        "ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER}"
        "${CMAKE_BINARY_DIR}/lib"
    )
endforeach()

install(TARGETS DecompileX
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)
