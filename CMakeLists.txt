cmake_minimum_required(VERSION 3.16)

project(DecompileX
    VERSION 0.1.0
    DESCRIPTION "A Game Decompiler written in C++"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DECOMPILEX_BUILD_SHARED "Build DecompileX as a shared library on Android" OFF)

file(GLOB_RECURSE DECOMPILEX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

if(ANDROID AND DECOMPILEX_BUILD_SHARED)
    add_library(DecompileX SHARED
        ${DECOMPILEX_SOURCES}
    )
else()
    add_executable(DecompileX
        ${DECOMPILEX_SOURCES}
    )
endif()

target_include_directories(DecompileX PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_definitions(DecompileX PRIVATE
    DECOMPILEX_VERSION="${PROJECT_VERSION}"
)

if(ANDROID)
    target_compile_definitions(DecompileX PRIVATE
        DECOMPILEX_ANDROID=1
    )

    if(CMAKE_ANDROID_ARCH_ABI)
        message(STATUS "Android ABI: ${CMAKE_ANDROID_ARCH_ABI}")
    elseif(ANDROID_ABI)
        message(STATUS "Android ABI: ${ANDROID_ABI}")
    endif()

    message(STATUS "Android API Level: ${ANDROID_PLATFORM}")

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

if(MSVC)
    target_compile_options(DecompileX PRIVATE
        /W4
        /EHsc
    )
else()
    target_compile_options(DecompileX PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

set_target_properties(DecompileX PROPERTIES
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

if(ANDROID)
    if(DECOMPILEX_BUILD_SHARED)
        install(TARGETS DecompileX
            LIBRARY DESTINATION lib
        )
    else()
        install(TARGETS DecompileX
            RUNTIME DESTINATION bin
        )
    endif()
else()
    install(TARGETS DecompileX
        RUNTIME DESTINATION bin
    )
endif()
