#include "sem.h"

sem::sem() {
    if (sem_init(&m_sem, 0, 0) != 0) {
        throw std::exception();
    }
}

sem::sem(int num) {
    if (sem_init(&m_sem, 0, num) != 0) {
        throw std::exception();
    }
}

sem::~sem() {
    sem_destroy(&m_sem);
}

bool sem::wait() {
    return sem_wait(&m_sem) == 0;
}

bool sem::post() {
    return sem_post(&m_sem) == 0;
}
    