#include <sys/epoll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>
#include <signal.h>
#include <memory.h>
#include <time.h>
using namespace std;

#define PROT 80 // 端口号

int soc;               // 套接字
int epfd;              // epoll描述符
epoll_event ev;        // epoll结构
epoll_event event[10]; // epoll结构数组

// 信号中断处理函数
void sig(int sig)
{
    cout << "服务器关闭! " << endl;
    close(soc);
    close(epfd);
    exit(0);
}

void do_service_end(int);

// 处理客户端新加入
void do_service_begin(int socket_in)
{
    sockaddr conn;
    socklen_t size = sizeof(conn);
    if (socket_in < 0)
    {
        cerr << "Accept error" << endl;
        return;
    }
    if (getpeername(socket_in, &conn, &size) < 0) // 获取客户端信息
    {
        cerr << "getpeername error" << endl;
        do_service_end(socket_in);
        return;
    }
    ev.data.fd = socket_in;                                 // 设置结构
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, socket_in, &ev) < 0) // 将新套接字添加到epoll红黑树中
    {
        cerr << "Client epoll ctrl error" << endl;
        do_service_end(socket_in);
    }
    sockaddr_in *s_in = (sockaddr_in *)&conn;
    char ip[INET_ADDRSTRLEN];
    cout << "================================" << endl;
    cout << "已连接! IP类型: " << ((s_in->sin_family == AF_INET) ? "IPV4" : "IPV6") << endl;
    auto ipaddr = inet_ntop(s_in->sin_family, &s_in->sin_addr, ip, INET_ADDRSTRLEN); // 网络字节序转本机字节序
    if (ipaddr)
        cout << "IP地址: " << ipaddr;
    cout << "  端口: " << ntohs(s_in->sin_port) << endl;
    cout << "================================" << endl;
}

//处理客户端断开
void do_service_end(int socket_in)
{
    sockaddr conn;
    socklen_t size = sizeof(conn);
    if (getpeername(socket_in, &conn, &size) < 0) // 获取客户端信息
        cerr << "getpeername error" << endl;

    sockaddr_in *s_in = (sockaddr_in *)&conn;
    char ip[INET_ADDRSTRLEN];
    cout << "============================================" << endl;
    auto ipaddr = inet_ntop(s_in->sin_family, &s_in->sin_addr, ip, INET_ADDRSTRLEN); // 字节序转换
    if (ipaddr)
        cout << "IP地址: " << ipaddr;
    cout << " 已断开连接! ";
    cout << "  端口: " << ntohs(s_in->sin_port) << endl;
    cout << "============================================" << endl;

    if (epoll_ctl(epfd, EPOLL_CTL_DEL, socket_in, nullptr) < 0) // 删除epoll红黑树节点
        cerr << "Del epoll error" << endl;
    close(socket_in); // 关闭对应套接字描述符
}

// 与客户端进行IO处理
void do_service_IO(int socket_in)
{
    char buf[512];

    memset(buf, 0, 512);
   /* if (read(socket_in, buf, 512) <= 0) // 读取客户端输入内容
    {
        do_service_end(socket_in);
        return;
    }
   */

    cout << "客户端输入 >" << endl;
    if (write(STDIN_FILENO, buf, 512) <= 0) // 在标准输出上显示读取的内容
    {
        do_service_end(socket_in);
        return;
    }
    
    time_t time_p = time(nullptr);
    char *time = ctime(&time_p);
    if (write(socket_in, time, 25) <= 0) // 将时间回写到客户端中
    {
        do_service_end(socket_in);
        return;
    }
    cout << "服务器已回写! " << endl;
}

int main(void)
{

    if ((soc = socket(AF_INET, SOCK_STREAM, 0)) < 0) // 创建套接字
    {
        cerr << "Socket error" << endl;
        exit(1);
    }
    signal(SIGINT, sig); // 注册信号处理函数

    sockaddr_in s_in;
    s_in.sin_family = AF_INET;
    s_in.sin_port = htons(PROT);
    s_in.sin_addr.s_addr = INADDR_ANY; // 定义结构体使用IPV4

    if (bind(soc, (sockaddr *)&s_in, sizeof(s_in)) < 0) // 绑定到套接字对应地址
    {
        cerr << "Bind error" << endl;
        close(soc);
        exit(1);
    }

    if (listen(soc, 10) < 0) // 启动端口监听
    {
        cerr << "Listen error" << endl;
        close(soc);
        exit(1);
    }

    ev.data.fd = soc;
    ev.events = EPOLLIN | EPOLLET;    // 设置事件为读
    epfd = epoll_create(1); // 创建epoll描述符
    if (epfd < 0)
    {
        cerr << "Epoll error" << endl;
        close(soc);
        exit(1);
    }
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, soc, &ev) < 0) // 将服务器自己的套接字加入到epoll中
    {
        cerr << "Epoll ctrl error" << endl;
        close(soc);
        close(epfd);
        exit(1);
    }
    cout << "服务器启动! " << endl;
    while (1)
    {
        int conn = epoll_wait(epfd, event, 10, -1); // 阻塞重排套接字准备
        for (int i = 0; i < conn; ++i)
        {
            if (soc == event[i].data.fd)                   // 服务器套接字收到新连接
                do_service_begin(accept(soc, nullptr, 0)); // 处理新连接
            else if (event[i].events == EPOLLIN)           // 处理已连接的客户端
                do_service_IO(event[i].data.fd);
        }
    }
    exit(0);
}
