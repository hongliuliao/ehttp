simple_server
=============
此组件是为了使用c++方便快速的构建http server,编写基于http协议json格式的接口,和nginx等传统服务器相比,更加重视开发的便捷性,项目参考[restbed](https://bitbucket.org/Corvusoft/restbed/overview) 实现
## 特点
* linux 2.6 +
* 单进程 + 单线程 + epoll
* g++3.4 +
* 强调简洁实用

## 依赖
 * [simple_log](https://github.com/hongliuliao/simple_log) 日志组件
 * [jsoncpp](https://github.com/open-source-parsers/jsoncpp) json序列化组件

## 性能
 * qps 12000+ (短连接 ab -c 10 -n 10000 localhost:3490/hello)
 * qps 16000+ (长连接 ab -c 10 -n 10000 -k localhost:3490/hello)

## 构建 && 测试
```
 make && make test && ./bin/http_server_test 
 curl "localhost:3490/hello"
```

## 功能列表
  * http 1.0/1.1(keep-alive 支持) GET/POST请求
  * 便捷的开发形式
  * Json格式的数据返回

## 例子
```c++
#include <sstream>
#include <cstdlib>
#include "simple_log.h"
#include "http_server.h"

void hello(Request &request, Response &response) {
    Json::Value root;
    root["hello"] = "world";
    response.set_body(root);
}

void sayhello(Request &request, Response &response) {
    std::string name = request.get_param("name");
    std::string age = request.get_param("age");

    Json::Value root;
    root["name"] = name;
    root["age"] = atoi(age.c_str());
    response.set_body(root);
}

void login(Request &request, Response &response) {
    std::string name = request.get_param("name");
    std::string pwd = request.get_param("pwd");

    LOG_DEBUG("login user which name:%s, pwd:%s", name.c_str(), pwd.c_str());
    Json::Value root;
    root["code"] = 0;
    root["msg"] = "login success!";
    response.set_body(root);
}

int main() {
	HttpServer http_server;

	http_server.add_mapping("/hello", hello);
	http_server.add_mapping("/sayhello", sayhello);
	http_server.add_mapping("/login", login, POST_METHOD);

	http_server.start(3490);
	return 0;
}


```

## 运行
```
liao@ubuntu:~/workspace/simple_server$ curl "localhost:3490/login" -d "name=tom&pwd=3"
{"code":0,"msg":"login success!"}

```

## TODO LIST
  * ~~支持POST方法~~
  * ~~支持JSON数据返回~~
  * 支持Path parameter
  * ~~增加一个proxy来处理高并发~~ 使用epoll来实现,2014-11-6
  * ~~解决CPU导致的瓶颈,提高qps~~ 

