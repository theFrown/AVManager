#include <iostream>
#include "AVRHandler.h"
#include "keyboard.h"

int main() {

    #ifndef NDEBUG
    std::cout << std::unitbuf;
    #endif

    std::cout << "KeyboardHook starting\n";
    KeyboardHook keyhook;
    if (keyhook.status != 0) {
        std::cout << "boo, error:\n" << keyhook.status << "\n";
        return 1;
    }
    int response = keyhook.Run();


    // std::cout << "AVManager starting\n";
    // AVRHandler avr{Verbosity::Debug};
    // int response = avr.status;
    // if (response != 0) {
    //     std::cout << "boo, error:\n" << response << "\n";
    //     return 1;
    // }
    // std::cout << "\n==============TEST LOOP==============\n\n";
    // response = avr.ControlLoop();

    if (response == 0) {
        std::cout << "yay\n";
    }
    else {
        std::cout << "boo, error:\n" << response << "\n";
        return 2;
    }   
    return 0;
}
