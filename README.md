# ehttp 
[![Build Status](https://travis-ci.org/hongliuliao/ehttp.svg?branch=master)](https://travis-ci.org/hongliuliao/ehttp)
[![codecov.io](http://codecov.io/github/hongliuliao/ehttp/coverage.svg?branch=master)](http://codecov.io/github/hongliuliao/ehttp?branch=master)

此库是为了使用c++简洁高效的构建http (json数据交互格式) 微服务

## Feature
* Base on linux epoll
* Multi-thread model

## Performance (without log print)
 * qps 12000+ (短连接 [ab](https://github.com/CloudFundoo/ApacheBench-ab) -c 10 -n 10000 localhost:3456/hello)
 * qps 18000+ (长连接 [ab](https://github.com/CloudFundoo/ApacheBench-ab) -c 10 -n 10000 -k localhost:3456/hello)

## Build && Test
```
 make && make test && ./output/bin/http_server_test 3456
 curl "localhost:3456/hello"
```

## Function List
  * http 1.0/1.1(keep-alive 支持) GET/POST请求
  * Json格式的数据返回

## Example
```c++
#include <sstream>
#include <cstdlib>
#include "simple_log.h"
#include "http_server.h"

// Make sure the callback method is threadsafe
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

## Run
```
liao@ubuntu:~/workspace/ehttp$ curl -d "name=tom&pwd=3" "localhost:3456/login"
{"code":0,"msg":"login success!"}
```

