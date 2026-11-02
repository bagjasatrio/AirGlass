#include "web_drop_server.h"
#include "drop_page_html.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>

#include "net/socket.h"
#include "net/httplib.h"
#include "log.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>

#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;

namespace ap::drop {

namespace {

std::string sanitize_filename(const std::string& raw) {
    fs::path p(raw);
    std::string clean = p.filename().string();
    if (clean.empty() || clean == "." || clean == "..") {
        clean = "received_file.bin";
    }
    return clean;
}

std::string get_downloads_folder() {
    PWSTR wpath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &wpath)) && wpath) {
        char narrow[MAX_PATH]{};
        WideCharToMultiByte(CP_UTF8, 0, wpath, -1, narrow, sizeof(narrow) - 1, NULL, NULL);
        CoTaskMemFree(wpath);
        fs::path p = fs::path(narrow) / "AirGlass";
        std::error_code ec;
        fs::create_directories(p, ec);
        return p.string();
    }
    fs::path fallback = fs::current_path() / "AirGlass_Drop";
    std::error_code ec;
    fs::create_directories(fallback, ec);
    return fallback.string();
}

std::string get_shared_staging_folder() {
    PWSTR wpath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &wpath)) && wpath) {
        char narrow[MAX_PATH]{};
        WideCharToMultiByte(CP_UTF8, 0, wpath, -1, narrow, sizeof(narrow) - 1, NULL, NULL);
        CoTaskMemFree(wpath);
        fs::path p = fs::path(narrow) / "AirGlass" / "SharedToIOS";
        std::error_code ec;
        fs::create_directories(p, ec);
        return p.string();
    }
    fs::path fallback = fs::current_path() / ".shared_to_ios";
    std::error_code ec;
    fs::create_directories(fallback, ec);
    return fallback.string();
}

} // namespace

WebDropServer::WebDropServer() {
    drop_dir_ = get_downloads_folder();
    shared_dir_ = get_shared_staging_folder();
    std::error_code ec;
    fs::create_directories(drop_dir_, ec);
    fs::create_directories(shared_dir_, ec);
}

WebDropServer::~WebDropServer() {
    stop();
}

bool WebDropServer::start(int preferred_port) {
    if (running_.load()) return true;

    ip_ = ap::net::primary_ipv4();
    if (ip_.empty() || ip_ == "0.0.0.0") {
        ip_ = "127.0.0.1";
    }

    server_ = std::make_unique<httplib::Server>();
    setup_routes();

    // Try binding on preferred_port or fallback ports
    int bound_port = 0;
    for (int p = preferred_port; p < preferred_port + 10; ++p) {
        if (server_->bind_to_port("0.0.0.0", p)) {
            bound_port = p;
            break;
        }
    }

    if (bound_port == 0) {
        LOG_ERROR << "web_drop: failed to bind to port " << preferred_port;
        server_.reset();
        return false;
    }

    port_ = bound_port;
    running_.store(true);

    thread_ = std::make_unique<std::thread>([this]() {
        LOG_INFO << "web_drop: listening on http://" << ip_ << ":" << port_;
        server_->listen_after_bind();
        running_.store(false);
    });

    return true;
}

void WebDropServer::stop() {
    if (!running_.load()) return;
    running_.store(false);
    if (server_) {
        server_->stop();
    }
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
    thread_.reset();
    server_.reset();
}

std::string WebDropServer::url() const {
    if (port_ <= 0) return "";
    return "http://" + ip_ + ":" + std::to_string(port_);
}

void WebDropServer::set_drop_dir(const std::string& new_dir) {
    if (new_dir.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    drop_dir_ = new_dir;
    std::error_code ec;
    fs::create_directories(drop_dir_, ec);

    // Clean up any legacy Shared_to_iPhone directory inside drop_dir_
    fs::path legacy = fs::path(drop_dir_) / "Shared_to_iPhone";
    if (fs::exists(legacy, ec)) {
        for (const auto& entry : fs::directory_iterator(legacy, ec)) {
            if (entry.is_regular_file(ec)) {
                fs::copy_file(entry.path(), fs::path(shared_dir_) / entry.path().filename(),
                              fs::copy_options::overwrite_existing, ec);
            }
        }
        fs::remove_all(legacy, ec);
    }
}

void WebDropServer::open_drop_folder() const {
    ShellExecuteA(NULL, "open", drop_dir_.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

int WebDropServer::add_shared_files(const std::vector<std::string>& file_paths) {
    int copied = 0;
    std::error_code ec;
    fs::create_directories(shared_dir_, ec);
    for (const auto& path_str : file_paths) {
        fs::path src = fs::u8path(path_str);
        if (!fs::exists(src, ec) || !fs::is_regular_file(src, ec)) continue;
        fs::path dest = fs::path(shared_dir_) / src.filename();
        if (fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec)) {
            copied++;
        }
    }
    return copied;
}

bool WebDropServer::delete_shared_file(const std::string& filename) {
    std::string safe = sanitize_filename(filename);
    fs::path target = fs::path(shared_dir_) / safe;
    std::error_code ec;
    return fs::remove(target, ec);
}

void WebDropServer::clear_shared_files() {
    std::error_code ec;
    if (!fs::exists(shared_dir_, ec)) return;
    for (const auto& entry : fs::directory_iterator(shared_dir_, ec)) {
        if (entry.is_regular_file(ec)) {
            fs::remove(entry.path(), ec);
        }
    }
}

std::vector<FileItem> WebDropServer::list_shared_files() const {
    std::vector<FileItem> results;
    std::error_code ec;
    if (!fs::exists(shared_dir_, ec)) return results;

    for (const auto& entry : fs::directory_iterator(shared_dir_, ec)) {
        if (entry.is_regular_file(ec)) {
            FileItem item;
            item.name = entry.path().filename().string();
            item.size = entry.file_size(ec);
            results.push_back(item);
        }
    }
    return results;
}

int WebDropServer::shared_files_count() const {
    std::error_code ec;
    if (!fs::exists(shared_dir_, ec)) return 0;
    int count = 0;
    for (const auto& entry : fs::directory_iterator(shared_dir_, ec)) {
        if (entry.is_regular_file(ec)) {
            count++;
        }
    }
    return count;
}

TransferProgress WebDropServer::get_transfer_progress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_;
}

void WebDropServer::report_failure(const std::string& filename, const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    progress_.failed_files++;
    progress_.last_error = error.empty() ? ("Failed: " + filename) : error;
    progress_.is_active = false;
    LOG_WARN << "web_drop: client reported failure for " << filename << " (" << progress_.last_error << ")";
}

void WebDropServer::reset_failures() {
    std::lock_guard<std::mutex> lock(mutex_);
    progress_.failed_files = 0;
    progress_.last_error.clear();
}

std::string WebDropServer::last_upload_name() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_.last_completed_file;
}

uint64_t WebDropServer::last_upload_bytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_.bytes_received;
}

int WebDropServer::upload_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_.completed_files;
}

void WebDropServer::on_transfer_start(const std::string& filename, uint64_t total_bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    progress_.is_active = true;
    progress_.current_file = filename;
    progress_.bytes_received = 0;
    progress_.bytes_total = total_bytes;
    progress_.percent = 0.0f;
}

void WebDropServer::on_transfer_chunk(uint64_t bytes_added) {
    std::lock_guard<std::mutex> lock(mutex_);
    progress_.bytes_received += bytes_added;
    if (progress_.bytes_total > 0) {
        progress_.percent = std::clamp(static_cast<float>(progress_.bytes_received) / static_cast<float>(progress_.bytes_total) * 100.0f, 0.0f, 100.0f);
    }
}

void WebDropServer::on_transfer_complete(const std::string& filename, uint64_t total_bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    progress_.is_active = false;
    progress_.completed_files++;
    progress_.last_completed_file = filename;
    progress_.bytes_received = total_bytes;
    progress_.percent = 100.0f;
    LOG_INFO << "web_drop: successfully saved " << filename << " (" << total_bytes << " bytes)";
}

void WebDropServer::on_transfer_failed(const std::string& filename, const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    progress_.is_active = false;
    progress_.failed_files++;
    progress_.last_error = error;
    LOG_ERROR << "web_drop: failed to transfer " << filename << " (" << error << ")";
}

void WebDropServer::setup_routes() {
    // 1. Root web app
    server_->Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(get_drop_page_html(), "text/html");
    });

    // 2. Real-time streaming upload handler (iOS -> PC: saves strictly to drop_dir_)
    server_->Post("/api/upload", [this](const httplib::Request& req, httplib::Response& res, const httplib::ContentReader& content_reader) {
        if (!req.is_multipart_form_data()) {
            res.status = 400;
            res.set_content("{\"error\":\"multipart required\"}", "application/json");
            return;
        }

        const uint64_t content_length = req.get_header_value_u64("Content-Length", 0);
        std::string current_filename;
        fs::path target_path;
        std::ofstream out;
        bool write_failed = false;
        std::string write_error;
        uint64_t bytes_written = 0;

        bool read_ok = content_reader(
            [&](const httplib::MultipartFormData& file) {
                current_filename = sanitize_filename(file.filename);
                // Save exclusively to drop_dir_ (Windows received folder)
                target_path = fs::path(drop_dir_) / current_filename;

                int counter = 1;
                while (fs::exists(target_path)) {
                    std::string stem = fs::path(current_filename).stem().string();
                    std::string ext = fs::path(current_filename).extension().string();
                    target_path = fs::path(drop_dir_) / (stem + "_" + std::to_string(counter++) + ext);
                }

                if (out.is_open()) out.close();
                out.open(target_path, std::ios::binary);
                if (!out) {
                    write_failed = true;
                    write_error = "Failed to create destination file";
                    return false;
                }

                bytes_written = 0;
                on_transfer_start(target_path.filename().string(), content_length);
                return true;
            },
            [&](const char* data, size_t data_length) {
                if (out.is_open() && data_length > 0) {
                    out.write(data, data_length);
                    if (!out) {
                        write_failed = true;
                        write_error = "Write error during transfer";
                        return false;
                    }
                    bytes_written += data_length;
                    on_transfer_chunk(data_length);
                }
                return true;
            }
        );

        if (out.is_open()) {
            out.close();
        }

        if (read_ok && !write_failed) {
            on_transfer_complete(target_path.filename().string(), bytes_written);
            res.set_content("{\"success\":true,\"saved\":1,\"name\":\"" + target_path.filename().string() + "\"}", "application/json");
        } else {
            // Remove incomplete partial file
            std::error_code ec;
            if (fs::exists(target_path, ec)) {
                fs::remove(target_path, ec);
            }
            std::string err = write_error.empty() ? "Connection lost or interrupted" : write_error;
            on_transfer_failed(current_filename.empty() ? "unknown" : current_filename, err);
            res.status = 500;
            res.set_content("{\"error\":\"" + err + "\"}", "application/json");
        }
    });

    // 3. Client failure report
    server_->Post("/api/report_failure", [this](const httplib::Request& req, httplib::Response& res) {
        std::string filename = req.has_param("name") ? req.get_param_value("name") : "unknown";
        std::string error = req.has_param("error") ? req.get_param_value("error") : "Client timeout or sleep";
        report_failure(filename, error);
        res.set_content("{\"success\":true}", "application/json");
    });

    // 4. Reset failures
    server_->Post("/api/reset_failures", [this](const httplib::Request&, httplib::Response& res) {
        reset_failures();
        res.set_content("{\"success\":true}", "application/json");
    });

    // 5. List shared files handler (PC -> iOS: lists ONLY shared_dir_)
    server_->Get("/api/files", [this](const httplib::Request&, httplib::Response& res) {
        auto files = list_shared_files();
        std::ostringstream json;
        json << "[";
        for (size_t i = 0; i < files.size(); ++i) {
            if (i > 0) json << ",";
            json << "{\"name\":\"";
            for (char c : files[i].name) {
                if (c == '"' || c == '\\') json << '\\';
                json << c;
            }
            json << "\",\"size\":" << files[i].size << "}";
        }
        json << "]";
        res.set_content(json.str(), "application/json");
    });

    // 6. Download file handler (PC -> iOS: downloads ONLY from shared_dir_)
    server_->Get("/api/download", [this](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("name")) {
            res.status = 400;
            res.set_content("name required", "text/plain");
            return;
        }

        std::string filename = sanitize_filename(req.get_param_value("name"));
        fs::path file_path = fs::path(shared_dir_) / filename;

        std::error_code ec;
        if (!fs::exists(file_path, ec) || !fs::is_regular_file(file_path, ec)) {
            res.status = 404;
            res.set_content("file not found in shared folder", "text/plain");
            return;
        }

        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            res.status = 500;
            res.set_content("cannot read file", "text/plain");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        res.set_header("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        res.set_content(std::move(content), "application/octet-stream");
    });

    // 7. Delete file from shared folder handler
    server_->Post("/api/delete", [this](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("name")) {
            res.status = 400;
            res.set_content("name required", "text/plain");
            return;
        }
        std::string filename = req.get_param_value("name");
        bool ok = delete_shared_file(filename);
        res.set_content(ok ? "{\"success\":true}" : "{\"error\":\"failed\"}", "application/json");
    });

    // 8. Clear all shared files handler
    server_->Post("/api/clear_shared", [this](const httplib::Request&, httplib::Response& res) {
        clear_shared_files();
        res.set_content("{\"success\":true}", "application/json");
    });

    // 9. Status and telemetry handler
    server_->Get("/api/status", [this](const httplib::Request&, httplib::Response& res) {
        TransferProgress p = get_transfer_progress();
        std::ostringstream json;
        json << "{\"app\":\"AirGlass\",\"web_drop\":true,\"port\":" << port_
             << ",\"active\":" << (p.is_active ? "true" : "false")
             << ",\"completed\":" << p.completed_files
             << ",\"failed\":" << p.failed_files
             << ",\"current_file\":\"" << p.current_file << "\""
             << ",\"percent\":" << p.percent
             << ",\"shared_count\":" << shared_files_count()
             << "}";
        res.set_content(json.str(), "application/json");
    });
}

WebDropServer& get_drop_server() {
    static WebDropServer instance;
    return instance;
}

} // namespace ap::drop
