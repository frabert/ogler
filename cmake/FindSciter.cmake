find_path(Sciter_INCLUDE_DIR NAMES "sciter-x.h")
mark_as_advanced(Sciter_INCLUDE_DIR)

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
        add_library(Sciter::Sciter INTERFACE IMPORTED)
        set_target_properties(Sciter::Sciter PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${Sciter_INCLUDE_DIRS}")
    endif()
endif()