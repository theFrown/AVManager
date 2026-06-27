#include <iostream>
// #include <thread>
// #include <atomic>
#include <winsock2.h>
#include <ws2tcpip.h>

const char* avr_ip = "192.168.1.200";
const short avr_port = 23;
const short avr_inbufferlen = 20;

namespace avr_cmd {
    constexpr const char* volumestatus = "MV?\r";
    constexpr const char* volumeup     = "MVUP\r";
    constexpr const char* volumedown   = "MVDOWN\r";
}

SOCKET avr_socket;
sockaddr_in avr_sockaddr{};
char avr_inbuffer[avr_inbufferlen + 1];
// std::atomic<bool> avr_stoplistening = 0;

int SetupConnect() {
    std::cout << "Setting up connection..\n";
    WSADATA avr_wsadata;
    int response = WSAStartup(MAKEWORD(2,2), &avr_wsadata);
    if (response != 0) {
        std::cout << "WSAStartup failed with response:\n" << response << "\n";
        return 1;
    }
    avr_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (avr_socket == INVALID_SOCKET) {
        std::cout << "Failed to create socket\n";
        return 2;
    }
    avr_sockaddr.sin_family = AF_INET;
    avr_sockaddr.sin_port   = htons(avr_port);
    inet_pton(AF_INET, avr_ip, &avr_sockaddr.sin_addr);
    response = connect(avr_socket, (sockaddr*)&avr_sockaddr, sizeof(avr_sockaddr));
    if (response != 0) { 
        std::cout << "Failed to connect socket with response:\n" << response << "\n";
        return 3;
    }
    return 0;
}

int CheckIncoming(int time_out = 0, bool verbose = false) {
    timeval tv{0, time_out * 1000};
    fd_set avr_readfds;
    FD_ZERO(&avr_readfds);
    FD_SET(avr_socket, &avr_readfds);
    int response = select(0, &avr_readfds, nullptr, nullptr, &tv);
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
    if (FD_ISSET(avr_socket, &avr_readfds)) {
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

int ReceiveAndParse(bool verbose = false) {
    int response = recv(avr_socket, avr_inbuffer, avr_inbufferlen, 0);
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
        if (avr_inbuffer[i] == '\r') {
            avr_inbuffer[i] = '\n';
        }
    }
    avr_inbuffer[response] = '\0';
    std::cout << avr_inbuffer;
    if (verbose) {
        std::cout << "\n";
    }
    return 0;
}

int testcoms() {
    std::cout << "sending test command to AVR\n";
    int response = send(avr_socket, avr_cmd::volumestatus, 
                        strlen(avr_cmd::volumestatus), 0);
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

int main() {
    std::cout << "AVManager starting\n";
    int response = SetupConnect();
    if (response != 0) {
        std::cout << "boo, error:\n" << response << "\n";
        return 1;
    }
    response = testcoms();
    if (response == 0) {
        std::cout << "yay\n";
    }
    else {
        std::cout << "boo, error:\n" << response << "\n";
        return 2;
    }   
    return 0;
}