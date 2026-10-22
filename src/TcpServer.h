/* TcpServer.h —— 服务器总装：Acceptor + 线程池 + 连接表 + 回调转发。 */
#pragma once

#include <signal.h>
#include "Acceptor.h"
#include "Connection.h"
#include "LoopThreadPool.h"

/* TcpServer：组装 Acceptor、Sub Reactor 线程池与连接表，向上层暴露回调接口。
 * 连接表只在主 Reactor 线程增删，单写者模型故无锁。
 * NetWork 是空类，构造时忽略 SIGPIPE；需在 main 中声明全局对象才生效。 */

class TcpServer
{
    using PtrConnection = std::shared_ptr<Connection>;
    using ConnectedCallBack = std::function<void(const PtrConnection &)>;
    using MessageCallBack = std::function<void(const PtrConnection &, Buffer *)>;
    using CloseCallBack = std::function<void(const PtrConnection &)>;
    using AnyEventCallBack = std::function<void(const PtrConnection &)>;
    using TaskCallBack = std::function<void()>;
private:
    void RemoveConnectionInLoop(const PtrConnection& conn);
    void SetTimerTaskInLoop(const TaskCallBack &cb, uint64_t delay);
    void RemoveConnection(const PtrConnection& conn);
    void NewConnection(int newfd);
public:
    TcpServer(uint16_t port);
    ~TcpServer();

    void SetConnectedCallBack(const ConnectedCallBack &cb);
    void SetMessageCallBack(const MessageCallBack &cb);
    void SetCloseCallBack(const CloseCallBack &cb);
    void SetAnyEventCallBack(const AnyEventCallBack &cb);
    void SetEnableInactiveRelease(uint64_t timeout);
    void SetTimerTask(const TaskCallBack &cb, uint64_t delay);
    void SetThreadCount(uint64_t threadcount);
    void Start();

private:
    uint16_t _port;
    uint64_t _id;
    uint64_t _timeout;
    bool _enable_inactive_release;
    uint64_t _thread_count;
    EventLoop _baseloop;
    Acceptor _acceptor;
    LoopThreadPool _subloops;
    std::unordered_map<uint64_t, PtrConnection> _connections;
    ConnectedCallBack _connected_cb;
    MessageCallBack _message_cb;
    CloseCallBack _close_cb;
    AnyEventCallBack _anyevent_cb;
};

class NetWork
{
public:
    NetWork();
    ~NetWork();
private:
};
