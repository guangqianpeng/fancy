cmake_minimum_required(VERSION 3.6)
project(fancy)

set(CMAKE_C_STANDARD 99)

include_directories(./base ./event ./http)

add_compile_options(-Wall -Werror)

add_subdirectory(./test)