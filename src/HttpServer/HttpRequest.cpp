// HttpRequest 实现
#include "HttpRequest.h"

/* 构造/析构 */
HttpRequest::HttpRequest()
{
}
HttpRequest::~HttpRequest()
{
}
/* 清空全部字段，供 Keep-Alive 复用 */
void HttpRequest::Reset()
{
    _method.clear();
    _path.clear();
    _version.clear();
    _headers.clear();
    _params.clear();
    _body.clear();
    std::smatch s;
    _smatch.swap(s);   // swap 置空更彻底
}
/* 存一条头部 */
void HttpRequest::AddHeader(const std::string &key, const std::string &val)
{
    _headers[key] = val;
}
/* 是否存在某头部 */
bool HttpRequest::HasHeader(const std::string &key) const
{
    return _headers.count(key);
}
/* 取头部值，没有返回空串 */
std::string HttpRequest::GetHeader(const std::string &key) const
{
    auto it = _headers.find(key);
    if (it == _headers.end())
        return "";
    else
        return it->second;
}
/* 存一条 query 参数 */
void HttpRequest::AddParam(const std::string &key, const std::string &val)
{
    _params[key] = val;
}
/* 是否存在某 query 参数 */
bool HttpRequest::HasParam(const std::string &key) const
{
    return _params.count(key);
}
/* 取 query 参数值，没有返回空串 */
std::string HttpRequest::GetParam(const std::string &key) const
{
    auto it = _params.find(key);
    if (it == _params.end())
        return "";
    else
        return it->second;
}

/* 取 Content-Length。头部值是任意的客户端输入，
 * stoi 失败按 0 处理；更严格的做法是解析头部时就拒绝非数字。 */
size_t HttpRequest::ContentLength() const
{
    auto it = _headers.find("Content-Length");
    if (it == _headers.end())
        return 0;
    try
    {
        return std::stoi(it->second);
    }
    catch (const std::exception &)
    {
        return 0;                       // 非法值按无正文处理
    }
}

/* 长短连接判定：显式 Connection 头优先，无头时按版本默认
 * （HTTP/1.1 默认持久连接，1.0 及更早默认短连接，见 RFC 7230）。 */
bool HttpRequest::Close() const
{
    // 头部键按客户端发来的原样存储，大小写敏感
    if (HasHeader("Connection"))
    {
        if (GetHeader("Connection") == "close")
            return true;
        if (GetHeader("Connection") == "keep-alive")
            return false;
    }
    return _version != "1.1";
}
