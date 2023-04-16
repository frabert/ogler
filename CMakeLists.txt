cmake_minimum_required(VERSION 3.17)

set(OGLER_VER_MAJOR 0)
set(OGLER_VER_MINOR 1)
set(OGLER_VER_REV 0)

project(ogler
    LANGUAGES C CXX
    VERSION ${OGLER_VER_MAJOR}.${OGLER_VER_MINOR}.${OGLER_VER_REV})

include(FetchContent)

FetchContent_Declare(
    reaper-sdk
    GIT_REPOSITORY https://github.com/justinfrankel/reaper-sdk
    GIT_TAG cf3897918283577ff06acdbfc81bea599f00c068
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    USES_TERMINAL_DOWNLOAD TRUE
)
FetchContent_Declare(
    galogen
    GIT_REPOSITORY https://github.com/frabert/galogen
    GIT_TAG 44db304c6ff5eb82cfd2d367c72badc8b7874150
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    USES_TERMINAL_DOWNLOAD TRUE
)
FetchContent_Declare(
    glrepo
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenGL-Registry
    GIT_TAG dc30595984d45eaa0be3198cade8726d2091c89d
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    USES_TERMINAL_DOWNLOAD TRUE
)
FetchContent_Declare(
    eglrepo
    GIT_REPOSITORY https://github.com/KhronosGroup/EGL-Registry
    GIT_TAG 9ab603608d6b165f79f17eee9ee1ced861625893
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    USES_TERMINAL_DOWNLOAD TRUE
)
FetchContent_Declare(
    wdl
    GIT_REPOSITORY https://github.com/justinfrankel/WDL
    GIT_TAG 139d0006275152b38ff7bbab56ccd112a219df91
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    USES_TERMINAL_DOWNLOAD TRUE
)

FetchContent_MakeAvailable(reaper-sdk wdl galogen glrepo eglrepo)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_executable(galogen
    "${galogen_SOURCE_DIR}/galogen.cpp"
    "${galogen_SOURCE_DIR}/third_party/tinyxml2.cpp")

add_custom_command(
    OUTPUT
    "${CMAKE_CURRENT_BINARY_DIR}/gl.h"
    "${CMAKE_CURRENT_BINARY_DIR}/gl.c"
    COMMAND galogen "${glrepo_SOURCE_DIR}/xml/gl.xml" --api gl --ver 4.6 --profile core
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS "${glrepo_SOURCE_DIR}/xml/gl.xml"
    COMMENT "Generating gl.h")

add_library(gl STATIC
    "${CMAKE_CURRENT_BINARY_DIR}/gl.c")
target_include_directories(gl PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${eglrepo_SOURCE_DIR}/api")

add_library(reaper_sdk STATIC "${CMAKE_CURRENT_SOURCE_DIR}/src/reaper_sdk.cpp")
target_include_directories(reaper_sdk
    PUBLIC
    "${reaper-sdk_SOURCE_DIR}/sdk"
    "${wdl_SOURCE_DIR}")

find_package(GLFW3 REQUIRED)
find_package(OpenGL REQUIRED COMPONENTS OpenGL)

add_library(ogler MODULE
    "src/main.cpp"
    "src/ogler.cpp"
    "src/opengl.cpp"
    "src/vst/ReaperVstPlugin.cpp")

set_target_properties(ogler
    PROPERTIES
    CXX_STANDARD 20)
target_compile_definitions(ogler
    PRIVATE
    OGLER_VER_MAJOR=${OGLER_VER_MAJOR}
    OGLER_VER_MINOR=${OGLER_VER_MINOR}
    OGLER_VER_REV=${OGLER_VER_REV})
target_link_libraries(ogler PRIVATE gl GLFW::GLFW3 OpenGL::GL reaper_sdk)