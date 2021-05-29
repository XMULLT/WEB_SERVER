#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <list>
#include "locker.h"
#include "sem.h"
#include <exception>
#include <cstdio>

// 线程池类,定义成模板类
template<typename T>
class threadpool {
public:
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    
    bool append(T* request); // 添加任务

private:
    static void* worker(void* arg);
    void run();
private:
    // 线程的数量
    int m_thread_number;

    // 线程池数组
    pthread_t* m_threads;

    // 请求队列中最多允许的等待处理的请求数量
    int m_max_requests;

    // 请求队列
    std::list<T*> m_workqueue;

    // 互斥锁
    locker m_queuelocker;

    // 信号量  用来判断是否有任务需要处理
    sem m_queuestat;

    // 是否结束线程
    bool m_stop;
};




#endif