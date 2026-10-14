#pragma once

#include <fstream>
#include <string>
#include <filesystem>

class Logger {
public:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger& operator=(Logger&&) = delete;

    int Start(bool portable = false);
    static std::filesystem::path GetExecutablePath();
    static std::filesystem::path GetLocalAppdataPath();
    static bool CreateFileIfNew(const std::filesystem::path& path);

private:
    constexpr static const char* start_marker_ = "===== start of log =====";
    constexpr static const char* end_marker_ = "===== end of log =====";
    constexpr static const char* program_folder_name_ = "AVManager";
    constexpr static const char* log_folder_name_ = "logs";
    constexpr static const char* log_file_format_ = "{}{}.txt";
    std::filesystem::path folder_path_;
    std::ofstream log_file_;

    bool MakeFolderPath(bool portable);
    bool CreateLog();
    bool Write(std::string_view string, bool timestamp = true);
};
