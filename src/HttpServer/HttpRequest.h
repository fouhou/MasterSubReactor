#pragma once

#include <iostream>
#include <string>
#include <regex>
#include <unordered_map>

/*
 * HttpRequest —— 解析完成的 HTTP 请求，纯数据对象。
 * _smatch 保存正则路由的捕获组；_params 为已解码的 query 参数。
 */
class HttpRequest
{
public:
    HttpRequest();
    ~HttpRequest();
    // 重置 HTTP 请求内容
    void Reset();
    // 添加头部字段
    void AddHeader(const std::string &key, const std::string &val);
    // 查询头部字段
    bool HasHeader(const std::string &key) const;
    // 获取头部字段
    std::string GetHeader(const std::string &key)  const;
    // 添加查询字符串
    void AddParam(const std::string &key, const std::string &val);
    // 查询查询字符串
    bool HasParam(const std::string &key) const;
    // 获取查询字符串
    std::string GetParam(const std::string &key)  const;
    // 获取请求正文的长度
    size_t ContentLength() const;
    // 判断是否是短连接
    bool Close() const;

    std::string _method;                                   // 请求方法
    std::string _path;                                     // 资源路径
    std::string _version;                                  // 请求版本
    std::string _body;                                     // 请求正文
    std::smatch _smatch;                                   // 正则路由捕获组
    std::unordered_map<std::string, std::string> _headers; // 头部字段
    std::unordered_map<std::string, std::string> _params;  // 查询字符串
};
