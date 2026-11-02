#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <chrono>

namespace httplib { class Server; }

namespace ap::drop {

struct FileItem {
    std::string name;
    uint64_t size = 0;
};

struct TransferProgress {
    bool is_active = false;
    std::string current_file;
    uint64_t bytes_received = 0;
    uint64_t bytes_total = 0;
    float percent = 0.0f; // 0.0 to 100.0f
    int completed_files = 0;
    int failed_files = 0;
    std::string last_error;
    std::string last_completed_file;
};

class WebDropServer {
public:
    WebDropServer();
    ~WebDropServer();

    bool start(int preferred_port = 53317);
    void stop();
    bool is_running() const { return running_.load(); }

    int port() const { return port_; }
    std::string ip() const { return ip_; }
    std::string url() const;

    // Incoming files directory (iOS -> PC)
    std::string drop_dir() const { return drop_dir_; }
    void set_drop_dir(const std::string& new_dir);
    void open_drop_folder() const;

    // Outgoing files directory (PC -> iOS): isolated staging folder in AppData
    std::string shared_dir() const { return shared_dir_; }
    std::vector<FileItem> list_shared_files() const;
    int shared_files_count() const;
    int add_shared_files(const std::vector<std::string>& file_paths);
    bool delete_shared_file(const std::string& filename);
    void clear_shared_files();

    // Live transfer progress and failure telemetry
    TransferProgress get_transfer_progress() const;
    void report_failure(const std::string& filename, const std::string& error);
    void reset_failures();

    std::string last_upload_name() const;
    uint64_t last_upload_bytes() const;
    int upload_count() const;

private:
    void setup_routes();
    void on_transfer_start(const std::string& filename, uint64_t total_bytes);
    void on_transfer_chunk(uint64_t bytes_added);
    void on_transfer_complete(const std::string& filename, uint64_t total_bytes);
    void on_transfer_failed(const std::string& filename, const std::string& error);

    std::atomic<bool> running_{false};
    int port_ = 0;
    std::string ip_;
    std::string drop_dir_;
    std::string shared_dir_;
    std::unique_ptr<std::thread> thread_;
    std::unique_ptr<httplib::Server> server_;

    mutable std::mutex mutex_;
    TransferProgress progress_;
};

WebDropServer& get_drop_server();

} // namespace ap::drop
