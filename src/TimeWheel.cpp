#include "TimeWheel.h"
#include "EventLoop.h"      // h 内只有前置声明

Task::Task(int id, int timeout, const handler_t &handler) : _id(id), _timeout(timeout), _handler(handler), _cancel(false)
{
}
// 析构即到期执行：格子被 clear 时 shared_ptr 归零触发
Task::~Task()
{
    if (!_cancel)
        _handler();
}
void Task::Cancel()
{
    _cancel = true;   // 任务仍占着格子等 tick 析构，避免中途删除
}
int Task::Timeout()
{
    return _timeout;
}

/* 读 timerfd，返回到期次数（进程休眠/繁忙时会积累到 >1） */
ssize_t TimeWheel::Read()
{
    uint64_t expirations;
    ssize_t n;
    while (true)
    {
        n = read(_timerfd, &expirations, sizeof(uint64_t));
        if (n == sizeof(uint64_t))
            break;
        if (n < 0)
        {
            if (errno == EINTR)
                continue;           // 被信号打断，重试
            if (errno == EAGAIN)
                return -1;
            LOG(LogLevel::ERROR) << "timerfd read error";
            return -1;
        }
    }
    return expirations;
}

// 清空格子使其中任务的 shared_ptr 归零，析构即执行
void TimeWheel::Run()
{
    _tick = (_tick + 1) % _capacity;
    _timewheel[_tick].clear();
}

void TimeWheel::OnTime()
{
    ssize_t n = Read();
    for(int i = 0; i < n; ++i)
        Run();
}

/* 建 1s 周期的 timerfd 并挂到 eventloop 监控 */
TimeWheel::TimeWheel(EventLoop *eventloop) : _capacity(MAX_CAPACITY), _tick(0), _timewheel(_capacity), _eventloop(eventloop)
{
    // 单调时钟，不受系统时间回拨影响
    _timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (_timerfd < 0)
    {
        LOG(LogLevel::FATAL) << "create timerfd error";
        abort();
    }
    struct itimerspec timeout;
    memset(&timeout, 0, sizeof(timeout));
    timeout.it_value.tv_sec = 1;
    timeout.it_interval.tv_sec = 1;    // 1s 周期
    timerfd_settime(_timerfd, 0, &timeout, nullptr);
    _timer_channel = std::make_unique<Channel>(_timerfd, _eventloop);
    _timer_channel->SetReadCallBack(std::bind(&TimeWheel::OnTime, this));
    _timer_channel->EnableRead();
}

/* 注册任务；timeout 超过一圈时取模后会落在提前的格子上被提前执行 */
void TimeWheel::AddTimerTask(int id, int timeout, const handler_t &handler)
{
    std::shared_ptr<Task> task = std::make_shared<Task>(id, timeout, handler);
    int pos = (_tick + task->Timeout()) % _capacity;
    _timewheel[pos].emplace_back(task);
    std::weak_ptr<Task> tmp = task;   // 索引存弱引用，不延长任务寿命
    _timers[id] = tmp;
}

/* 续命：把任务重新挂到新格子重置倒计时。
 * 新旧副本指向同一 Task，旧格子清空只减引用计数，不会重复执行 */
void TimeWheel::flushTimerTask(int id)
{
    auto iter = _timers.find(id);
    if (iter == _timers.end())
        return;
    if (_timers[id].expired())
    {
        _timers.erase(id);   // 清理失效索引
        return;
    }
    std::shared_ptr<Task> task = _timers[id].lock();
    int pos = (_tick + task->Timeout()) % _capacity;
    _timewheel[pos].emplace_back(task);
}

/* 取消：置标记并清索引，任务等格子 tick 时自然析构 */
void TimeWheel::CancelTimerTask(int id)
{
    auto iter = _timers.find(id);
    if (iter == _timers.end())
        return;
    if (_timers[id].expired())
    {
        _timers.erase(id);             // 清理失效索引
        return;
    }
    std::shared_ptr<Task> task = _timers[id].lock();
    task->Cancel();
    _timers.erase(id);
}

/* 只查索引；约定由调用方保证在所属 loop 线程内调用 */
bool TimeWheel::HasTimerTask(int id)
{
    return _timers.count(id);
}

TimeWheel::~TimeWheel()
{
}
