#include <iostream>
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
// FD_ZERO(*avr_fds);

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

int testcoms() {
    std::cout << "sending test command to AVR\n";
    int response = send(avr_socket, avr_cmd::volumestatus, 
                        strlen(avr_cmd::volumestatus), 0);
    if (response == SOCKET_ERROR) { 
        std::cout << "Failed to send test command\n";
        return 1;        
    }
    response = recv(avr_socket, avr_inbuffer, avr_inbufferlen, 0);
    if (response == SOCKET_ERROR) { 
        std::cout << "Failed to receive response from AVR\n";
        return 2;        
    }
    std::cout << "Received following from AVR:\n";
    for (int i = 0; i < avr_inbufferlen; i++) {
        std::cout << avr_inbuffer[i];
    }
    std::cout << "\n";
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