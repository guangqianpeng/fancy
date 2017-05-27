add_compile_options(-Wall -Werror)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/html)

set(ERROR ../base/log.h ../base/log.c ../base/Signal.c ../base/base.h)
set(PALLOC ../base/palloc.h ../base/palloc.c ${ERROR})
set(BUFFER ../base/buffer.h ../base/buffer.c ${ERROR} ${PALLOC})
set(ARRAY ../base/array.h ../base/array.c ${PALLOC} ${ERROR})
set(RBTREE ../base/rbtree.h ../base/rbtree.c ${ERROR})
set(LIST ../base/list.h ../base/list.c)
set(EVENT
        ../event/connection.c ../event/connection.h
        ../event/event.c ../event/event.h
        ../event/timer.c ../event/timer.h
        ${BUFFER} ${ARRAY} ${RBTREE} ${LIST})

set(PARSE ../http/http_parser.c
        ../http/http_parser.h
        ../http/request.h ${EVENT})
set(PROCESS
        ../http/request.c
        ../http/request.h
        ../http/http_parser.h
        ../http/http_parser.c
        ${EVENT} ${PARSE})
set(REQUEST ${PROCESS})

add_executable(test_palloc test_palloc.c ${PALLOC})

add_executable(test_rbtree test_rbtree.c ${RBTREE})

add_executable(test_array test_array.c ${ARRAY})

add_executable(test_list test_list.c ${LIST})

add_executable(echo ../app/app.c ${EVENT})

add_executable(echo_cli ../app/echo_cli.c ${EVENT})

add_executable(test_process_request test_process_request.c ${PROCESS})

add_executable(http
        ../app/http.c ../app/http.h
        ../http/upstream.c ${REQUEST})