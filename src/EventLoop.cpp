#include "EventLoop.h"

/* 读走 eventfd 计数器并清零，否则 epoll 会一直报可读 */
void EventLoop::Read()
{
    uint64_t cnt = 0;
    while (true)
    {
        ssize_t n = read(_eventfd, &cnt, sizeof(uint64_t));
        if (n == sizeof(uint64_t))
            return;
        if (n < 0)
        {
            if (errno == EINTR)
                continue;           // 被信号打断，重试
            if (errno == EAGAIN)
                return;
            LOG(LogLevel::ERROR) << "eventfd read error";
            return;
        }
    }
}

/* 写 eventfd 唤醒阻塞在 epoll_wait 的本线程 */
void EventLoop::WakeUp()
{
    uint64_t val = 1;

    while (true)
    {
        ssize_t n = write(_eventfd, &val, sizeof(val));
        if (n == sizeof(val))
            return;
        if (n < 0)
        {
            if (errno == EINTR)
                continue;           // 被信号打断，重试
            if (errno == EAGAIN)
                return;
            LOG(LogLevel::ERROR) << "eventfd write error";
            return;
        }
    }
}

/* 注意：构造所在的线程即本 loop 的服务线程 */
EventLoop::EventLoop() : _eventfd(-1), _thread_id(std::this_thread::get_id()), _timewheel(std::make_unique<TimeWheel>(this))
{
    // EFD_CLOEXEC 防 exec 泄漏 fd；EFD_NONBLOCK 使无计数时 read 返回 EAGAIN 而非阻塞
    _eventfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (_eventfd < 0)
    {
        LOG(LogLevel::FATAL) << "create eventfd error";
        abort();
    }
    _eventfd_channel = std::make_unique<Channel>(_eventfd, this);
    _eventfd_channel->SetReadCallBack(std::bind(&EventLoop::Read, this));
    _eventfd_channel->EnableRead();
}

EventLoop::~EventLoop()
{
    if (close(_eventfd) < 0)
    {
        LOG(LogLevel::ERROR) << "close error";
        return;
    }
}

/* 当前线程是否为本 loop 线程 */
bool EventLoop::IsInLoop()
{
    return _thread_id == std::this_thread::get_id();
}

/* 入队任务并唤醒；只入队不唤醒的话，任务要等下次有网络事件才会被执行 */
void EventLoop::QueueInLoop(const Functor &cb)
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _tasks.emplace_back(cb);
    }
    WakeUp();
}

/* 跨线程协作的总入口：本线程直接执行，否则入队转投到本线程执行，
 * 使各连接的数据只被所属 loop 线程访问，无需加锁 */
void EventLoop::RunInLoop(const Functor &cb)
{
    if (IsInLoop())
    {
        cb();
        return;
    }
    QueueInLoop(cb);
}

void EventLoop::UpdateEvent(Channel *channel)
{
    _poller.UpdateEvent(channel);
}

void EventLoop::RemoveEvent(Channel *channel)
{
    _poller.RemoveEvent(channel);
}

/* 事件循环主体：等事件、分发回调、执行跨线程投递的任务。
 * 任务队列锁内只做 swap 换出，执行放在锁外，不阻塞其他线程投递 */
void EventLoop::Start()
{
    while (true)
    {
        std::vector<Channel *> actives;
        _poller.Poll(&actives);

        for (auto &active : actives)
            active->HandleEvents();

        std::vector<Functor> functors;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            functors.swap(_tasks);
        }
        for (auto functor : functors)
            functor();
    }
}

/* 时间轮接口的转发：TimeWheel 不加锁，靠只在所属 loop 线程访问保证安全，
 * 跨线程调用经 QueueInLoop 转投 */
void EventLoop::AddTimerTask(int id, int timeout, const handler_t &handler)
{
    if(IsInLoop())
        _timewheel->AddTimerTask(id, timeout, handler);
    else
        QueueInLoop([this, id, timeout, handler]()
        {
            _timewheel->AddTimerTask(id, timeout, handler);
        });
}

/* 续命：重置任务倒计时 */
void EventLoop::flushTimerTask(int id)
{
    if(IsInLoop())
        _timewheel->flushTimerTask(id);
    else
        QueueInLoop([this, id]()
        {
            _timewheel->flushTimerTask(id);
        });
}

void EventLoop::CancelTimerTask(int id)
{
    if(IsInLoop())
        _timewheel->CancelTimerTask(id);
    else
        QueueInLoop([this, id]()
        {
            _timewheel->CancelTimerTask(id);
        });
}

/* 只查索引，不做线程归位；约定由上层保证在绑定线程调用 */
bool EventLoop::HasTimerTask(int id)
{
    return _timewheel->HasTimerTask(id);
}
