/* 服务器入口示例：构造、线程数、webroot、路由注册与启动 */
#include "HttpServer.h"

#define WEBROOT "./webroot"     // 静态资源根目录（相对运行目录）

int main()
{
    std::shared_ptr<HttpServer> httpsvr = std::make_shared<HttpServer>(8080);   // 端口 8080，超时默认 30s
    httpsvr->SetThreadCount(2);     // 2 个 Sub Reactor 线程（设 0 则退化为单 Reactor 模式）
    httpsvr->SetWebroot(WEBROOT);   // 访问 / 时返回 ./webroot/index.html

    // 注册业务处理函数
    httpsvr->Get("/hello", [](const HttpRequest &req, HttpResponse *rsp){   // 精确路由 GET /hello
        rsp->_statuscode = 200;
        rsp->_body = "Hello, HttpServer!";
        rsp->AddHeader("Content-Type", "text/plain; charset=UTF-8");
    });
    // 回显路由：body 原样返回，供 HttpClientTest 做一致性校验
    httpsvr->Post("/echo", [](const HttpRequest &req, HttpResponse *rsp){
        rsp->_statuscode = 200;
        rsp->_body = req._body;
        rsp->AddHeader("Content-Type", "text/plain; charset=UTF-8");
    });
    // 正则路由：捕获组取参数
    httpsvr->Get(R"(/user/(\d+))", [](const HttpRequest &req, HttpResponse *rsp){
        rsp->_statuscode = 200;
        rsp->_body = "user id: " + req._smatch[1].str();
        rsp->AddHeader("Content-Type", "text/plain; charset=UTF-8");
    });
    // query 参数示例
    httpsvr->Get("/query", [](const HttpRequest &req, HttpResponse *rsp){
        rsp->_statuscode = 200;
        rsp->_body = "k=" + (req.HasParam("k") ? req.GetParam("k") : std::string("none"));
        rsp->AddHeader("Content-Type", "text/plain; charset=UTF-8");
    });
    httpsvr->Start();               // 阻塞在事件主循环
    return 0;                       // 实际不可达
}
