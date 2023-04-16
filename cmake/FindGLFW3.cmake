find_path(GLFW3_INCLUDE_DIR NAMES "GLFW/glfw3.h")
mark_as_advanced(GLFW3_INCLUDE_DIR)

# Look for the necessary library
find_library(GLFW3_LIBRARY NAMES GLFW3)
mark_as_advanced(GLFW3_LIBRARY)

# Extract version information from the header file
if(GLFW3_INCLUDE_DIR)
    file(STRINGS "${GLFW3_INCLUDE_DIR}/GLFW/glfw3.h" _ver_major
        REGEX "^#define +GLFW_VERSION_MAJOR +[0-9]+")
    string(REGEX MATCH "[0-9]+" GLFW3_VERSION_MAJOR "${_ver_major}")
    unset(_ver_major)
    file(STRINGS "${GLFW3_INCLUDE_DIR}/GLFW/glfw3.h" _ver_minor
        REGEX "^#define +GLFW_VERSION_MINOR +[0-9]+")
    string(REGEX MATCH "[0-9]+" GLFW3_VERSION_MINOR "${_ver_minor}")
    unset(_ver_minor)
    file(STRINGS "${GLFW3_INCLUDE_DIR}/GLFW/glfw3.h" _ver_patch
        REGEX "^#define +GLFW_VERSION_REVISION +[0-9]+")
    string(REGEX MATCH "[0-9]+" GLFW3_VERSION_PATCH "${_ver_patch}")
    unset(_ver_patch)

    set(GLFW3_VERSION "${GLFW3_VERSION_MAJOR}.${GLFW3_VERSION_MINOR}.${GLFW3_VERSION_PATCH}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLFW3
    REQUIRED_VARS GLFW3_INCLUDE_DIR GLFW3_LIBRARY
    VERSION_VAR GLFW3_VERSION)

# Create the imported target
if(GLFW3_FOUND)
    set(GLFW3_INCLUDE_DIRS ${GLFW3_INCLUDE_DIR})
    set(GLFW3_LIBRARIES ${GLFW3_LIBRARY})

    if(NOT TARGET GLFW::GLFW3)
        add_library(GLFW::GLFW3 UNKNOWN IMPORTED)
        set_target_properties(GLFW::GLFW3 PROPERTIES
            IMPORTED_LOCATION "${GLFW3_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${GLFW3_INCLUDE_DIR}")
    endif()
endif()