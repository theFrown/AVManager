#pragma once

#include <array>
#include <string>
#include <winsock2.h>

enum class Verbosity { Silent, Error, Warning, Info, Debug };

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
    // int SendAndReport(const char* cmd, char* buffer, int bufferlen);

private:
    constexpr static const char* ip_string_ = "192.168.1.200";
    constexpr static int port_ = 23;
    constexpr static int inbufferlen_ = 50;

    std::array<char, inbufferlen_ + 1> inbuffer_;
    std::string inchain_;
    std::string inmessage_;
    SOCKET socket_ = INVALID_SOCKET;
    sockaddr_in sockaddr_;
    Verbosity verbosity_;

    int SetupSocket();
    int Connect();
    int CheckIncoming(int time_out = 0);
    int Receive();
    int Parse();
};