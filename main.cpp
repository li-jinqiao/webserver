#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

#define MAX_FD 65536           // 最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000 // 监听的最大的事件数量

extern void addfd(int epollfd, int fd, bool one_shot); // 添加文件描述符到epoll实例中
extern void removefd(int epollfd, int fd);             // 从epoll实例中删除文件描述符

void addsig(int sig, void(handler)(int)) // 注册信号捕捉
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

int main(int argc, char *argv[])
{
    if (argc <= 1) // 参数检查
    {
        printf("%s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]); // 获取用户输入的服务器端口号

    addsig(SIGPIPE, SIG_IGN); // 将SIGPIPE信号设置为忽略处理

    threadpool<http_conn> *pool = nullptr;
    try
    {
        pool = new threadpool<http_conn>; // 创建线程池对象
    }
    catch (...)
    {
        return 1;
    }

    http_conn *users = new http_conn[MAX_FD]; // 创建任务对象数组

    int listenfd = socket(PF_INET, SOCK_STREAM, 0); // 创建监听套接字

    int ret = 0;

    //  给定服务端地址信息
    struct sockaddr_in address;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    //  设置重用关闭后的socket文件描述符
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address)); // 绑定
    ret = listen(listenfd, 5);                                          // 监听

    epoll_event events[MAX_EVENT_NUMBER]; // 创建事件数组
    int epollfd = epoll_create(5);        // 创建epoll对象
    addfd(epollfd, listenfd, false);      // 将监听文件描述符添加到epoll对象中
    http_conn::m_epollfd = epollfd;       // 确定epoll文件描述符

    while (true) // 服务器循环运行
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1); // 获取检测到的有变化文件描述符的数量

        if (number < 0 && errno != EINTR)
        {
            printf("epoll failure\n");
            break;
        }

        for (int i = 0; i < number; i++) // 遍历获取有数据到来的文件描述符
        {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) // 如果是监听文件描述符的数据代表有新客户端连接
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength); // 连接
                if (connfd < 0)
                {
                    printf("errno is: %d\n", errno);
                    continue;
                }
                if (http_conn::m_user_count >= MAX_FD)
                {
                    close(connfd);
                    continue;
                }
                users[connfd].init(connfd, client_address); // 分配并初始化一个任务类
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                users[sockfd].close_conn();
            }
            else if (events[i].events & EPOLLIN)
            {
                if (users[sockfd].read())
                {
                    pool->append(users + sockfd);
                }
                else
                {
                    users[sockfd].close_conn();
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                if (!users[sockfd].write())
                {
                    users[sockfd].close_conn();
                }
            }
        }
    }

    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;

    return 0;
}