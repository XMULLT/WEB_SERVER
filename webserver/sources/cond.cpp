#include "cond.h"

cond::cond() {
    if (pthread_cond_init(&m_cond, nullptr) != 0) {
        throw std::exception();
    }
}

cond::~cond() {
    pthread_cond_destroy(&m_cond);
}

bool cond::wait(pthread_mutex_t* mutex) {
    return pthread_cond_wait(&m_cond, mutex) == 0;
} 

bool cond::timedwait(pthread_mutex_t* mutex, struct timespec t) {
    return pthread_cond_timedwait(&m_cond, mutex, &t) == 0;
} 

bool cond::signal(pthread_mutex_t* mutex) {
    return pthread_cond_signal(&m_cond) == 0;
} 

bool cond::broadcast() {
    return pthread_cond_broadcast(&m_cond) == 0;
}