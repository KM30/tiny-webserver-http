#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include "locker.h"

/* 线程池类，将它定义为模板类是为了代码复用，模板参数T是任务类 */
template<typename T>
class threadpool{
private:
    // 1_线程的数量
    int m_thread_number;
    // 1_描述线程池的数组，存线程ID，大小为m_thread_number
    pthread_t* m_threads;
    // 2_请求队列中最多允许的、等待处理的请求的数量
    int m_max_requests;
    // 2_请求队列 T* data
    std::list<T*> m_workqueue;
    // 3_保护请求队列的互斥锁
    locker m_queuelocker;
    // 3_是否有任务需要处理
    sem m_queuestat;
    // 4_是否结束线程
    bool m_stop;

public:
    // 声明初始化构造线程池和析构线程池函数
    threadpool(int thread_number = 6, int max_requests = 1000);
    ~threadpool();
    // 声明将任务添加入工作队列函数
    bool append(T* request);
private:
    // 声明工作线程运行的函数不断从工作队列中取出任务并执行之
    static void* work(void* arg);
    void run();
};

template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests)
    : m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL), m_stop(false){
    // 鲁棒性检查
    if((thread_number <= 0) || (max_requests <= 0)){
        throw std::exception();
    }
    // 创建线程池，线程ID被写到该变量中
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads){
        throw std::exception();
    }
    // 创建thread_number个线程，并将他们设置为线程脱离
    for(int i = 0; i < thread_number; ++i){
        printf("create the %dth thread\n", i);
        if(pthread_create(m_threads + i, NULL, work, this) != 0){
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])){
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool(){
    delete[] m_threads;
    m_stop = true;
}

template<typename T>
bool threadpool<T>::append(T* request){
    // 操作工作队列时一定要加锁，因为它被所有线程共享
    m_queuelocker.lock();
    if(m_workqueue.size() > m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuestat.post();
    m_queuelocker.unlock();
    return true;
}

template<typename T>
void* threadpool<T>::work(void* arg){
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run(){
    while(!m_stop){
        m_queuestat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty()){
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request){
            continue;
        }
        request->process();
    }
}

#endif


/* 
    1-同一个文件include多次
      -#program once

    2-include <list>
      -双向链表
      -标准模板库

    3-static function
      -pthread_create(m_threads + i, NULL, work, this)
      -静态函数没有this指针，只能访问静态的成员函数和成员变量
      -要想访问某对象的类内其它成员就要传入该对象的this指针
*/