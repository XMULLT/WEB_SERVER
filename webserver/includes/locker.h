#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <exception>
// 线程同步机制封装类

// 互斥锁类
class locker {
public:
    locker();
    ~locker();

    bool lock();
    bool unlock();
    pthread_mutex_t* get();
private:
    pthread_mutex_t m_mutex;
};

#endif