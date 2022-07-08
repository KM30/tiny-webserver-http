#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <exception>
#include <semaphore.h>

/* 
    线程同步机制：类锁封装
    locker obj;  -- pthread_mutex_*
        -obj.lock()
        -obj.unlock()
        -obj.get()
            
    cond obj;    -- pthread_cond_*
        -obj.wait(pthread_mutex_t* m_mutex)
        -obj.signal()
            
    sem obj;     -- sem_*
        -obj.wait()
        -obj.post()
*/

// 1-互斥锁类 : pthread_mutex_* //
class locker{
private:
    pthread_mutex_t m_mutex;// 互斥量
public:
    locker(){
        if(pthread_mutex_init(&m_mutex, NULL) != 0){
            throw std::exception();
        }
    }
    ~locker(){
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock(){
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlock(){
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    pthread_mutex_t* get(){
        return &m_mutex;
    }
};

// 2-条件变量类: pthread_cond_* //
class cond{
private:
    pthread_cond_t m_cond;// 条件量
public:
    cond(){
        if(pthread_cond_init(&m_cond, NULL) != 0){
            throw std::exception();
        }
    }
    ~cond(){
        pthread_cond_destroy(&m_cond);
    }
    bool wait(pthread_mutex_t* m_mutex){
        // 等待要互斥量的目的：1、阻塞时解锁。2、继续执行时有枷锁
        return pthread_cond_wait(&m_cond, m_mutex) == 0;
    }
    bool timewait(pthread_mutex_t* m_mutex, timespec t){
        // 等待要互斥量的目的：1、阻塞时解锁。2、继续执行时有枷锁
        return pthread_cond_timedwait(&m_cond, m_mutex, &t) == 0;
    }
    bool signal(){
        return pthread_cond_signal(&m_cond) == 0;
    }
    bool broadcast(){
        return pthread_cond_broadcast(&m_cond) == 0;
    }
};

// 3-信号量类: sem_* //
class sem{
private:
    sem_t m_sem;// 信号量
public:
    sem(){
        if(sem_init(&m_sem, 0 , 0) != 0){
            throw std::exception();
        }
    }
    sem(int num){
        if(sem_init(&m_sem, 0, num) !=0){
            throw std::exception();
        }
    }
    ~sem(){
        sem_destroy(&m_sem);
    }
    // 等待信号量
    bool wait(){
        // wait调用一次对信号量的值-1，如果值为0，就阻塞
        return sem_wait(&m_sem) == 0;
    }
    // 增加信号量
    bool post(){
        // post调用一次对信号量的值+1
        return sem_post(&m_sem) == 0;
    }
};

#endif


/*
    1-头文件编写规范实例
      #ifndef GRAPHICS_H      //作用：防止graphics.h被重复引用
      #define GRAPHICS_H      //作用：防止头文件的重复包含和编译
      #include<....>          //引用标准库的头文件
      #include"..."           //引用非标准库的头文件
      void Function1(...);    //全局函数声明
      inline();               //inline函数的定义
      class Box{};            //作用：类结构声明
      #endif

    2-类内私有成员变量
      -在类外可以通过类内共有的成员函数来间接访问类内定义和声明的私有成员变量；

    3-内联函数inline
      -在类内定义和实现的函数为内联函数；
*/