cmake_minimum_required(VERSION 3.28)
project(tokenpizder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

add_library(tokenpizder STATIC
    memory.ixx
    process.ixx
)
target_link_libraries(tokenpizder PUBLIC Psapi)

add_executable(token
    main.cxx
)
target_link_libraries(token PRIVATE tokenpizder Psapi)
