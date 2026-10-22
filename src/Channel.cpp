/* Channel.cpp —— Channel 类实现 */
#include "Channel.h"
#include "EventLoop.h"

/* 构造：登记 fd 与所属 EventLoop，事件集从 0 开始 */
Channel::Channel(int fd, EventLoop *eventloop) : _fd(fd), _events(0), _revents(0), _eventloop(eventloop)
{
}
int Channel::Fd()
{
    return _fd;
}
void Channel::SetRevents(uint32_t revents)
{
    _revents = revents;
}
uint32_t Channel::Events()
{
    return _events;
}
Channel::~Channel()
{
    // fd 的关闭归 Socket 管；但析构前必须已 Remove()，否则 Poller 账本残留悬空指针
}
void Channel::SetReadCallBack(const Event_CallBack &cb)
{
    _read_callback = cb;
}
void Channel::SetWriteCallBack(const Event_CallBack &cb)
{
    _write_callback = cb;
}
void Channel::SetErrorCallBack(const Event_CallBack &cb)
{
    _error_callback = cb;
}
void Channel::SetCloseCallBack(const Event_CallBack &cb)
{
    _close_callback = cb;
}
void Channel::SetEventCallBack(const Event_CallBack &cb)
{
    _event_callback = cb;
}
bool Channel::Readable()
{
    return (_events & EPOLLIN);
}
bool Channel::Writeable()
{
    return (_events & EPOLLOUT);
}

/* Enable/Disable 系列：修改事件位后立即经 EventLoop 同步给 epoll */
void Channel::EnableRead()
{
    _events |= EPOLLIN;
    _eventloop->UpdateEvent(this);
}
void Channel::EnableWrite()
{
    _events |= EPOLLOUT;
    _eventloop->UpdateEvent(this);
}
void Channel::DisableRead()
{
    _events &= ~EPOLLIN;
    _eventloop->UpdateEvent(this);
}
/* 发送缓冲发完后必须摘掉 EPOLLOUT：LT 模式下不摘会一直空触发 */
void Channel::DisableWrite()
{
    _events &= ~EPOLLOUT;
    _eventloop->UpdateEvent(this);
}
/* 清空事件集但仍保留在 epoll 中（MOD 而非 DEL） */
void Channel::DisableAll()
{
    _events = 0;
    _eventloop->UpdateEvent(this);
}
/* 彻底从 epoll 和 Poller 账本摘除，连接关闭时必须先于关 fd 调用 */
void Channel::Remove()
{
    _eventloop->RemoveEvent(this);
}

/* 就绪事件分发：先错误、再挂断，最后正常读写 */
void Channel::HandleEvents()
{
    if (_revents & EPOLLERR)
    {
        if(_error_callback)
            _error_callback();
    }
    else if (_revents & EPOLLHUP)
    {
        // 对端双向关闭，连接已不可用
        if(_close_callback)
            _close_callback();
    }
    else
    {
        // EPOLLPRI=带外数据可读；EPOLLRDHUP=对端半关闭（仍可读）
        if ((_revents & EPOLLIN) || (_revents & EPOLLPRI) || (_revents & EPOLLRDHUP))
        {
            if(_event_callback)
                _event_callback();
            if(_read_callback)
                _read_callback();
        }
        if (_revents & EPOLLOUT)
        {
            if(_event_callback)
                _event_callback();
            if(_write_callback)
                _write_callback();
        }
    }
    // 兜底再刷一次活跃度，与上面分支可能重复调用，回调需幂等
    if(_event_callback)
        _event_callback();
}
