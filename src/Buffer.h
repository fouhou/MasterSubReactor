#pragma once
#include <iostream>
#include <vector>
#include <cassert>
#include <cstdint>
#include <algorithm>

/*
 * Buffer.h —— 应用层读写缓冲
 * vector<char> + _read/_write 双下标，布局为 头部废弃区 | 可读数据 | 尾部可写区。
 * TCP 字节流存在半包/粘包，数据先攒到这里，协议层能解多少解多少，网络层与协议层解耦。
 * 用 vector<char> 而非 string：网络数据是二进制，string 遇 \0 语义会坏。
 */

class Buffer
{
private:
    char *Begin();
    char *End();
    uint64_t GetHeadSpace();
    uint64_t GetTailSpace();
    void EnsureWriteSpace(uint64_t len);

public:
    char *WriteOffsetAddress();
    char *ReadOffsetAddress();
    void ChangeWriteOffset(uint64_t len);
    void ChangeReadOffset(uint64_t len);
    Buffer();
    Buffer(const Buffer &other);
    ~Buffer();
    uint64_t GetReadSpace();
    /* 写/读各两套：不带 Push/Pop 的只拷数据不动下标（peek 语义），
     * 带后缀的在拷贝后自动推进对应下标 */
    void Write(const void *buf, uint64_t len);
    void WriteAndPush(const void *buf, uint64_t len);
    void Read(void *buf, uint64_t len);
    void ReadAndPop(void *buf, uint64_t len);
    void WriteString(const std::string &buf);
    void WriteStringAndPush(const std::string &buf);
    std::string ReadString(uint64_t len);
    std::string ReadStringAndPop(uint64_t len);
    void Clear();

    // HTTP 按行解析原语
private:
    char *FindCRLF();

public:
    // 取一行（含 \r\n），返回空串表示完整行尚未到齐
    std::string GetLine();
    std::string GetLineAndPop();

private:
    uint64_t _read;
    uint64_t _write;
    uint64_t _capacity;
    std::vector<char> _buffer;
};
