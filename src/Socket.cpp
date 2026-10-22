/* Socket.cpp —— Socket 类实现：socket 系统调用逐个包成成员函数 */
#include "Socket.h"

/* 无参构造，需配合 Create() 使用 */
Socket::Socket() : _sockfd(DEFAULT_SOCKET)
{
}

Socket::Socket(int sockfd) : _sockfd(sockfd)
{
}

int Socket::Fd()
{
    return _sockfd;
}

Socket::~Socket()
{
    Close();
}

bool Socket::Create()
{
    _sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (_sockfd < 0)
    {
        LOG(LogLevel::ERROR) << "socket error";
        return false;
    }
    return true;
}

bool Socket::Bind(uint16_t port, const std::string &ip)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0)
    {
        LOG(LogLevel::ERROR) << "inet_pton error";
        return false;
    }
    socklen_t len = sizeof(struct sockaddr_in);
    if (bind(_sockfd, (struct sockaddr *)&addr, len) < 0)
    {
        LOG(LogLevel::ERROR) << "bind error";
        return false;
    }
    return true;
}

bool Socket::Listen(int backlog)
{
    if (listen(_sockfd, backlog) < 0)
    {
        LOG(LogLevel::ERROR) << "listen error";
        return false;
    }
    return true;
}

/* Recv 返回约定：>0 收到 n 字节；0 = EAGAIN 暂无数据，等下一轮；
 * -1 = 真错误或对端关闭（对端关闭返回的 0 归一成 -1，上层统一走错误分支） */
ssize_t Socket::Recv(void *buf, int len, int flag)
{
    ssize_t n = recv(_sockfd, buf, len, flag);
    if (n > 0)
    {
        return n;
    }
    else if (n == 0)
    {
        // 对端关闭，归一成 -1
        return -1;
    }
    else
    {
        // 非阻塞下 EAGAIN 表示暂无数据，不算错误
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        LOG(LogLevel::ERROR) << "recv error";
        return -1;
    }
}

// MSG_DONTWAIT：单次调用临时非阻塞，不改变 fd 的阻塞属性
ssize_t Socket::NonBlockRecv(void *buf, int len)
{
    return Recv(buf, len, MSG_DONTWAIT);
}

/* Send 返回约定：>0 已发出 n 字节，可能小于 len，剩余由上层续传；
 * 0 = EAGAIN/EINTR 暂不写入；-1 = 真错误（需提前忽略 SIGPIPE） */
ssize_t Socket::Send(const void *buf, int len, int flag)
{
    ssize_t n = send(_sockfd, buf, len, flag);
    if (n < 0)
    {
        // EAGAIN=发送缓冲区满，剩余数据留在应用层 Buffer 等 EPOLLOUT 再发
        if (errno == EAGAIN || errno == EINTR)
            return 0;
        LOG(LogLevel::ERROR) << "send error";
        return -1;
    }
    else
    {
        return n;
    }
}

ssize_t Socket::NonBlockSend(const void *buf, int len)
{
    return Send(buf, len, MSG_DONTWAIT);
}

int Socket::Accept()
{
    int ret = accept(_sockfd, nullptr, nullptr);
    if (ret < 0)
        LOG(LogLevel::ERROR) << "accept error";
    return ret;
}

bool Socket::Connect(uint16_t port, const std::string &ip)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0)
    {
        LOG(LogLevel::ERROR) << "inet_pton error";
        return false;
    }
    socklen_t len = sizeof(struct sockaddr_in);
    if (connect(_sockfd, (struct sockaddr *)&addr, len) < 0)
    {
        LOG(LogLevel::ERROR) << "connect error";
        return false;
    }
    return true;
}

void Socket::Close()
{
    // 防重复 close：close 后 fd 号可能被内核复用，二次 close 会误关别的连接
    if(_sockfd == -1)
        return;
    if (close(_sockfd) < 0)
        LOG(LogLevel::ERROR) << "close error";
    _sockfd = -1;
}

bool Socket::SetNonBlock()
{
    // 必须先 F_GETFL 取出原标志位再或上 O_NONBLOCK 写回，直接设置会清掉其他标志位
    int fl = fcntl(_sockfd, F_GETFL);
    if (fl < 0)
    {
        LOG(LogLevel::ERROR) << "fcntl F_GETFL error";
        return false;
    }
    if (fcntl(_sockfd, F_SETFL, fl | O_NONBLOCK) < 0)
    {
        LOG(LogLevel::ERROR) << "fcntl F_SETFL error";
        return false;
    }
    return true;
}

/* 地址复用：重启可立即重绑仍处 TIME_WAIT 的端口，必须在 bind 前设置 */
bool Socket::SetReuseAddr()
{
    int op = 1;
    if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(int)) < 0)
    {
        LOG(LogLevel::ERROR) << "setsockopt reuseaddr error";
        return false;
    }
    op = 1;
    if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, &op, sizeof(int)) < 0)
    {
        LOG(LogLevel::ERROR) << "setsockopt reuseport error";
        return false;
    }
    return true;
}

/* 服务器监听 socket 一键初始化：Create → ReuseAddr → Bind → 非阻塞 → Listen */
bool Socket::CreateServer(uint16_t port, const std::string &ip)
{
    if (!Create())
        return false;
    if (!SetReuseAddr())
        return false;
    if (!Bind(port, ip))
        return false;
    if (!SetNonBlock())
        return false;
    if (!Listen())
        return false;
    return true;
}

bool Socket::CreateClient(uint16_t port, const std::string &ip)
{
    if (!Create())
        return false;
    if (!Connect(port, ip))
        return false;
    return true;
}
