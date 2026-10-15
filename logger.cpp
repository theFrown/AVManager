#include <fstream>
#include <string>
#include <iostream>
#include <format>
#include <filesystem>
#include <system_error>
#include <memory>
#include <windows.h> //lean + nomin in CMakeLists
#include <shlobj.h>
#include "logger.h"
#include "timing.h"

Logger::~Logger() {
    if (log_file_.is_open()) {
        Write(log_file_, "Closing down log", Timestamp{});
        bool response = Write(log_file_, end_marker_, std::nullopt);
        if (!response) {
            Write(std::cerr, "[WARNING] Logger could not write end to log file!", Timestamp{});
        }
        log_file_.close();
    }
}

bool Logger::Start(bool portable) {
    has_visible_output_ = false;
    has_log_output_ = false;
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        Write(std::cerr, std::format("[ERROR] Logger failed to check if we have a console or pipe"
                                     " with error: {}", error), Timestamp{});
    }
    else {
        has_visible_output_ = (handle != NULL);
    }
    bool result = MakeFolderPath(portable);
    if (!result) {
        has_log_output_ = false;
        if (has_visible_output_) {
            Write(std::cerr, "[WARNING] Logger failed to create folder, proceeding with only "
                             "the console!\n", Timestamp{});
            return true;
        }
        return false;
    }
    result = CreateLog();
    if (!result) {
        has_log_output_ = false;
        if (has_visible_output_) {
            Write(std::cerr, "[WARNING] Logger failed to create file, proceeding with only the "
                             "console!\n", Timestamp{});
            return true;
        }
        return false;
    }
    has_log_output_ = true;
    if (!has_visible_output_) Log("Proceeding without console");
    return true;
}

bool Logger::Log(std::string_view message, bool timestamp) {
    if (!has_visible_output_ && !has_log_output_) return false;
    std::optional<Timestamp> time;
    if (timestamp) time = Timestamp{};
    bool visible_success = false, log_success = false;
    if (has_visible_output_) {
        if (message.starts_with("[ERROR]") || message.starts_with("[WARNING]")) {
            visible_success = Write(std::cerr, message, time);
        }
        else {
            visible_success = Write(std::cout, message, time);
        }
    }
    if (has_log_output_) log_success = Write(log_file_, message, time);
    return (visible_success || log_success);
}

bool Logger::CreateLog() {
    if (folder_path_.empty()) return false;
    std::string date_time = Timestamp{}.File();
    std::filesystem::path file_path = folder_path_ / std::format(log_file_format_, date_time, "");
    bool available = CreateFileIfNew(file_path);
    int i = 0;
    while (!available) {
        i++;
        if (i > 100) {
            Write(std::cerr, "[ERROR] unable to create log file! Too many conflicting file names\n",
                  Timestamp{});
            return false;
        }
        file_path = folder_path_ / std::format(log_file_format_, date_time, 
                                               std::format("_{:03d}", i));
        available = CreateFileIfNew(file_path);
    }
    log_file_.open(file_path, std::ios::app);
    if (log_file_.fail()) {
        Write(std::cerr, std::format("[ERROR] Logger failed to open created log file:\n    {}\n",
                                     file_path.string()), Timestamp{});
        return false;
    }
    Write(log_file_, start_marker_, std::nullopt);
    return Write(log_file_, "Log created", Timestamp{});
}

bool Logger::Write(std::ostream& stream, std::string_view message, std::optional<Timestamp> time) {
    if (time) stream << time->Prefix();
    stream << message;
    stream << (message.ends_with('\n') ? "    [WARNING] unnecessary endline!\n" : "\n");
    stream << std::flush;
    return stream.good();
}

bool Logger::MakeFolderPath(bool portable) {
    if (portable) {
        std::filesystem::path path = GetExecutablePath();
        if (path.empty()) return false;
        folder_path_ = path.parent_path() / log_folder_name_;
    }
    else {
        std::filesystem::path path = GetLocalAppdataPath();
        if (path.empty()) return false;
        folder_path_ = path / program_folder_name_ / log_folder_name_;
    }
    std::error_code ec;
    std::filesystem::create_directories(folder_path_, ec);
    if (ec) {
        Write(std::cerr, std::format("[ERROR] Logger failed to create/find the log folder:\n    {}"
                                     "\n    with error code: {} and error message: {}\n", 
                                     folder_path_.string(), ec.value(), ec.message()), Timestamp{});
        folder_path_.clear();
        return false;
    }
    Write(std::cout, std::format("Storing logs to path: {}", folder_path_.string()), Timestamp{});
    return true;
}

std::filesystem::path Logger::GetExecutablePath() {
    std::filesystem::path file_path;
    const DWORD pathsize = 300;
    wchar_t filepath_array[pathsize]; 
    DWORD returnsize = GetModuleFileNameW(nullptr, filepath_array, pathsize);
    if ((returnsize == 0) || (returnsize == pathsize)) {
        DWORD error = GetLastError();
        Write(std::cerr, std::format("[ERROR] Failed to retrieve path to executable with error: "
                                     "{}\n", error), Timestamp{});
        return {};
    }
    return std::filesystem::path(filepath_array);
}

std::filesystem::path Logger::GetLocalAppdataPath() {
    std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> managed_string{nullptr, &CoTaskMemFree};
    HRESULT result = -1;
    {
        PWSTR windows_string = nullptr;
        result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &windows_string);
        managed_string.reset(windows_string);
    }
    if (result != S_OK) {
        Write(std::cerr, std::format("[ERROR] Failed to retrieve path to Local Appdata with error: "
                                     "{:#x}\n", static_cast<unsigned long>(result)), Timestamp{});
        return {};
    } 
    return std::filesystem::path(managed_string.get());
}

bool Logger::CreateFileIfNew(const std::filesystem::path& path) {
    HANDLE windows_file_handle = CreateFileW(path.wstring().c_str(), GENERIC_WRITE, 0, nullptr, 
                                             CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (windows_file_handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_EXISTS) {
            Write(std::cerr, std::format("[WARNING] Logger tried to create a second log file with "
                                         "the same name:\n    {}\n", path.string()), Timestamp{});
        }
        else {
            Write(std::cerr, std::format("[ERROR] Logger could not create file with name:\n    {}\n"
                                         "    with error: {}\n", path.string(), error), 
                                         Timestamp{});
        }
        return false;
    }
    BOOL result = CloseHandle(windows_file_handle);
    if (result == 0) {
        DWORD error = GetLastError();
        Write(std::cerr, std::format("[WARNING] Logger could not close Windows file handle to file:"
                                     "\n    {}\n    with error: {}\n", path.string(), error),
                                     Timestamp{});
        return false; //this is a choice, returning false ensures nothing else tries to open it
    }
    return true;
}
