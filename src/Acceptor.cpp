/* Acceptor.cpp —— Acceptor 实现。 */
#include "Acceptor.h"

/* Read —— 监听 fd 可读时被调用：accept 拿 newfd，交给上层回调。 */
void Acceptor::Read()
{
    int newfd = _socket.Accept();
    if(newfd < 0)
        return ;
    if(_accept_cb)
        _accept_cb(newfd);
}

/* 构造：建监听 socket，挂 Read 回调到 Channel。 */
Acceptor::Acceptor(EventLoop *eventloop, uint32_t port)
:_eventloop(eventloop)
{
    assert(_socket.CreateServer(port));
    _channel = std::make_unique<Channel>(_socket.Fd(), eventloop);
    _channel->SetReadCallBack(std::bind(&Acceptor::Read, this));
}

Acceptor::~Acceptor()
{
}

void Acceptor::SetAcceptCallBack(const AcceptCallBack &cb)
{
    _accept_cb = cb;
}

/* Listen —— 挂 EPOLLIN，开始接收新连接。 */
void Acceptor::Listen()
{
    _channel->EnableRead();
}
