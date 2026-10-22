// HttpContext 实现：把字节流解析成 HttpRequest
#include "HttpContext.h"

#define MAX_LINE_SIZE 8192  // 单行长度上限，超长即拒绝

/* 收请求行。两种"取不到行"要区分：数据不够是半包，等下一批；
 * 堆了很多字节仍无换行说明请求行超长，回 414。 */
bool HttpContext::RecvReqLine(Buffer *buf)
{
    // 状态守卫：RecvHttpRequest 里 switch 的 fall-through 依赖各阶段入口的守卫
    if (_recv_status != RECV_REQ_LINE)
        return false;
    std::string line = buf->GetLineAndPop();   // 空串表示没有完整行
    if (line.empty())
    {
        if (buf->GetReadSpace() > MAX_LINE_SIZE)
        {
            _resp_status = 414;
            _recv_status = RECV_ERROR;
            return false;
        }
        // 真半包，等下一批
        return true;
    }
    if (line.size() > MAX_LINE_SIZE)
    {
        _resp_status = 414;
        _recv_status = RECV_ERROR;
        return false;
    }
    bool ret = ParseReqLine(line);
    if (ret)
        _recv_status = RECV_REQ_HEAD;   // 进入头部阶段
    return ret;
}
/* 正则拆解请求行：方法 / 路径 / 可选 query / 版本 */
bool HttpContext::ParseReqLine(const std::string &line)
{
    // static 保证正则只编译一次，构造 regex 的开销不小
    static const std::regex pattern(
        R"(^([A-Z]+) ([^ ?]+)(?:\?([^ ]*))? HTTP/([0-9]+\.[0-9]+)\r\n$)");
    std::smatch matches;

    if (!std::regex_match(line, matches, pattern))   // 整行匹配
    {
        _resp_status = 400;
        _recv_status = RECV_ERROR;
        return false;
    }

    _req._method = matches[1].str();
    _req._path = Util::DeCode(matches[2].str(), false);   // 路径解码：+ 不当空格
    _req._version = matches[4].str();

    // query 按 & 切分、= 分键值，W3C 模式解码（+ 也当空格）
    std::string query = matches[3].str();
    std::vector<std::string> query_strs;
    Util::Split(query, "&", &query_strs);
    for (auto &str : query_strs)
    {
        size_t pos = str.find('=');

        if (pos == std::string::npos)
        {
            _resp_status = 400;       // 缺少 = 的参数直接拒绝
            _recv_status = RECV_ERROR;
            return false;
        }
        std::string key = Util::DeCode(str.substr(0, pos), true);
        std::string val = Util::DeCode(str.substr(pos + 1), true);

        _req.AddParam(key, val);
    }

    return true;
}
#define MAX_HEAD_SIZE 8192  // 单条头部行长度上限
/* 循环收头部，直到遇到空行 */
bool HttpContext::RecvReqHead(Buffer *buf)
{
    if (_recv_status != RECV_REQ_HEAD)      // 状态守卫
        return false;
    while (1)
    {
        std::string line = buf->GetLineAndPop();
        if (line.empty())
        {
            if (buf->GetReadSpace() > MAX_HEAD_SIZE)
            {
                _resp_status = 431;   // Request Header Fields Too Large
                _recv_status = RECV_ERROR;
                return false;
            }
            // 半包：等下一批
            return true;
        }
        // 空行 = 头部结束
        if (line == "\r\n")
            break;
        if (line.size() > MAX_HEAD_SIZE)
        {
            _resp_status = 431;
            _recv_status = RECV_ERROR;
            return false;
        }
        bool ret = ParseReqHead(line);
        if (!ret)
            return false;
    }
    _recv_status = RECV_REQ_BODY;     // 进入正文阶段
    return true;
}
/* 按 ": " 切分头部键值 */
bool HttpContext::ParseReqHead(const std::string &line)
{
    size_t pos = line.find(": ");
    if (pos == std::string::npos)
    {
        _resp_status = 400;
        _recv_status = RECV_ERROR;
        return false;
    }
    std::string key = line.substr(0, pos);
    std::string val = line.substr(pos + 2, line.size() - pos - 4);      // 长度减 4 去掉尾部 \r\n
    _req.AddHeader(key, val);
    return true;
}
/* 按 Content-Length 收正文，跨事件累积直到攒齐 */
bool HttpContext::RecvReqBody(Buffer *buf)
{
    if (_recv_status != RECV_REQ_BODY)      // 状态守卫
        return false;
    size_t contextlength = _req.ContentLength();    // 头里声明的正文长度
    // 无正文（GET 等请求）
    if (contextlength == 0)
    {
        _recv_status = RECV_OVER;   // 请求收齐
        return true;
    }
    size_t currentlength = _req._body.size();      // 已收到的部分
    size_t length = contextlength - currentlength;
    if (buf->GetReadSpace() >= length)
    {
        _req._body += buf->ReadStringAndPop(length);
        _recv_status = RECV_OVER;   // 请求收齐
        return true;
    }
    // 有多少拿多少，余下等下一批
    _req._body += buf->ReadStringAndPop(buf->GetReadSpace());
    return true;
}
/* 初始状态：从请求行开始 */
HttpContext::HttpContext() : _resp_status(200), _recv_status(RECV_REQ_LINE)
{
}
HttpContext::~HttpContext()
{
}
/* 取解析出错时的 HTTP 错误码 */
int HttpContext::RespStatus()
{
    return _resp_status;
}
/* 取当前解析阶段 */
HTTP_RECV_STATUS HttpContext::RecvStatus()
{
    return _recv_status;
}
/* 取解析中的请求 */
HttpRequest &HttpContext::Request()
{
    return _req;
}

/* 状态机总入口。switch 故意不加 break（fall-through）：一次喂入的数据
 * 从请求行一路推进到正文，粘包时一次能解出多个请求；
 * 各阶段函数入口的状态守卫保证半包停在中间态时不会越阶段解析。 */
void HttpContext::RecvHttpRequest(Buffer *buf)
{
    switch (_recv_status)
    {
    case RECV_REQ_LINE:
        RecvReqLine(buf);       // 故意 fall-through
    case RECV_REQ_HEAD:
        RecvReqHead(buf);
    case RECV_REQ_BODY:
        RecvReqBody(buf);
    }
}
/* 复位状态机并清空请求，Keep-Alive 下继续解下一个请求 */
void HttpContext::Reset()
{
    _resp_status = 200;
    _recv_status = RECV_REQ_LINE;
    _req.Reset();
}
