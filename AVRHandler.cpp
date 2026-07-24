#include <iostream>
#include <ws2tcpip.h>
#include "AVRHandler.h"
#include "DenonProtocol.h"


AVRHandler::AVRHandler() {
    status = SetupConnect();
}

int AVRHandler::SetupConnect() {
    std::cout << "Setting up connection..\n";
    WSADATA wsadata;
    int response = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (response != 0) {
        std::cout << "WSAStartup failed with response:\n" << response << "\n";
        return 1;
    }
    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == INVALID_SOCKET) {
        std::cout << "Failed to create socket\n";
        return 2;
    }
    sockaddr_.sin_family = AF_INET;
    sockaddr_.sin_port   = htons(port_);
    inet_pton(AF_INET, ip_string_, &sockaddr_.sin_addr);
    response = connect(socket_, (sockaddr*)&sockaddr_, sizeof(sockaddr_));
    if (response != 0) { 
        std::cout << "Failed to connect socket with response:\n" << response << "\n";
        return 3;
    }
    return 0;
}

int AVRHandler::CheckIncoming(int time_out, bool verbose) {
    timeval tv{0, time_out * 1000};
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(socket_, &readfds);
    int response = select(0, &readfds, nullptr, nullptr, &tv);
    if (response == 0) {
        if (verbose) {
            std::cout << "no incoming messages\n";
        }
        return 1;
    }
    else if (response == SOCKET_ERROR) {
        if (verbose) {
            std::cout << "issue with checking for messages\n";
        }
        return 2;
    }
    if (FD_ISSET(socket_, &readfds)) {
        if (verbose) {
            std::cout << "incoming message from AVR\n";
        }
        return 0;
    }
    if (verbose) {
        std::cout << "incoming message from a mystery socket\n";
    }
    return 3;
}

int AVRHandler::ReceiveAndParse(bool verbose) {
    int response = recv(socket_, inbuffer_, inbufferlen_, 0);
    if (response == SOCKET_ERROR) { 
        if (verbose) {
            std::cout << "Failed to receive response from AVR\n";
        }
        return 1;        
    }
    if (verbose) {
        std::cout << "Received following from AVR:\n";
    }
    for (int i = 0; i < response; i++) {
        if (inbuffer_[i] == '\r') {
            inbuffer_[i] = '\n';
        }
    }
    inbuffer_[response] = '\0';
    std::cout << inbuffer_;
    if (verbose) {
        std::cout << "\n";
    }
    return 0;
}

int AVRHandler::testcoms() {
    std::cout << "sending test command to AVR\n";
    int response = send(socket_, denon_cmd::volumestatus, 
                        strlen(denon_cmd::volumestatus), 0);
    if (response == SOCKET_ERROR) { 
        std::cout << "Failed to send test command\n";
        return 1;        
    }
    for (int i = 0; i < 10; i++) {
        if (CheckIncoming(10, true) == 0) {
            response = ReceiveAndParse(true);
            if (response != 0) {
                return 2;
            }
        }
    }
    return 0;
}