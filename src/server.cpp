#include "server.h"
#include "http.h"
#include "log.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cstring>
#include <sstream>
#include <errno.h>
#include <iostream>

#include <filesystem>
#include <format>
#include <fstream>

#define MAX_EVENTS 128

static void set_nonblock(int fd)
{
    int f = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, f | O_NONBLOCK);
}

http::response handler(http::header&& header)
{
    static std::filesystem::path root = "/home/elena/networks/course/src/static";
    if (header.path == "/")
        header.path = "/index.html";
    std::string filename = std::format("{}{}", root.string(), header.path);
    if (!std::filesystem::exists(filename))
        return http::not_found_404;

    std::ifstream fin(filename, std::ios_base::in);
    if (!fin.is_open())
        return http::not_found_404;

    if (header.path.find("..") != std::string::npos)
        return http::forbidden_403;


    fin.seekg(0, std::ios::end);
    size_t file_size = fin.tellg();
    fin.seekg(0, std::ios::beg);

    std::string body;
    body.resize(file_size);
    fin.read(body.data(), file_size);

    http::response response;
    response.code = "200 OK";
    std::string ext;
    auto pos = header.path.rfind('.');
    if (pos != std::string::npos)
        ext = header.path.substr(pos + 1);

    if (ext == "html")
        response.content_type = "text/html; charset=utf-8";
    else if (ext == "css")
        response.content_type = "text/css";
    else
        response.content_type = "text/plain";

    response.connection = "close";
    response.content_length = body.length();
    response.body = std::move(body);

    return response;
}

void run_server(int port, const std::string &root, size_t threads)
{
    ThreadPool pool(threads);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    // set_nonblock(s);

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(s, (sockaddr*)&addr, sizeof(addr));

    listen(s, 512);

    while(true)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int connfd = accept(s, (struct sockaddr *) &client_addr, &client_len);
        if (connfd > 0)
        {
            std::cout << "new connection: " << connfd << std::endl;
            pool.add_task(connfd, task_t{ .handler = &handler, .state = task_t::states::read });
        }
    }
}
