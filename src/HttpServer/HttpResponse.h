#pragma once

#include <iostream>
#include <string>
#include <unordered_map>

/*
 * HttpResponse —— 业务 handler 填写的响应对象，框架负责拼包发送。
 * Connection / Content-Length 由 HttpServer::WriteResponse 统一回写。
 */
class HttpResponse
{
public:
    HttpResponse();                     // 默认状态码 200
    HttpResponse(int statuscode);
    ~HttpResponse();
    // 重置 HTTP 响应
    void Reset(int statuscode = 200);
    // 开启重定向，默认 302 临时（301 为永久）
    void EnableRedirect(const std::string &url, int statuscode = 302);
    // 添加头部字段
    void AddHeader(const std::string &key, const std::string &val);
    // 查询头部字段
    bool HasHeader(const std::string &key);
    // 检测头部字段
    std::string GetHeader(const std::string &key);
    // 设置响应正文
    void SetContent(const std::string &content, const std::string &type);
    // 判断是否是短连接
    bool Close();

public:
    int _statuscode;                                       // 状态码
    std::string _body;                                     // 响应正文
    std::unordered_map<std::string, std::string> _headers; // 头部字段
};
