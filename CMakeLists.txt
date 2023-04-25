cmake_minimum_required(VERSION 3.17)

set(OGLER_VER_MAJOR 0)
set(OGLER_VER_MINOR 2)
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
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    USES_TERMINAL_DOWNLOAD TRUE
)

set(SKIP_GLSLANG_INSTALL TRUE CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(reaper-sdk wdl scintilla glslang)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

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

add_library(reaper_sdk STATIC "${CMAKE_CURRENT_SOURCE_DIR}/src/reaper_sdk.cpp")
target_include_directories(reaper_sdk
    PUBLIC
    "${reaper-sdk_SOURCE_DIR}/sdk"
    "${wdl_SOURCE_DIR}")

find_package(Vulkan REQUIRED COMPONENTS glslang)
find_package(nlohmann_json 3 REQUIRED)

add_library(ogler MODULE
    "src/compile_shader.cpp"
    "src/main.cpp"
    "src/ogler.cpp"
    "src/ogler_editor.cpp"
    "src/vulkan_context.cpp"
    "src/vst/ReaperVstPlugin.cpp")

set(OGLER_VULKAN_VER "1_0")
set_target_properties(ogler
    PROPERTIES
    CXX_STANDARD 20)
target_compile_definitions(ogler
    PRIVATE
    OGLER_VER_MAJOR=${OGLER_VER_MAJOR}
    OGLER_VER_MINOR=${OGLER_VER_MINOR}
    OGLER_VER_REV=${OGLER_VER_REV}
    OGLER_VULKAN_VER=${OGLER_VULKAN_VER})
target_link_libraries(ogler PRIVATE Vulkan::Vulkan Vulkan::Headers reaper_sdk scintilla ComCtl32 nlohmann_json::nlohmann_json glslang SPIRV)

install(
    TARGETS ogler
    LIBRARY DESTINATION ".")

set(CPACK_PACKAGE_VENDOR "Francesco Bertolaccini")
set(CPACK_PACKAGE_CONTACT "francesco@bertolaccini.dev")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Use OpenGL shaders in REAPER")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_VERBATIM_VARIABLES TRUE)
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Steinberg\\VstPlugins")
set(CPACK_NSIS_UNINSTALL_NAME "Uninstall ogler")
include(CPack)