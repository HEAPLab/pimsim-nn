//
// Created by xyfuture on 2023/5/2.
//

#include "Simulator.h"
#include "chip/Chip.h"
#include "utils/Timer.h"

#include <utility>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <fmt/core.h>
#include <zstr.hpp>
#include <ghc/filesystem.hpp>
#include <chrono>


//namespace fs = std::filesystem;
namespace fs = ghc::filesystem;

namespace {

nlohmann::json parseJsonFile(const std::string& path, const std::string& description) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("Failed to open " + description + ": " + path);
    try {
        return nlohmann::json::parse(input);
    } catch (const std::exception& error) {
        throw std::runtime_error("Failed to parse " + description + " '" + path + "': " + error.what());
    }
}

const char* inputModeName(InstructionInputMode mode) {
    switch (mode) {
        case InstructionInputMode::Auto: return "auto";
        case InstructionInputMode::SingleFile: return "single-file";
        case InstructionInputMode::PerCoreDirectory: return "per-core-directory";
    }
    return "unknown";
}

InstructionInputMode resolveInputMode(const std::string& path, InstructionInputMode selected_mode) {
    const bool is_file = fs::is_regular_file(path);
    const bool is_directory = fs::is_directory(path);
    if (selected_mode == InstructionInputMode::Auto) {
        if (is_file)
            return InstructionInputMode::SingleFile;
        if (is_directory)
            return InstructionInputMode::PerCoreDirectory;
        throw std::runtime_error("Instruction input path is neither a regular file nor a directory: " + path);
    }
    if (selected_mode == InstructionInputMode::SingleFile && !is_file)
        throw std::runtime_error("Instruction input mode 'single-file' requires a regular file, but got: " + path);
    if (selected_mode == InstructionInputMode::PerCoreDirectory && !is_directory)
        throw std::runtime_error("Instruction input mode 'per-core-directory' requires a directory, but got: " + path);
    return selected_mode;
}

std::string unfinishedCoreReport(const Chip& chip) {
    std::stringstream report;
    bool first = true;
    for (const auto& core : chip.core_array) {
        if (core->isFinish())
            continue;
        report << (first ? "" : ", ") << "core " << core->getCoreID()
               << " (pc=" << core->inst_fetch.getCurrentPC()
               << ", max_pc=" << core->getMaxPC()
               << ", fetch_complete=" << (core->inst_fetch.isFinish() ? "true" : "false") << ")";
        first = false;
    }
    return first ? "none" : report.str();
}

} // namespace

Simulator::Simulator(std::string config_file_path_, std::string inst_file_path_):
        Simulator(std::move(config_file_path_), std::move(inst_file_path_), InstructionInputMode::Auto) {}

Simulator::Simulator(std::string config_file_path_, std::string inst_file_path_,
                     InstructionInputMode input_mode_):
        config_file_path(std::move(config_file_path_)), inst_file_path(std::move(inst_file_path_)),
        input_mode(input_mode_) {}

void Simulator::runSimulation() {

    auto start = std::chrono::high_resolution_clock::now();

    fs::path config_path(config_file_path);
    auto config_parent_path = config_path.parent_path();

    std::cout<<"Loading Inst and Config --- "<<std::endl;
    const auto json_config = parseJsonFile(config_file_path, "simulator config");
    global_config = json_config.get<GlobalConfig>();
    global_config.checkValid();
    const InstructionInputMode resolved_mode = resolveInputMode(inst_file_path, input_mode);

    chip_ptr = std::make_shared<Chip>(global_config.chip_config,global_config.sim_config);

    std::cout<<"Reading Instructions From File --- "<<std::endl;
    if (resolved_mode == InstructionInputMode::PerCoreDirectory) {
        chip_ptr->initializeCoresWithDir(inst_file_path);
    } else {
        if (fs::path(inst_file_path).extension() == ".pim")
            throw std::runtime_error("Instruction input '" + inst_file_path
                    + "' is a per-core binary .pim file; use a directory for binary instruction input");
        zstr::ifstream instruction_file(inst_file_path, std::ios::binary);
        if (!instruction_file)
            throw std::runtime_error("Failed to open single-file instruction input: " + inst_file_path);
        try {
            chip_ptr->initializeCores(nlohmann::json::parse(instruction_file));
        } catch (const nlohmann::json::exception& error) {
            throw std::runtime_error("Failed to parse single-file instruction input '" + inst_file_path
                    + "': " + error.what());
        }
    }
    chip_ptr->network.readLatencyEnergyFile(config_parent_path.string());
    std::cout<<"Read finish"<<std::endl;

    std::cout<<"Start Simulation --- "<<std::endl;
    if (global_config.sim_config.sim_mode == 0) {
        int levels = is_run_in_gui?100:10;
        ProgressBar bar(SC_MS,levels,global_config.sim_config.sim_time,[this](int progress){
            if (is_run_in_gui)
                std::cout<<fmt::format("<{}>",progress)<<std::endl;
            else
                std::cout<<fmt::format("Progress --- <{}0%>",progress)<<std::endl;
        });
        sc_start(global_config.sim_config.sim_time,sc_core::SC_MS);
    } else {
        while (!chip_ptr->isFinish())
            sc_start(global_config.sim_config.sim_time, sc_core::SC_MS);
    }
    std::cout<<"Simulation Finish"<<std::endl;

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    double executionTime = duration.count();
    std::cout<<"simulator execution time:"<<executionTime<<"s"<<std::endl;
    std::cout<<getSimulationReport()<<std::endl;

}

std::string Simulator::getSimulationReport() {
    std::stringstream  s;

    auto sub_line = "  - {:<20}{}\n";
    s<<"|*************** Simulation Report ***************|\n";
    s<<"Basic Information:\n";
    s<<fmt::format(sub_line, "config file:",config_file_path);
    s<<fmt::format(sub_line, "inst file:",inst_file_path);
    s<<fmt::format(sub_line, "verbose level:", global_config.sim_config.report_verbose_level);
    s<<fmt::format(sub_line, "core count:", global_config.chip_config.core_cnt);
    s<<fmt::format(sub_line, "simulation mode:",global_config.sim_config.sim_mode);

    if (global_config.sim_config.sim_mode == 0)
        s<<fmt::format("  - {:<20}{} ms\n","simulation time:",global_config.sim_config.sim_time);


    return s.str() + chip_ptr->getSimulationReport();
}

void Simulator::progressBar() {
    int progress = (sc_time_stamp().to_seconds() / (global_config.sim_config.sim_time/1000)) * 100;
    if (is_run_in_gui)
        std::cout<<fmt::format("<{}>",progress)<<std::endl;
    else
        std::cout<<fmt::format("Progress --- <{}%>",progress)<<std::endl;
}

void Simulator::setRunInGUI(bool mode) {
    is_run_in_gui = mode;
}

std::string Simulator::getBasicInformation() {
    stringstream s;
    auto sub_line = "  - {:<20}{}\n";

    s<<"Basic Information:\n";
    s<<fmt::format(sub_line, "config file:",config_file_path);
    s<<fmt::format(sub_line, "inst file:",inst_file_path);
    s<<fmt::format(sub_line, "verbose level:", global_config.sim_config.report_verbose_level);
    s<<fmt::format(sub_line, "core count:", global_config.chip_config.core_cnt);
    s<<fmt::format(sub_line, "simulation mode:",global_config.sim_config.sim_mode);

    return s.str();
}
