/* Connection.h —— 单条 TCP 连接的封装
 * 持有 fd、收发缓冲、状态机与回调，绑定到一个 EventLoop。 */
#pragma once
#include <iostream>
#include <any>
#include "Socket.h"
#include "Channel.h"
#include "Buffer.h"

class EventLoop;    // 前置声明，成员只需 EventLoop* 指针

/* 一条 TCP 连接：持有 fd、收发缓冲、状态机与回调，绑定到一个 EventLoop。
 * 状态机：CONNECTING → CONNECTED → DISCONNECTING → DISCONNECTED。
 * 继承 enable_shared_from_this 以便回调中安全传递自身 shared_ptr。 */

enum ConnectionStatus
{
    CONNECTING,
    CONNECTED,
    DISCONNECTING,
    DISCONNECTED
};

class Connection : public std::enable_shared_from_this<Connection>
{
private:
    using PtrConnection = std::shared_ptr<Connection>;
    using ConnectedCallBack = std::function<void(const PtrConnection &)>;
    using MessageCallBack = std::function<void(const PtrConnection &, Buffer *)>;
    using CloseCallBack = std::function<void(const PtrConnection &)>;
    using AnyEventCallBack = std::function<void(const PtrConnection &)>;

    void SendInLoop(const char *buffer, uint64_t len);
    void ShutdownInLoop();
    void EnableInactiveReleaseInLoop(uint64_t time);
    void CancelInactiveReleaseInLoop();
    void UpgradeInLoop(const std::any &context, const ConnectedCallBack &connected_cb,
                       const MessageCallBack &message_cb,
                       const CloseCallBack &close_cb,
                       const AnyEventCallBack &anyevent_cb);
    void EstablishedInLoop();

    void HandleRead();
    void HandleWrite();
    void HandleClose();
    void HandleError();
    void HandleEvent();
    void Release();
    void ReleaseInLoop();
public:
    Connection(uint64_t connection_id, int sockfd, EventLoop * eventloop);
    ~Connection();

    uint64_t ID();
    int Fd();
    std::any* GetContext();
    void SetContext(const std::any &context);
    bool Connected();

    void Send(const char *buffer, uint64_t len);
    void Shutdown();
    void EnableInactiveRelease(uint64_t time);
    void CancelInactiveRelease();
    void Established();

    void Upgrade(const std::any &context, const ConnectedCallBack &connected_cb,
                 const MessageCallBack &message_cb,
                 const CloseCallBack &close_cb,
                 const AnyEventCallBack &anyevent_cb);

    void SetConnectedCallBack(const ConnectedCallBack &cb);
    void SetMessageCallBack(const MessageCallBack &cb);
    void SetCloseCallBack(const CloseCallBack &cb);
    void SetAnyEventCallBack(const AnyEventCallBack &cb);

    void SetServerCloseCallBack(const CloseCallBack &cb);

private:
    uint64_t _connection_id;
    // int _task_id;
    int _sockfd;
    bool _enable_inactive_release;
    ConnectionStatus _status;
    Socket _socket;
    EventLoop *_eventloop;
    Channel _channel;
    Buffer _in_buffer;
    Buffer _out_buffer;
    std::any _context;

    ConnectedCallBack _connected_callback;
    MessageCallBack _message_callback;
    CloseCallBack _close_callback;
    AnyEventCallBack _anyevent_callback;

    CloseCallBack _serverclose_callback;
};
