#pragma once
#include <iostream>
#include <cstdint>
#include <functional>
#include <sys/epoll.h>

class EventLoop;    // 前置声明，避免与 EventLoop.h 循环包含

/*
 * Channel.h —— 事件通道：一个 fd + 关心的事件集 + 就绪回调
 * 不拥有 fd（关闭归 Socket 管），事件集变化经 EventLoop 同步给 Poller。
 * Connection/Acceptor/EventLoop/TimeWheel 共用同一套机制，实现统一事件源。
 * _events 是想让 epoll 监控什么（输入），_revents 是 epoll 实际通知了什么（输出）。
 */

using Event_CallBack = std::function<void()>;

class Channel
{
public:
    Channel(int fd, EventLoop* poller);
    int Fd();
    void SetRevents(uint32_t revents);  // 仅 Poller 填入就绪事件
    uint32_t Events();
    ~Channel();                         // 析构前必须已 Remove()，否则 Poller 账本残留悬空指针
    void SetReadCallBack(const Event_CallBack &cb);
    void SetWriteCallBack(const Event_CallBack &cb);
    void SetErrorCallBack(const Event_CallBack &cb);
    void SetCloseCallBack(const Event_CallBack &cb);
    void SetEventCallBack(const Event_CallBack &cb);   // 任意事件就绪都触发（用于刷新连接活跃度）
    bool Readable();
    bool Writeable();
    void EnableRead();
    void EnableWrite();
    void DisableRead();
    void DisableWrite();
    void DisableAll();
    void Remove();      // 彻底从 epoll 摘除，区别于 DisableAll 只是清空事件集
    void HandleEvents();
private:
    int _fd;
    uint32_t _events;
    uint32_t _revents;
    EventLoop *_eventloop;
    Event_CallBack _read_callback;
    Event_CallBack _write_callback;
    Event_CallBack _error_callback;
    Event_CallBack _close_callback;
    Event_CallBack _event_callback;
};
