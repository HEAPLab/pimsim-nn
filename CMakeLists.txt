
# project
cmake_minimum_required(VERSION 3.1)
#cmake_policy(SET CMP0091 NEW)
project(pimsim-nn LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 11 CACHE STRING "Use C++ 11")
set(CMAKE_VERBOSE_MAKEFILE ON)
enable_testing()

# dependency
add_subdirectory(packages/fmt)
add_subdirectory(packages/systemc)
add_subdirectory(packages/json)
add_subdirectory(packages/zlib)
add_subdirectory(packages/filesystem)



add_library(pimsim-nn STATIC "" )
set_target_properties(pimsim-nn PROPERTIES OUTPUT_NAME "pimsim-nn")
target_include_directories(pimsim-nn PRIVATE
        src
        )


# header-only dependency
target_include_directories(pimsim-nn PUBLIC packages/header-only)
target_include_directories(pimsim-nn PUBLIC packages/header-only/zstr/src)

add_dependencies(pimsim-nn systemc zlibstatic nlohmann_json fmt ghc_filesystem)

target_link_libraries(pimsim-nn PUBLIC
        systemc
        zlibstatic
        nlohmann_json
        fmt
        ghc_filesystem
        )


target_sources(pimsim-nn PRIVATE
        src/chip/Chip.cpp
        src/comm/InitiatorSocket.cpp
        src/comm/TargetSocket.cpp
        src/core/BaseCoreModule.cpp
        src/core/component/Controller.cpp
        src/core/component/InstDecode.cpp
        src/core/component/InstFetch.cpp
        src/core/component/MatrixUnit.cpp
        src/core/component/RegFile.cpp
        src/core/component/ScalarUnit.cpp
        src/core/component/TransferUnit.cpp
        src/core/component/VectorUnit.cpp
        src/core/Core.cpp
        src/core/xbar/XbarArray.cpp
        src/memory/ExternalMemory.cpp
        src/memory/Memory.cpp
        src/memory/MemoryWrapper.cpp
        src/network/Network.cpp
        src/network/Switch.cpp
        src/network/SwitchSocket.cpp
        src/Simulator.cpp
        src/utils/ClockDomain.cpp
        src/utils/EnergyCounter.cpp
        src/utils/Timer.cpp
        src/isa/Instruction.cpp
        src/core/payloads/ExecInfo.cpp
        src/analysis/PerformanceCounter.cpp
        src/core/xbar/ArrayGroup.cpp
        src/core/component/ReorderBuffer.cpp 
        src/core/component/Dispatcher.cpp
        )


# entry point
add_executable(ChipTest "")
add_dependencies(ChipTest pimsim-nn)
target_include_directories(ChipTest PRIVATE
    src
)

target_link_libraries(ChipTest PRIVATE
        pimsim-nn
    )

#target_link_libraries(ChipTest PRIVATE
#    systemc
#    pimsim-nn
#    zlibstatic
#)

target_sources(ChipTest PRIVATE
    src/ChipTest.cpp
)

add_executable(InstructionBinaryTest
    src/isa/InstructionBinaryTest.cpp
)
target_include_directories(InstructionBinaryTest PRIVATE
    src
)
target_link_libraries(InstructionBinaryTest PRIVATE
    pimsim-nn
)
add_test(NAME InstructionBinaryTest COMMAND InstructionBinaryTest)

add_executable(InstructionDirectoryTest
    src/isa/InstructionDirectoryTest.cpp
)
target_include_directories(InstructionDirectoryTest PRIVATE
    src
)
target_link_libraries(InstructionDirectoryTest PRIVATE
    pimsim-nn
)
add_test(NAME InstructionDirectoryTest COMMAND InstructionDirectoryTest)

set(TEST_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures")
add_test(NAME AutomaticSingleFile
    COMMAND ChipTest "${TEST_FIXTURES}/single.json" "${TEST_FIXTURES}/latency_config.json")
add_test(NAME ExplicitSingleFile
    COMMAND ChipTest "${TEST_FIXTURES}/single.json" "${TEST_FIXTURES}/latency_config.json"
            --instruction-mode=single)
add_test(NAME AutomaticDirectory
    COMMAND ChipTest "${TEST_FIXTURES}/directory" "${TEST_FIXTURES}/latency_config.json")
add_test(NAME ExplicitPerCoreDirectory
    COMMAND ChipTest "${TEST_FIXTURES}/directory" "${TEST_FIXTURES}/latency_config.json"
            --instruction-mode=directory)
add_test(NAME ThroughputProgress
    COMMAND ChipTest "${TEST_FIXTURES}/throughput.json" "${TEST_FIXTURES}/throughput_config.json")
set_tests_properties(ThroughputProgress PROPERTIES PASS_REGULAR_EXPRESSION "Progress --- <100%>")
add_test(NAME ThroughputGuiProgress
    COMMAND ChipTest "${TEST_FIXTURES}/throughput.json" "${TEST_FIXTURES}/throughput_config.json" --gui)
set_tests_properties(ThroughputGuiProgress PROPERTIES PASS_REGULAR_EXPRESSION "<100>")

add_test(NAME DirectoryModeRejectsFile
    COMMAND ChipTest "${TEST_FIXTURES}/single.json" "${TEST_FIXTURES}/latency_config.json"
            --instruction-mode=directory)
add_test(NAME SingleModeRejectsDirectory
    COMMAND ChipTest "${TEST_FIXTURES}/directory" "${TEST_FIXTURES}/latency_config.json"
            --instruction-mode=single)
add_test(NAME MalformedArrayGroupMap
    COMMAND ChipTest "${TEST_FIXTURES}/malformed_map" "${TEST_FIXTURES}/latency_config.json")
add_test(NAME MissingArrayGroupMap
    COMMAND ChipTest "${TEST_FIXTURES}/missing_map" "${TEST_FIXTURES}/latency_config.json")
add_test(NAME MalformedPerCoreFilename
    COMMAND ChipTest "${TEST_FIXTURES}/malformed_filename" "${TEST_FIXTURES}/latency_config.json")
add_test(NAME DuplicatePerCoreFile
    COMMAND ChipTest "${TEST_FIXTURES}/duplicate" "${TEST_FIXTURES}/latency_config.json")
add_test(NAME MissingPerCoreFile
    COMMAND ChipTest "${TEST_FIXTURES}/missing" "${TEST_FIXTURES}/latency_config.json")
add_test(NAME LatencyTimeoutDiagnostics
    COMMAND ChipTest "${TEST_FIXTURES}/latency_timeout.json" "${TEST_FIXTURES}/latency_config.json")
set_tests_properties(
    DirectoryModeRejectsFile SingleModeRejectsDirectory MalformedArrayGroupMap
    MissingArrayGroupMap MalformedPerCoreFilename DuplicatePerCoreFile
    MissingPerCoreFile LatencyTimeoutDiagnostics
    PROPERTIES WILL_FAIL TRUE)

# compress
add_executable(zc "")
target_include_directories(zc PRIVATE packages/header-only/zstr/src)

target_link_libraries(zc PRIVATE zlibstatic)

target_sources(zc PRIVATE tools/zc.cpp)
