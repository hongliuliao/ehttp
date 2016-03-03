ehttp
=============
此组件是为了使用c++方便快速的构建http server,编写基于http协议json格式的接口,和nginx等传统服务器相比,更加重视开发的便捷性,项目参考[restbed](https://bitbucket.org/Corvusoft/restbed/overview) 实现

## 特点
* base on epoll
* 强调简洁实用
* 多线程模型

## 依赖
 * [jsoncpp](https://github.com/open-source-parsers/jsoncpp) v0.6.0 json序列化组件
 * [http-parser](https://github.com/nodejs/http-parser) For parse http requset 

## 性能(without log print)
 * qps 12000+ (短连接 ab -c 10 -n 10000 localhost:3456/hello)
 * qps 18000+ (长连接 ab -c 10 -n 10000 -k localhost:3456/hello)

## 构建 && 测试
```
 make && make test && ./output/bin/http_server_test 3456
 curl "localhost:3456/hello"
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

void login(Request &request, Json::Value &root) {
    std::string name = request.get_param("name");
    std::string pwd = request.get_param("pwd");

    LOG_DEBUG("login user which name:%s, pwd:%s", name.c_str(), pwd.c_str());
    
    root["code"] = 0;
    root["msg"] = "login success!";
}

int main() {
    HttpServer http_server;
    
    http_server.add_mapping("/login", login, POST_METHOD);

    http_server.set_port(3456);
    http_server.start_sync();
    return 0;
}


```

## 运行
```
liao@ubuntu:~/workspace/ehttp$ curl -d "name=tom&pwd=3" "localhost:3456/login"
{"code":0,"msg":"login success!"}

```

