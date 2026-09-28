#include <iostream>
#include "AVRHandler.h"
// #include "keyboard.h"
// #include "timing.h"

int main() {

    #ifndef NDEBUG
    std::cout << std::unitbuf;
    #endif

    // std::cout << "KeyboardHook starting\n";
    // Timer sleeper{1000};
    // KeyboardHook keyhook;
    // keyhook.PrintStates();
    // bool response = keyhook.Start();
    // for (int i = 0; i < 10; i++) {
    //     sleeper.Set();
    //     sleeper.Wait();
    //     keyhook.PrintStates();
    //     switch (keyhook.GetPingKeyStatus()) {
    //         case KeyboardHook::PingStatus::Unknown:
    //             std::cout << "no key ping processed\n";
    //             break;
    //         case KeyboardHook::PingStatus::Received:
    //             std::cout << "previous key ping succesful\n";
    //             break;
    //         default:
    //             std::cout << std::format("unexpected key ping state: {}\n)",
    //                                      static_cast<int>(keyhook.GetPingKeyStatus()));        }
    //     switch (keyhook.GetPingMsgStatus()) {
    //         case KeyboardHook::PingStatus::Unknown:
    //             std::cout << "no msg ping processed\n";
    //             break;
    //         case KeyboardHook::PingStatus::Received:
    //             std::cout << "previous msg ping succesfull\n";
    //             break;
    //         default:
    //             std::cout << std::format("unexpected msg ping state: {}\n)",
    //                                      static_cast<int>(keyhook.GetPingMsgStatus()));
    //     }
    //     keyhook.PingKey();
    //     keyhook.PingMsg();
    //     if (keyhook.GetWorkerStatus() != KeyboardHook::WorkerStatus::Running) break;
    // }
    // std::cin.get();
    // std::cin.ignore(1000, '\n');
    // keyhook.Stop();
    // keyhook.Start();
    // keyhook.Start();
    // std::cin.get();
    // keyhook.Stop();
    // keyhook.Stop();

    std::cout << "AVManager starting\n";
    AVRHandler avr{Verbosity::Debug};
    int response = avr.status;
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
