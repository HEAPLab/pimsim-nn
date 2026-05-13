//
// Created by xyfuture on 2023/4/22.
//

#include<fstream>
#include <string>
#include "chip/Chip.h"
#include "Simulator.h"



int sc_main(int argc,char* argv[]){

    if (argc < 3 ) throw "Need Inst and Config args";
    Simulator sim(argv[2],argv[1]);
    if (argc == 4) {
        std::string gui_arg = argv[3];
        sim.setRunInGUI(gui_arg == "true" || gui_arg == "1");
    }
    sim.runSimulation();
//    std::cout<<sim.getSimulationReport()<<std::endl;

    return  0;
}
