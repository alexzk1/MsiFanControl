#pragma once

#include "device.h"
#include "running_avr.h"

#include <cstddef>
#include <optional>
#include <utility>

template <std::size_t AvrSamplesCount>
class BoosterOnOffDecider
{
public:
    BoosterState GetUpdatedState(std::optional<FullInfoBlock> newInfo)
    {
        if (newInfo)
        {
            storedInfo = std::move(*newInfo);
        }

        const bool isHot = IsHotNow();

        if (storedInfo.boosterState == BoosterState::OFF)
        {
            return isHot ? BoosterState::ON : BoosterState::NO_CHANGE;
        }

        return !isHot ? BoosterState::OFF : BoosterState::NO_CHANGE;
    }
private:
    FullInfoBlock storedInfo{};
    RunningAvr<float, AvrSamplesCount> cpuAvrTemp;
    RunningAvr<float, AvrSamplesCount> gpuAvrTemp;

    bool IsHotNow()
    {
        static constexpr int kDegreeLimitBoth = 80;//celsium, nvidia gpu max is 93C.
        static constexpr int kCpuOnlyDegree =
            91; //if cpu is such hot - boost, even if gpu is off
        static_assert(kDegreeLimitBoth < kCpuOnlyDegree, "Revise here.");

        const auto avrCpu = cpuAvrTemp.GetCurrent(storedInfo.info.cpu.temperature);
        const auto avrGpu = gpuAvrTemp.GetCurrent(storedInfo.info.gpu.temperature);

        return (avrCpu && *avrCpu > kCpuOnlyDegree)
               || (avrCpu && avrGpu && avrCpu > kDegreeLimitBoth
                   && avrGpu > kDegreeLimitBoth);
    }
};
