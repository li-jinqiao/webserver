#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>
using namespace std;

class locker // 互斥锁类
{
public:
    locker()
    {
        if (pthread_mutex_init(&m_mutex, NULL) != 0) // 初始化互斥锁
        {
            throw exception();
        }
    }
    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    pthread_mutex_t *get()
    {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};

class cond // 条件变量类
{
public:
    cond()
    {
        if (pthread_cond_init(&m_cond, NULL) != 0)
        {
            throw exception();
        }
    }
    ~cond()
    {
        pthread_cond_destroy(&m_cond);
    }
    bool wait(pthread_mutex_t *m_mutex)
    {
        int ret = 0;
        ret = pthread_cond_wait(&m_cond, m_mutex);
        return ret == 0;
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t)
    {
        int ret = 0;
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        return ret == 0;
    }
    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0;
    }
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    pthread_cond_t m_cond;
};

class sem // 信号量类
{
public:
    sem(int num = 0) // 初始化信号量(提供默认参数)
    {
        if (sem_init(&m_sem, 0, num) != 0)
        {
            throw exception();
        }
    }
    ~sem() // 销毁信号量
    {
        sem_destroy(&m_sem);
    }
    bool wait() // 减少信号量数
    {
        return sem_wait(&m_sem) == 0;
    }
    bool post() // 增加信号量数
    {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};

#endif