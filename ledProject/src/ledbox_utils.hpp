#ifndef LEDBOX_UTILS_HPP
#define LEDBOX_UTILS_HPP
#include <iostream>

#include <iostream>

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <wiringPi.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <ctime>
#include <json/json.h>
#include <functional>

int run_ledSyncVideo();
int run_calibrate();

int forkFunction(const std::function<int()>& func);

int forkExecvp(const char *path, char *const argv[]);

namespace JsonStatus {
    constexpr int Ok                = 0;
    constexpr int InvalidPath       = -1;
    constexpr int OpenFailed        = -2;
    constexpr int ReadFailed        = -3;
    constexpr int EmptyFile         = -4;
    constexpr int ParseFailed       = -5;
    constexpr int WriteFailed       = -6;
}




#endif // LEDBOX_UTILS_HPP

