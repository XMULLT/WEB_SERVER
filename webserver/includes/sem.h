#ifndef SEM_H
#define SEM_H

#include <pthread.h>
#include <exception>
#include <semaphore.h>

// 线程同步机制封装类

// 信号量类
class sem {
public:
    sem();
    sem(int num);

    ~sem();
    bool wait();
    bool post();
private:
    sem_t m_sem;
};

#endif