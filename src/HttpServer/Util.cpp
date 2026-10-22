// Util 实现
#include "Util.h"

/* 两张静态查表：状态码描述与 MIME 类型，启动时构造一次 */

// 状态码 → 描述
static std::unordered_map<int, std::string> status_desc = {
    // 1xx Informational
    {100, "Continue"},
    {101, "Switching Protocols"},

    // 2xx Success
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {204, "No Content"},
    {206, "Partial Content"},

    // 3xx Redirection
    {301, "Moved Permanently"},     // 永久重定向（浏览器缓存新地址）
    {302, "Found"},                 // 临时重定向（每次还问旧地址）
    {303, "See Other"},
    {304, "Not Modified"},          // 协商缓存命中
    {307, "Temporary Redirect"},
    {308, "Permanent Redirect"},

    // 4xx Client Error
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {408, "Request Timeout"},
    {409, "Conflict"},
    {411, "Length Required"},
    {413, "Payload Too Large"},
    {414, "URI Too Long"},
    {415, "Unsupported Media Type"},
    {416, "Range Not Satisfiable"},
    {429, "Too Many Requests"},
    {431, "Request Header Fields Too Large"},

    // 5xx Server Error
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"}};

// 文件后缀 → MIME 类型
static std::unordered_map<std::string, std::string> mime_types = {
    // HTML
    {".html", "text/html"},
    {".htm", "text/html"},

    // Text
    {".txt", "text/plain"},
    {".css", "text/css"},
    {".csv", "text/csv"},

    // JavaScript / JSON / XML
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".xml", "application/xml"},

    // Images
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".bmp", "image/bmp"},
    {".webp", "image/webp"},
    {".svg", "image/svg+xml"},
    {".ico", "image/x-icon"},

    // Audio
    {".mp3", "audio/mpeg"},
    {".wav", "audio/wav"},
    {".ogg", "audio/ogg"},

    // Video
    {".mp4", "video/mp4"},
    {".webm", "video/webm"},
    {".avi", "video/x-msvideo"},

    // Documents
    {".pdf", "application/pdf"},
    {".zip", "application/zip"},
    {".gz", "application/gzip"},

    // Fonts
    {".ttf", "font/ttf"},
    {".otf", "font/otf"},
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},

    // Binary
    {".bin", "application/octet-stream"}};

/* 按分隔符切分字符串，返回段数 */
size_t Util::Split(const std::string &src, const std::string &sep, std::vector<std::string> *out)
{
    if (sep.empty())
        return 0;
    size_t offset = 0;
    while (offset < src.size())
    {
        size_t pos = src.find(sep, offset);
        if (pos == std::string::npos)
        {
            out->emplace_back(src.substr(offset));   // 最后一段
            return out->size();
        }
        else
        {
            // 连续分隔符不产生空段，ValidPath 的层级统计依赖这点
            if (pos != offset)
                out->emplace_back(src.substr(offset, pos - offset));
            offset = pos + sep.size();
        }
    }
    return out->size();
}

/* 读取整个文件进 string（二进制安全）。全量读内存，大文件场景可换 mmap/sendfile */
bool Util::ReadFile(const std::string &filename, std::string *out)
{
    std::ifstream ifs(filename, std::ifstream::binary);   // 文本模式会改写换行符
    if (!ifs.is_open())
    {
        LOG(LogLevel::ERROR) << "OPEN " << filename << " ERROR!";
        return false;
    }
    ifs.seekg(0, std::ifstream::end);
    size_t filesize = ifs.tellg();
    ifs.seekg(0, std::ifstream::beg);
    out->resize(filesize);
    ifs.read(out->data(), out->size());
    if (!ifs.good())
    {
        LOG(LogLevel::ERROR) << "READ " << filename << " ERROR!";
        ifs.close();
        return false;
    }
    ifs.close();
    return true;
}

/* 覆盖写入文件（当前主流程未使用） */
bool Util::WriteFile(const std::string &filename, const std::string &in)
{
    std::ofstream ofs(filename, std::ofstream::binary | std::ofstream::trunc);
    if (!ofs.is_open())
    {
        LOG(LogLevel::ERROR) << "OPEN " << filename << " ERROR!";
        return false;
    }
    ofs.write(in.c_str(), in.size());
    if (!ofs.good())
    {
        LOG(LogLevel::ERROR) << "WRITE " << filename << " ERROR!";
        ofs.close();
        return false;
    }
    ofs.close();
    return true;
}

/* URL 编码：非安全字符转 %XX，w3c 模式下空格转 + */
std::string Util::EnCode(const std::string &url, bool w3c)
{
    std::string res;
    for (auto &c : url)
    {
        // unreserved 字符（RFC 3986）不编码：字母数字与 . - _ ~
        if (c == '.' || c == '-' || c == '_' || c == '~' || isalnum(c))
        {
            res += c;
        }
        // w3c 模式空格转 +（RFC 3986 则用 %20）
        else if (c == ' ' && w3c)
        {
            res += '+';
        }
        else
        {
            char buf[4] = {0};
            snprintf(buf, sizeof(buf), "%%%02X", c);
            res += buf;
        }
    }
    return res;
}

/* URL 解码：%XX 三字符还原为一个字节，w3c 模式下 + 转空格 */
std::string Util::DeCode(const std::string &url, bool w3c)
{
    auto HexToInt = [](char c) -> int
    {
        if (c >= '0' && c <= '9')
            return c - '0';

        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;

        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;

        return -1;   // 非法十六进制字符
    };

    std::string res;
    size_t i = 0;
    while (i < url.size())
    {
        if (url[i] == '%' && i + 2 < url.size())
        {
            int x = HexToInt(url[i + 1]);
            int y = HexToInt(url[i + 2]);
            if (x != -1 && y != -1)
            {
                res += (x << 4) + y;
                i += 3;
            }
            else
            {
                // 非法 % 序列原样保留
                res += url[i];
                ++i;
            }
        }
        else if (url[i] == '+' && w3c)
        {
            // w3c 模式 + 表示空格
            res += ' ';
            ++i;
        }
        else
        {
            res += url[i];
            ++i;
        }
    }
    return res;
}

/* 状态码 → 描述 */
std::string Util::StatusDes(int statuscode)
{
    auto it = status_desc.find(statuscode);
    if (it == status_desc.end())
        return "Unknown";
    else
        return it->second;
}

/* 文件后缀 → Content-Type */
std::string Util::ExtMime(const std::string &filename)
{
    size_t pos = filename.rfind('.');
    if (pos == std::string::npos)
        return "application/octet-stream";   // 无后缀按二进制流
    std::string suffix = filename.substr(pos);
    for (auto &e : suffix)
        e = std::tolower(e);   // .JPG 和 .jpg 同等对待
    auto it = mime_types.find(suffix);
    if (it == mime_types.end())
        return "application/octet-stream";
    else
        return it->second;
}

/* 判断路径是否目录 */
bool Util::IsDirectory(const std::string &filename)
{
    struct stat st;
    if (stat(filename.c_str(), &st) < 0)
        return false;

    return S_ISDIR(st.st_mode);
}

/* 判断路径是否普通文件 */
bool Util::IsRegular(const std::string &filename)
{
    struct stat st;
    if (stat(filename.c_str(), &st) < 0)
        return false;

    return S_ISREG(st.st_mode);
}

/* 目录穿越防御：按 "/" 分段统计层级，".." 回退一层，
 * 回退穿根即拒绝（如 GET /../../etc/passwd）。 */
bool Util::ValidPath(const std::string &filename)
{
    std::vector<std::string> strs;
    Split(filename, "/", &strs);
    int level = 0;
    for (auto &str : strs)
    {
        if (str == ".")
            continue;
        if (str == "..")
        {
            if (--level < 0)
                return false;
        }
        else
        {
            ++level;
        }
    }
    return true;
}
