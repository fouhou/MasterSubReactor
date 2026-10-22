/* Buffer.cpp —— Buffer 类实现
 * 关键设计：空间不够时先回收头部废弃区（有效数据搬到开头），整体不够才扩容，
 * 使长期运行下缓冲区容量不持续增长。 */
#include "Buffer.h"

#define BUFFER_SIZE 1024    // 初始容量，不足时自动扩容

/* 每条连接持有收/发两个 Buffer，构造时读写下标归零 */
Buffer::Buffer()
    : _read(0), _write(0), _capacity(BUFFER_SIZE), _buffer(_capacity)
{
}

Buffer::~Buffer()
{
}

/* 拷贝构造：vector 成员自动深拷贝内容 */
Buffer::Buffer(const Buffer &other)
    : _read(other._read),
      _write(other._write),
      _capacity(other._capacity),
      _buffer(other._buffer)
{
}

char *Buffer::Begin()
{
    return _buffer.data();
}

char *Buffer::End()
{
    return Begin() + _capacity;
}

/* 下一个可写字节的地址，外部可直接向此处写（如 recv 直灌缓冲） */
char *Buffer::WriteOffsetAddress()
{
    return _write + Begin();
}

/* 第一个未读字节的地址，可直接交给 send，避免二次拷贝 */
char *Buffer::ReadOffsetAddress()
{
    return _read + Begin();
}

void Buffer::ChangeWriteOffset(uint64_t len)
{
    _write += len;
}

void Buffer::ChangeReadOffset(uint64_t len)
{
    _read += len;
}

/* 头部废弃区大小，可回收 */
uint64_t Buffer::GetHeadSpace()
{
    return ReadOffsetAddress() - Begin();
}

/* 尾部直接可写空间 */
uint64_t Buffer::GetTailSpace()
{
    return End() - WriteOffsetAddress();
}

/* 未消费数据量 */
uint64_t Buffer::GetReadSpace()
{
    return WriteOffsetAddress() - ReadOffsetAddress();
}

/* 空间保证：尾部够写直接返回；否则把有效数据搬到开头，回收头部废弃区；
 * 整体不够才扩容 */
void Buffer::EnsureWriteSpace(uint64_t len)
{
    if (len <= GetTailSpace())
        return;
    if (len <= GetTailSpace() + GetHeadSpace())
    {
        std::copy(ReadOffsetAddress(), WriteOffsetAddress(), Begin());
        uint64_t data_size = GetReadSpace();
        _read = 0;
        _write = data_size;
    }
    else
    {
        _capacity = _write + len;
        _buffer.resize(_capacity);
    }
}

/* 写入但不推进写下标（peek 语义），配 ChangeWriteOffset 手动推进 */
void Buffer::Write(const void *buf, uint64_t len)
{
    EnsureWriteSpace(len);
    std::copy((char *)buf, (char *)buf + len, WriteOffsetAddress());
}

/* 写入并推进写下标 */
void Buffer::WriteAndPush(const void *buf, uint64_t len)
{
    Write(buf, len);
    ChangeWriteOffset(len);
}

/* 读取但不推进读下标（peek 语义） */
void Buffer::Read(void *buf, uint64_t len)
{
    assert(len <= GetReadSpace());   // 不能读超过存量，读超说明上层逻辑有 bug
    std::copy(ReadOffsetAddress(), ReadOffsetAddress() + len, (char *)buf);
}

/* 读取并推进读下标 */
void Buffer::ReadAndPop(void *buf, uint64_t len)
{
    Read(buf, len);
    ChangeReadOffset(len);
}

/* string 版写入 */
void Buffer::WriteString(const std::string &buf)
{
    Write(buf.c_str(), buf.size());
}

/* string 版写入并推进 */
void Buffer::WriteStringAndPush(const std::string &buf)
{
    WriteAndPush(buf.c_str(), buf.size());
}

/* 读 len 字节返回 string（不推进） */
std::string Buffer::ReadString(uint64_t len)
{
    std::string ret;
    ret.resize(len);              // resize 而非 reserve，撑出可写空间
    Read(&(*ret.begin()), len);
    return ret;
}

/* 读取并推进读下标 */
std::string Buffer::ReadStringAndPop(uint64_t len)
{
    std::string ret = ReadString(len);
    ChangeReadOffset(len);
    return ret;
}

/* 清空：读写下标归零，容量保留不缩容 */
void Buffer::Clear()
{
    _read = 0;
    _write = 0;
}

/* 在未消费区找 "\r\n"，返回指向 \n 的指针；nullptr 表示完整行未到齐 */
char *Buffer::FindCRLF()
{
    char *begin = ReadOffsetAddress();
    char *end = WriteOffsetAddress();
    for (char *pos = begin; pos != end; ++pos)
    {
        if (*pos == '\n' && pos != begin && *(pos - 1) == '\r')
        {
            return pos;
        }
    }
    return nullptr;
}

/* 提取一行但不消费（peek 语义） */
std::string Buffer::GetLine()
{
    char *ret = FindCRLF();
    if (ret == nullptr)
        return std::string();   // 完整行未到齐
    uint64_t len = ret - ReadOffsetAddress() + 1;   // +1 包含 \n
    return ReadString(len);
}

/* 提取一行并消费，返回内容含 \r\n；空串表示完整行未到齐 */
std::string Buffer::GetLineAndPop()
{
    char *ret = FindCRLF();
    if (ret == nullptr)
        return std::string();   // 完整行未到齐
    uint64_t len = ret - ReadOffsetAddress() + 1;   // +1 包含 \n
    return ReadStringAndPop(len);
}
