#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <microhttpd.h>
#include <json/json.h>

#include "ledbox_utils.hpp"

#define PORT 3000

// Struct to hold state for each connection (for POST data accumulation)
struct ConnectionInfo {
    std::string postData;
};

// Helper: Add CORS headers
void add_cors_headers(struct MHD_Response *response) {
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type");
}

// Helper: Send quick JSON response
MHD_Result send_json(struct MHD_Connection *connection, int status_code, const Json::Value& json) {
    Json::StreamWriterBuilder writer;
    writer["indentation"] = ""; // compact output
    std::string out = Json::writeString(writer, json);
    struct MHD_Response *response = MHD_create_response_from_buffer(out.size(), (void*)out.c_str(), MHD_RESPMEM_MUST_COPY);
    add_cors_headers(response);
    MHD_add_response_header(response, "Content-Type", "application/json");
    MHD_Result ret = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);
    return ret;
}

// Helper: Send plain text response
MHD_Result send_text(struct MHD_Connection *connection, int status_code, const std::string& text) {
    struct MHD_Response *response = MHD_create_response_from_buffer(text.size(), (void*)text.c_str(), MHD_RESPMEM_MUST_COPY);
    add_cors_headers(response);
    MHD_add_response_header(response, "Content-Type", "text/plain");
    MHD_Result ret = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);
    return ret; 
}

// Main connection handler
static MHD_Result answer_to_connection(void *cls, struct MHD_Connection *connection,
    const char *url, const char *method, const char *version,
    const char *upload_data, size_t *upload_data_size, void **con_cls) {

    if (nullptr == *con_cls) {
        // First call: initialize connection info for tracking upload
        ConnectionInfo *ci = new ConnectionInfo();
        *con_cls = ci;
        return MHD_YES;
    }

    ConnectionInfo *ci = static_cast<ConnectionInfo*>(*con_cls);

    if (*upload_data_size != 0) {
        // Accumulate POST data chunks
        ci->postData.append(upload_data, *upload_data_size);
        *upload_data_size = 0;
        return MHD_YES; // wait for more data
    }

    // Handle OPTIONS (CORS preflight)
    if (strcmp(method, "OPTIONS") == 0) {
        struct MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
        add_cors_headers(response);
        MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    }

    // Resolve Env/Path config
    std::string baseDir = getenv("BASE_DIR") ? getenv("BASE_DIR") : "/home/magodi12/LedBox/ledProject";
    std::string configFilename = getenv("CONFIG_FILENAME") ? getenv("CONFIG_FILENAME") : "config.txt";
    std::string statusFile = getenv("LEDSYNCVIDEO_STATUS_FILE") ? getenv("LEDSYNCVIDEO_STATUS_FILE") : "/run/ledbox/ledSyncVideo_status.json";
    std::string screenshotFile = getenv("LATEST_SCREENSHOT_FILE") ? getenv("LATEST_SCREENSHOT_FILE") : "preview.jpg";

    std::string url_str(url);

    // GET /api/status -> return metadata
    if (url_str == "/api/status" && strcmp(method, "GET") == 0) {
        Json::Value resJson;
        Json::Value metadata;
        std::ifstream file(statusFile);
        if (file) {
            file >> metadata;
            // Copy the actual file fields directly to top-level so they match exactly
            if (metadata.isObject()) {
                for (const auto &key : metadata.getMemberNames()) {
                    resJson[key] = metadata[key];
                }
            }
            // Also include the full metadata object for backward compatibility
            // resJson["metadata"] = metadata;
        } else {
            resJson["running"] = false;
            resJson["pid"] = Json::Value::null;
            resJson["lastStart"] = Json::Value::null;
            resJson["message"] = "no metadata";
            resJson["metadata"] = Json::Value::null;
        }
        return send_json(connection, MHD_HTTP_OK, resJson);
    }

    // GET /api/config -> Returns config.txt contents
    if (url_str == "/api/config" && strcmp(method, "GET") == 0) {
        std::string configPath = baseDir + "/" + configFilename;
        std::ifstream file(configPath);
        if (!file) {
            return send_text(connection, MHD_HTTP_NOT_FOUND, "config.txt not found");
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return send_text(connection, MHD_HTTP_OK, buffer.str());
    }

    // POST /api/config -> Replace config.txt with POST body
    if (url_str == "/api/config" && strcmp(method, "POST") == 0) {
        std::string configPath = baseDir + "/" + configFilename;
        std::ofstream file(configPath, std::ios::trunc);
        if (!file) {
            Json::Value err; err["error"] = "Could not write config";
            return send_json(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, err);
        }
        file << ci->postData;
        file.close();
        Json::Value res; res["ok"] = true;
        return send_json(connection, MHD_HTTP_CREATED, res);
    }

    // POST /api/control/calibrate
    if (url_str == "/api/control/calibrate" && strcmp(method, "POST") == 0) {
        int pid = forkFunction(run_calibrate);
        if (pid == -1) {
            Json::Value err; err["error"] = "Failed to start calibration";
            return send_json(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, err);
        }
        Json::Value res; res["ok"] = true;
        return send_json(connection, MHD_HTTP_OK, res);
    }

    // POST /api/control/sync
    if (url_str == "/api/control/sync" && strcmp(method, "POST") == 0) {
        int pid = forkFunction(run_ledSyncVideo);
        if (pid == -1) {
            Json::Value err; err["error"] = "Failed to start sync";
            return send_json(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, err);
        }
        Json::Value res; res["ok"] = true;
        return send_json(connection, MHD_HTTP_OK, res);
    }

    // GET /api/screenshot -> Serve latest preview image
    if (url_str == "/api/screenshot" && strcmp(method, "GET") == 0) {
        std::string imgPath = baseDir + "/" + screenshotFile;
        int fd = open(imgPath.c_str(), O_RDONLY);
        if (fd == -1) {
            return send_text(connection, MHD_HTTP_NOT_FOUND, "no screenshots found");
        }
        struct stat st;
        fstat(fd, &st);
        struct MHD_Response *response = MHD_create_response_from_fd(st.st_size, fd);
        add_cors_headers(response);
        MHD_add_response_header(response, "Content-Type", "image/jpeg");
        MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    }

    // Default 404 Route
    Json::Value err; err["error"] = "Not Found";
    return send_json(connection, MHD_HTTP_NOT_FOUND, err);
}

// Cleanup connection state when request is complete
static void request_completed(void *cls, struct MHD_Connection *connection,
                              void **con_cls, enum MHD_RequestTerminationCode toe) {
    ConnectionInfo *ci = static_cast<ConnectionInfo*>(*con_cls);
    if (ci) delete ci;
}

int main() {
    int port = getenv("PORT") ? atoi(getenv("PORT")) : PORT;

    // Use internal polling thread and handle multiple connections concurrently
    struct MHD_Daemon *daemon = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_THREAD_PER_CONNECTION, 
        port, nullptr, nullptr,
        &answer_to_connection, nullptr,
        MHD_OPTION_NOTIFY_COMPLETED, request_completed, nullptr,
        MHD_OPTION_END
    );

    if (nullptr == daemon) {
        std::cerr << "Failed to start microhttpd API daemon on port " << port << std::endl;
        return 1;
    }

    std::cout << "LedBox C++ API listening on port " << port << std::endl;

    // Block here endlessly while MHD runs in background threads
    while (true) {
        sleep(1);
    }

    MHD_stop_daemon(daemon);
    return 0;
}



