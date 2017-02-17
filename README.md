# Fancy

Fancy is an event-driven(epoll) based high-performance Web server. The principle is similar to Nginx and can be viewed as a simplified version of Nginx. Fancy is being developed and currently only supports GET method and static page access.

Fancy是一个基于事件驱动(epoll)的高性能Web服务器。其原理与Nginx类似，可以看作简化版Nginx.Fancy正在开发，目前仅支持GET方法和静态页面访问。Fancy的特性如下：

- epoll和非阻塞IO实现高并发。众所周知，在面对大规模并发请求时，epoll性能远优于select和poll. 除此以外，epoll_wait的触发方式有两种：水平触发和边沿触发(EPOLLET)。水平触发与传统的select和poll类似，只要read/write不阻塞就会返回；而边沿触发是epoll独有的，仅当socket接收到新的数据且read/write不阻塞时epoll_wait才返回。显然，边沿触发需要内核做的事情更少，性能也自然更好，然而这也需要更复杂的编码。Fancy采用了边沿触发方式。
- 计时器。事件有两类：网络IO事件和计时器事件。前者基于epoll，后者基于Fancy自定义的计时器。计时器使用红黑树实现，性能不差。
- 协议解析。HTTP协议解析用状态机实现，难点在于与非阻塞IO的配合。在极端情况下，请求会一个字节一个字节地发送，此时会反复调用状态机，如何即正确地解析又不重复扫描字符流？Fancy在解析时会记录一个状态变量，这样下次解析时从先前的状态开始，而不是重新解析。
- 静态页面访问。文件的读取/发送也是非阻塞的. 但不同于一般的read/write方法，Fancy使用了sendfile调用避免了内核与用户区之间的数据拷贝，性能更好。

Fancy往后会支持动态内容访问（如fastCGI）以及其他的方法（HEAD和POST），敬请关注。

**彩蛋：**

- 作为练习和测试，还写了一个echo(test/echo.c)服务程序，它同样基于epoll和非阻塞IO, 有兴趣可以看看。


