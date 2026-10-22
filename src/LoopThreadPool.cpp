#include "LoopThreadPool.h"

LoopThreadPool::LoopThreadPool(EventLoop *baseloop)
:_thread_count(0),      // 默认 0 = 单 Reactor 模式
_next_idx(0),
_baseloop(baseloop)
{
}

LoopThreadPool::~LoopThreadPool()
{
    for(int i = 0; i < _thread_count; ++i)
        delete _loopthreads[i];
}

void LoopThreadPool::SetCount(uint64_t count)
{
    _thread_count = count;
}

/* 创建各工作线程并取回其 EventLoop。
 * GetEventLoop 会阻塞等待各线程就绪，线程是并行启动的，
 * 返回后保证所有 loop 均可用 */
void LoopThreadPool::Create()
{
    _loopthreads.resize(_thread_count);
    _eventloops.resize(_thread_count);
    for(int i = 0; i < _thread_count; ++i)
    {
        _loopthreads[i] = new LoopThread();
        _eventloops[i] = _loopthreads[i]->GetEventLoop();
    }
}

/* 轮询分配；count=0 时返回主 Reactor */
EventLoop *LoopThreadPool::NextLoop()
{
    if(_thread_count == 0)
        return _baseloop;
    _next_idx = (_next_idx + 1) % _thread_count;
    return _eventloops[_next_idx];
}
