### 各种IO模型

- [各种IO模型的介绍](https://mp.weixin.qq.com/s/VkqBEKKBYIFWL2XsFmMIVw)

### IO多路复用

- I/O多路复用使得程序能同时监听多个文件描述符，能够提高程序的性能，Linux下实现I/O多路复用的系统调用主要有select、 poll和epoll。
- select介绍：
  - 首先要构造一个关于文件描述符的列表，将要监听的文件描述符添加到该列表中。
  - 调用select，监听该列表中的文件描述符，直到这些描述符中的一个或者多个进行l/O操作时，该函数才返回。
    - 这个函数是阻塞
    - 函数对文件描述符的检测的操作是由内核完成的
  - 在返回时，它会告诉进程有多少(哪些）描述符要进行I/O操作。

```c++
// sizeof(fd_set) = 128 1024
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
int select(int nfds, fd_set *readfds, fd_set *writefds,
fd_set *exceptfds, struct timeval *timeout);
- 参数：
	- nfds : 委托内核检测的最大文件描述符的值 + 1
	- readfds : 要检测的文件描述符的读的集合，委托内核检测哪些文件描述符的读的属性
		- 一般检测读操作
		- 对应的是对方发送过来的数据，因为读是被动的接收数据，检测的就是读缓冲区
		- 是一个传入传出参数
	- writefds : 要检测的文件描述符的写的集合，委托内核检测哪些文件描述符的写的属性
		- 委托内核检测写缓冲区是不是还可以写数据（不满的就可以写）
	- exceptfds : 检测发生异常的文件描述符的集合
	- timeout : 设置的超时时间
struct timeval {
	long tv_sec; /* seconds */
	long tv_usec; /* microseconds */
};
	- NULL : 永久阻塞，直到检测到了文件描述符有变化
	- tv_sec = 0 tv_usec = 0， 不阻塞
	- tv_sec > 0 tv_usec > 0， 阻塞对应的时间
- 返回值 :
	- -1 : 失败
	- >0(n) : 检测的集合中有n个文件描述符发生了变化
// 将参数文件描述符fd对应的标志位设置为0
void FD_CLR(int fd, fd_set *set);
// 判断fd对应的标志位是0还是1， 返回值 ： fd对应的标志位的值，0，返回0， 1，返回1
int FD_ISSET(int fd, fd_set *set);
// 将参数文件描述符fd 对应的标志位，设置为1
void FD_SET(int fd, fd_set *set);
// fd_set一共有1024 bit, 全部初始化为0
void FD_ZERO(fd_set *set);
```

- select可监听的文件描述符个数默认为1024个，由fd_set标识,如下图所示：

![](https://github.com/XMULLT/IMG/raw/master/thread_img/文件描述符列表.jpg)

- 用select做多路复用器的服务端程序流程图：

![](https://github.com/XMULLT/IMG/raw/master/thread_img/select多路复用.jpg)

- 参考代码：

```c++
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

int main() {

    // 创建socket
    int lfd = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in saddr;
    saddr.sin_port = htons(9999);
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;

    // 绑定
    bind(lfd, (struct sockaddr *)&saddr, sizeof(saddr));

    // 监听
    listen(lfd, 8);

    // 创建一个fd_set的集合，存放的是需要检测的文件描述符
    fd_set rdset, tmp;
    FD_ZERO(&rdset);
    FD_SET(lfd, &rdset);
    int maxfd = lfd;

    while(1) {

        tmp = rdset;

        // 调用select系统函数，让内核帮检测哪些文件描述符有数据
        int ret = select(maxfd + 1, &tmp, NULL, NULL, NULL);
        if(ret == -1) {
            perror("select");
            exit(-1);
        } else if(ret == 0) {
            continue;
        } else if(ret > 0) {
            // 说明检测到了有文件描述符的对应的缓冲区的数据发生了改变
            if(FD_ISSET(lfd, &tmp)) {
                // 表示有新的客户端连接进来了
                struct sockaddr_in cliaddr;
                int len = sizeof(cliaddr);
                int cfd = accept(lfd, (struct sockaddr *)&cliaddr, &len);

                // 将新的文件描述符加入到集合中
                FD_SET(cfd, &rdset);

                // 更新最大的文件描述符
                maxfd = maxfd > cfd ? maxfd : cfd;
            }

            for(int i = lfd + 1; i <= maxfd; i++) {
                if(FD_ISSET(i, &tmp)) {
                    // 说明这个文件描述符对应的客户端发来了数据
                    char buf[1024] = {0};
                    int len = read(i, buf, sizeof(buf));
                    if(len == -1) {
                        perror("read");
                        exit(-1);
                    } else if(len == 0) {
                        printf("client closed...\n");
                        close(i);
                        FD_CLR(i, &rdset);
                    } else if(len > 0) {
                        printf("read buf = %s\n", buf);
                        write(i, buf, strlen(buf) + 1);
                    }
                }
            }

        }

    }
    close(lfd);
    return 0;
}
```

- select缺点：
  - 每次调用select，都需要把fd集合从用户态拷贝到内核态，这个开销在fd很多时会很大
  - 同时每次调用select都需要在内核遍历传递进来的所有fd，这个开销在fd很多时也很大
  - select支持的文件描述符数量太小了,默认是1024
  - fds集合不能重用，每次都需要重置

- poll介绍

```c++
#include <poll.h>
struct pollfd {
int fd; /* 委托内核检测的文件描述符 */
	short events; /* 委托内核检测文件描述符的什么事件 */
	short revents; /* 文件描述符实际发生的事件 */
};
struct pollfd myfd;
myfd.fd = 5;
myfd.events = POLLIN | POLLOUT; // 要监听的事件 POLLIN读事件 POLLOUT写事件
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
- 参数：
	- fds : 是一个struct pollfd 结构体数组，这是一个需要检测的文件描述符的集合
	- nfds : 这个是第一个参数数组中最后一个有效元素的下标 + 1
	- timeout : 阻塞时长
		0 : 不阻塞
		-1 : 阻塞，当检测到需要检测的文件描述符有变化，解除阻塞
		>0 : 阻塞的时长
- 返回值：
	-1 : 失败
	>0（n） : 成功,n表示检测到集合中有n个文件描述符发生变化
```

- poll没有监听文件描述符个数的限制，也可以复用，但是和select一样，需要对文件描述符遍历找出发生了监听事件的文件描述符。
- 用select做多路复用器的服务端程序参考代码

```c++
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>


int main() {

    // 创建socket
    int lfd = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in saddr;
    saddr.sin_port = htons(9999);
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;

    // 绑定
    bind(lfd, (struct sockaddr *)&saddr, sizeof(saddr));

    // 监听
    listen(lfd, 8);

    // 初始化检测的文件描述符数组
    struct pollfd fds[1024];
    for(int i = 0; i < 1024; i++) {
        fds[i].fd = -1;
        fds[i].events = POLLIN;
    }
    fds[0].fd = lfd;
    int nfds = 0;

    while(1) {

        // 调用poll系统函数，让内核帮检测哪些文件描述符有数据
        int ret = poll(fds, nfds + 1, -1);
        if(ret == -1) {
            perror("poll");
            exit(-1);
        } else if(ret == 0) {
            continue;
        } else if(ret > 0) {
            // 说明检测到了有文件描述符的对应的缓冲区的数据发生了改变
            if(fds[0].revents & POLLIN) {
                // 表示有新的客户端连接进来了
                struct sockaddr_in cliaddr;
                int len = sizeof(cliaddr);
                int cfd = accept(lfd, (struct sockaddr *)&cliaddr, &len);

                // 将新的文件描述符加入到集合中
                for(int i = 1; i < 1024; i++) {
                    if(fds[i].fd == -1) {
                        fds[i].fd = cfd;
                        fds[i].events = POLLIN;
                        break;
                    }
                }

                // 更新最大的文件描述符的索引
                nfds = nfds > cfd ? nfds : cfd;
            }

            for(int i = 1; i <= nfds; i++) {
                if(fds[i].revents & POLLIN) {
                    // 说明这个文件描述符对应的客户端发来了数据
                    char buf[1024] = {0};
                    int len = read(fds[i].fd, buf, sizeof(buf));
                    if(len == -1) {
                        perror("read");
                        exit(-1);
                    } else if(len == 0) {
                        printf("client closed...\n");
                        close(fds[i].fd);
                        fds[i].fd = -1;
                    } else if(len > 0) {
                        printf("read buf = %s\n", buf);
                        write(fds[i].fd, buf, strlen(buf) + 1);
                    }
                }
            }

        }

    }
    close(lfd);
    return 0;
}
```

- epoll介绍
- 创建一个新的epoll实例。在内核中创建了一个数据，这个数据中有两个比较重要的数据，一个是需要检测的文件描述符的信息（红黑树），还有一个是就绪列表，存放检测到数据发送改变的文件描述符信息（双向链表）。

```c++
#include <sys/epoll.h>
int epoll_create(int size);
- 参数：
	size : 目前没有意义了。随便写一个数，必须大于0
- 返回值：
	-1 : 失败
	> 0 : 文件描述符，操作epoll实例的
typedef union epoll_data {
	void *ptr;
	int fd;
	uint32_t u32;
	uint64_t u64;
} epoll_data_t;
struct epoll_event {
	uint32_t events; /* Epoll events */
	epoll_data_t data; /* User data variable */
};
常见的Epoll检测事件：
- EPOLLIN	// 读事件
- EPOLLOUT	// 写事件
- EPOLLERR	// 错误事件
// 对epoll实例进行管理：添加文件描述符信息，删除信息，修改信息
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
- 参数：
	- epfd : epoll实例对应的文件描述符
	- op : 要进行什么操作
		EPOLL_CTL_ADD: 添加
		EPOLL_CTL_MOD: 修改
		EPOLL_CTL_DEL: 删除
	- fd : 要检测的文件描述符
	- event : 检测文件描述符什么事情
// 检测函数
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int
timeout);
- 参数：
	- epfd : epoll实例对应的文件描述符
	- events : 传出参数，保存了发送了变化的文件描述符的信息
	- maxevents : 第二个参数结构体数组的大小
	- timeout : 阻塞时间
		- 0 : 不阻塞
		- -1 : 阻塞，直到检测到fd数据发生变化，解除阻塞
		- > 0 : 阻塞的时长（毫秒）
- 返回值：
	- 成功，返回发送变化的文件描述符的个数 > 0
	- 失败 -1
            
Epoll的工作模式: 
LT模式(水平触发)
LT是缺省的工作方式，并且同时支持block和no-block socket。在这种做法中，内核告诉你一个文件描述符是否就绪了，然后你可以对这个就绪的fd进行IO操作。如果你不作任何操作，内核还是会继续通知你的。
假设委托内核检测读事件->检测fd的读缓冲区读缓冲区有数据->epoll检测到了会给用户通知
	a.用户不读数据，数据一直在缓冲区，epoll会一直通知
    b.用户只读了一部分数据，epoll会通知
	c.缓冲区的数据读完了，不通知

ET模式(边沿触发)
ET是高速工作方式，只支持no-block socket。在这种模式下，当描述符从未就绪变为就绪时，内核通过epoll告诉你。然后它会假设你知道文件描述符已经就绪，并且不会再为那个文件描述符发送更多的就绪通知，直到你做了某些操作导致那个文件描述符不再为就绪状态了。但是请注意，如果一直不对这个fd作IO操作（从而导致它再次变成未就绪)，内核不会发送更多的通知（only once)。ET模式在很大程度上减少了epoll事件被重复触发的次数，因此效率要比LT模式高。epoll工作在ET模式的时候，必须使用非阻塞套接口，以避免由于一个文件句柄的阻塞读/阻塞写操作把处理多个文件描述符的任务饿死。
假设委托内核检测读事件>检测fd的读缓冲区读缓冲区有数据-> epoll检测到了会给用户通知
	a.用户不读数据，数据一致在缓冲区中，epoll下次检测的时候就不通知了
    b.用户只读了一部分数据，epoll不通知
	c.缓冲区的数据读完了，不通知
```

- epoll返回只有发生监听事件的文件描述符，用户态不需要做无意义的遍历，并且对监听的文件描述符采用红黑树存储，效率高。
- 用epoll做多路复用器的服务端程序流程图：

![](https://github.com/XMULLT/IMG/raw/master/thread_img/epoll多路复用.jpg)

- 参考程序：

```c++
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>

int main() {

    // 创建socket
    int lfd = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in saddr;
    saddr.sin_port = htons(9999);
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;

    // 绑定
    bind(lfd, (struct sockaddr *)&saddr, sizeof(saddr));

    // 监听
    listen(lfd, 8);

    // 调用epoll_create()创建一个epoll实例
    int epfd = epoll_create(100);

    // 将监听的文件描述符相关的检测信息添加到epoll实例中
    struct epoll_event epev;
    epev.events = EPOLLIN;
    epev.data.fd = lfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &epev);

    struct epoll_event epevs[1024];

    while(1) {

        int ret = epoll_wait(epfd, epevs, 1024, -1);
        if(ret == -1) {
            perror("epoll_wait");
            exit(-1);
        }

        printf("ret = %d\n", ret);

        for(int i = 0; i < ret; i++) {

            int curfd = epevs[i].data.fd;

            if(curfd == lfd) {
                // 监听的文件描述符有数据达到，有客户端连接
                struct sockaddr_in cliaddr;
                int len = sizeof(cliaddr);
                int cfd = accept(lfd, (struct sockaddr *)&cliaddr, &len);

                epev.events = EPOLLIN;
                epev.data.fd = cfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &epev);
            } else {
                if(epevs[i].events & EPOLLOUT) {
                    continue;
                }   
                // 有数据到达，需要通信
                char buf[1024] = {0};
                int len = read(curfd, buf, sizeof(buf));
                if(len == -1) {
                    perror("read");
                    exit(-1);
                } else if(len == 0) {
                    printf("client closed...\n");
                    epoll_ctl(epfd, EPOLL_CTL_DEL, curfd, NULL);
                    close(curfd);
                } else if(len > 0) {
                    printf("read buf = %s\n", buf);
                    write(curfd, buf, strlen(buf) + 1);
                }

            }

        }
    }

    close(lfd);
    close(epfd);
    return 0;
}
```

