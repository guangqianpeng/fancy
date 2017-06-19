##  fancy: 高性能Web服务器

### 简介

fancy是一个基于事件驱动(epoll)的多进程web服务器. 其原理与Nginx类似，即 **one loop per process**. 除去测试部分代码，fancy代码量约为4000行. fancy的主要特性有：

- epoll+nonblocking IO. epoll是编写高性能服务器的基础设施，Nginx和muduo库都采用了epoll.（*event.c & connection.c*）
- 反向代理HTTP服务器. 简单来说就是转发HTTP request and response.（*upstream.c & http.c*)
- 支持chunked transfer encoding.（*chunk_reader.c*）
- 踢掉空闲连接. fancy使用红黑树实现定时器， 并用于踢掉空闲连接.（*rbtree.c & timer.c*）
- 自适应buffer. TCP连接的读写必须有用户态buffer。fancy参考了muduo库buffer类的设计，实现了一个简易的自适应buffer. 此外，对于长连接，buffer可以重用.（*buffer.c*）
- 协议解析. 使用状态机解析http request和response。在极端情况下，请求会1个字节1个字节地发送。fancy在解析时会记录一个状态变量，这样每次解析都从从先前的状态开始.（*http_parser.c*）
- 配置文件. fancy 的配置文件风格与Nginx一致.（*config.c*)
- 日志. 日志有debug, info, warn, err, fatal五个级别，可以在配置文件中调整日志级别. (*log.c*)

### 内存管理

fancy使用内存池（*palloc.c*)来简化内存管理. 内存池的使用遵循两个原则：

- 多次分配，一次释放。例如，每收到一个http request，fancy会首先分配4个buffer；若要经过proxy pass，则会在处理过程中再分配4个buffer. 这8次内存分配均使用同一个内存池，并在请求处理结束后统一释放. 这里若不使用内存池，则需要4次或者8次内存释放，若忘记释放，就会内存泄漏.
- 内存池与对象的生命周期保持一致. 若对象的生命周期短于内存池，即多个对象反复使用一个内存池，则造成内存资源的浪费；若对象的生命周期长于内存池，即一个对象反复创建内存池，则违背了我们简化内存管理的初衷.

内存池不仅让fancy摆脱了内存泄漏，还大大简化了编码。

### 计时器

fancy使用红黑树来管理可能超时的连接. 例如，每来一个新的连接，它都会被挂在红黑树上，若规定时间内没有发送完整的请求，则会被踢掉：

```C
// http.c
void read_request_headers_h(event *ev)
{
    connection *conn = ev->conn;
  
    /* 对端发送请求超时 */
    if (ev->timeout) {
        LOG_WARN("%s request timeout (%dms)",
                 conn_str(conn), request_timeout);
        conn_disable_read(conn);
        response_and_close(conn, STATUS_REQUEST_TIME_OUT);
        return;
    }
    ...
}
```

由于每个进程只有一个线程，计时器必须与epoll_wait协作：

```c
// timer.c
void event_and_timer_process()
{
    int         n_ev;
    n_ev = event_process(timer_recent());
    if (n_ev == FCY_ERROR) {
        return;
    }
    timer_expired_process();
}
```

每次进入`epoll_wait`时都会传入一个超时参数`timer_recent`，它的作用是返回即将超时事件与当前时间的差值，这样就能让`epoll_wait`及时返回并在`timer_expired_process`中处理超时事件了.

## 使用

```
cmake .
make
```