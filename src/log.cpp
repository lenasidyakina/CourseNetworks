#include "log.h"
#include <fstream>
#include <ctime>

void log_msg(const std::string &msg){
    std::ofstream f("server.log", std::ios::app);

    time_t t = time(NULL);
    f << std::ctime(&t) << ": " << msg << "\n";
}
