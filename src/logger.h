#pragma once

#include <fstream>
#include <string>
#include <filesystem>
#include <optional>

class Timestamp; //defined in timing.h, which logger.cpp pulls in before using it

class Logger {
public:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger& operator=(Logger&&) = delete;

    bool Start(bool portable = false);
    bool Log(std::string_view string, bool timestamp = true);
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
    bool has_visible_output_ = false;
    bool has_log_output_ = false;

    bool MakeFolderPath(bool portable);
    bool CreateLog();
    static bool Write(std::ostream& stream, std::string_view string,
                      std::optional<Timestamp> timestamp);
};
