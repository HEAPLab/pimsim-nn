//
// Created by xyfuture on 2023/4/10.
//



#ifndef CHIP_CHIP_H_
#define CHIP_CHIP_H_

#include "core/Core.h"
#include "memory/ExternalMemory.h"
#include "network/Network.h"
#include "config/Config.hpp"
#include "utils/ClockDomain.h"
#include <string>

// Chip: Contains many cores, noc, global memory
class Chip {
public:
    Chip(const ChipConfig& chip_config_,const SimConfig& sim_config_);

    void setInstBuffer(const std::vector<std::vector<Instruction>>& inst_buffers);

    void readInstFromJson(const nlohmann::json& json_inst);

    void initializeCores(const nlohmann::json& json_inst);
    void initializeCoresFromDirectory(const nlohmann::json& json_config, const std::string& instruction_dir);
    void initializeCoresWithDir(const std::string& instruction_dir);

    bool isFinish();

    double getRunRounds();
    void enableFastThroughput();
    void updateFastThroughput();
    bool isFastThroughputConverged() const;

    void setEnergyCounter();

    std::string getSimulationReport();

    std::map<std::string,double> getChipWeightedTime();

    std::string getChipWeightedTimeReport();
    std::string getCoreWeightedTimeReport();

    void setRunningTime();


public:
    std::vector<std::shared_ptr<Core> > core_array; // size unsure
                                                    // lazy initialize

    ExternalMemory global_memory;
    Network network;

    ClockDomain clk;

public:

    const ChipConfig chip_config;
    const SimConfig sim_config;

    sc_time running_time;


private:
    void initializeCore(int core_id, const std::vector<int>& array_group_map,
                        const std::vector<Instruction>& instructions);

    EnergyCounter energy_counter;

    bool fast_throughput = false;
    bool fast_throughput_converged = false;
    int fast_next_boundary_round = 2;
    std::vector<sc_time> fast_window_boundaries;


};


#endif //CHIP_CHIP_H_
