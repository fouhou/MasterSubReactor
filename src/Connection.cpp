/* Connection.cpp —— Connection 类实现。
 * public 接口经 RunInLoop/QueueInLoop 归位到 loop 线程，事件回调在 loop 线程执行。 */
#include "Connection.h"
#include "EventLoop.h"

/* HandleRead —— Channel 读事件回调：recv 一次进收缓冲，通知上层解析。
 * LT 模式下未读完的事件下轮还会通知，故只 recv 一次。 */
void Connection::HandleRead()
{
#define BUFF_SIZE 65535
    char buf[BUFF_SIZE] = {0};
    ssize_t n = _socket.NonBlockRecv(buf, BUFF_SIZE);
    if (n < 0)
    {
        ShutdownInLoop();
        return;
    }
    _in_buffer.WriteAndPush(buf, n);
    if (_message_callback)
        _message_callback(shared_from_this(), &_in_buffer);
}
/* HandleWrite —— Channel 写事件回调：把发缓冲数据写出去。
 * 发完必须摘 EPOLLOUT，否则 LT 下发送缓冲持续可写会空转 CPU。 */
void Connection::HandleWrite()
{
    ssize_t n = _socket.NonBlockSend(_out_buffer.ReadOffsetAddress(), _out_buffer.GetReadSpace());
    if (n < 0)
    {
        if (_in_buffer.GetReadSpace() > 0)
            if (_message_callback)
                _message_callback(shared_from_this(), &_in_buffer);
        Release();
        return;
    }
    _out_buffer.ChangeReadOffset(n);
    if (_out_buffer.GetReadSpace() == 0)
    {
        _channel.DisableWrite();
        if (_status == DISCONNECTING)
            Release();
    }
}
/* HandleClose —— 挂断事件：先把收缓冲残余数据交给上层，再释放。 */
void Connection::HandleClose()
{
    if (_in_buffer.GetReadSpace() > 0)
        if (_message_callback)
            _message_callback(shared_from_this(), &_in_buffer);

    Release();
}
/* HandleError —— 错误事件：走和挂断相同的收尾。 */
void Connection::HandleError()
{
    HandleClose();
}
/* HandleEvent —— 任意事件：刷新非活跃计时，执行用户回调。 */
void Connection::HandleEvent()
{
    if (_enable_inactive_release)
        _eventloop->flushTimerTask(_connection_id);
    if (_anyevent_callback)
        _anyevent_callback(shared_from_this());
}

/* ReleaseInLoop —— 真正的释放，必须在 loop 线程执行。
 * DISCONNECTED 守卫防 double close；先摘 epoll 再关 fd，避免悬空指针。 */
void Connection::ReleaseInLoop()
{
    if (_status == DISCONNECTED)
        return;
    _status = DISCONNECTED;
    _channel.Remove();
    _socket.Close();
    if (_eventloop->HasTimerTask(_connection_id))
        _eventloop->CancelTimerTask(_connection_id);
    if (_close_callback)
        _close_callback(shared_from_this());
    if (_serverclose_callback)
        _serverclose_callback(shared_from_this());
}

/* Release —— 把释放动作二次投递到下一轮 loop。
 * 避免在回调栈深处析构正在执行成员函数的对象。 */
void Connection::Release()
{
    _eventloop->QueueInLoop(std::bind(&Connection::ReleaseInLoop, this));
}

/* SendInLoop —— 写发送缓冲，挂 EPOLLOUT 等待可写通知（纯写事件驱动）。 */
void Connection::SendInLoop(const char *buffer, uint64_t len)
{
    if (_status == DISCONNECTED)
        return;
    _out_buffer.WriteAndPush(buffer, len);
    if (_out_buffer.GetReadSpace() > 0)
        if (!_channel.Writeable())
            _channel.EnableWrite();
}

/* ShutdownInLoop —— 优雅关闭：处理完收/发缓冲的存量数据再释放。 */
void Connection::ShutdownInLoop()
{
    _status = DISCONNECTING;
    if (_in_buffer.GetReadSpace() > 0)
        if (_message_callback)
            _message_callback(shared_from_this(), &_in_buffer);
    if (_out_buffer.GetReadSpace() > 0)
        if (!_channel.Writeable())
            _channel.EnableWrite();
    if (_out_buffer.GetReadSpace() == 0)
        Release();
}

/* EnableInactiveReleaseInLoop —— 挂非活跃超时任务，到点调 Release。 */
void Connection::EnableInactiveReleaseInLoop(uint64_t time)
{
    _enable_inactive_release = true;
    if (_eventloop->HasTimerTask(_connection_id))
        _eventloop->flushTimerTask(_connection_id);
    else
        _eventloop->AddTimerTask(_connection_id, time, std::bind(&Connection::Release, this));
}
/* CancelInactiveReleaseInLoop —— 取消非活跃超时任务。 */
void Connection::CancelInactiveReleaseInLoop()
{
    _enable_inactive_release = false;
    if (_eventloop->HasTimerTask(_connection_id))
        _eventloop->CancelTimerTask(_connection_id);
}

/* UpgradeInLoop —— 协议切换：替换上下文与全套回调，连接对象本身不动。 */
void Connection::UpgradeInLoop(const std::any &context, const ConnectedCallBack &connected_cb,
                               const MessageCallBack &message_cb,
                               const CloseCallBack &close_cb,
                               const AnyEventCallBack &anyevent_cb)
{
    _context = context;
    _connected_callback = connected_cb;
    _message_callback = message_cb;
    _close_callback = close_cb;
    _anyevent_callback = anyevent_cb;
}
/* EstablishedInLoop —— 连接建立：状态置 CONNECTED，挂读事件，通知上层。 */
void Connection::EstablishedInLoop()
{
    assert(_status == CONNECTING);
    _status = CONNECTED;
    _channel.EnableRead();

    if (_connected_callback)
        _connected_callback(shared_from_this());
}

/* 构造：绑定 id/fd/loop，把 5 个事件回调挂到 Channel。 */
Connection::Connection(uint64_t connection_id, int sockfd, EventLoop *eventloop)
:_connection_id(connection_id),
_sockfd(sockfd),
_enable_inactive_release(false),
_status(CONNECTING),
_socket(_sockfd),
_eventloop(eventloop),
_channel(_sockfd, _eventloop)
{
    _channel.SetReadCallBack(std::bind(&Connection::HandleRead, this));
    _channel.SetWriteCallBack(std::bind(&Connection::HandleWrite, this));
    _channel.SetCloseCallBack(std::bind(&Connection::HandleClose, this));
    _channel.SetErrorCallBack(std::bind(&Connection::HandleError, this));
    _channel.SetEventCallBack(std::bind(&Connection::HandleEvent, this));
}
Connection::~Connection()
{
}

uint64_t Connection::ID()
{
    return _connection_id;
}
int Connection::Fd()
{
    return _sockfd;
}
std::any *Connection::GetContext()
{
    return &_context;
}
void Connection::SetContext(const std::any &context)
{
    _context = context;
}
bool Connection::Connected()
{
    return _status == CONNECTED;
}

/* 以下 public 接口均经 RunInLoop 归位到 loop 线程执行，任意线程可调。 */

/* Send —— 先把数据拷进 std::string 再 move 进 lambda，
 * 避免调用方栈变量在异步执行时已销毁。 */
void Connection::Send(const char *buffer, uint64_t len)
{
    std::string data(buffer, len);

    _eventloop->RunInLoop([this, data = std::move(data)]
    {
        SendInLoop(data.data(), data.size());
    });
}
void Connection::Shutdown()
{
    _eventloop->RunInLoop(std::bind(&Connection::ShutdownInLoop, this));
}
void Connection::EnableInactiveRelease(uint64_t time)
{
    _eventloop->RunInLoop(std::bind(&Connection::EnableInactiveReleaseInLoop, this, time));
}
void Connection::CancelInactiveRelease()
{
    _eventloop->RunInLoop(std::bind(&Connection::CancelInactiveReleaseInLoop, this));
}
void Connection::Established()
{
    _eventloop->RunInLoop(std::bind(&Connection::EstablishedInLoop, this));
}

void Connection::Upgrade(const std::any &context, const ConnectedCallBack &connected_cb,
             const MessageCallBack &message_cb,
             const CloseCallBack &close_cb,
             const AnyEventCallBack &anyevent_cb)
{
    _eventloop->RunInLoop(std::bind(&Connection::UpgradeInLoop, this, context, connected_cb, message_cb, close_cb, anyevent_cb));
}

void Connection::SetConnectedCallBack(const ConnectedCallBack &cb)
{
    _connected_callback = cb;
}
void Connection::SetMessageCallBack(const MessageCallBack &cb)
{
    _message_callback = cb;
}
void Connection::SetCloseCallBack(const CloseCallBack &cb)
{
    _close_callback = cb;
}
void Connection::SetAnyEventCallBack(const AnyEventCallBack &cb)
{
    _anyevent_callback = cb;
}

void Connection::SetServerCloseCallBack(const CloseCallBack &cb)
{
    _serverclose_callback = cb;
}
