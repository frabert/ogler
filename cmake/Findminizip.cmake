find_path(minizip_INCLUDE_DIR unzip.h PATH_SUFFIXES minizip)

find_library(minizip_LIBRARY NAMES minizip)

set(minizip_INCLUDE_DIRS "${minizip_INCLUDE_DIR}")
set(minizip_LIBRARIES "${minizip_LIBRARY}")

find_package_handle_standard_args(minizip DEFAULT_MSG minizip_LIBRARIES minizip_INCLUDE_DIRS)

mark_as_advanced(minizip_INCLUDE_DIR minizip_LIBRARY)

if(NOT TARGET minizip::minizip)
    add_library(minizip::minizip UNKNOWN IMPORTED)
    target_include_directories(minizip::minizip INTERFACE ${minizip_INCLUDE_DIRS})
    set_target_properties(minizip::minizip PROPERTIES IMPORTED_LOCATION ${minizip_LIBRARY})
endif()