#include <iostream>
#include <array>
#include <optional>
#include <stdexcept>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "AVRHandler.h"
#include "DenonProtocol.h"

AVRHandler::AVRHandler(Verbosity verbosity) 
    : verbosity_(verbosity)
{
    status = SetupSocket();
    if (status == 0) {
        status = Connect();
    }
}

AVRHandler::~AVRHandler() {
    if (socket_ != INVALID_SOCKET) {
        int response = shutdown(socket_, SD_SEND);
        if (response == 0) {
            if (verbosity_ >= Verbosity::Warning) {
                std::cerr << "WARNING: DESTRUCTOR INCOMPLETE\n";
            }
            //[receive incoming messages for 2 more seconds]
        }
        else {
            if (verbosity_ >= Verbosity::Warning) {
                std::cerr << "Socket shutdown failed with response:\n" << response << "\n";
                std::cerr << "proceding with hard shutdown\n";
            }
            bool value = true;
            int size = sizeof(value);
            setsockopt(socket_, SOL_SOCKET, SO_DONTLINGER, (char *) &value, size);
        }
        closesocket(socket_);
    }
    WSACleanup();
}

int AVRHandler::ControlLoop() {
    return 0;
}

int AVRHandler::SetupSocket() {
    if (verbosity_ >= Verbosity::Info) {
        std::cout << "Setting up connection\n";
    }
    WSADATA wsadata;
    int response = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (response != 0) {
        if (verbosity_ >= Verbosity::Error) {
            std::cerr << "WSAStartup failed with response:\n" << response << "\n";
        }
        return 1;
    }
    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == INVALID_SOCKET) {
        if (verbosity_ >= Verbosity::Error) {
            std::cerr << "Failed to create socket\n";
        }
        return 2;
    }
    sockaddr_.sin_family = AF_INET;
    sockaddr_.sin_port   = htons(port_);
    response = inet_pton(AF_INET, ip_string_, &sockaddr_.sin_addr);
    if (response != 1) {
        if (verbosity_ >= Verbosity::Error) {
            std::cerr << "Failed to convert IP address\n";
        }
        return 3;
    }
    return 0;
}

int AVRHandler::Connect() {
    if (verbosity_ >= Verbosity::Info) {
        std::cout << "Opening connection\n";
    }
    int response = connect(socket_, (sockaddr*) &sockaddr_, sizeof(sockaddr_));
    if (response != 0) { 
        if (verbosity_ >= Verbosity::Error) {
            std::cerr << "Failed to connect socket with response:\n" << response << "\n";
        }
        return 1;
    }
    return 0;
}

int AVRHandler::Send(std::string_view cmd) {
    if (verbosity_ >= Verbosity::Info) {
        std::cout << "Sending following command to AVR: [" << cmd << "]\n";
    }
    int response = send(socket_, cmd.data(), static_cast<int>(cmd.size()), 0);
    if (response == SOCKET_ERROR) {
        if (verbosity_ >= Verbosity::Warning) {
            std::cerr << "Failed to send command: [" << cmd << "]\n";
        }
        return 1;        
    }
    return 0;
}

std::string AVRHandler::MakeCommand(std::string cmd, std::optional<int> num) {
    if (num) {
        if (cmd.compare(denon_cmd::volumeprefix) == 0) {
            cmd += dbtostring(*num);
        }
        else {
            cmd += std::to_string(*num);
        }
    }
    cmd += '\r';
    return cmd;
}

int AVRHandler::CheckIncoming(int time_out) {
    if (verbosity_ >= Verbosity::Debug) {
        std::cout << "Checking for incoming messages\n";
    }
    timeval tv{0, time_out * 1000};
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(socket_, &readfds);
    int response = select(0, &readfds, nullptr, nullptr, &tv);
    if (response == 0) {
        if (verbosity_ >= Verbosity::Debug) {
            std::cout << "no incoming messages\n";
        }
        return 1;
    }
    else if (response == SOCKET_ERROR) {
        if (verbosity_ >= Verbosity::Warning) {
            std::cerr << "issue with checking for messages\n";
        }
        return 2;
    }
    if (FD_ISSET(socket_, &readfds)) {
        if (verbosity_ >= Verbosity::Debug) {
            std::cout << "incoming message from AVR\n";
        }
        return 0;
    }
    if (verbosity_ >= Verbosity::Warning) {
        std::cerr << "incoming message from a mystery socket\n";
    }
    return 3;
}

int AVRHandler::Receive() {
    if (verbosity_ >= Verbosity::Debug) {
        std::cout << "Receiving incoming message\n";
    }
    int response = recv(socket_, inbuffer_.data(), static_cast<int>(inbuffer_.size()), 0);
    if (response == SOCKET_ERROR) { 
        if (verbosity_ >= Verbosity::Warning) {
            std::cerr << "Failed to receive response from AVR\n";
        }
        return 1;        
    }
    inchain_.append(inbuffer_.data(), response);
    if (verbosity_ >= Verbosity::Debug) {
        inbuffer_[response] = '\0';
        std::cout << "Received following from AVR: [" << inbuffer_.data() << "]\n";
        std::cout << "Full chain now contains: [" << inchain_ << "]\n";
    }
    return 0;
}

int AVRHandler::Parse() {
    if (verbosity_ >= Verbosity::Debug) {
        std::cout << "Parsing received message chain\n";
    }
    auto index = inchain_.find('\r');
    if (index == std::string::npos) {
        if (verbosity_ >= Verbosity::Debug) {
            std::cout << "Chain does not contain \\r: [" << inchain_ << "]\n";
        }
        return 1;
    }
    else if (index < 1) {
        if (verbosity_ >= Verbosity::Warning) {
            std::cerr << "Chain contained \\r character at position 0\n";
        }
        inchain_.erase(0, 1);
        return 2;
    }
    inmessage_ = inchain_.substr(0, index);
    inchain_.erase(0, index + 1);
    if (verbosity_ >= Verbosity::Info) {
        std::cout << "Received message: [" << inmessage_ << "]\n";
    }
    if (verbosity_ >= Verbosity::Debug) { 
        std::cout << "Remaining content of chain: [" << inchain_ << "]\n";
    }
    return 0;
}

std::string AVRHandler::dbtostring(int db) {
    std::string result;
    db = static_cast<int>(round(db / 5.0)) * 5;
    db += 800;
    if (db <= 0) {
        result = "00";
    }
    else if (db >= 980) {
        result = "98";
    }
    else {
        if (db < 100) {
            result = "0";
        }
        result += std::to_string(db / 10);
        if ((db % 10) > 0) {
            result += '5';
        }
    }
    return result;
}

std::optional<int> AVRHandler::stringtodb(std::string str) {
    if (str.size() == 2) {
        str += '0';
    }
    try {
        return std::stoi(str) - 800;
    }
    catch (const std::invalid_argument&) {
        if (verbosity_ >= Verbosity::Warning) {
            std::cerr << "Invalid string characters for db conversion: " << str << "\n";
        }
        return std::nullopt;
    }
    catch (const std::out_of_range&) {
        if (verbosity_ >= Verbosity::Warning) {
            std::cerr << "String too large for db conversion: " << str << "\n";
        }
        return std::nullopt;
    }
}

int AVRHandler::testcoms() {
    std::cout << "Sending test command to AVR\n";
    int response = Send(MakeCommand(denon_cmd::volumeprefix, -200));
    //int response = Send(denon_cmd::volumestatus);
    for (int i = 0; i < 10; i++) {
        if (CheckIncoming(10) == 0) {
            response = Receive();
            if (response != 0) {
                return 2;
            }
            response = Parse();
            if (response != 0) {
                return 3;
            }
        }
    }
    return 0;
}