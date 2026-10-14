#include <fstream>
#include <string>
#include <iostream>
#include <format>
#include <filesystem>
#include <system_error>
#include <windows.h> //lean + nomin in CMakeLists
#include <shlobj.h>
#include "logger.h"
#include "timing.h"

Logger::~Logger() {
    if (log_file_.is_open()) {
        Write("Closing down log");
        bool response = Write(end_marker_, false);
        if (!response) std::cerr << "[WARNING] Logger could not write end to log file!\n";
        log_file_.close();
    }
}

int Logger::Start(bool portable) {
    bool result = MakeFolderPath(portable);
    if (!result) return 1;
    result = CreateLog();
    if (!result) return 2;
    return 0;
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
            std::cerr << "[ERROR] unable to create log file! Too many conflicting file names\n";
            return false;
        }
        file_path = folder_path_ / std::format(log_file_format_, date_time, 
                                               std::format("_{:03d}", i));
        available = CreateFileIfNew(file_path);
    }
    log_file_.open(file_path, std::ios::app);
    if (log_file_.fail()) {
        std::wcerr << std::format(L"[ERROR] Logger failed to open created log file:\n    {}\n",
                                  file_path.wstring());
    }
    Write(start_marker_, false);
    return Write("Log created");
}

bool Logger::Write(std::string_view string, bool timestamp) {
    if  (!log_file_.is_open()) return false;
    if (timestamp) log_file_ << Timestamp{}.Prefix();
    if (log_file_.fail()) return false;
    log_file_ << string << "\n" << std::flush;
    if (string.back() == '\n') log_file_ <<"    [WARNING] unnecessary endline!\n" << std::flush;
    return log_file_.good();
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
        std::cerr << std::format("[ERROR] Logger failed to create/find the log folder:\n    {}\n"
                                   "with error code: {} and error message: {}\n", 
                                   folder_path_.string(), ec.value(), ec.message());
        folder_path_.clear();
        return false;
    }
    std::cout << "Storing logs to path: " << folder_path_ << "\n";
    return true;
}

std::filesystem::path Logger::GetExecutablePath() {
    std::filesystem::path file_path;
    const DWORD pathsize = 300;
    wchar_t filepath_array[pathsize]; 
    DWORD returnsize = GetModuleFileNameW(nullptr, filepath_array, pathsize);
    if ((returnsize == 0) || (returnsize == pathsize)) {
        DWORD error = GetLastError();
        std::cerr << std::format("[ERROR] Failed to retrieve path to executable with error: "
                                    "{}\n", error);
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
        std::cerr << std::format("[ERROR] Failed to retrieve path to Local Appdata with error: "
                                    "{:#x}\n", static_cast<unsigned long>(result));
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
            std::wcerr << std::format(L"[WARNING] Logger tried to create a second log file with the"
                                      L" same name:\n    {}\n", path.wstring());
        }
        else {
            std::wcerr << std::format(L"[ERROR] Logger could not create file with name:\n    {}\n"
                                      L"    with error: {}\n", path.wstring(), error);
        }
        return false;
    }
    BOOL result = CloseHandle(windows_file_handle);
    if (result == 0) {
        DWORD error = GetLastError();
        std::wcerr << std::format(L"[WARNING] Logger could not close Windows file handle to file:\n"
                                  L"    {}\n    with error: {}\n", path.wstring(), error);
        return false; //this is a choice, returning false ensures nothing else tries to open it
    }
    return true;
}
