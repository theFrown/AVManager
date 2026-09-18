#include <iostream>
#include "AVRHandler.h"
#include "keyboard.h"

int main() {

    #ifndef NDEBUG
    std::cout << std::unitbuf;
    #endif

    Timer sleeper{50};
    std::cout << "KeyboardHook starting\n";
    KeyboardHook keyhook;
    keyhook.PrintStates();
    bool response = keyhook.Start();
    for (int i = 0; i < 1000; i++) {
        sleeper.Set();
        sleeper.Wait();
        keyhook.PrintStates();
        if (keyhook.GetWorkerStatus() != KeyboardHook::WorkerStatus::Running) break;
    }
    std::cin.get();
    std::cin.ignore(1000, '\n');
    keyhook.Stop();
    keyhook.Start();
    keyhook.Start();
    std::cin.get();
    keyhook.Stop();
    keyhook.Stop();

    // std::cout << "AVManager starting\n";
    // AVRHandler avr{Verbosity::Debug};
    // int response = avr.status;
    // if (response != 0) {
    //     std::cout << "boo, error:\n" << response << "\n";
    //     return 1;
    // }
    // std::cout << "\n==============TEST LOOP==============\n\n";
    // response = avr.ControlLoop();

    if (response == true) {
        std::cout << "yay\n";
    }
    else {
        std::cout << "boo, error:\n" << response << "\n";
        return 2;
    }   
    return 0;
}
