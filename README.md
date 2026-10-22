# MasterSubReactor

基于 C++17 从零实现的 Linux 高并发 HTTP 服务器，采用主从 Reactor 多线程模型（one loop per thread）：Main Reactor 专职连接接收，新连接轮询分配给 Sub Reactor 线程，此后这条连接的读写、定时与释放全部收敛在所属事件循环内，业务处理路径无锁。网络层自研 Socket / Channel / Poller / EventLoop / Buffer / TimeWheel 等组件，应用层在其上实现 HTTP/1.1 解析、正则路由、静态资源服务与 Keep-Alive，并以 timerfd 驱动的时间轮完成非活跃连接超时释放。

## 整体架构

```
客户端 ──TCP──> Acceptor（Main Reactor 专职 accept，不碰业务）
                    │ 新连接轮询分配
                    ▼
        Sub Reactor × N ── one loop per thread，连接终身绑定单线程
                    │
                    │ 同一条 epoll_wait 统一等待三类事件:
                    │   网络 I/O(epoll LT) / timerfd 定时 / eventfd 跨线程唤醒
                    ▼
        Connection：recv 进 in_buffer ──> message_callback
                    ▲                            │
                    │ out_buffer                 ▼
                    │            HttpContext 三阶段状态机解析
                    │            Route：路由匹配 / 静态资源
                    │            HttpResponse 拼装响应
                    └── Send → EPOLLOUT → HandleWrite 写回对端
```

分层单向依赖：TcpServer 及以下是通用网络库，不含任何 HTTP 痕迹；HttpServer 作为使用者，只通过连接建立 / 消息到达 / 连接关闭 / 任意事件 4 个回调接入——改协议只需换这一层的使用者。

一条连接的一生：

1. **接入**：Main Reactor accept 出 newfd，轮询选中一个 Sub Reactor，连接对象在该线程 Established，挂上读事件
2. **收包**：读事件触发 recv（单次 ≤64KB），数据进 in_buffer，回调 HTTP 层
3. **解析**：HttpContext 状态机从 Buffer 逐步取行，攒齐完整请求；半包存着等、粘包循环消费
4. **处理**：Route 先查静态资源再查路由表（精确 / 正则），产出 HttpResponse
5. **回写**：Send 把数据拷入独立 string 后入队 out_buffer（调用方栈变量可能已销毁），挂 EPOLLOUT 等写事件发尽
6. **保活**：HTTP/1.1 默认长连接，连接留在 Sub Reactor 上等下一个请求；期间任意事件刷新时间轮倒计时
7. **释放**：对端关闭 / 出错 / 非活跃超时 → 先消费完缓冲区残余数据，再走二次投递延迟到下一轮事件循环析构，连接表由主线程单写者摘除

## 目录结构

```
MasterSubReactor/
└── src/
    │   ── 网络库层（TcpServer 及以下，与协议无关）──
    ├── Socket.h / Socket.cpp                 # fd 的 RAII 封装：非阻塞创建、收发、关闭
    ├── Channel.h / Channel.cpp               # 事件绑定体：fd + 关心事件 + 五类回调
    ├── Poller.h / Poller.cpp                 # epoll 封装：事件增删改查 + fd→Channel 反查
    ├── EventLoop.h / EventLoop.cpp           # 事件循环：epoll_wait / 任务队列 / eventfd / 时间轮
    ├── Acceptor.h / Acceptor.cpp             # 监听 socket 生命周期，Main Reactor 上的接收
    ├── Connection.h / Connection.cpp         # 连接对象：收发缓冲、四态状态机、优雅关闭
    ├── LoopThread.h / LoopThread.cpp         # EventLoop 线程：子线程内构造定归属
    ├── LoopThreadPool.h / LoopThreadPool.cpp # Sub Reactor 池：轮询分配，0 线程退化单 Reactor
    ├── Buffer.h / Buffer.cpp                 # 应用层缓冲：三段空间管理 + 按行提取
    ├── TimeWheel.h / TimeWheel.cpp           # timerfd 秒级时间轮：非活跃连接超时
    ├── TcpServer.h / TcpServer.cpp           # 组装者：Acceptor + 线程池 + 连接表
    ├── Log.h / Log.cpp                       # 分级日志：策略模式落盘
    │
    │   ── HTTP 层（HttpServer/，网络库的使用者）──
    ├── HttpServer/HttpServer.h / .cpp        # 路由分发：静态资源优先，精确 + 正则路由
    ├── HttpServer/HttpContext.h / .cpp       # 解析状态机：请求行 → 头部 → 正文
    ├── HttpServer/HttpRequest.h / .cpp       # 请求对象：头部 / query / 正文 / 正则捕获
    ├── HttpServer/HttpResponse.h / .cpp      # 响应对象：状态码 / 头 / 正文，报文拼装
    ├── HttpServer/Util.h / .cpp              # 工具集：编解码 / 状态码 / MIME 表 / 路径校验
    ├── HttpServer/HttpServerTest.cpp         # 服务器入口：注册路由并启动
    ├── HttpServer/HttpClientTest.cpp         # 并发一致性测试客户端
    └── HttpServer/webroot/index.html         # 静态资源示例
```

## 快速开始

依赖：Linux、g++（C++17）、pthread。

```bash
cd src/HttpServer
make              # 生成 Main（g++ -std=c++17 -g -Wall -lpthread）
./Main            # 监听 8080，静态资源取运行目录下 ./webroot
```

功能验证：

```bash
curl http://127.0.0.1:8080/hello                              # Hello, HttpServer!
curl -X POST -d "test-body-123" http://127.0.0.1:8080/echo    # body 原样回显
curl http://127.0.0.1:8080/user/12345                         # user id: 12345
curl "http://127.0.0.1:8080/query?k=abc"                      # k=abc
curl http://127.0.0.1:8080/                                   # ./webroot/index.html
```

业务接入只需四步：

```cpp
HttpServer server(8080);          // 端口 8080，非活跃超时默认 30s
server.SetThreadCount(2);         // 2 个 Sub Reactor（设 0 = 单 Reactor 模式）
server.SetWebroot("./webroot");   // 静态资源根目录（相对运行目录）
server.Get(R"(/user/(\d+))", [](const HttpRequest &req, HttpResponse *rsp){
    rsp->_statuscode = 200;
    rsp->_body = "user id: " + req._smatch[1].str();
    rsp->AddHeader("Content-Type", "text/plain; charset=UTF-8");
});
server.Start();                   // 阻塞在主事件循环
```

## 核心机制

### 1. 主从 Reactor 线程模型

Main Reactor 专职 accept，连接按轮询分给 Sub Reactor 池，one loop per thread——连接的全部状态（Buffer、Channel、解析上下文）单线程独占。跨线程操作经 RunInLoop 投递到数据所属线程执行；连接表只在主线程增删（单写者）。两级"逻辑锁"都转化为线程归属约束，业务路径零锁。线程数配 0 即退化为单 Reactor，同一套代码覆盖调试与压测两种形态。

### 2. 统一事件源

网络 I/O、timerfd 定时、eventfd 跨线程唤醒全部收敛到一条 epoll_wait 路径，循环内只处理"就绪 Channel → 分发回调"一件事。跨线程任务队列在互斥锁内整体 swap 换出、锁外批量执行，临界区只有一次指针交换。

### 3. Buffer 与 HTTP/1.1 解析

应用层 Buffer 用读写双指针划出三段空间（头部废弃区 / 可读区 / 尾部可写区），写入前自动整理或扩容——头部废弃区回收使长期运行的缓冲不增长。HTTP 解析以三阶段状态机（请求行 → 头部 → 正文）基于 Buffer 流式进行：半包存着等下一批数据，粘包循环消费，超长请求行 / 头部分别返回 414 / 431。路由支持精确与 std::regex 正则匹配（捕获组提取参数）、Query 参数解析；静态资源按 30 项后缀 MIME 表回包，路径层级校验防御目录穿越。

### 4. 连接生命周期

连接以四态状态机（CONNECTING / CONNECTED / DISCONNECTING / DISCONNECTED）管理，shared_ptr 全程托管、回调经 shared_from_this 传递杜绝异步悬空。关闭走优雅路径：先消费完接收缓冲存量、再等发送缓冲发尽。释放动作二次投递到下一轮事件循环，避免在回调调用栈深处析构活跃对象。非活跃超时由 timerfd 时间轮驱动：连接任意事件 O(1) 续期，任务对象"析构即执行"。

## 边界输入处理

- 头部值属于不可信输入：Content-Length 非数字按 0 处理，残余数据由状态机拒收，解析层不信任任何客户端声明
- 长短连接判定：显式 Connection 头优先，缺省按 RFC 7230 版本语义（1.1 持久连接，1.0 短连接）
- timerfd 读取按 errno 区分 EINTR / EAGAIN，信号中断与暂无数据均正确处理

## 测试

测试环境：VMware Ubuntu 24.04（4 vCPU / 4GB），g++ 13.3 / C++17，Sub Reactor ×2。

### 功能测试（curl 实测，8 项全过）

| #   | 用例                        | 结果                           |
| --- | ------------------------- | ---------------------------- |
| 1   | GET /hello                | 200 "Hello, HttpServer!"     |
| 2   | POST /echo                | body 原样回显                    |
| 3   | GET /user/12345           | 正则捕获 "user id: 12345"        |
| 4   | GET /query?k=abc          | "k=abc"                      |
| 5   | 请求不带 Connection 头         | 响应 keep-alive（1.1 默认长连接）     |
| 6   | 显式 Connection: close      | 响应 close                     |
| 7   | 非法 Content-Length 头（非数字值） | 400 拒收（非法值按 0 处理，残余数据由状态机拒收） |
| 8   | GET /nope / GET /         | 404 / 200 静态页                |

### 并发一致性

HttpClientTest：100 客户端进程 × 每连接 10 请求（前 9 个 keep-alive 复用连接，末尾显式 close），以 /echo 回显做逐字节一致性断言。结果：成功 100 / 失败 0。覆盖 Keep-Alive 连接复用、单连接连续多请求的粘包消费路径与多进程并发正确性。

复现：服务启动后执行 `make client && ./client`（测试客户端源码即 HttpClientTest.cpp，独立无项目内依赖，目标地址 127.0.0.1:8080，输出成功 / 失败进程统计）。

## 压测

wrk 4.1.0，回环压测，目标 GET /hello：

| 并发   | 命令                 | QPS   | 平均延迟    | P99 延迟  |
| ---- | ------------------ | ----- | ------- | ------- |
| 100  | `-t4 -c100 -d15s`  | 50231 | 1.95ms  | 3.37ms  |
| 1000 | `-t2 -c1000 -d15s` | 48970 | 20.23ms | 29.68ms |

100 → 1000 并发 QPS 持平（处理能力饱和），平均延迟 1.95ms → 20.23ms 符合 Little's law（1000 ÷ 49000 ≈ 20ms，即排队等待）。回环无网卡开销，数字仅在同等条件下可复现比较。

## 设计取舍与改进方向

| 决策                  | 取舍          | 改进方向                                                |
| ------------------- | ----------- | ------------------------------------------------------------------------------- |
| epoll LT + 单次 recv  | 实现简单，正确性有保证 | ET 模式 + 循环读到 EAGAIN（nginx 的事件处理方式），减少重复唤醒；更远期可关注 io_uring（Linux 5.1+ 异步 I/O 接口） |
| Send 先入缓冲挂 EPOLLOUT | 时序简单确定      | 先尝试直写、EAGAIN 再挂写事件（muduo 的做法），少一轮事件循环延迟                                         |
| 静态资源 ReadFile 全量读内存 | 实现简单通用      | sendfile / mmap 零拷贝发送（nginx 静态资源标准方案）                                           |
| 日志每条打开关闭一次文件        | 实现简单        | 常驻 fd + 异步日志（muduo AsyncLogging 的前后端双缓冲交换设计）                                    |
| 轮询分配连接              | O(1) 无统计开销  | SO_REUSEPORT 内核级多 listener 负载均衡（现代 Linux 服务端标准做法）                               |
