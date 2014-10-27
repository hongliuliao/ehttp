simple_server
=============
此组件是为了使用c++方便快速的构建http server,编写基于http的api,和nginx等传统服务器相比,更加重视开发的便捷性,项目参考[restbed](https://bitbucket.org/Corvusoft/restbed/overview) 实现
## 特点
* 只支持linux
* 单进程+单线程
* g++3.4 以上编译器都支持

## 依赖
 * [simple_log](https://github.com/hongliuliao/simple_log)组件

## 性能
 * qps 3000 (vmware虚拟机上ubuntu系统测试结果)

## 构建 && 测试
```
 make && make test && ./bin/http_server_test 
 curl "localhost:3490/hello"
```

## 功能列表
  * http 1.0/1.1 GET/POST请求
  * 便捷的开发形式

## 例子
```c++
#include <sstream>
#include "simple_log.h"
#include "http_server.h"

Response hello(Request &request) {
	return Response(STATUS_OK, "hello world! \n");
}

Response sayhello(Request &request) {
	LOG_DEBUG("start process request...");

	std::string name = request.get_param("name");
	std::string age = request.get_param("age");

	std::stringstream ss;
	ss << "hello " << name << ", age:" + age << "\n";

	return Response(STATUS_OK, ss.str());
}

int main() {
	HttpServer http_server;

	http_server.add_mapping("/hello", hello);
	http_server.add_mapping("/sayhello", sayhello);

	http_server.start(3490);
	return 0;
}

```

## 编译
```
g++ -I dependency/simple_log/include/ -I bin/include test/http_server_test.cpp dependency/simple_log/lib/libsimplelog.a bin/lib/libsimpleserver.a -o bin/http_server_test
```

## 运行
```
liao@ubuntu:~/workspace/simple_server$ curl "localhost:3490/hello"
hello world! 
liao@ubuntu:~/workspace/simple_server$ curl "localhost:3490/sayhello?name=tom&age=3"
hello tom, age:3
```

## TODO LIST
  * ~~支持POST方法~~
  * 支持JSON数据返回
  * 支持Path parameter

