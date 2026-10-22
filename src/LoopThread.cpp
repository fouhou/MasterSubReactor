#include "LoopThread.h"

LoopThread::LoopThread()
:_eventloop(nullptr)
{
    _thread = std::thread(&LoopThread::ThreadRun, this);
}

LoopThread::~LoopThread()
{
}

/* 阻塞直到子线程完成 EventLoop 构造，避免拿到未初始化指针 */
EventLoop *LoopThread::GetEventLoop()
{
    EventLoop* eventloop;
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cond.wait(lock, [this](){
            return _eventloop != nullptr;
        });
        eventloop = _eventloop;
    }
    return eventloop;
}

void LoopThread::ThreadRun()
{
    // EventLoop 在子线程内构造，_thread_id 记录的才是本线程
    EventLoop eventloop;
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _eventloop = &eventloop;
        _cond.notify_all();
    }
    eventloop.Start();
}
