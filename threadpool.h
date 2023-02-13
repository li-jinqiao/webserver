#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include "locker.h"

template <typename T>
class threadpool // 线程池类
{
public:
    threadpool(int thread_number = 8, int max_requests = 10000) : m_threads(nullptr)
    {
        if ((thread_number <= 0) || (max_requests <= 0)) // 检查参数正确性
        {
            throw exception();
        }
        m_thread_number = thread_number;            // 设置预分配线程数量
        m_max_requests = max_requests;              // 设置请求数量上限
        m_threads = new pthread_t[m_thread_number]; // 堆区开辟线程池数组
        if (!m_threads)
        {
            throw exception();
        }
        for (int i = 0; i < thread_number; ++i) // 创建多线程并将他们设置为脱离线程
        {
            if (pthread_create(m_threads + i, NULL, worker, this) != 0) // 创建线程失败
            {
                delete[] m_threads;
                throw exception();
            }
            if (pthread_detach(m_threads[i]) != 0) // 线程分离失败
            {
                delete[] m_threads;
                throw exception();
            }
        }
        m_stop = false; // 设置线程处于运行状态
    }

    ~threadpool() // 线程池析构函数
    {
        delete[] m_threads; // 回收堆区线程数组
        m_stop = true;      // 线程停止运行
    }

    bool append(T *request) // 向请求队列添加任务
    {
        m_queuelocker.lock();                     // 操作请求队列时一定要加锁因为它被所有线程共享
        if (m_workqueue.size() >= m_max_requests) // 请求队列最大请求数已满无法加入新请求
        {
            m_queuelocker.unlock();
            return false;
        }
        m_workqueue.push_back(request); // 加入新的请求
        m_queuelocker.unlock();         // 给请求队列解锁
        m_queuestat.post();             // 信号量数量增加则请求可被子线程处理
        return true;
    }

private:
    static void *worker(void *arg) // 工作线程的回调函数从工作队列中取出任务并执行
    {
        threadpool *pool = (threadpool *)arg; // 获取线程池类的对象
        pool->run();                          // 这才是线程的真正运行方法
        return NULL;                          // 返回什么应该没关系
    }

    void run() // 为什么不直接在worker里面进行线程运行工作
    {
        while (!m_stop) // 检查线程是否停止工作
        {
            m_queuestat.wait();   // 检查是否有可提取的请求
            m_queuelocker.lock(); // 给工作队列上锁
            if (m_workqueue.empty())
            {
                m_queuelocker.unlock();
                continue;
            }
            T *request = m_workqueue.front(); // 获取并移除最先装入的任务对象
            m_workqueue.pop_front();
            m_queuelocker.unlock(); // 给工作队列解锁
            if (!request)
            {
                continue;
            }
            request->process(); // 任务执行进入任务类的成员方法
        }
    }

private:
    int m_thread_number;   // 线程的数量
    int m_max_requests;    // 请求队列中最多允许的等待处理的请求的数量
    pthread_t *m_threads;  // 指向堆区线程数组的指针
    list<T *> m_workqueue; // 请求队列
    locker m_queuelocker;  // 保护请求队列的互斥锁
    sem m_queuestat;       // 请求队列中任务的信号量
    bool m_stop;           // 是否结束线程
};

#endif