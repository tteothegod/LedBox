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

#include "ledbox_utils.hpp"

using namespace std;

int main() {
    return run_ledSyncVideo();
}
