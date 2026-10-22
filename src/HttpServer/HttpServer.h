#pragma once

#include <memory>
#include <functional>
#include <regex>
#include "../TcpServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "HttpContext.h"

/*
 * HttpServer —— HTTP 应用层，只通过 4 个回调使用 TcpServer，不感知网络细节。
 * 路由表按方法各一张（正则 + 处理函数）；静态资源优先于路由匹配。
 */

#define DEFAULT_TIME 30 // 非活跃连接超时（秒）

class HttpServer
{
    using PtrConnection = std::shared_ptr<Connection>;
    using Handler = std::function<void(const HttpRequest &, HttpResponse *)>;   // 业务处理器签名
    using Handlers = std::vector<std::pair<std::regex, Handler>>;               // 路由表：正则+处理器的序列

private:
    // 连接关闭回调（预留）
    void OnClosed(const PtrConnection &conn);
    // 连接建立：给连接挂解析上下文
    void OnConnected(const PtrConnection &conn);
    // 解析或路由出错：按状态码构建错误响应
    void HandlerError(HttpResponse *rsp);
    // 拼响应报文并发送
    void WriteResponse(const PtrConnection &conn, const HttpRequest &req, HttpResponse *rsp);
    // 判断是否是静态资源
    bool IsFileHandler(const HttpRequest &req);
    void FileHandler(const HttpRequest &req, HttpResponse *rsp);
    void Dispatcher(HttpRequest &req, HttpResponse *rsp, const Handlers &handlers);
    // 分发请求：静态资源或路由表
    void Route(HttpRequest &req, HttpResponse *rsp);
    // 消息到达入口
    void OnMessage(const PtrConnection &conn, Buffer *buf);
public:
    HttpServer(uint16_t port, int timeout = DEFAULT_TIME);  // 构造：建 TcpServer 并接上回调
    ~HttpServer();
    // 设置 webroot
    void SetWebroot(const std::string &webroot);
    // 添加路由方法
    void Get(const std::string &reg, const Handler &handler);
    void Post(const std::string &reg, const Handler &handler);
    void Put(const std::string &reg, const Handler &handler);
    void Delete(const std::string &reg, const Handler &handler);
    // 设置线程数量
    void SetThreadCount(uint64_t threadcount);
    // 启动 HTTP 服务器
    void Start();

private:
    std::string _webroot;               // 静态资源所处的目录
    Handlers _get_route;                // GET 请求方法对应的处理函数
    Handlers _post_route;               // POST 请求方法对应的处理函数
    Handlers _put_route;                // PUT 请求方法对应的处理函数
    Handlers _delete_route;             // DELETE 请求方法对应的处理函数
    std::unique_ptr<TcpServer> _tcpsvr; // 基于TCP协议的服务器
};
