
#ifndef COURSE_SERVER_H
#define COURSE_SERVER_H

#include <string>
#include "threadpool.h"

void run_server(int port, const std::string &root, size_t threads);

#endif //COURSE_SERVER_H
