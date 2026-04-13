#include "ledbox_utils.hpp"

using namespace std;

// Common
constexpr char* SYNC_VIDEO_STATUS_PATH = "/run/ledbox/ledSyncVideo_status.json";
constexpr char* CALIBRATE_STATUS_PATH = "/run/ledbox/calibrate_status.json";

//// Calibrate ////
constexpr int GPIO_LEFT_BTN = 23;
constexpr int GPIO_ACTION_BTN = 24;
constexpr int GPIO_RIGHT_BTN = 25;
constexpr char* CALIBRATE_PROGRAM_PATH = "./calibrate";

//// ledSyncVideo Launcher ////
constexpr char* SYNC_VIDEO_PROGRAM_PATH = "./ledSyncVideo";
constexpr char* AUDIO_DEVICE = "plughw:2,0";
constexpr char* RAM_DISK_FILE = "/dev/shm/hdmi_test.wav";
// arecord -D {AUDIO_DEVICE} -d 1 -f S16_LE -r 48000 {RAM_DISK_FILE} 2>/dev/null && sox {RAM_DISK_FILE} -n stat 2>&1
constexpr float RMS_THRESHOLD = 0.010;

int forkFunction(const std::function<int()>& func) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process: run provided function and exit with its return code.
        int rc = 1;
        try {
            rc = func();
        } catch (const std::exception& e) {
            cerr << "Error: exception in child function: " << e.what() << endl;
        } catch (...) {
            cerr << "Error: unknown exception in child function." << endl;
        }
        _exit(rc);
    } else if (pid < 0) {
        cerr << "Error: Failed to fork process." << endl;
        return -1;
    }
    return pid; // Parent process returns child's pid
}

// @brief Forks a new process and executes the specified program
// @return pid of the child process, or -1 on failure
int forkExecvp(const char *path, char *const argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execvp(path, argv);
        cerr << "Error: Failed to execute " << path << endl;
        exit(1);
    } else if (pid < 0) {
        cerr << "Error: Failed to fork process." << endl;
        return -1;
    }
    return pid; // Parent process returns child's pid
}

int writeJsonToFile(const char* path, const Json::Value& value) {
    if (path == nullptr || path[0] == '\0') {
        cerr << "writeJsonToFile: invalid file path." << endl;
        return JsonStatus::InvalidPath;
    }

        Json::StreamWriterBuilder writer;
    string jsonString = Json::writeString(writer, value);

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        cerr << "writeJsonToFile: failed to open " << path << endl;
        return JsonStatus::OpenFailed;
    }

    ssize_t bytesWritten = write(fd, jsonString.c_str(), jsonString.size());
    if (bytesWritten == -1) {
        cerr << "writeJsonToFile: failed to write to " << path << endl;
        close(fd);
        return JsonStatus::WriteFailed;
    }

    close(fd);
    return 0;
}

int readJsonFromFile(const char* path, Json::Value& returnValue) {
    if (path == nullptr || path[0] == '\0') {
        cerr << "readJsonFromFile: invalid file path." << endl;
        return JsonStatus::InvalidPath;
    }

    const int fd = open(path, O_RDONLY);
    if (fd == -1) {
        cerr << "readJsonFromFile: failed to open " << path << endl;
        return JsonStatus::OpenFailed;
    }

    string content;
    char buffer[512];
    while (true) {
        const ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
        if (bytesRead > 0) {
            content.append(buffer, static_cast<size_t>(bytesRead));
            continue;
        }
        if (bytesRead == 0) {
            break; // EOF
        }

        cerr << "readJsonFromFile: failed to read from " << path << endl;
        close(fd);
        return JsonStatus::ReadFailed;
    }

    close(fd);

    if (content.empty()) {
        cerr << "readJsonFromFile: file is empty: " << path << endl;
        return JsonStatus::EmptyFile;
    }

    Json::CharReaderBuilder reader;
    Json::Value parsedJson;
    string errs;
    istringstream s(content);

    if (!Json::parseFromStream(reader, s, &parsedJson, &errs)) {
        cerr << "readJsonFromFile: JSON parse error in " << path << ": " << errs << endl;
        return JsonStatus::ParseFailed;
    }

    returnValue = parsedJson;
    return JsonStatus::Ok;
}

// Refactored launcher helpers + two thin wrappers
static int writeStatusFile(const char* path, const Json::Value& value) {
    return writeJsonToFile(path, value);
}

// @returns value on success, or negative error code on failure
float queryRMS() {
    // Record 1s of audio to RAM disk and run sox stat to get RMS, return the RMS amplitude
    std::string cmd = std::string("arecord -D ") + AUDIO_DEVICE +
                      " -d 1 -f S16_LE -r 48000 " + RAM_DISK_FILE +
                      " 2>/dev/null && sox " + RAM_DISK_FILE + " -n stat 2>&1";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        cerr << "queryRMS: popen failed." << endl;
        return -1.0f;
    }

    std::string output;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        output.append(buf);
    }
    int rc = pclose(pipe);
    (void)rc; // unused but keep for possible future checks

    if (output.empty()) {
        cerr << "queryRMS: no output from command." << endl;
        return -2.0f;
    }

    // Look for a line containing "RMS" and "amplitude" and take the last token as the numeric value
    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.find("RMS") != std::string::npos && line.find("amplitude") != std::string::npos) {
            std::istringstream ls(line);
            std::string token, last;
            while (ls >> token) last = token;
            // strip trailing ':' if present
            if (!last.empty() && last.back() == ':') last.pop_back();
            char* endptr = nullptr;
            float val = strtof(last.c_str(), &endptr);
            if (endptr != nullptr && *endptr == '\0') {
                return val;
            }
        }
    }

    cerr << "queryRMS: failed to parse RMS from output." << endl;
    return -3.0f;
}

static void spawnWaitAndFinalize(int pid, const char* statusPath) {
    std::thread([pid, statusPath]() {
        int wstatus = 0;
        pid_t r = waitpid(pid, &wstatus, 0);
        Json::Value endStatus;
        endStatus["running"] = false;
        endStatus["pid"] = pid;
        {
            time_t t = std::time(nullptr);
            char buf[64];
            std::strftime(buf, sizeof(buf), "%FT%TZ", std::gmtime(&t));
            endStatus["lastStart"] = buf;
        }
        if (r == pid) {
            if (WIFEXITED(wstatus)) {
                endStatus["exitCode"] = WEXITSTATUS(wstatus);
                endStatus["message"] = "exited";
            } else if (WIFSIGNALED(wstatus)) {
                endStatus["signal"] = WTERMSIG(wstatus);
                endStatus["message"] = "signaled";
            } else {
                endStatus["message"] = "stopped";
            }
        } else {
            endStatus["message"] = "waitpid_failed";
        }
        writeStatusFile(statusPath, endStatus);
    }).detach();
}

int kill_program(const char* statusPath, int pid = -1) {
    if ( pid > 0) {
        cout << "Killing program with pid " << pid << "." << endl;
        kill(pid, SIGTERM);
        return 0;
    }

    Json::Value status;
    int res = readJsonFromFile(statusPath, status);
    if (res != 0) {
        cerr << "kill_program: failed to read status file." << endl;
        return -1;
    }
    pid = (pid != -1) ? pid : status.get("pid", -1).asInt();
    if (pid > 0) {
        cout << statusPath << ": Killing program with pid " << pid << "." << endl;
        kill(pid, SIGTERM);
        return 0;
    } else {
        cout << statusPath << ": Program is not running." << endl;
        return 0;
    }
}

static int runProgram(const char* programPath, char *const argv[], const char* statusPath, const char* otherStatusPath, bool abortIfOtherrunning, bool killOtherIfrunning) {
    // Return codes:
    //  0  = OK, launched successfully
    //  1  = Skipped: already running (own status)
    //  2  = Skipped: other program running and abortIfOtherrunning is true
    // -1  = Fork/exec failed
    cout << "Launching " << programPath << "..." << endl;

    Json::Value myStatus;
    Json::Value otherStatus;
    int resMy = readJsonFromFile(statusPath, myStatus);
    int resOther = otherStatusPath ? readJsonFromFile(otherStatusPath, otherStatus) : 0;

    if (resOther != 0 && otherStatusPath) {
        cerr << "Warning: could not read other status (" << resOther << ")." << endl;
        // don't treat as fatal; proceed
    } else if (otherStatusPath) {
        bool otherrunning = otherStatus.get("running", false).asBool();
        if (otherrunning) {
            if (abortIfOtherrunning) {
                cout << "Other program is running. Skipping launch of " << programPath << "." << endl;
                return 2; // skipped due to other running
            }
            if (killOtherIfrunning) {
                int otherPid = otherStatus.get("pid", -1).asInt();
                if (otherPid > 0) {
                    cout << "Killing other program pid " << otherPid << " to start " << programPath << "." << endl;
                    kill_program(otherStatusPath, otherPid);
                }
            }
        }
    }

    if (resMy == 0) {
        bool alreadyrunning = myStatus.get("running", false).asBool();
        if (alreadyrunning) {
            cout << programPath << " is already running. Skipping launch." << endl;
            return 1; // skipped because already running
        }
    } else {
        // Could not read own status file; report but continue trying to launch.
        cerr << "Warning: could not read own status (" << resMy << "). Continuing launch attempt." << endl;
    }

    int pid = forkExecvp(programPath, argv);
    if (pid == -1) {
        cerr << "Failed to launch " << programPath << " (fork/exec failed)." << endl;
        return -1;
    }
    spawnWaitAndFinalize(pid, statusPath);


    Json::Value newStatus;
    newStatus["running"] = true;
    newStatus["pid"] = pid;
    {
        time_t t = std::time(nullptr);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%FT%TZ", std::gmtime(&t));
        newStatus["lastStart"] = buf;
    }
    newStatus["message"] = "running";
    if (writeStatusFile(statusPath, newStatus) != 0) {
        cerr << "Warning: failed to write status file after launching " << programPath << "." << endl;
        // Not fatal for launch; proceed.
    }

    cout << programPath << " launched with pid: " << pid << endl;
    return 0;
}

int run_ledSyncVideo() {
    // WiringPi setup
    cout << "Starting Master LedBox Program..." << endl;
    if (wiringPiSetupGpio() == -1) {
        cerr << "Error: Failed to setup wiringPi." << endl;
        return 1;
    }
    
    mkdir("/run/ledbox", 0755); // ensure status directory exists; ignore error if it already exists

    Json::Value resetStatus;
    resetStatus["running"] = false;
    resetStatus["pid"] = Json::Value::null;
    {
        time_t t = std::time(nullptr);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%FT%TZ", std::gmtime(&t));
        resetStatus["lastStart"] = buf;
    }
    resetStatus["message"] = "reset";
    writeStatusFile(SYNC_VIDEO_STATUS_PATH, resetStatus);
    writeStatusFile(CALIBRATE_STATUS_PATH, resetStatus);

    cout << "Reset status files. Entering main loop." << endl;

    pinMode(GPIO_ACTION_BTN, INPUT);
    pullUpDnControl(GPIO_ACTION_BTN, PUD_UP);

    int prevActionState = HIGH;
    float prevRMS = 0.0f;
    while (true) {
        int actionBtnState = digitalRead(GPIO_ACTION_BTN);
        cout << "Action Button State: " << (actionBtnState == LOW ? "LOW" : "HIGH") << endl;
        if (actionBtnState == LOW && prevActionState == HIGH) {
            cout << "Main: Action button pressed. Launching calibrate program..." << endl;
            run_calibrate();
            // Wait for button release to debounce and prevent retriggering
            while (digitalRead(GPIO_ACTION_BTN) == LOW) {
                delay(50);
            }
        }
        prevActionState = actionBtnState;

        float rms = queryRMS();
        if (rms >= 0.0f) {
            cout << "Current RMS Amplitude: " << rms << endl;

            // Ensure we check the current ledSyncVideo status so we can relaunch after calibrate exits
            Json::Value syncStatus;
            bool syncrunning = false;
            int resSync = readJsonFromFile(SYNC_VIDEO_STATUS_PATH, syncStatus);
            if (resSync == 0) {
                syncrunning = syncStatus.get("running", false).asBool();
            }

            if ((rms >= RMS_THRESHOLD && prevRMS < RMS_THRESHOLD) ||
                (rms >= RMS_THRESHOLD && !syncrunning)) {
                cout << "HDMI signal detected. Launching ledSyncVideo program..." << endl;
                static char* argv[] = {const_cast<char*>(SYNC_VIDEO_PROGRAM_PATH), "--save-picture", nullptr};

                int rc = runProgram(SYNC_VIDEO_PROGRAM_PATH, argv,
                                SYNC_VIDEO_STATUS_PATH,
                                CALIBRATE_STATUS_PATH,
                                true,   // abortIfOtherrunning (calibrate)
                                false); // killOtherIfrunning
                if (rc != 0) {
                    cerr << "Failed to launch ledSyncVideo (code " << rc << ")." << endl;
                }
            } else if (rms < RMS_THRESHOLD && prevRMS >= RMS_THRESHOLD) {
                cout << "HDMI signal lost. Killing ledSyncVideo program..." << endl;
                int rc = kill_program(SYNC_VIDEO_STATUS_PATH);
                if (rc != 0) {
                    cerr << "Failed to kill ledSyncVideo (code " << rc << ")." << endl;
                }
            }
        } else {
            cerr << "Failed to get RMS amplitude." << endl;
        }

        prevRMS = rms;
        delay(20); // Poll every 20ms
    }
    return 0;
}

int run_calibrate() {
    // For calibrate: if ledSyncVideo is running, attempt to kill it.
    static char* argv[] = {const_cast<char*>(CALIBRATE_PROGRAM_PATH), nullptr};
    return runProgram(CALIBRATE_PROGRAM_PATH, argv,
                      CALIBRATE_STATUS_PATH,
                      SYNC_VIDEO_STATUS_PATH,
                      false,  // abortIfOtherrunning
                      true);  // killOtherIfrunning
}