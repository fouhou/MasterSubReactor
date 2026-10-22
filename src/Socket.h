#pragma once
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include "Log.h"

/*
 * Socket.h —— socket fd 的 RAII 封装，析构自动 close
 * 所有 fd 均工作在非阻塞模式：事件驱动下任何阻塞都会卡住整个线程。
 * Recv/Send 返回值约定：>0 收发 n 字节；0 = EAGAIN/EINTR 暂无数据，等下一轮；
 * -1 = 真错误或对端关闭（对端关闭的 0 归一成 -1，上层统一走错误分支）。
 */

#define DEFAULT_SOCKET
#define MAX_BACKLOG 1024        // listen 全连接队列长度
using namespace LogModule;

class Socket
{
public:
    Socket();
    Socket(int sockfd);         // 包装已存在的 fd（如 accept 返回的连接 fd）
    ~Socket();                  // RAII：析构自动 Close
    int Fd();
    bool Create();
    bool Bind(uint16_t port, const std::string &ip);
    bool Listen(int backlog = MAX_BACKLOG);
    ssize_t Recv(void *buf, int len, int flag = 0);
    ssize_t NonBlockRecv(void *buf, int len);
    ssize_t Send(const void* buf, int len, int flag = 0);
    ssize_t NonBlockSend(const void* buf, int len);
    int Accept();
    bool Connect(uint16_t port, const std::string &ip);
    void Close();
    bool SetNonBlock();
    bool SetReuseAddr();
    // 服务器/客户端一键初始化
    bool CreateServer(uint16_t port, const std::string &ip = "0.0.0.0");
    bool CreateClient(uint16_t port, const std::string &ip);
private:
    int _sockfd;
};
