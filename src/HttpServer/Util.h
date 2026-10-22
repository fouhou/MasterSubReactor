#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <ios>
#include <ctype.h>
#include <unordered_map>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../Log.h"
using namespace LogModule;      // 引入日志

/*
 * Util —— HTTP 层工具集（全静态函数）：字符串切分、文件读写、
 * URL 编解码、状态码/MIME 查表、路径校验。
 */

class Util
{
public:
    // 按照指定分隔符分割源字符串
    static size_t Split(const std::string &src, const std::string &sep, std::vector<std::string> *out);
    // 读取文件内容
    static bool ReadFile(const std::string &filename, std::string *out);
    // 将数据写入文件中
    static bool WriteFile(const std::string &filename, const std::string &in);
    // URL编码
    static std::string EnCode(const std::string &url, bool w3c);
    // URL解码
    static std::string DeCode(const std::string &url, bool w3c);
    // 根据状态码获取状态描述信息
    static std::string StatusDes(int statuscode);
    // 根据文件后缀获取mime
    static std::string ExtMime(const std::string &filename);
    // 判断一个文件是不是目录文件
    static bool IsDirectory(const std::string &filename);
    // 判断一个文件是不是普通文件
    static bool IsRegular(const std::string &filename);
    // 校验请求路径是否合法（防目录穿越）
    static bool ValidPath(const std::string &filename);
};
