#pragma once

#include <winsock2.h>

class AVRHandler {
public:
    AVRHandler();
    AVRHandler(const AVRHandler&) = delete;
    AVRHandler(AVRHandler&&) = delete;
    AVRHandler& operator=(const AVRHandler&) = delete;
    AVRHandler& operator=(AVRHandler&&) = delete;
    // ~AVRHandler();

    int status;
    int testcoms();
    // int SendAndReport(const char* cmd, char* buffer, int bufferlen);

private:
    constexpr static const char* ip_string_ = "192.168.1.200";
    constexpr static int port_ = 23;
    constexpr static int inbufferlen_ = 20;

    char inbuffer_[inbufferlen_ + 1];
    SOCKET socket_;
    sockaddr_in sockaddr_;

    int SetupConnect();
    int CheckIncoming(int time_out = 0, bool verbose = false);
    int ReceiveAndParse(bool verbose = false);
    // int Receive(bool verbose);
    // int Parse(bool verbose);
    // int Send(bool verbose);
    // int Report(char* buffer, int bufferlen);
};