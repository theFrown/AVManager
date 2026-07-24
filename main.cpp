#include <iostream>
// #include <thread>
// #include <atomic>
// #include <winsock2.h>
#include "AVRHandler.h"

int main() {
    std::cout << "AVManager starting\n";
    AVRHandler avr{};
    int response = avr.status;
    if (response != 0) {
        std::cout << "boo, error:\n" << response << "\n";
        return 1;
    }
    response = avr.testcoms();
    if (response == 0) {
        std::cout << "yay\n";
    }
    else {
        std::cout << "boo, error:\n" << response << "\n";
        return 2;
    }   
    return 0;
}