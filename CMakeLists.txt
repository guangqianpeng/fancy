cmake_minimum_required(VERSION 2.6)
project(fancy)

set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

set(C_FLAGS
		-g
		-Wall
		-Wextra
		-Werror
		-march=native
		-rdynamic
		)
string(REPLACE ";" " " CMAKE_C_FLAGS "${C_FLAGS}")

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

include_directories(./base ./event ./http)

add_subdirectory(./base)
add_subdirectory(./event)
add_subdirectory(./http)

if(CMAKE_BUILD_TESTS)
	add_subdirectory(./test)
endif()

aux_source_directory(. SRC)

add_executable(fancy ${SRC})
target_link_libraries(fancy base event http)

install(FILES fancy.conf DESTINATION ./)
install(DIRECTORY html DESTINATION ./)
install(TARGETS fancy DESTINATION ./)