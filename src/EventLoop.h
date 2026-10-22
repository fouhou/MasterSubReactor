#pragma once

#include <thread>
#include <mutex>
#include <vector>
#include <functional>
#include <memory>
#include <sys/eventfd.h>
#include <stdint.h>
#include <unistd.h>
#include "Poller.h"
#include "TimeWheel.h"

/* EventLoop —— 事件循环，每个实例绑定一个线程（one loop per thread）。
 * 循环内容：Poll 等待事件、分发就绪回调、执行任务队列。
 * 任务队列配合 eventfd 唤醒，供其他线程把操作投递到本线程执行；
 * Poller 与时间轮只在本线程访问，因此无需加锁。 */

class EventLoop
{
    using Functor = std::function<void()>;   // 投递到本线程执行的任务
    bool IsInLoop();
    void Read();
    void WakeUp();                          // 写 eventfd 唤醒阻塞在 epoll_wait 的本线程
public:
    EventLoop();
    ~EventLoop();
    void Start();

    void RunInLoop(const Functor& cb);      // 本线程直接执行，否则入队并唤醒
    void QueueInLoop(const Functor& cb);    // 入队，等本线程下一轮循环执行

    void UpdateEvent(Channel *channel);
    void RemoveEvent(Channel *channel);

    // 以下接口转发时间轮；跨线程调用时内部转投本 loop
    void AddTimerTask(int id, int timeout, const handler_t& handler);
    void flushTimerTask(int id);
    void CancelTimerTask(int id);
    bool HasTimerTask(int id);

private:
    int _eventfd;                           // 跨线程唤醒 fd
    std::thread::id _thread_id;             // 构造时记录的线程 id
    std::unique_ptr<Channel> _eventfd_channel; // eventfd 的 Channel，可读时清计数
    Poller _poller;
    std::vector<Functor> _tasks;            // 待执行任务队列（跨线程投递）
    std::mutex _mutex;
    std::unique_ptr<TimeWheel> _timewheel;  // 秒级时间轮，管理非活跃连接
};
