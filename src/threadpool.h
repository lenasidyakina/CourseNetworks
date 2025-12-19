#ifndef COURSE_THREADPOOL_H
#define COURSE_THREADPOOL_H

#include <thread>
#include <vector>
#include <functional>
#include <mutex>
#include "http.h"

using handler_f = http::response(*)(http::header&&);

struct task_t
{
    enum class states
    {
        read,
        received,
        send,
        done,
        error
    };

    handler_f handler;
    std::string data;
    std::size_t bytes_written;
    states state;
};

class ThreadPool {
public:
    ThreadPool(size_t n);
    void add_task(int fd, task_t&& task);
private:
    void run(size_t thread_id);
    void read_file(int fd, task_t& task);
    void write_file(int fd, task_t& task);
private:
    std::vector<int> epoll_fds;
    int cur = 0;
    size_t n_threads;

    std::vector<std::unordered_map<int, task_t>> tasks;
    std::vector<std::jthread> m_threads;

    std::vector<std::mutex> mtx;
};

#endif //COURSE_THREADPOOL_H
