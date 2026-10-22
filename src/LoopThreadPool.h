#pragma once
#include "LoopThread.h"

/* LoopThreadPool —— Sub Reactor 线程池，轮询分配新连接。
 * 线程数设为 0 时不建任何线程，连接全部挂主 Reactor（单 Reactor 模式）。 */
class LoopThreadPool
{
public:
    LoopThreadPool(EventLoop *baseloop);
    ~LoopThreadPool();
    void SetCount(uint64_t count);          // 须在 Create 之前调用
    void Create();
    EventLoop *NextLoop();                  // 轮询分配一个 EventLoop
private:
    uint64_t _thread_count;     // 默认 0 = 单 Reactor 模式
    int _next_idx;              // 轮询游标
    EventLoop *_baseloop;       // 主 Reactor
    std::vector<LoopThread *> _loopthreads;
    std::vector<EventLoop *> _eventloops;   // 与 _loopthreads 一一对应
};
