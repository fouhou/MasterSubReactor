/*
 * HttpContext —— HTTP 解析状态机：每连接一个实例（挂在 Connection 的 std::any 上下文里），
 * 按 请求行 → 头部 → 正文 推进；数据不够停在中间态（半包），
 * 解完一个请求 Reset 后继续解下一个（粘包/Keep-Alive）。
 */

#include "HttpRequest.h"
#include "../Buffer.h"
#include "Util.h"

enum HTTP_RECV_STATUS
{
    RECV_ERROR,      // 解析失败（错误码存在 _resp_status 里）
    RECV_REQ_LINE,   // 正在收请求行
    RECV_REQ_HEAD,   // 正在收头部
    RECV_REQ_BODY,   // 正在收正文
    RECV_OVER        // 一个完整请求接收完成
};

class HttpContext
{
private:
    bool RecvReqLine(Buffer* buf);      // 收一行请求行
    bool ParseReqLine(const std::string &line);   // 正则解析请求行
    bool RecvReqHead(Buffer* buf);      // 循环收头部直到空行
    bool ParseReqHead(const std::string &line);   // 按 ": " 切分键值
    bool RecvReqBody(Buffer* buf);      // 按 Content-Length 定长收正文
public:
    HttpContext();                      // 初始：200 + RECV_REQ_LINE
    ~HttpContext();
    // 重置上下文数据
    void Reset();
    // 获取响应状态码
    int RespStatus();
    // 获取当前HTTP请求处理的状态
    HTTP_RECV_STATUS RecvStatus();
    // 获取处理的HTTP请求
    HttpRequest &Request();
    // 解析 HTTP 请求
    void RecvHttpRequest(Buffer* buf);

private:
    int _resp_status;                 // 解析出错时记下的 HTTP 错误码
    HTTP_RECV_STATUS _recv_status;    // 当前解析阶段
    HttpRequest _req;                 // 解析中的 HTTP 请求
};
