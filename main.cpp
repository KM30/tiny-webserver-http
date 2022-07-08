#include <stdio.h>
#include <libgen.h>

#include <signal.h>
#include <string.h>
#include <assert.h>

#include "threadpool.h"
#include "http_conn.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>

#include <sys/epoll.h>

#include <errno.h>
#include <unistd.h>

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 1000

void addsig(int sig,void(handler)(int)){
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig,&sa,NULL) != -1);
}

extern void addfd(int epollfd,int fd,bool one_shot);
extern void removefd(int epollfd,int fd);
extern void modfd(int epollfd,int fd,int ev);

int main(int argc,char* argv[]){
// 1-鲁棒性检测
    // 检查端口号:命令行的命令参数
    if(argc <= 1){
        // basename返回最后一个路径分隔符之后的内容[可执行文件]
        printf("usage: %s port_number\n",basename(argv[0]));
        return 1;
    }
    // 向没有读端的管道写数据:SIGPIPE,SIG_IGN:忽略该信号
    addsig(SIGPIPE,SIG_IGN);

// 2-创建线程池(线程数组6个+工作链表) + 创建客户数组
    // pthread_t* m_threads = new pthread_t[m_thread_number];
    // std::list<http_conn*> m_workqueue
    // append(T* request) + work(run(request->process)
    threadpool<http_conn>* pool = NULL;
    try{
        pool = new threadpool<http_conn>();
    }catch(...){
        return 1;
    }
    // 创建数组用于保存所有客户端信息,下标[文件描述符]
    http_conn* users = new http_conn[MAX_FD];

// 3-网络通信socket
    // 创建套接字
    int listenfd = socket(PF_INET,SOCK_STREAM,0);
    // 端口复用
    int reuse = 1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    // 绑定套接字：获取端口号+封装socket地址
    struct sockaddr_in address;
    int port = atoi(argv[1]);
    address.sin_port = htons(port);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    bind(listenfd,(struct sockaddr*)&address,sizeof(address));
    // 监听套接字    
    listen(listenfd,5);
 
// 4-I/O多路复用
    // 事件数组,函数epoll_wait()传出参数
    epoll_event events[MAX_EVENT_NUMBER];
    // 创建epoll对象:红黑树+双向链表
    int epollfd = epoll_create(5);
    // 添加要监听的文件描述符
    addfd(epollfd,listenfd,false);
    // 所有socket上的事件都被注册到同一个epoll内核事件中，所以设置成静态的
    // static int m_epollfd;
    http_conn::m_epollfd = epollfd;

// 5-循环接收客户端的连接请求和交换通信数据
    while(true){
        // 5-1获取发生了改变的文件描述符或事件
        int number_fd = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number_fd < 0) && (errno != EINTR)){
            printf("epoll failure\n");
            break;
        }

        // 5-2遍历发生改变的文件描述符：连接请求 or 数据通信
        for(int i = 0;i < number_fd;++i){
            int sockfd = events[i].data.fd;

            // 5-2-1有新的客户端：连接请求
            if(sockfd == listenfd){
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd,(struct sockaddr*)&client_address,&client_addrlength);
                if(connfd < 0){
                    printf("errno is: %d\n",errno);
                    continue;
                } 
                if(http_conn::m_user_count >= MAX_FD){
                    // 目前连接数已经满了
                    // 给客户端写一条信息：服务器正忙
                    close(connfd);
                    continue;
                }
                // 为新客户端分配内存http_conn,并将其信息存入其中
                users[connfd].init(connfd,client_address);
            }

            // 5-2-2该文件描述符或通信连接发生错误事件
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                users[sockfd].close_conn();
            }

            // 5-2-3该文件描述符可读：数据通信
            else if(events[i].events & EPOLLIN){
                // 一次性把数据读完
                if(users[sockfd].read()){
                    // 加入工作队列中，等待子线程处理
                    pool->append(users + sockfd);
                }else{
                    users[sockfd].close_conn();
                }
            }

            // 5-2-4该文件描述符可写：数据通信
            else if(events[i].events & EPOLLOUT){
                if(!users[sockfd].write()){
                    users[sockfd].close_conn();
                }
            }
        }
    }

// 6-通信结束，关闭文件描述符，释放动态内存空间，结束该进程
    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    return 0;
}


/* 
    项目难点:
    1- 如何唤醒子线程 
        -pthread_create(m_threads + i, NULL, work, this)
        -work{threadpool* pool->run();}
        -run{http_conn* request = m_workqueue.front(); request->process();}
    2- I/O多路复用
        -EPOLLONESHOT
        -ET边缘触发

    知识点总结:
    1-(int argc, char* argv[])
        - 命令行的参数个数和参名
        - include <libgen.h>
        - basename(argv[0])
    2-void addsig(int sig,void(handler)(int))
        - #include <signal.h> 信号
        - int sigfillset(sigset_t *set)
            - 功能：将信号集中的所有的标志位置为1
            - 参数：set 传出参数，需要操作的信号集
            - 返回值：成功返回0， 失败返回-1
        - int sigaction(int signum,const struct sigaction *act,
                        struct sigaction *oldact);
            - 功能：检查|改变信号的处理(信号捕捉)
            - 参数：
                - signum : 需要捕捉的信号的编号或者宏值（信号的名称）
                - act ：捕捉到信号之后的处理动作
                - oldact : 上一次对信号捕捉相关的设置，一般不使用，传递NULL
            - 返回值：
                成功 0
                失败 -1
        - #include <assert.h> 断言
        - assert()
        - #include <string.h>
        - memset()
    3-threadpool<http_conn>* pool = new threadpool<http_conn>();
      http_conn* users = new http_conn[MAX_FD];
        - pool：内存为8字节，存的地址
        - new threadpool<http_conn>()
            内存空间为：
            @ m_thread_number + m_max_requests
                = 4 + 4
            @ *m_threads + *m_workqueue
                = 8 + 8
            @ m_mutex + m_sem
                = 8 + 8 union(long int)
            int m_thread_number;
            pthread_t* m_threads;
                = new pthread_t[m_thread_number];
            int m_max_requests;
            std::list<http_conn*> m_workqueue;
                list_node* node;头节点
                list_node{
                    pointer prev;
                    pointer next;
                    http_conn* data;
                }
            locker m_queuelocker;
                pthread_mutex_t m_mutex;  // 互斥锁
            sem m_queuestat;
                sem_t m_sem;  // 信号量
            bool m_stop;
        - #include <exception>
            - try{}catch{}
            - throw std::exception();
    4-I/O多了复用
        struct epoll_event{
            uint32_t events; //Epoll events
            epoll_data_t data; //User data variable
        };
        typedef union epoll_data{
            void *ptr;
            int fd;
            uint32_t u32;
            uint64_t u64;
        } epoll_data_t;
    5-extern void addfd(int epollfd,int fd,bool one_shot)
        - extern
*/