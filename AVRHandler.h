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
    AVRHandler(Verbosity verbosity = Verbosity::Debug); //change to warning later
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
    constexpr static int inbufferlen_ = 270;

    std::array<char, inbufferlen_ + 1> inbuffer_;
    std::string inchain_;
    std::string inmessage_;
    SOCKET socket_ = INVALID_SOCKET;
    sockaddr_in sockaddr_;
    Verbosity verbosity_;
    DenonState requested_ = {};
    DenonState commanded_ = {};
    DenonState reported_ = {};

    int ControlLoop();
    int SetupSocket();
    int Connect();
    int Send(std::string_view cmd);
    std::string MakeCommand(std::string cmd, std::optional<int> num = std::nullopt);
    int CheckIncoming(int time_out = 0);
    int Receive();
    int Parse();
    int SyncIn();
    std::string dbtostring(int db);
    std::optional<int> stringtodb(std::string str);
};