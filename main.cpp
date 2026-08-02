#include <iostream>
// #include <thread>
// #include <atomic>
// #include <winsock2.h>
#include "AVRHandler.h"

int main() {

    #ifndef NDEBUG
    std::cout << std::unitbuf;
    #endif

    std::cout << "AVManager starting\n";
    AVRHandler avr{Verbosity::Debug};
    int response = avr.status;
    if (response != 0) {
        std::cout << "boo, error:\n" << response << "\n";
        return 1;
    }
    std::cout << "\n==============TEST COMS==============\n\n";
    response = avr.testcoms();
    if (response != 0) {
        std::cout << "boo, error:\n" << response << "\n";
        return 1;
    }
    std::cout << "\n==============TEST LOOP==============\n\n";
    response = avr.ControlLoop();
    if (response == 0) {
        std::cout << "yay\n";
    }
    else {
        std::cout << "boo, error:\n" << response << "\n";
        return 2;
    }   
    return 0;
}