// HttpResponse 实现
#include "HttpResponse.h"

/* 默认 200 / 指定状态码；每轮请求栈上新建 */
HttpResponse::HttpResponse() : _statuscode(200)
{
}
HttpResponse::HttpResponse(int statuscode) : _statuscode(statuscode)
{
}
HttpResponse::~HttpResponse()
{
}
/* 清空并重设状态码（预留） */
void HttpResponse::Reset(int statuscode)
{
    _statuscode = statuscode;

    _body.clear();
    _headers.clear();
}
/* 设 301/302 状态码并加 Location 头，浏览器收到后自动跳转 */
void HttpResponse::EnableRedirect(const std::string &url, int statuscode)
{
    _statuscode = statuscode;
    AddHeader("Location", url);
}
/* 存一条响应头 */
void HttpResponse::AddHeader(const std::string &key, const std::string &val)
{
    _headers[key] = val;
}
/* 是否存在某响应头 */
bool HttpResponse::HasHeader(const std::string &key)
{
    return _headers.count(key);
}
/* 取响应头值 */
std::string HttpResponse::GetHeader(const std::string &key)
{
    auto it = _headers.find(key);
    if (it == _headers.end())
        return "";
    else
        return it->second;
}
/* 设正文并附带 Content-Type */
void HttpResponse::SetContent(const std::string &content, const std::string &type)
{
    _body = content;
    AddHeader("Content-Type", type);
}
/* 判断是否短连接（预留；响应的 Connection 头实际由 WriteResponse 按请求决定） */
bool HttpResponse::Close()
{
    return !(HasHeader("Connection") && GetHeader("Connection") == "keep-alive");
}
