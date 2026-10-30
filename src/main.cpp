#include <iostream>
#include <format>
#include "avr_handler.h"
#include "keyboard_hook.h"
#include "timing.h"
#include "logger.h"

int main(int argc, char* argv[]) {

    #ifndef NDEBUG
    std::cout << std::unitbuf;
    #endif

    std::string ip_address_string = (argc < 2) ? "192.168.1.200" : argv[1];
    if (argc > 2) std::cout << "Note: command line arguments beyond the first are ignored!\n";

    std::cout << "Logger starting\n";
    Logger log;
    int response = log.Start(true) ? 0 : 1;
    if (response != 0) {
        std::cerr << "[ERROR] Logger failed to start with error code: " << response << "\n";
        return 1;
    }

    {
        auto log_lambda = [&log](std::string_view message) -> bool { return log.Log(message); };
        log.Log("AVManager starting");
        AVRHandler avr{log_lambda, ip_address_string, Verbosity::Info};
        AVRHandler::CtorCode avr_status = avr.GetCtorStatus();
        if (avr_status != AVRHandler::CtorCode::Healthy) {
            log.Log(std::format("[ERROR] AVRHandler creation failed with error code: {}",
                    static_cast<int>(avr_status)));
            return 2;
        }

        log.Log("KeyboardHook starting");
        KeyboardHook keyhook;
        response = keyhook.Start() ? 0 : 1;
        if (response != 0) {
            log.Log("[ERROR] KeyboardHook failed to start");
            return 3;
        }

        log.Log("Main loop starting\n=============================================\n ");
        AVRHandler::ExitCode exitcode = avr.ControlLoop();
        log.Log("\n=============================================", false);
        log.Log("Main loop ended");
        keyhook.Stop();
        if ((exitcode != AVRHandler::ExitCode::Normal) && (exitcode != AVRHandler::ExitCode::Commanded)) {
            log.Log(std::format("[ERROR] Main loop ended uncommanded with error: {}",
                    static_cast<int>(exitcode)));
            return 4;
        }
    }
    return 0;
}
