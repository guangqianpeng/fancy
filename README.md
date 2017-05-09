# Fancy: High Perfermence Web Server

Fancy is an event-driven(epoll) high-performance Web server. The principle is similar to Nginx and can be viewed as a simplified version of Nginx. Fancy is being developed and currently only supports GET method and static page access.


## 介绍

Fancy是一个基于事件驱动(epoll)的高性能Web服务器。其原理与Nginx类似，可以看作简化版Nginx. Fancy正在开发，目前仅支持GET方法和静态页面访问。Fancy的特性如下：

- epoll和非阻塞IO实现高并发。在面对高并发请求时，epoll性能远优于select和poll. epoll_wait的触发方式有两种：水平触发和边沿触发(EPOLLET)。水平触发与传统的select和poll类似，只要read/write不阻塞内核就会通知；而边沿触发是epoll独有的，仅当socket接收到新的数据且read/write不阻塞时epoll_wait才返回。显然，边沿触发需要内核做的事情更少，性能也自然更好，然而这也需要更复杂的编码。Fancy采用了边沿触发方式。
- 计时器。通过对一个连接计时可以避免客户端持续占用内存资源却不发送任何请求。Fancy使用自定义的计时器来管理超时事件。计时器事件有两种选择：**红黑树**和**优先队列**。Fancy用了《算法导论》第三版中的红黑树来实现计时器。
- 协议解析。HTTP协议解析用状态机实现，难点在于与非阻塞IO的配合。在极端情况下，请求会一个字节一个字节地发送，此时会反复调用状态机，如何即正确地解析又不重复扫描字符流？Fancy在解析时会记录一个状态变量，这样下次解析时从先前的状态开始。
- 静态文件访问。文件的读取/发送也是非阻塞的. 但不同于一般的read/write方法，Fancy使用了sendfile调用避免了内核与用户区之间的数据拷贝，性能更好。
- 性能。就目前的测试结果看来，Fancy的性能约为Nginx的50%。

下个目标：

- Fancy目前只占用一个进程。之后会引入多进程（即Master & Workers）以提高性能，然而这也会引入诸如进程间通信，负载均衡等问题。
- Hash表。字符串查找和匹配还是线性查找的方法。之后会参考Nginx的代码实现一个高效的Hash表。
- 配置文件。目前Fancy没有配置文件，所有参数都只有默认值，这是很简陋的。之后会支持jason格式的配置文件。

Fancy暂时不考虑动态内容访问的功能。

## 使用

平台：64位 Ubuntu 16.04LTS

IDE：CLion

主函数在app/http.c中，默认端口9877。请把Web Pages直接放在working directory中。

链接：https://github.com/guangqianpeng/Fancy/