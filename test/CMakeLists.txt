add_executable(test_array test_array.c)
target_link_libraries(test_array base)

add_executable(test_list test_list.c)
target_link_libraries(test_list base)

add_executable(test_palloc test_palloc.c)
target_link_libraries(test_palloc base)

add_executable(test_rbtree test_rbtree.c)
target_link_libraries(test_rbtree base)

add_executable(test_buffer test_buffer.c)
target_link_libraries(test_buffer base)