cmake_minimum_required(VERSION 3.17)

set(OGLER_VER_MAJOR 0)
set(OGLER_VER_MINOR 9)
set(OGLER_VER_REV 0)

project(ogler
    LANGUAGES C CXX
    VERSION ${OGLER_VER_MAJOR}.${OGLER_VER_MINOR}.${OGLER_VER_REV})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FetchContent)

FetchContent_Declare(
    reaper-sdk
    GIT_REPOSITORY https://github.com/justinfrankel/reaper-sdk
    GIT_TAG cf3897918283577ff06acdbfc81bea599f00c068
)
FetchContent_Declare(
    wdl
    GIT_REPOSITORY https://www-dev.cockos.com/wdl/WDL.git
    GIT_TAG 139d0006275152b38ff7bbab56ccd112a219df91
)
FetchContent_Declare(
    scintilla
    URL https://www.scintilla.org/scintilla534.tgz
    URL_HASH SHA3_224=1be146d25195ff05514a6853afb847cd2488ad6fb2be094ed7586b1a
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(
    glslang
    GIT_REPOSITORY https://github.com/KhronosGroup/glslang
    GIT_TAG 14e5a04e70057972eef8a40df422e30a3b70e4b5 # v12.1.0
)
FetchContent_Declare(
    re2c
    URL https://github.com/skvadrik/re2c/releases/download/3.0/re2c-3.0.tar.lz
    URL_HASH SHA3_224=7a5ca7acc8720ea070f6f2998f71f5c52265a4d6f43153907014ebf4
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(
    clap
    GIT_REPOSITORY https://github.com/free-audio/clap/
    GIT_TAG 41964fa3f44fdbf6feb20df528a9cdf2cc3b05ba # v1.1.8
)

set(SKIP_GLSLANG_INSTALL TRUE CACHE BOOL "" FORCE)
set(RE2C_BUILD_RE2GO FALSE CACHE BOOL "" FORCE)
set(RE2C_BUILD_RE2RUST FALSE CACHE BOOL "" FORCE)
set(RE2C_BUILD_BENCHMARKS FALSE CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(reaper-sdk wdl scintilla glslang re2c clap)

add_library(scintilla STATIC
    "${scintilla_SOURCE_DIR}/src/AutoComplete.cxx"
    "${scintilla_SOURCE_DIR}/src/CallTip.cxx"
    "${scintilla_SOURCE_DIR}/src/CaseConvert.cxx"
    "${scintilla_SOURCE_DIR}/src/CaseFolder.cxx"
    "${scintilla_SOURCE_DIR}/src/CellBuffer.cxx"
    "${scintilla_SOURCE_DIR}/src/ChangeHistory.cxx"
    "${scintilla_SOURCE_DIR}/src/CharacterCategoryMap.cxx"
    "${scintilla_SOURCE_DIR}/src/CharacterType.cxx"
    "${scintilla_SOURCE_DIR}/src/CharClassify.cxx"
    "${scintilla_SOURCE_DIR}/src/ContractionState.cxx"
    "${scintilla_SOURCE_DIR}/src/DBCS.cxx"
    "${scintilla_SOURCE_DIR}/src/Decoration.cxx"
    "${scintilla_SOURCE_DIR}/src/Document.cxx"
    "${scintilla_SOURCE_DIR}/src/EditModel.cxx"
    "${scintilla_SOURCE_DIR}/src/Editor.cxx"
    "${scintilla_SOURCE_DIR}/src/EditView.cxx"
    "${scintilla_SOURCE_DIR}/src/Geometry.cxx"
    "${scintilla_SOURCE_DIR}/src/Indicator.cxx"
    "${scintilla_SOURCE_DIR}/src/KeyMap.cxx"
    "${scintilla_SOURCE_DIR}/src/LineMarker.cxx"
    "${scintilla_SOURCE_DIR}/src/MarginView.cxx"
    "${scintilla_SOURCE_DIR}/src/PerLine.cxx"
    "${scintilla_SOURCE_DIR}/src/PositionCache.cxx"
    "${scintilla_SOURCE_DIR}/src/RESearch.cxx"
    "${scintilla_SOURCE_DIR}/src/RunStyles.cxx"
    "${scintilla_SOURCE_DIR}/src/ScintillaBase.cxx"
    "${scintilla_SOURCE_DIR}/src/Selection.cxx"
    "${scintilla_SOURCE_DIR}/src/Style.cxx"
    "${scintilla_SOURCE_DIR}/src/UniConversion.cxx"
    "${scintilla_SOURCE_DIR}/src/UniqueString.cxx"
    "${scintilla_SOURCE_DIR}/src/ViewStyle.cxx"
    "${scintilla_SOURCE_DIR}/src/XPM.cxx"

    "${scintilla_SOURCE_DIR}/call/ScintillaCall.cxx"

    "${scintilla_SOURCE_DIR}/win32/HanjaDic.cxx"
    "${scintilla_SOURCE_DIR}/win32/PlatWin.cxx"
    "${scintilla_SOURCE_DIR}/win32/ScintillaWin.cxx")
target_include_directories(scintilla
    PUBLIC "${scintilla_SOURCE_DIR}/include"
    PRIVATE "${scintilla_SOURCE_DIR}/src")
target_link_libraries(scintilla PUBLIC gdi32 user32 imm32 ole32 uuid oleaut32 msimg32)
set_target_properties(scintilla PROPERTIES CXX_STANDARD 17)

add_library(reaper_sdk INTERFACE)
target_include_directories(reaper_sdk
    INTERFACE
    "${reaper-sdk_SOURCE_DIR}/sdk"
    "${wdl_SOURCE_DIR}")

find_package(Vulkan REQUIRED)
find_package(nlohmann_json 3 REQUIRED)
find_package(Sciter REQUIRED)

add_custom_target(
    generate_resources ALL
    COMMAND "${Sciter_Packfolder}" "${CMAKE_CURRENT_SOURCE_DIR}/resources" "${CMAKE_CURRENT_BINARY_DIR}/ogler_resources_data.inc"
)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ogler_lexer_lex.cpp"
    COMMAND re2c
    ARGS "${CMAKE_CURRENT_SOURCE_DIR}/src/ogler_lexer_lex.re"
    --output "${CMAKE_CURRENT_BINARY_DIR}/ogler_lexer_lex.cpp"
    -I "${CMAKE_CURRENT_SOURCE_DIR}/src"
    --depfile "${CMAKE_CURRENT_BINARY_DIR}/ogler_lexer_lex.cpp.d"
    DEPFILE "${CMAKE_CURRENT_BINARY_DIR}/ogler_lexer_lex.cpp.d"
)

add_library(ogler_editor STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ogler_editor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ogler_lexer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ogler_resources.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sciter_scintilla.cpp"

    "${CMAKE_CURRENT_BINARY_DIR}/ogler_lexer_lex.cpp")
add_dependencies(ogler_editor generate_resources)

target_link_libraries(ogler_editor
    PUBLIC
    scintilla
    ComCtl32
    Sciter::Sciter
    nlohmann_json::nlohmann_json)
target_include_directories(ogler_editor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_BINARY_DIR}")
set_target_properties(ogler_editor
    PROPERTIES
    CXX_STANDARD 20)

add_library(ogler MODULE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/compile_shader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/IReaper.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ogler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ogler_debug.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ogler_params.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan_context.cpp")

set(OGLER_VULKAN_VER "1_0")
set_target_properties(ogler
    PROPERTIES
    CXX_STANDARD 20
    SUFFIX ".clap")
target_compile_definitions(ogler
    PRIVATE
    OGLER_VER_MAJOR=${OGLER_VER_MAJOR}
    OGLER_VER_MINOR=${OGLER_VER_MINOR}
    OGLER_VER_REV=${OGLER_VER_REV}
    OGLER_VULKAN_VER=${OGLER_VULKAN_VER})
target_link_libraries(ogler
    PRIVATE
    Vulkan::Vulkan
    Vulkan::Headers
    reaper_sdk
    nlohmann_json::nlohmann_json
    glslang
    SPIRV
    clap
    ogler_editor)
target_include_directories(ogler PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(ogler_editor_standalone WIN32
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor_standalone.cpp"
)
target_link_libraries(ogler_editor_standalone
    PRIVATE
    ogler_editor)
set_target_properties(ogler_editor_standalone
    PROPERTIES
    CXX_STANDARD 20)

install(
    TARGETS ogler
    LIBRARY DESTINATION ".")

set(CPACK_PACKAGE_VENDOR "Francesco Bertolaccini")
set(CPACK_PACKAGE_CONTACT "francesco@bertolaccini.dev")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Use GLSL shaders in REAPER")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_VERBATIM_VARIABLES TRUE)
include(CPack)