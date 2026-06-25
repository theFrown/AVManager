#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

const char* my_avr_ip = "192.168.1.200";
const short my_avr_port = 23;

SOCKET my_socket;
sockaddr_in my_sockaddr{};

int SetupConnect() {
    std::cout << "Setting up connection..\n";
    WSADATA my_wsadata;
    int response;
    response = WSAStartup(MAKEWORD(2,2), &my_wsadata);
    if (response != 0) {
        std::cout << "WSAStartup failed with response:\n";
        std::cout << response;
        std::cout << "\n";
        return 1;
    }
    my_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (my_socket == INVALID_SOCKET) {
        std::cout << "Failed to create socket\n";
        return 2;
    }
    my_sockaddr.sin_family = AF_INET;
    my_sockaddr.sin_port   = htons(my_avr_port);
    inet_pton(AF_INET, my_avr_ip, &my_sockaddr.sin_addr);
    response = connect(my_socket, (sockaddr*)&my_sockaddr, sizeof(my_sockaddr));
    if (response != 0) { 
        std::cout << "Failed to connect socket with response:\n";
        std::cout << response;
        std::cout << "\n";
        return 3;
    }
    return 0;
}

int main() {
    std::cout << "AVManager starting\n";
    int response = SetupConnect();
    if (response == 0) {
        std::cout << "yay\n";
    }
    else {
        std::cout << "boo, error:\n";
        std::cout << response;
        std::cout << "\n";
        return 1;
    }
    return 0;
}