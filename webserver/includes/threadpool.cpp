#include "threadpool.h"

template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) :
    m_thread_number(thread_number), m_max_requests(max_requests),
    m_stop(false), m_threads(nullptr) {
    if ((thread_number <= 0) || (max_requests <= 0)) {
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number];
    if (!m_threads) {
        throw std::exception();
    }

    for (int i = 0; i < m_thread_number; ++i) {
        printf("create the %dth thread\n", i);
        if (pthread_create(m_threads + i, nullptr, worker, this) != 0) {
            delete[] m_threads;
            throw std::exception();

            if (pthread_detach(m_threads[i]) != 0) {
                delete[] m_threads;
                throw std::exception();
            }
        }

    }
}

template<typename T>
threadpool<T>::~threadpool() {
    delete[] m_threads;
    m_stop = true;
}

template<typename T>
bool threadpool<T>::append(T* request) {
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.emplace_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
void* threadpool<T>::worker(void* arg) {
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run() {
    while (!m_stop) {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }

        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if (!request) {
            continue;
        }
        request->process();
    }
}