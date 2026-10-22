/*
 * 并发测试客户端：fork 100 个进程，每条连接发 10 个 POST /echo
 * （前 9 个 keep-alive，最后一个 close），校验回显 body 与请求完全一致。
 */
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>

constexpr const char *SERVER_IP = "127.0.0.1";  // 本机回环
constexpr int SERVER_PORT = 8080;               // 被测服务器端口

// 并发客户端数量
constexpr int PROCESS_COUNT = 100;

// 每个 Connection 发送的请求数量
constexpr int REQUEST_COUNT = 10;                   // 验证 Keep-Alive


/* 循环发送直到全部发出（send 可能只发部分） */
bool SendAll(int sockfd, const std::string &data)
{
    size_t total = 0;

    while (total < data.size())
    {
        ssize_t n = send(
            sockfd,
            data.data() + total,
            data.size() - total,
            0
        );

        if (n <= 0)
            return false;               // 发送失败/对端关闭

        total += n;
    }

    return true;
}


/* 收一个完整响应：按 \r\n\r\n 定界头部，再按 Content-Length 收齐 body */
bool RecvHttpResponse(int sockfd, std::string &body)
{
    std::string response;               // 累积原始字节
    char buf[4096];

    // 读取完整头部
    size_t header_end = std::string::npos;

    while ((header_end = response.find("\r\n\r\n"))   // 空行 = 头部结束
           == std::string::npos)
    {
        ssize_t n = recv(sockfd, buf, sizeof(buf), 0);

        if (n <= 0)
            return false;               // 对端关闭/出错

        response.append(buf, n);
    }

    // 解析 Content-Length
    std::string header =
        response.substr(0, header_end + 4);

    std::string key = "Content-Length:";

    size_t pos = header.find(key);

    if (pos == std::string::npos)
        return false;                   // 没有 Content-Length：无法定界

    pos += key.size();

    while (pos < header.size() && header[pos] == ' ')
        ++pos;

    size_t end = header.find("\r\n", pos);

    size_t content_length =
        std::stoul(header.substr(pos, end - pos));

    // 收齐 body
    size_t body_start = header_end + 4;

    while (response.size() - body_start < content_length)   // 未收齐则继续 recv
    {
        ssize_t n = recv(sockfd, buf, sizeof(buf), 0);

        if (n <= 0)
            return false;

        response.append(buf, n);
    }

    body = response.substr(body_start, content_length);

    return true;
}


/* 单个客户端进程的测试流程 */
bool ClientWorker(int id)
{
    // 建立 TCP 连接
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
        return false;

    sockaddr_in server{};                               // 目标地址
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server.sin_addr) <= 0)
    {
        close(sockfd);
        return false;                   // IP 字符串非法
    }

    if (connect(
            sockfd,
            reinterpret_cast<sockaddr *>(&server),
            sizeof(server)) < 0)
    {
        close(sockfd);
        return false;                   // 连接失败
    }


    // 同一连接上连续发送请求
    for (int i = 0; i < REQUEST_COUNT; ++i)
    {
        std::string body =
            "client-" + std::to_string(id) +
            "-request-" + std::to_string(i);

        bool last = (i == REQUEST_COUNT - 1);           // 最后一个请求发 close

        std::string request =
            "POST /echo HTTP/1.1\r\n"                   // /echo 需服务端注册
            "Host: " + std::string(SERVER_IP) + ":" +
            std::to_string(SERVER_PORT) + "\r\n"
            "Connection: " +
            std::string(last ? "close" : "keep-alive") +    // 前 9 长连接，最后一个短连接
            "\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: " +
            std::to_string(body.size()) +
            "\r\n"
            "\r\n" +
            body;


        if (!SendAll(sockfd, request))
        {
            close(sockfd);
            return false;
        }


        std::string response_body;

        if (!RecvHttpResponse(sockfd, response_body))
        {
            close(sockfd);
            return false;
        }


        if (response_body != body)      // 校验回显一致性
        {
            std::cerr
                << "[Client " << id << "] "
                << "响应错误\n"
                << "send: " << body << "\n"
                << "recv: " << response_body << "\n";

            close(sockfd);
            return false;
        }
    }

    close(sockfd);                      // 全部通过：正常关闭

    return true;
}


/* fork 子进程并发测试，wait 回收并统计结果 */
int main()
{
    std::cout
        << "========== 并发测试开始 ==========\n"
        << "客户端进程数: " << PROCESS_COUNT << "\n"
        << "每个连接请求数: " << REQUEST_COUNT << "\n"
        << "总请求数: "
        << PROCESS_COUNT * REQUEST_COUNT
        << "\n\n";


    // 创建客户端子进程
    for (int i = 0; i < PROCESS_COUNT; ++i)
    {
        pid_t pid = fork();             // 每个客户端一个进程（进程级并发）

        if (pid < 0)
        {
            perror("fork");
            return 1;
        }

        if (pid == 0)
        {
            bool success = ClientWorker(i);

            _exit(success ? 0 : 1);     // 子进程用退出码报告成败
        }
    }


    // 父进程等待所有客户端

    int success = 0;
    int failed = 0;

    for (int i = 0; i < PROCESS_COUNT; ++i)
    {
        int status = 0;

        pid_t pid = wait(&status);      // 回收子进程（顺序无所谓）

        if (pid < 0)
        {
            ++failed;
            continue;
        }

        if (WIFEXITED(status) &&
            WEXITSTATUS(status) == 0)   // 正常退出且退出码 0 = 成功
        {
            ++success;
        }
        else
        {
            ++failed;
        }
    }


    // 输出测试结果
    std::cout
        << "\n========== 测试结果 ==========\n"
        << "成功客户端: " << success << "\n"
        << "失败客户端: " << failed << "\n"
        << "成功请求数（全部成功时）: "
        << success * REQUEST_COUNT
        << "\n";

    if (failed == 0)
    {
        std::cout
            << "\n[PASS] 所有并发请求全部成功\n";
    }
    else
    {
        std::cout
            << "\n[FAIL] 存在请求失败\n";
    }

    return 0;
}
