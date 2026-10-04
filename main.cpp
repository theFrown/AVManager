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
    AVRHandler::CtorCode avr_status = avr.GetCtorStatus();
    if (avr_status != AVRHandler::CtorCode::Healthy) {
        std::cout << "boo, error:\n" << static_cast<int>(avr_status) << "\n";
        return 1;
    }

    std::cout << "KeyboardHook starting\n";
    Timer sleeper;
    KeyboardHook keyhook;
    keyhook.PrintStates();
    int response = keyhook.Start() ? 0 : 1;
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
    AVRHandler::ExitCode exitcode = avr.ControlLoop();
    std::cout << "keypresses after control loop:\n";
    sleeper.Set(5000);
    sleeper.Wait();
    keyhook.Stop();
    if (exitcode == AVRHandler::ExitCode::Normal) {
        std::cout << "yay, good loop\n";
    }
    else {
        std::cout << "boo, loop error:\n" << response << "\n";
        return 3;
    }   
    return 0;
}
