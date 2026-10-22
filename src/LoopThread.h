#pragma once
#include "EventLoop.h"
#include <condition_variable>

/* LoopThread —— 在子线程内部构造 EventLoop，保证线程归属正确。
 * std::thread 构造返回不代表 EventLoop 已建好，
 * GetEventLoop 用条件变量等待指针就绪后再返回。 */
class LoopThread
{
private:
    void ThreadRun();               // 线程入口：构造 EventLoop 并进入事件循环
public:
    LoopThread();                   // 构造即启动线程
    ~LoopThread();
    EventLoop *GetEventLoop();      // 阻塞直到 EventLoop 构造完成

private:
    EventLoop *_eventloop;          // 指向线程栈上的 EventLoop，线程不退出故安全
    std::thread _thread;
    std::mutex _mutex;              // 与 _cond 配合，同步 EventLoop 指针的发布
    std::condition_variable _cond;
};
