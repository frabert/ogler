find_path(uv_INCLUDE_DIR uv.h)

find_library(uv_LIBRARY NAMES uv libuv)

set(uv_INCLUDE_DIRS "${uv_INCLUDE_DIR}")
set(uv_LIBRARIES "${uv_LIBRARY}")

find_package_handle_standard_args(uv DEFAULT_MSG uv_LIBRARIES uv_INCLUDE_DIRS)

mark_as_advanced(uv_INCLUDE_DIR uv_LIBRARY)

if(NOT TARGET uv::uv)
    add_library(uv::uv UNKNOWN IMPORTED)
    target_include_directories(uv::uv INTERFACE ${uv_INCLUDE_DIRS})
    set_target_properties(uv::uv PROPERTIES IMPORTED_LOCATION ${uv_LIBRARY})
endif()