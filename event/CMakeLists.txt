aux_source_directory(. SRC)
add_library(event STATIC ${SRC})
target_link_libraries(event base)