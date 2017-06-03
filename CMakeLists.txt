cmake_minimum_required(VERSION 3.6)
project(fancy)

set(CMAKE_C_STANDARD 11)

add_compile_options(-Wall -Werror)

set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -finline-limit=1000 -DNDEBUG")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

include_directories(./base ./event ./http)

add_subdirectory(./base)
add_subdirectory(./event)
add_subdirectory(./http)
add_subdirectory(./test)

aux_source_directory(. SRC)

add_executable(fancy ${SRC})
target_link_libraries(fancy base event http)