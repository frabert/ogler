find_package(ZLIB REQUIRED)

add_executable(ogler_archiver "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
target_link_libraries(ogler_archiver PRIVATE ZLIB::ZLIB SQLite::SQLite3)
set_target_properties(ogler_archiver
    PROPERTIES
    CXX_STANDARD 20
)