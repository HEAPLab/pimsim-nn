//
// Created by xyfuture on 2023/4/22.
//

#include <iostream>
#include <stdexcept>
#include <string>
#include "Simulator.h"

namespace {

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " <instruction_path> <config_file>\n"
              << "       [--gui[=true|false]]\n";
}

bool parseBool(const std::string& value, const std::string& option) {
    if (value == "true" || value == "1")
        return true;
    if (value == "false" || value == "0")
        return false;
    throw std::runtime_error(option + " expects true, false, 1, or 0");
}

} // namespace

int sc_main(int argc,char* argv[]){
    if (argc == 2 && std::string(argv[1]) == "--help") {
        printUsage(argv[0]);
        return 0;
    }
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    try {
        bool gui = false;
        for (int index = 3; index < argc; ++index) {
            const std::string argument(argv[index]);
            if (argument == "--gui") {
                gui = true;
            } else if (argument.compare(0, 6, "--gui=") == 0) {
                gui = parseBool(argument.substr(6), "--gui");
            } else {
                throw std::runtime_error("Unknown argument: " + argument);
            }
        }

        Simulator simulator(argv[2], argv[1]);
        simulator.setRunInGUI(gui);
        simulator.runSimulation();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Simulation error: " << error.what() << std::endl;
    } catch (const char* error) {
        std::cerr << "Simulation error: " << error << std::endl;
    }
    return 1;
}
