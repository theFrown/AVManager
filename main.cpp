#include <iostream>
#include "AVRHandler.h"
#include "keyboard.h"
#include "timing.h"

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

    std::cout << "KeyboardHook starting\n";
    Timer sleeper;
    KeyboardHook keyhook;
    keyhook.PrintStates();
    response = keyhook.Start() ? 0 : 1;
    if (response == 0) {
        std::cout << "yay, good keyboardhook\n";
    }
    else {
        std::cout << "boo, keyboardhook error\n";
        return 2;
    }   

    std::cout << "keypresses before control loop:\n";
    sleeper.Set(5000);
    sleeper.Wait(); 
    std::cout << "\n==============TEST LOOP==============\n\n";
    response = avr.ControlLoop();
    std::cout << "keypresses after control loop:\n";
    sleeper.Set(5000);
    sleeper.Wait();
    keyhook.Stop();
    if (response == 0) {
        std::cout << "yay, good loop\n";
    }
    else {
        std::cout << "boo, loop error:\n" << response << "\n";
        return 3;
    }   
    return 0;
}
