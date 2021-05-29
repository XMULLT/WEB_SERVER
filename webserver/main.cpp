#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>

#include "locker.h"
#include "threadpool.cpp"
#include "http_conn.h"
#include "lst_timer.h"

const int MAX_FD = 65535; // 最大的文件描述符个数
const int MAX_EVENT_NUMBER = 1000;
const int TIMESLOT = 5;

static int pipefd[2];
static sort_timer_lst timer_lst;


void sig_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

void timer_handler()
{
    // 定时处理任务，实际上就是调用tick()函数
    timer_lst.tick();
    // 因为一次 alarm 调用只会引起一次SIGALARM 信号，所以我们要重新定时，以不断触发 SIGALARM信号。
    alarm(TIMESLOT);
}

// 定时器回调函数，它删除非活动连接socket上的注册事件，并关闭之。
void cb_func(http_conn* user_data)
{
    epoll_ctl(user_data->m_epollfd, EPOLL_CTL_DEL, user_data->m_sockfd, 0);
    assert( user_data );
    close(user_data->m_sockfd);
    printf("close fd %d\n", user_data->m_sockfd);
}

// 添加信号捕捉
void addsig(int sig, void(*handler)(int)) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, nullptr);
}

void addsig(int sig) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset( &sa.sa_mask );
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 设置非阻塞
extern void setnonblocking(int fd);

// 添加文件描述符到epoll中
extern void addfd(int epollfd, int fd, bool one_shot);

// 从epoll中删除文件描述符
extern void removefd(int epollfd, int fd);

// 修改epoll中的文件描述符
extern void modfd(int epollfd, int fd, int ev);

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        printf("请按照如下格式运行: %s 端口号\n", basename(argv[0]));
        exit(-1);
    }

    // 获取端口号
    int port = atoi(argv[1]);

    // 对SIGPIPE信号进行处理
    addsig(SIGPIPE, SIG_IGN);

    // 创建线程池 初始化线程池
    threadpool<http_conn>* pool = nullptr;
    try {
        pool = new threadpool<http_conn>;
    } catch(...) {
        exit(-1);
    }

    // 创建一个数组用于保存所有的客户端信息
    http_conn* users = new http_conn[MAX_FD];

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);

    // 设置端口复用
    int reuse = 1; 
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 绑定
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    bind(listenfd, (struct sockaddr*)&address, sizeof(address));

    // 监听
    listen(listenfd, 5);
    
    // 创建epoll对象，事件的数组，添加
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);

    // 从监听的文件描述符添加到epoll对象中
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    // 创建管道
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0], false);

    // 设置信号处理函数
    addsig(SIGALRM);

    bool timeout = false;
    alarm(TIMESLOT);

    while (true) {
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (num < 0 && errno != EINTR) {
            printf("epoll 调用失败\n");
            break;
        }
        // printf("%d\n", num);
        // 循环遍历事件数组
        for (int i = 0; i < num; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlen);

                if (http_conn::m_user_count > MAX_FD) {
                    // 目前连接数满了  给客户端写一个信息，服务器内部正忙
                    close(connfd);
                    continue;
                }

                // 将新的客户的数据初始化 放到数组中
                users[connfd].init(connfd, client_address);

                util_timer* timer = new util_timer;
                timer->user_data = &users[connfd];
                timer->cb_func = cb_func;
                time_t cur = time(NULL);
                timer->expire = cur + 3 * TIMESLOT;
                users[connfd].timer = timer;
                timer_lst.add_timer(timer);
            } else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)) {
                // printf("in\n");
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1 || ret == 0) continue;
                else {
                    for (int i = 0; i < ret; ++i) {
                        if (signals[i] == SIGALRM) {
                            timeout = true;
                            break;
                        }
                    }
                }
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP |EPOLLERR)) {
                // 对方异常断开或者其它错误事件
                timer_lst.del_timer(users[sockfd].timer);
                users[sockfd].close_conn();
            } else if (events[i].events & EPOLLIN) {
                if (users[sockfd].read()) {
                    // 一次性读完数据
                    if (users[sockfd].timer) {
                        time_t cur = time(NULL);
                        users[sockfd].timer->expire = cur + 3 * TIMESLOT;
                        timer_lst.adjust_timer(users[sockfd].timer);
                    }
                    pool->append(users + sockfd);
                } else {
                    // 对方关闭连接，服务器关闭连接
                    timer_lst.del_timer(users[sockfd].timer);
                    users[sockfd].close_conn();
                } 
            } else if (events[i].events & EPOLLOUT) {
                if (!users[sockfd].write()) {
                    timer_lst.del_timer(users[sockfd].timer);
                    users[sockfd].close_conn();
                } else {
                    if (users[sockfd].timer) {
                        time_t cur = time(NULL);
                        users[sockfd].timer->expire = cur + 3 * TIMESLOT;
                        timer_lst.adjust_timer(users[sockfd].timer);
                    }
                }
            }
        }
        if (timeout) {
            timer_handler();
            timeout = false;
        }
    }
    close(epollfd);
    close(listenfd);
    close (pipefd[0]);
    close (pipefd[1]);
    delete[] users;
    delete pool;
    return 0;
}
