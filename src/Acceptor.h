/* Acceptor.h —— 监听套接字的管理者，负责 accept 新连接。 */
#pragma once
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

/* Acceptor：封装监听 socket，挂到主 Reactor 上接收新连接。
 * 构造时建监听 socket，Listen() 时才挂 EPOLLIN 开始 accept。 */

class Acceptor
{
private:
    using AcceptCallBack = std::function<void(int)>;
    void Read();
public:
    Acceptor(EventLoop *eventloop, uint32_t port);
    ~Acceptor();
    void SetAcceptCallBack(const AcceptCallBack &cb);

    void Listen();
private:
    Socket _socket;
    std::unique_ptr<Channel> _channel;
    EventLoop *_eventloop;
    AcceptCallBack _accept_cb;
};
