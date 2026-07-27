//
// Created by xyfuture on 2023/4/10.
//
#include "Chip.h"
#include <fstream>
#include <fmt/core.h>
#include <ghc/filesystem.hpp>
#include <algorithm>
#include <limits>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace fs = ghc::filesystem;

namespace {

nlohmann::json parseJsonFile(const fs::path& path, const std::string& description) {
    std::ifstream input(path.string());
    if (!input)
        throw std::runtime_error("Failed to open " + description + ": " + path.string());
    try {
        return nlohmann::json::parse(input);
    } catch (const std::exception& error) {
        throw std::runtime_error("Failed to parse " + description + " '" + path.string() + "': " + error.what());
    }
}

int getCoreCount(const nlohmann::json& config, int configured_core_count, const std::string& context) {
    if (!config.contains("core_cnt") || !config.at("core_cnt").is_number_integer())
        throw std::runtime_error(context + " field 'core_cnt' must be a positive integer");
    const int core_count = config.at("core_cnt").get<int>();
    if (core_count <= 0 || core_count > configured_core_count)
        throw std::runtime_error(context + " field 'core_cnt' is " + std::to_string(core_count)
                + ", but the simulator config supports 1.." + std::to_string(configured_core_count));
    return core_count;
}

std::vector<int> resolveArrayGroupMap(const nlohmann::json& config, const std::string& core_name,
                                      const std::vector<Instruction>& instructions, int crossbar_count) {
    if (!config.contains("array_group_map") || !config.at("array_group_map").is_object())
        throw std::runtime_error("Instruction config field 'array_group_map' must be an object");
    if (!config.at("array_group_map").contains(core_name))
        throw std::runtime_error("Instruction config is missing 'array_group_map." + core_name + "'");
    const auto& entry = config.at("array_group_map").at(core_name);
    if (!entry.is_array())
        throw std::runtime_error("Instruction config field 'array_group_map." + core_name + "' must be an array");

    std::vector<int> mapping;
    long long mapped_crossbars = 0;
    for (const auto& value : entry) {
        if (!value.is_number_integer() || value.get<int>() <= 0)
            throw std::runtime_error("Instruction config field 'array_group_map." + core_name
                    + "' must contain positive crossbar counts");
        mapping.push_back(value.get<int>());
        mapped_crossbars += value.get<int>();
    }
    if (mapped_crossbars > crossbar_count)
        throw std::runtime_error("Instruction config field 'array_group_map." + core_name
                + "' assigns " + std::to_string(mapped_crossbars) + " crossbars, but hardware provides "
                + std::to_string(crossbar_count));

    for (const auto& instruction : instructions) {
        if (instruction.op == +Opcode::mvmul
                && (!instruction.matrix || instruction.matrix->group < 0
                    || instruction.matrix->group >= static_cast<int>(mapping.size())))
            throw std::runtime_error("Instruction for " + core_name + " references array group "
                    + std::to_string(instruction.matrix ? instruction.matrix->group : -1)
                    + " outside 'array_group_map." + core_name + "'");
    }
    return mapping;
}

std::vector<Instruction> readCoreInstructions(const fs::path& path) {
    try {
        if (path.extension() == ".pim") {
            std::ifstream input(path.string(), std::ios::binary);
            if (!input)
                throw std::runtime_error("Failed to open binary instruction file");
            return readSingleCoreInstFromBinary(input);
        }
        const auto json = parseJsonFile(path, "JSON instruction file");
        if (!json.is_array())
            throw std::runtime_error("JSON instruction file must contain an array");
        return readSingleCoreInstFromJson(json);
    } catch (const std::exception& error) {
        throw std::runtime_error("Failed to load instructions from '" + path.string() + "': " + error.what());
    }
}

} // namespace

Chip::Chip(const ChipConfig &chip_config_,const SimConfig& sim_config_)
: global_memory("global_memory", chip_config_.global_memory_config, chip_config_.global_memory_switch_id),
  network("global_network", chip_config_.network_config),
  chip_config(chip_config_), sim_config(sim_config_),
  clk("ClockDomain",chip_config_.core_config.period){

    global_memory.mem_switch.bind(&network);

    // initializeCores() will process all cores

}

void Chip::setEnergyCounter() {
    energy_counter.initialize(); // set to zero

    energy_counter.setRunningTimeNS(running_time.to_seconds() * 1e9);

    for (auto &core: core_array) {
        auto cur_energy_counter = core->getEnergyCounter();
        energy_counter += cur_energy_counter;
    }

    energy_counter += network.getEnergyCounter();
    energy_counter += global_memory.getEnergyCounter();


    if (sim_config.sim_mode == 1){ // latency mode
        energy_counter.setRunningTimeNS(running_time.to_seconds() * 1e9);
    }  else if (sim_config.sim_mode == 0)
        energy_counter.setRunningTimeNS(sc_time_stamp().to_seconds()*1e9);
}

void Chip::setInstBuffer(const std::vector<std::vector<Instruction>> &inst_buffers) {
    assert(inst_buffers.size() == core_array.size());
    for (int i=0;i<chip_config.core_cnt;i++){
        core_array[i]->setInstBuffer(inst_buffers[i]);
    }
}

void Chip::readInstFromJson(const nlohmann::json &json_inst) {
    assert(json_inst.size() == core_array.size());
    for (int i=0;i<chip_config.core_cnt;i++){
        auto core_key = std::string ("core")+std::to_string(i);
        const auto& core_json_inst = json_inst.at(core_key);
        core_array[i]->readInstFromJson(core_json_inst);
    }
}

bool Chip::isFinish() {
    for (auto& core_ptr:core_array){
        if (!core_ptr->isFinish())
            return false;
    }
    return true;
}


std::string Chip::getSimulationReport() {
    // chip-level simulation results
    setRunningTime();
    setEnergyCounter();
    std::stringstream  s;

    s<<"Chip Simulation Result:\n";


    if (sim_config.sim_mode == 0){
        // throughput mode
        const double run_rounds = getRunRounds();
        s<<fmt::format("  - {:<20}{:.3} samples\n","output count:",run_rounds);
        s<<fmt::format("  - {:<20}{:.3} samples/s\n","throughput:",(run_rounds/(sim_config.sim_time/1e3)));
        s<<fmt::format("  - {:<20}{:.10} ms\n","average latency:",
                      run_rounds == 0 ? 0 : sim_config.sim_time/run_rounds);
        s<<fmt::format("  - {:<20}{:.10} mW\n","average power:",(energy_counter.getAveragePowerMW()));
        s<<fmt::format("  - {:<20}{:.10} pJ/it\n","average energy:",
                      run_rounds == 0 ? 0 : energy_counter.getTotalEnergyPJ()/run_rounds);
    }
    else if (sim_config.sim_mode == 1){
        // latency mode
        s<<fmt::format("  - {:<20}{:.10} ms\n","latency:",running_time.to_seconds()*1000);
        s<<fmt::format("  - {:<20}{:.10} mW\n","average power:",(energy_counter.getAveragePowerMW()));
        s<<fmt::format("  - {:<20}{} pJ\n","average energy:",(energy_counter.getTotalEnergyPJ()));
    }

    // more information
    if (sim_config.report_verbose_level >= 1){
        // core information
        for (const auto& core:core_array){
            s<<core->getSimulationReport();
        }
    }

    return s.str();
}

double Chip::getRunRounds() {
    double cnt=0;
    int cores = 0;
    for (const auto& core_ptr:core_array){
        cnt += core_ptr->getRunRounds();
        cores ++ ;
    }
    // average rounds
//    return cnt/chip_config.core_cnt;
    return cnt/cores;
}

void Chip::initializeCores(const nlohmann::json &json_inst) {
    if (!json_inst.contains("config") || !json_inst.at("config").is_object())
        throw std::runtime_error("Single-file instruction input requires an object field 'config'");
    const auto& config = json_inst.at("config");
    const int core_count = getCoreCount(config, chip_config.core_cnt, "Instruction config");
    std::map<int, std::vector<Instruction>> instructions_by_core;
    std::map<int, std::vector<int>> mappings_by_core;

    for (int core_id = 0; core_id < core_count; ++core_id) {
        const auto core_name = std::string("core") + std::to_string(core_id);
        if (!json_inst.contains(core_name))
            continue;
        if (!json_inst.at(core_name).is_array())
            throw std::runtime_error("Single-file instruction field '" + core_name + "' must be an array");
        instructions_by_core[core_id] = readSingleCoreInstFromJson(json_inst.at(core_name));
        mappings_by_core[core_id] = resolveArrayGroupMap(
                config, core_name, instructions_by_core.at(core_id),
                chip_config.core_config.matrix_config.xbar_array_count);
    }

    if (instructions_by_core.empty())
        throw std::runtime_error("Single-file instruction input contains no configured core instructions");
    for (const auto& item : instructions_by_core)
        initializeCore(item.first, mappings_by_core.at(item.first), item.second);
}

void Chip::initializeCoresFromDirectory(const nlohmann::json& json_config, const std::string& instruction_dir) {
    const fs::path directory(instruction_dir);
    if (!fs::is_directory(directory))
        throw std::runtime_error("Per-core instruction input is not a directory: " + instruction_dir);
    const int core_count = getCoreCount(json_config, chip_config.core_cnt, "Instruction directory config");
    const std::regex core_file_pattern("^core_([0-9]+)\\.(json|pim)$");
    std::map<int, fs::path> files_by_core;

    std::vector<fs::path> directory_entries;
    for (const auto& entry : fs::directory_iterator(directory))
        directory_entries.push_back(entry.path());
    std::sort(directory_entries.begin(), directory_entries.end());

    for (const auto& path : directory_entries) {
        if (!fs::is_regular_file(path))
            continue;
        const std::string filename = path.filename().string();
        if (filename.compare(0, 5, "core_") != 0)
            continue;
        std::smatch match;
        if (!std::regex_match(filename, match, core_file_pattern))
            throw std::runtime_error("Malformed per-core instruction filename '" + filename
                    + "' in " + instruction_dir + "; expected core_<id>.json or core_<id>.pim");
        unsigned long parsed_core_id = 0;
        try {
            parsed_core_id = std::stoul(match[1].str());
        } catch (const std::exception&) {
            throw std::runtime_error("Malformed core identifier in instruction file: " + path.string());
        }
        if (parsed_core_id > static_cast<unsigned long>(std::numeric_limits<int>::max()))
            throw std::runtime_error("Core identifier is too large in instruction file: " + path.string());
        const int core_id = static_cast<int>(parsed_core_id);
        if (core_id < 0 || core_id >= core_count)
            throw std::runtime_error("Instruction file '" + path.string() + "' assigns core "
                    + std::to_string(core_id) + ", but config core_cnt is " + std::to_string(core_count));
        if (!files_by_core.emplace(core_id, path).second)
            throw std::runtime_error("Duplicate instruction files assign core " + std::to_string(core_id)
                    + " in " + instruction_dir);
    }

    if (files_by_core.empty())
        throw std::runtime_error("Instruction directory contains no core_<id>.json or core_<id>.pim files: "
                + instruction_dir);
    for (int core_id = 0; core_id < core_count; ++core_id) {
        if (!files_by_core.count(core_id))
            throw std::runtime_error("Instruction directory is missing core_" + std::to_string(core_id)
                    + ".json or core_" + std::to_string(core_id) + ".pim: " + instruction_dir);
    }

    std::map<int, std::vector<Instruction>> instructions_by_core;
    std::map<int, std::vector<int>> mappings_by_core;
    for (const auto& item : files_by_core) {
        const auto core_name = std::string("core") + std::to_string(item.first);
        instructions_by_core[item.first] = readCoreInstructions(item.second);
        mappings_by_core[item.first] = resolveArrayGroupMap(
                json_config, core_name, instructions_by_core.at(item.first),
                chip_config.core_config.matrix_config.xbar_array_count);
    }
    for (const auto& item : instructions_by_core)
        initializeCore(item.first, mappings_by_core.at(item.first), item.second);
}

void Chip::initializeCoresWithDir(const std::string& instruction_dir) {
    const fs::path config_path = fs::path(instruction_dir) / "config.json";
    initializeCoresFromDirectory(parseJsonFile(config_path, "instruction directory config"), instruction_dir);
}

void Chip::initializeCore(int core_id, const std::vector<int>& array_group_map,
                          const std::vector<Instruction>& instructions) {
    const auto core_name = std::string("core") + std::to_string(core_id);
    auto core = std::make_shared<Core>(
            core_name.c_str(), chip_config.core_config, sim_config, core_id, array_group_map, this, &clk);
    core->switchBind(&network);
    core->setInstBuffer(instructions);
    core_array.push_back(core);
}

std::map<std::string, double> Chip::getChipWeightedTime() {
    std::map<std::string,double> chip_weighted_time;
    for (const auto& core : core_array){
        auto weighted_time= core->reorder_buffer.perf_counter.getWeightedStatistics();
        for(const auto& item:weighted_time){
            if (chip_weighted_time.count(item.first)){
                chip_weighted_time[item.first] += item.second;
            } else
                chip_weighted_time[item.first] = item.second;
        }
    }
    return chip_weighted_time;
}

void Chip::setRunningTime() {

    if (sim_config.sim_mode == 0){
        running_time = sc_time_stamp();
    } else if (sim_config.sim_mode == 1){
        // the last core time
        running_time = sc_time(0,SC_NS);

        for (const auto& core : core_array)
            if (core->getFinishTime()>running_time)
                running_time = core->getFinishTime();
    }


}

std::string Chip::getChipWeightedTimeReport() {
    std::stringstream  s;
    auto chip_weighted_time = getChipWeightedTime();
    s<<"Chip Weighted Time:\n";
    for(const auto& item : chip_weighted_time){
        s<<item.first<<" : "<<item.second<<"\n";
    }

    return s.str();
}


std::string Chip::getCoreWeightedTimeReport(){
    std::stringstream  s;

    s<<"Cores Weighted Time:\n";

    for (const auto& core_ptr:core_array){
        auto core_id = core_ptr->getCoreID();
        auto weighted_time = core_ptr->reorder_buffer.perf_counter.getWeightedStatistics();
        s<<"Core:"<<core_id<<std::endl;
        for (const auto& item:weighted_time){
            std::cout<<item.first<<" time: "<<item.second<<std::endl;
        }
        s<<std::endl;
    }

    return s.str();
}
