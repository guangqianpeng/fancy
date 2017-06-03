##  fancy: 高性能Web服务器

fancy是一个基于事件驱动(epoll)的多进程web服务器。其原理与Nginx类似，即 **one loop per process**. fancy的特性如下：

- epoll+nonblocking IO。epoll是编写高性能服务器的基础设施，Nginx和muduo库都采用了epoll。 epoll_wait的触发方式两种：水平触发（LT）和边沿触发（ET）。对于连接事件（accept），采用水平触发；对于其他IO事件(connect, read, write)采用边沿触发。
- 反向代理HTTP服务器。简单来说就是转发HTTP request and response。
- 通过计时器踢掉空闲连接。fancy使用红黑树实现定时器。
- buffer。TCP连接的读写必须有用户态buffer。fancy参考了muduo库buffer的设计，实现了一个简易的自适应buffer。此外，对于长连接，buffer可以重用。
- 协议解析。使用状态机解析http request和response。在极端情况下，请求会1个字节1个字节地发送，此时会反复调用状态机。fancy在解析时会记录一个状态变量，这样下次解析时从先前的状态开始。
- 配置文件。fancy 的配置文件风格与Nginx一致。
- 日志。日志有debug, info, warn, err, fatal五个级别，可以在配置文件中调整日志级别。
- 性能。本机测试静态文件访问，对于100个并发连接，单进程QPS为11k。

## 使用

```
cmake .
make
```