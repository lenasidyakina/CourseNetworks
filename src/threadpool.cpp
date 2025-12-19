#include "threadpool.h"
#include "log.h"
#include <errno.h>
#include <format>
#include <iostream>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>

ThreadPool::ThreadPool(size_t n) : n_threads(n)
{
    tasks = std::vector<std::unordered_map<int, task_t>>(n);
    epoll_fds = std::vector<int>(n);
    mtx = std::vector<std::mutex>(n);
    for (size_t i = 0; i < n;i++) {
        epoll_fds[i] = epoll_create1(0);
        m_threads.emplace_back(&ThreadPool::run, this, i);
    }
}

void ThreadPool::add_task(int fd, task_t&& task) {
    if (task.handler == nullptr)
    {
        close(fd);
        return;
    }
    int f = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, f | O_NONBLOCK);

    size_t thread_id = cur % n_threads;
    {
        std::unique_lock<std::mutex> lock{ mtx[thread_id] };
        tasks[thread_id][fd] = std::move(task);
    }
    struct epoll_event ev {};
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    ev.data.fd = fd;
    epoll_ctl(epoll_fds[thread_id], EPOLL_CTL_ADD, fd, &ev);

    std::cout << std::format("Added task to thread {}, fd={}\n", thread_id, fd);
    cur++;
}

void ThreadPool::read_file(int fd, task_t& task){
    char buf[4096];
    ssize_t bytes = recv(fd, buf, sizeof(buf) - 1, 0);
    buf[bytes] = '\0';


    if (bytes > 0)
    {
        task.data.append(buf, bytes);
        if (task.data.find("\r\n\r\n") != std::string::npos)
            task.state = task_t::states::received;
    }
    else if (bytes == 0)
    {
        task.state = task_t::states::error;
        return;
    }
    else
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            task.state = task_t::states::error;
        }
    }
}

void ThreadPool::write_file(int fd, task_t& task){

    ssize_t bytes = send(fd, task.data.data() + task.bytes_written,
                         task.data.length() - task.bytes_written, 0);
    task.state = task_t::states::send;
    if (bytes == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        task.state = task_t::states::error;
        return;
    }
    task.bytes_written += bytes;
    if (task.bytes_written == task.data.length())
        task.state = task_t::states::done;
}

void ThreadPool::run(size_t thread_id)
{
    int ep_fd = epoll_fds[thread_id];
    epoll_event events[512];
    while(true)
    {
        int n = epoll_wait(ep_fd, events, 512, -1);
        if (n < 1)
            continue;
        std::cout << std::format("Thread id: {}, nfds: {}\n", thread_id, n);
        for(int i=0; i<n; i++) {
            int fd = events[i].data.fd;
            {
                std::unique_lock<std::mutex> lock{ mtx[thread_id] };
                auto it = tasks[thread_id].find(fd);

                if (it == tasks[thread_id].end()) {
                    continue;
                }
            }
            task_t& task = tasks[thread_id][fd];
            if (task.state == task_t::states::read)
            {
                std::cout << std::format("Read from {}\n", fd);
                if (events[i].events & EPOLLIN){
                    read_file(fd, task);
                }

            }
            if (task.state == task_t::states::received)
            {
                std::cout << std::format("Received from {}\n", fd);
                http::header header = http::parse_header(task.data);
                http::response res = task.handler(std::move(header));
                task.data.clear();
                task.data = std::format("HTTP/1.1 {}\r\nContent-Type: {}\r\nContent-Length: {}\r\nConnection: {}\r\n\r\n{}",
                                        res.code,
                                        res.content_type,
                                        res.content_length,
                                        res.connection,
                                        res.body
                );
                task.state = task_t::states::send;
            }
            if (task.state == task_t::states::send)
            {
                std::cout << std::format("Write to: {}\n", fd);
                if (events[i].events & EPOLLOUT){
                    write_file(fd, task);
                }
            }
            if (task.state == task_t::states::done)
            {
                std::cout << std::format("Done with: {}\n", fd);
                epoll_ctl(ep_fd, EPOLL_CTL_DEL, fd, nullptr);
                close(fd);
                std::unique_lock<std::mutex> lock{mtx[thread_id]};
                tasks[thread_id].erase(fd);
                log_msg("done");
            }
            if (task.state == task_t::states::error)
            {
                close(fd);
                log_msg("error");
            }
        }
    }
}