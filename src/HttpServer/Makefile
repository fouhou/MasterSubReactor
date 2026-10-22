# 服务器构建脚本：make 生成 Main，make client 生成并发测试客户端
Main:HttpServerTest.cpp Util.cpp HttpServer.cpp HttpContext.cpp HttpRequest.cpp HttpResponse.cpp  ../Socket.cpp ../Log.cpp ../Channel.cpp ../Poller.cpp ../EventLoop.cpp ../Acceptor.cpp ../Connection.cpp ../Buffer.cpp ../TimeWheel.cpp ../LoopThread.cpp ../LoopThreadPool.cpp ../TcpServer.cpp
	g++ -o $@ $^ -std=c++17 -g -Wall -lpthread

# 并发一致性测试客户端：独立 socket 程序，无项目内依赖，目标 127.0.0.1:8080
client:HttpClientTest.cpp
	g++ -o $@ $^ -std=c++17 -g -Wall -lpthread

.PHONY:clean
clean:
	rm -f Main client
