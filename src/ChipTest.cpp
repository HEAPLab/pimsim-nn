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
              << "       [--instruction-mode=auto|single|directory] [--gui[=true|false]]\n";
}

InstructionInputMode parseInstructionMode(const std::string& value) {
    if (value == "auto")
        return InstructionInputMode::Auto;
    if (value == "single")
        return InstructionInputMode::SingleFile;
    if (value == "directory")
        return InstructionInputMode::PerCoreDirectory;
    throw std::runtime_error("Unknown instruction mode '" + value
            + "'; expected auto, single, or directory");
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
        InstructionInputMode input_mode = InstructionInputMode::Auto;
        bool gui = false;
        for (int index = 3; index < argc; ++index) {
            const std::string argument(argv[index]);
            if (argument == "--gui") {
                gui = true;
            } else if (argument.compare(0, 6, "--gui=") == 0) {
                gui = parseBool(argument.substr(6), "--gui");
            } else if (argument.compare(0, 19, "--instruction-mode=") == 0) {
                input_mode = parseInstructionMode(argument.substr(19));
            } else {
                throw std::runtime_error("Unknown argument: " + argument);
            }
        }

        Simulator simulator(argv[2], argv[1], input_mode);
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
