/* Poller.cpp —— Poller 类实现 */
#include "Poller.h"

bool Poller::IsExistChannel(Channel *channel)
{
    return _channels.count(channel->Fd());
}

/* epoll_ctl 包装，op 为 ADD/MOD */
void Poller::Update(Channel *channel, int op)
{
    int fd = channel->Fd();
    uint32_t events = channel->Events();
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = events;
    if (epoll_ctl(_epfd, op, fd, &ev) < 0)
    {
        LOG(LogLevel::ERROR) << "epoll_ctl error";
        return;
    }
}

/* 每个 EventLoop 独享一个 Poller，即一个 epoll 实例 */
Poller::Poller()
{
    _epfd = epoll_create1(0);
    if (_epfd < 0)
    {
        LOG(LogLevel::FATAL) << "epoll create error!";
        abort();   // 无 epoll 服务器无法工作，直接终止
    }
}

Poller::~Poller()
{
    close(_epfd);
}

/* 按 fd 是否已登记自动选择 ADD/MOD，上层无需关心 */
void Poller::UpdateEvent(Channel *channel)
{
    if (IsExistChannel(channel))
    {
        Update(channel, EPOLL_CTL_MOD);
    }
    else
    {
        Update(channel, EPOLL_CTL_ADD);
        _channels.emplace(channel->Fd(), channel);
    }
}

/* 从 epoll 和账本彻底摘除，幂等 */
void Poller::RemoveEvent(Channel *channel)
{
    if (IsExistChannel(channel))
    {
        int fd = channel->Fd();
        _channels.erase(fd);
        // EPOLL_CTL_DEL 时内核不关心事件集，ev 传 nullptr
        if (epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, nullptr) < 0)
        {
            LOG(LogLevel::ERROR) << "epoll_ctl error";
            return;
        }
    }
}

/* 阻塞等待就绪事件（timeout 为 -1 永久阻塞），就绪 Channel 填入 active */
void Poller::Poll(std::vector<Channel *> *active)
{
    int n = epoll_wait(_epfd, _revents, MAX_REVENTS_SIZE, -1);
    if (n < 0)
    {
        // EINTR 为信号打断，不算错误，下一轮重新等
        if (errno == EINTR)
            return;
        LOG(LogLevel::ERROR) << "epoll wait error";
        return;
    }
    for(int i = 0; i < n; ++i)
    {
        int fd = _revents[i].data.fd;
        uint32_t revents = _revents[i].events;
        assert(_channels.count(fd));           // fd 必须已登记，否则说明增删逻辑有 bug
        _channels[fd]->SetRevents(revents);
        active->emplace_back(_channels[fd]);
    }
}
