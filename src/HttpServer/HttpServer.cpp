// HttpServer 实现：请求分发、静态资源与响应拼包
#include "HttpServer.h"

/* 连接关闭回调，预留 */
void HttpServer::OnClosed(const PtrConnection &conn)
{
}

/* 连接建立：给每条连接一个独立的解析上下文 */
void HttpServer::OnConnected(const PtrConnection &conn)
{
    // 每连接一个独立解析上下文塞进 std::any，网络库不感知 HTTP 类型；OnMessage 里取回
    conn->SetContext(std::any(HttpContext()));
}
/* 把 rsp 改成错误页：填简单 HTML 和 Content-Type */
void HttpServer::HandlerError(HttpResponse *rsp)
{
    std::string status_desc = Util::StatusDes(rsp->_statuscode);
    // 构建简单错误页
    rsp->_body.clear();
    rsp->_body += "<!DOCTYPE html>\n";
    rsp->_body += "<html>\n";
    rsp->_body += "<head>\n";
    rsp->_body += "    <meta charset=\"UTF-8\">\n";
    rsp->_body += "    <title>";
    rsp->_body += std::to_string(rsp->_statuscode) + " " + status_desc;
    rsp->_body += "</title>\n";
    rsp->_body += "</head>\n";
    rsp->_body += "<body>\n";
    rsp->_body += "    <h1>";
    rsp->_body += std::to_string(rsp->_statuscode) + " " + status_desc;
    rsp->_body += "</h1>\n";
    rsp->_body += "</body>\n";
    rsp->_body += "</html>\n";

    rsp->AddHeader("Content-Type", "text/html; charset=UTF-8");
}
/* 拼出完整响应报文交给连接发送：Connection 头 → Content-Length → 状态行 → 头 → 空行 → body */
void HttpServer::WriteResponse(const PtrConnection &conn, const HttpRequest &req, HttpResponse *rsp)
{
    // Connection 头按请求回写，客户端据此决定是否复用连接
    if (req.Close())
        rsp->AddHeader("Connection", "close");
    else
        rsp->AddHeader("Connection", "keep-alive");
    // 客户端靠 Content-Length 定界 body
    rsp->AddHeader("Content-Length", std::to_string(rsp->_body.size()));
    // 响应行版本回写请求的版本：请求 1.1 就答 1.1
    std::string str;
    str += "HTTP/" + req._version + " " + std::to_string(rsp->_statuscode) + " " + Util::StatusDes(rsp->_statuscode) + "\r\n";
    for (auto &[key, val] : rsp->_headers)
        str += key + ": " + val + "\r\n";
    str += "\r\n";
    str += rsp->_body;
    conn->Send(str.c_str(), str.size());
}
/* 判定是否走静态资源路径 */
bool HttpServer::IsFileHandler(const HttpRequest &req)
{
    if (_webroot.empty())
        return false;
    if (req._method != "GET" && req._method != "HEAD")
        return false;
    // 目录穿越防御：拦截 /../ 穿出 webroot 的路径
    if (!Util::ValidPath(req._path))
        return false;
    std::string path = _webroot + req._path;
    if (req._path == "/")
        path += "index.html";   // 根路径默认返回首页
    if (!Util::IsRegular(path))
        return false;           // 必须是存在的普通文件
    return true;
}
/* 读静态文件进 body，并按后缀设置 Content-Type */
void HttpServer::FileHandler(const HttpRequest &req, HttpResponse *rsp)
{
    std::string filename = _webroot + req._path;
    if (req._path == "/")
        filename += "index.html";
    Util::ReadFile(filename, &rsp->_body);
    std::string suffix = Util::ExtMime(filename);
    rsp->AddHeader("Content-Type", suffix);
}
/* 线性匹配路由表，命中即执行；捕获组写入 req._smatch 供 handler 取参 */
void HttpServer::Dispatcher(HttpRequest &req, HttpResponse *rsp, const Handlers &handlers)
{
    for (auto &handler : handlers)
    {
        const std::regex &reg = handler.first;
        const Handler &functor = handler.second;
        if (std::regex_match(req._path, req._smatch, reg))
        {
            functor(req, rsp);   // 先注册的优先
            return;
        }
    }
    rsp->_statuscode = 404;             // Not Found
    HandlerError(rsp);
}
/* 分发：静态资源优先，其次按方法进路由表，未支持的方法 405 */
void HttpServer::Route(HttpRequest &req, HttpResponse *rsp)
{
    if (IsFileHandler(req))
    {
        FileHandler(req, rsp);      // favicon 之类的文件无需注册路由
        return;
    }
    if (req._method == "GET" || req._method == "HEAD")
        Dispatcher(req, rsp, _get_route);
    else if (req._method == "POST")
        Dispatcher(req, rsp, _post_route);
    else if (req._method == "PUT")
        Dispatcher(req, rsp, _put_route);
    else if (req._method == "DELETE")
        Dispatcher(req, rsp, _delete_route);
    else
    {
        rsp->_statuscode = 405;   // Method Not Allowed
        HandlerError(rsp);
    }
}

/* HTTP 层总入口：粘包 / Keep-Alive / 错误处理都在这个循环里 */
void HttpServer::OnMessage(const PtrConnection &conn, Buffer *buf)
{
    // 缓冲区可能挤着多个完整请求（粘包），解出一个处理一个
    while (buf->GetReadSpace() > 0)
    {
        HttpResponse rsp;               // 每轮栈上新建，无共享
        // 必须用 any_cast 指针版：值版是拷贝语义，状态推进会落在临时副本上，Keep-Alive 失效
        HttpContext *context = std::any_cast<HttpContext>(conn->GetContext());
        context->RecvHttpRequest(buf);
        if (context->RecvStatus() == RECV_ERROR)
        {
            rsp._statuscode = context->RespStatus();        // 取状态机记下的错误码（400/414/431）
            // 出错后请求边界不可知，不复用连接：回错误页并断开
            HandlerError(&rsp);
            WriteResponse(conn, context->Request(), &rsp);
            context->Reset();
            buf->Clear();           // 清缓冲，残余字节全部作废
            conn->Shutdown();
            return;
        }
        // 半包：等下一批数据到了继续解析
        if (context->RecvStatus() != RECV_OVER)
            return;
        Route(context->Request(), &rsp);
        WriteResponse(conn, context->Request(), &rsp);
        if (context->Request().Close())
        {
            conn->Shutdown();       // 短连接：回完就断
            return;
        }
        context->Reset();   // 复位后继续解缓冲区里的下一个请求
    }
}

/* 建 TcpServer 并接上 4 个回调：网络库与应用层的全部耦合点 */
HttpServer::HttpServer(uint16_t port, int timeout) : _tcpsvr(std::make_unique<TcpServer>(port))
{
    _tcpsvr->SetConnectedCallBack([this](const PtrConnection &conn)
                                  { OnConnected(conn); });
    _tcpsvr->SetMessageCallBack([this](const PtrConnection &conn, Buffer *buf)
                                { OnMessage(conn, buf); });
    _tcpsvr->SetCloseCallBack([this](const PtrConnection &conn)
                              { OnClosed(conn); });
    _tcpsvr->SetEnableInactiveRelease(timeout);   // 非活跃超时踢除
}
HttpServer::~HttpServer()
{
}
/* 设置静态资源根目录，须在 Start 前调用 */
void HttpServer::SetWebroot(const std::string &webroot)
{
    _webroot = webroot;
}
/* 注册 GET 路由；须在 Start 前注册，运行期路由表只读 */
void HttpServer::Get(const std::string &reg, const Handler &handler)
{
    _get_route.emplace_back(std::regex(reg), handler);
}
/* 注册 POST 路由 */
void HttpServer::Post(const std::string &reg, const Handler &handler)
{
    _post_route.emplace_back(std::regex(reg), handler);
}
/* 注册 PUT 路由 */
void HttpServer::Put(const std::string &reg, const Handler &handler)
{
    HttpServer::_put_route.emplace_back(std::regex(reg), handler);
}
/* 注册 DELETE 路由 */
void HttpServer::Delete(const std::string &reg, const Handler &handler)
{
    HttpServer::_delete_route.emplace_back(std::regex(reg), handler);
}
/* 设置线程数，须在 Start 前调用 */
void HttpServer::SetThreadCount(uint64_t threadcount)
{
    _tcpsvr->SetThreadCount(threadcount);
}
/* 启动服务器，阻塞在事件主循环 */
void HttpServer::Start()
{
    _tcpsvr->Start();
}
