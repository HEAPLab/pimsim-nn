#include "chip/Chip.h"
#include "isa/Instruction.h"

#include <fstream>
#include <ghc/filesystem.hpp>
#include <string>
#include <systemc>
#include <unistd.h>

namespace fs = ghc::filesystem;

int sc_main(int, char*[]) {
    const fs::path directory = fs::temp_directory_path()
            / ("pimsim-instruction-directory-" + std::to_string(getpid()));
    fs::create_directory(directory);

    try {
        std::ofstream config((directory / "config.json").string());
        config << R"({"core_cnt":1,"array_group_map":{"core0":[1]}})";
        config.close();

        const auto json = nlohmann::json::parse(R"([{"imm":7,"op":"sldi","rd":0}])");
        const auto expected = readSingleCoreInstFromJson(json);
        std::ofstream json_file((directory / "core_0.json").string());
        json_file << R"([{"imm":3,"op":"sldi","rd":0}])";
        json_file.close();
        std::ofstream binary((directory / "core_0.pim").string(), std::ios::binary);
        writeSingleCoreInstToBinary(binary, expected);
        binary.close();

        ChipConfig chip_config;
        chip_config.core_cnt = 1;
        chip_config.core_config.matrix_config.xbar_array_count = 4;
        SimConfig sim_config;
        Chip chip(chip_config, sim_config);
        chip.initializeCoresWithDir(directory.string());

        const bool valid = chip.core_array.size() == 1
                && chip.core_array.front()->inst_fetch.inst_buffer == expected;
        fs::remove_all(directory);
        return valid ? 0 : 1;
    } catch (...) {
        fs::remove_all(directory);
        throw;
    }
}
