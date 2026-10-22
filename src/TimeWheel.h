#pragma once

#include <iostream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <unistd.h>
#include <sys/timerfd.h>
#include <cstring>

/* TimeWheel —— 秒级时间轮，60 格，管理非活跃连接的超时释放。
 * timerfd 每秒到期一次，经 epoll 与网络事件统一分发。
 * 到期执行由 Task 析构完成：格子被 clear 时 shared_ptr 归零即触发回调，
 * 相比升序链表/最小堆方案，插入与重置均为 O(1)。
 * 本文件只以前置声明引用 Channel/EventLoop，避免与 EventLoop.h 循环包含。 */

class Channel;
class EventLoop;

using handler_t = std::function<void()>;

/* 定时任务：析构时执行回调，未被取消才执行 */
class Task
{
public:
    Task(int id, int timeout, const handler_t &handler);
    ~Task();
    void Cancel();                                         // 仅置标记，等待静默析构
    int Timeout();                                         // 续命时重算格子位置用

private:
    int _id;
    int _timeout;      // 距到期的秒数
    handler_t _handler;
    bool _cancel;
};

class TimeWheel
{
private:
    ssize_t Read();     // 读 timerfd，返回本轮到期次数
    void Run();         // 秒针走一格并清空该格
    void OnTime();      // timerfd 可读回调
    const size_t MAX_CAPACITY = 60;

public:
    TimeWheel(EventLoop *eventloop);

    void AddTimerTask(int id, int timeout, const handler_t &handler);   // 挂到 (_tick + timeout) % 60

    void flushTimerTask(int id);        // 重新挂到新格子，重置倒计时

    void CancelTimerTask(int id);       // 置取消标记并清索引，不从格子删除

    bool HasTimerTask(int id);          // 约定只在绑定线程调用

    ~TimeWheel();

private:
    int _capacity;
    int _tick;          // 当前秒针位置
    std::vector<std::vector<std::shared_ptr<Task>>> _timewheel;
    std::unordered_map<int, std::weak_ptr<Task>> _timers;   // id 索引，不延长任务寿命
    int _timerfd;
    std::unique_ptr<Channel> _timer_channel;
    EventLoop *_eventloop;
};
