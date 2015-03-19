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
 make && make test && ./bin/http_server_test 3490
 curl "localhost:3490/hello"
```

## 功能列表
  * http 1.0/1.1(keep-alive 支持) GET请求
  * 便捷的开发形式
  * Json格式的数据返回

## 例子
```c++
#include <sstream>
#include <cstdlib>
#include "simple_log.h"
#include "http_server.h"

void login(Request &request, Json::Value &root) {
    std::string name = request.get_param("name");
    std::string pwd = request.get_param("pwd");

    LOG_DEBUG("login user which name:%s, pwd:%s", name.c_str(), pwd.c_str());
    
    root["code"] = 0;
    root["msg"] = "login success!";
}

int main() {
	HttpServer http_server;

	http_server.add_mapping("/login", login);

	http_server.start(3490);
	return 0;
}


```

## 运行
```
liao@ubuntu:~/workspace/simple_server$ curl "localhost:3490/login?name=tom&pwd=3"
{"code":0,"msg":"login success!"}

```

## TODO LIST
  * ~~支持JSON数据返回~~
  * 支持Path parameter
  * ~~增加一个proxy来处理高并发~~ 使用epoll来实现,2014-11-6
  * ~~解决CPU导致的瓶颈,提高qps~~ 

