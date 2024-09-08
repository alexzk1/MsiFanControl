#pragma once

#include "communicator.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <ostream>
#include <vector>

//! @brief Supplies how often we should read real values from the BIOS.
//! Measure unit is "once per N reader loop iterations".
class ReadsPeriodDetector
{
public:
    ReadsPeriodDetector(const bool& isPingOk, const CSharedDevice& sharedDevice) :
        isPingOk(isPingOk), sharedDevice(sharedDevice)

    {}

    // We're trying to read data is less as possible because each read triggers IRQ9 which
    // leads to more power consumption eventually. However, when CPU is hot, it means it is loaded,
    // so IRQ-9 will not add too much and we can call check more often.
    // So what we do here is dynamic range - higher the temp - more often we do
    // update from the daemon.
    std::size_t operator()() const
    {
        //This must be ordered by the temp.
        static const std::vector<TTempPeriodRecord> tempPeriodRecords =
            Validate(
        {
            {0, 1}, //it was no reads yet, avoid delays.
            {39, 35}, // no user around ?
            {42, 23},
            {45, 17},
            {47, 15},
            {50, 13},
            {60, 10},
            {65, 7},
            {70, 4},
            {75, 3},
            {80, 2},
            //I think we should not add divider 1 at the end ever.
        });

        //If ping failed, request updates as fast as possible.
        if (!isPingOk)
        {
            return 1u;
        }

        const auto it = std::lower_bound(tempPeriodRecords.begin(), tempPeriodRecords.end(),
                                         sharedDevice.LastKnownInfo().info.cpu.temperature,
                                         [](const TTempPeriodRecord& record, auto current_temp)
        {
            return record.cpuTemp < current_temp;
        });

        return it == tempPeriodRecords.end() ? tempPeriodRecords.back().loopPeriod :
               it->loopPeriod;
    }
private:
    struct TTempPeriodRecord
    {
        std::uint16_t cpuTemp;
        std::size_t loopPeriod;
    };

    static std::vector<TTempPeriodRecord> Validate(
        std::initializer_list<TTempPeriodRecord> records)
    {
        auto it = std::adjacent_find(records.begin(), records.end(), [](const auto& left,
                                     const auto& right)
        {
            return right.cpuTemp < left.cpuTemp;
        });

        if (it != records.end())
        {
            std::cerr <<
                      "Temp-Period records must be ordered by the temperature field. Please check the source code."
                      << std::endl << std::flush;
            std::abort();
        }

        it = std::find_if(records.begin(), records.end(), [](const auto& val)
        {
            return 0u == val.loopPeriod;
        });

        if (it != records.end())
        {
            std::cerr << "Temp-Period cannot have zero-period. Please check the source code." <<
                      std::endl << std::flush;
            std::abort();
        }

        return records;
    }

    const bool& isPingOk;
    const CSharedDevice& sharedDevice;
};
