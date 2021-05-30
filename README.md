# 入门级并发web服务器项目(C/C++)  ~~详细解析

### 服务器基本框架

![](https://github.com/XMULLT/IMG/raw/master/thread_img/服务器基本框架.jpg)

|     模块     |            功能            |
| :----------: | :------------------------: |
| I/O 处理单元 | 处理客户连接，读写网络数据 |
|   逻辑单元   |       业务进程或线程       |
| 网络存储单元 |     数据库、文件或缓存     |
|   请求队列   |    各单元之间的通信方式    |

### 服务器需要处理的事件

- 服务器程序通常需要处理三类事件：I/O 事件、信号及定时事件。有两种高效的事件处理模式：Reactor 和 Proactor，同步 I/O 模型通常用于实现 Reactor 模式，异步 I/O 模型通常用于实现 Proactor 模式。具体可参考：[点击](https://zhuanlan.zhihu.com/p/60189318)

### 该项目的程序处理流程

![](https://github.com/XMULLT/IMG/raw/master/thread_img/程序处理流程.jpg)

- 项目使用同步 I/O 方式模拟Proactor模式，工作流程如下： 
  - 主线程往epoll内核事件表中注册socket上的读就绪事件。
  - 主线程调用epoll_wait()等待socket上有数据可读。 
  - 当 socket 上有数据可读时，epoll_wait()通知主线程。主线程从socket循环读取数据，直到没有更多数据可读，然后将读取到的数据封装成一个请求对象并插入请求队列。 
  - 睡眠在请求队列上的某个工作线程被唤醒，它获得请求对象并处理客户请求，然后往 epoll 内核事件表中注册socket上的写就绪事件。 
  - 主线程调用 epoll_wait 等待 socket 可写。
  - 当 socket 可写时，epoll_wait()通知主线程。主线程往 socket 上写入服务器处理客户请求的结果。
  - 同时主线程通过定时事件，每隔一段时间检测不活跃的连接，并关闭不活跃的连接来释放被占用的文件描述符。

### 项目说明

#### 文件结构说明

![](https://github.com/XMULLT/IMG/raw/master/thread_img/文件夹结构图.jpg)

- 主文件夹为webserver
- includes文件夹存放.h文件
- sources文件夹存放.cpp文件
- others文件夹存放请求的资源
- main.cpp为主函数

#### 运行说明

- 平台：Linux （我用的是kubantu-20.04版本）

- 在webserver文件夹下执行**make**命令，会生成名为**myweb**的可执行文件

- 在webserver文件夹下运行

```c++
  ./myweb 端口号
  如：./myweb 10000
```

- 启动服务器后在浏览器中输入网址即可访问服务器

```c++
http://xxx.xxx.xxx.xxx:***/index.html //xxx.xxx.xxx.xxx:***为ip+端口号
如：http://192.168.137.11:10000/index.html
```

#### 压力测试结果

- 使用webbench对服务器的并发访问进行测试

```c++
./webbench -c 10000 -t 5 http://10.24.88.50:10000/index.html
// 10000个客户端访问5秒
Webbench - Simple Web Benchmark 1.5
Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.

Benchmarking: GET http://10.24.88.50:10000/index.html
10000 clients, running 5 sec.

Speed=969048 pages/min, 2567977 bytes/sec.
Requests: 80754 susceed, 0 failed.
```

- 结果表明，服务器可以承受近万访问请求。

#### 参考

- [B站视频](https://www.bilibili.com/video/BV1iJ411S7UA?from=search&seid=16481824339543181936)
- 《UNIX网络编程》等

有错的地方希望大佬们指正~~
