add_compile_options(-Wall -Werror)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/bin)

set(ERROR ../base/error.h ../base/error.c ../base/base.h)
set(PALLOC ../base/palloc.h ../base/palloc.c ${ERROR})
set(BUFFER ../base/buffer.h ../base/buffer.c ${ERROR} ${PALLOC})
set(ARRAY ../base/array.h ../base/array.c ${PALLOC} ${ERROR})
set(RBTREE ../base/rbtree.h ../base/rbtree.c ${ERROR})
set(LIST ../base/list.h ../base/list.c)
set(EVENT
        ../event/conn_pool.c ../event/conn_pool.h
        ../event/event.c ../event/event.h
        ../event/timer.c ../event/timer.h
        ${BUFFER} ${ARRAY} ${RBTREE} ${LIST})

set(PARSE ../http/parse_request.c
        ../http/request.h ${EVENT})
set(PROCESS ../http/process_request.c
        ../http/request.c
        ../http/request.h ${EVENT} ${PARSE})
set(REQUEST ${PARSE} ${PROCESS})

add_executable(test_palloc test_palloc.c ${PALLOC})

add_executable(test_rbtree test_rbtree.c ${RBTREE})

add_executable(test_array test_array.c ${ARRAY})

add_executable(test_list test_list.c ${LIST})

add_executable(echo echo.c app.c ${EVENT})

add_executable(echo_cli echo_cli.c ${EVENT})

add_executable(test_parse_request test_parse_request.c ${PARSE})

add_executable(test_process_request test_process_request.c ${PROCESS})

add_executable(http http.c app.c ${REQUEST})