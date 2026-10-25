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

file(GLOB_RECURSE DECOMPILEX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(DecompileX
    ${DECOMPILEX_SOURCES}
)

target_include_directories(DecompileX PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_definitions(DecompileX PRIVATE
    DECOMPILEX_VERSION="${PROJECT_VERSION}"
)

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
)

foreach(CONFIG Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${CONFIG}" CONFIG_UPPER)

    set_target_properties(DecompileX PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER}"
        "${CMAKE_BINARY_DIR}/bin"
    )
endforeach()

install(TARGETS DecompileX
    RUNTIME DESTINATION bin
)
