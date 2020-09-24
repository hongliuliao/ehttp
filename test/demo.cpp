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
