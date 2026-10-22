/* TcpServer.cpp —— TcpServer 与 NetWork 实现。 */
#include "TcpServer.h"

/* RemoveConnectionInLoop —— 从连接表删除，必须在主 Reactor 线程执行。 */
void TcpServer::RemoveConnectionInLoop(const PtrConnection &conn)
{
    _connections.erase(conn->ID());
}

/* SetTimerTaskInLoop —— 给主 Reactor 挂用户定时任务。 */
void TcpServer::SetTimerTaskInLoop(const TaskCallBack &cb, uint64_t delay)
{
    ++_id;
    _baseloop.AddTimerTask(_id, delay, cb);
}

/* RemoveConnection —— 连接关闭回调，归还主线程再删连接表。 */
void TcpServer::RemoveConnection(const PtrConnection &conn)
{
    _baseloop.RunInLoop(std::bind(&TcpServer::RemoveConnectionInLoop, this, conn));
}

/* NewConnection —— accept 回调，在主 Reactor 线程执行。
 * 分配 id、创建 Connection（绑定 Sub loop）、装回调、挂超时、Established、入表。 */
void TcpServer::NewConnection(int newfd)
{
    ++_id;
    std::shared_ptr<Connection> conn = std::make_shared<Connection>(_id, newfd, _subloops.NextLoop());
    conn->SetConnectedCallBack(_connected_cb);
    conn->SetMessageCallBack(_message_cb);
    conn->SetAnyEventCallBack(_anyevent_cb);
    conn->SetCloseCallBack(_close_cb);
    conn->SetServerCloseCallBack(std::bind(&TcpServer::RemoveConnection, this, std::placeholders::_1));
    if (_enable_inactive_release)
        conn->EnableInactiveRelease(_timeout);
    conn->Established();
    _connections.emplace(_id, conn);
}

/* 构造：初始化成员，把 NewConnection 交给 Acceptor。 */
TcpServer::TcpServer(uint16_t port)
    : _port(port), _timeout(0), _enable_inactive_release(false), _thread_count(0), _acceptor(&_baseloop, _port), _subloops(&_baseloop)
{
    _acceptor.SetAcceptCallBack(std::bind(&TcpServer::NewConnection, this, std::placeholders::_1));
}

TcpServer::~TcpServer()
{
}

void TcpServer::SetConnectedCallBack(const ConnectedCallBack &cb)
{
    _connected_cb = cb;
}
void TcpServer::SetMessageCallBack(const MessageCallBack &cb)
{
    _message_cb = cb;
}
void TcpServer::SetCloseCallBack(const CloseCallBack &cb)
{
    _close_cb = cb;
}
void TcpServer::SetAnyEventCallBack(const AnyEventCallBack &cb)
{
    _anyevent_cb = cb;
}

void TcpServer::SetEnableInactiveRelease(uint64_t timeout)
{
    _enable_inactive_release = true;
    _timeout = timeout;
}

void TcpServer::SetTimerTask(const TaskCallBack &cb, uint64_t delay)
{
    _baseloop.RunInLoop(std::bind(&TcpServer::SetTimerTaskInLoop, this, cb, delay));
}

void TcpServer::SetThreadCount(uint64_t threadcount)
{
    _thread_count = threadcount;
    _subloops.SetCount(_thread_count);
}

/* Start —— 启动：先建 Sub Reactor 线程池，再开监听，最后主 loop 进入事件循环。 */
void TcpServer::Start()
{
    _subloops.Create();
    _acceptor.Listen();
    _baseloop.Start();
}

/* NetWork 构造：忽略 SIGPIPE，避免对端关闭后 write 导致进程被杀。 */
NetWork::NetWork()
{
    signal(SIGPIPE, SIG_IGN);
}

NetWork::~NetWork()
{

}
