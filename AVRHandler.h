#pragma once

#include <array>
#include <string>
#include <optional>
#include <winsock2.h>

enum class Verbosity { Silent, Error, Warning, Info, Debug };

struct DenonState {
    int volume; //in 0.1db increments: 10dB stored as 100, 10.5db as 105
    int maxvolume;
};

class AVRHandler {
public:
    AVRHandler(Verbosity verbosity = Verbosity::Info);
    AVRHandler(const AVRHandler&) = delete;
    AVRHandler(AVRHandler&&) = delete;
    AVRHandler& operator=(const AVRHandler&) = delete;
    AVRHandler& operator=(AVRHandler&&) = delete;
    ~AVRHandler();

    int status;
    int testcoms();

private:
    constexpr static const char* ip_string_ = "192.168.1.200";
    constexpr static int port_ = 23;
    constexpr static size_t inbufferlen_ = 270; //2x denon message max size (135 chars)
    enum class Report_ { OK, Data, NoData, Unknown, BadInput, Disconnected, SocketError };

    std::array<char, inbufferlen_ + 1> inbuffer_;
    std::string inchain_;
    std::string inmessage_;
    SOCKET socket_ = INVALID_SOCKET;
    sockaddr_in sockaddr_;
    const Verbosity verbosity_;
    DenonState requested_ = {};
    DenonState commanded_ = {};
    DenonState reported_ = {};

    int ControlLoop();
    Report_ SetupSocket();
    Report_ Connect();
    Report_ Send(std::string_view cmd);
    std::string MakeCommand(std::string cmd, std::optional<int> num = std::nullopt) const;
    Report_ CheckIncoming(int time_out = 0) const;
    Report_ Receive();
    Report_ Parse();
    Report_ SyncIn();
    std::string dbtostring(int db) const;
    std::optional<int> stringtodb(std::string str) const;
    void Print(Verbosity level, std::string_view msg) const;
    void Print(std::string_view msg) const { Print(Verbosity::Info, msg); }
};