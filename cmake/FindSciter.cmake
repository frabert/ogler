find_path(Sciter_INCLUDE_DIR NAMES "sciter-js/sciter-x.h")
mark_as_advanced(Sciter_INCLUDE_DIR)

function(check_is_static validator_result_var item)
    if(NOT item MATCHES [[\.(lib|a)$]])
        set(${validator_result_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

find_library(Sciter_LIBRARY
    NAMES "sciter" "sciter-static-release" "sciter-static-debug"
    HINTS "${Sciter_INCLUDE_DIR}/../../lib"
    PATH_SUFFIXES "windows/x64" "windows/x86"
    VALIDATOR check_is_static)

find_program(Sciter_Packfolder "packfolder"
    HINTS "${Sciter_INCLUDE_DIR}/../bin"
    PATH_SUFFIXES "windows" "linux" "macosx")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sciter
    REQUIRED_VARS Sciter_INCLUDE_DIR Sciter_Packfolder)

# Create the imported target
if(Sciter_FOUND)
    set(Sciter_INCLUDE_DIRS ${Sciter_INCLUDE_DIR})

    if(NOT TARGET Sciter::Sciter)
        if(Sciter_LIBRARY STREQUAL "Sciter_LIBRARY-NOTFOUND")
            add_library(Sciter::Sciter INTERFACE IMPORTED)
            set_target_properties(Sciter::Sciter PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${Sciter_INCLUDE_DIRS}")
        else()
            message(NOTICE "Found static Sciter library")
            set(Sciter_LIBRARIES "${Sciter_LIBRARY}")
            add_library(Sciter::Sciter STATIC IMPORTED)
            set_target_properties(Sciter::Sciter PROPERTIES
                IMPORTED_LOCATION "${Sciter_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${Sciter_INCLUDE_DIRS}")
            target_compile_definitions(Sciter::Sciter INTERFACE STATIC_LIB)
        endif()

        add_executable(packfolder IMPORTED)
        set_property(TARGET packfolder PROPERTY IMPORTED_LOCATION ${Sciter_Packfolder})

        add_executable(Sciter::packfolder ALIAS packfolder)
    endif()
endif()