#pragma once
#include <iostream>
#include <sys/epoll.h>
#include <unordered_map>
#include <cstdlib>
#include <vector>
#include <cassert>
#include "Channel.h"
#include "Log.h"

/*
 * Poller.h —— epoll 封装：create1 建实例、ctl 增删改监控、wait 批量等就绪
 * 内核红黑树之外自持 fd → Channel* 的 _channels 账本，
 * epoll_wait 只返回 fd，回调分发要靠它反查 Channel。
 */

using namespace LogModule;
#define MAX_REVENTS_SIZE 1024   // epoll_wait 单次返回的就绪事件上限

class Poller
{
private:
    bool IsExistChannel(Channel *channel);
    void Update(Channel *channel, int op);

public:
    Poller();
    ~Poller();
    void UpdateEvent(Channel *channel);
    void RemoveEvent(Channel *channel);
    void Poll(std::vector<Channel *> *active);

private:
    int _epfd;
    std::unordered_map<int, Channel *> _channels; // fd → Channel* 反查表
    struct epoll_event _revents[MAX_REVENTS_SIZE]; // epoll_wait 出参缓冲，复用避免重复分配
};
