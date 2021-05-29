#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <exception>

// 线程同步机制封装类

// 条件变量类
class cond {
public:
    cond();
    ~cond();
    
    bool wait(pthread_mutex_t* mutex);
    bool timedwait(pthread_mutex_t* mutex, struct timespec t);
    bool signal(pthread_mutex_t* mutex);
    bool broadcast();
private:
    pthread_cond_t m_cond;
};

#endif