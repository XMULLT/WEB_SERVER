#include "locker.h"

locker::locker() {
    if (pthread_mutex_init(&m_mutex, nullptr) != 0) {
        throw std::exception();
    }
}

locker::~locker() {
    pthread_mutex_destroy(&m_mutex);
}

bool locker::lock() {
    return pthread_mutex_lock(&m_mutex) == 0;
}

bool locker::unlock() {
    return pthread_mutex_unlock(&m_mutex) == 0;
}

pthread_mutex_t* locker::get() {
    return &m_mutex;
}