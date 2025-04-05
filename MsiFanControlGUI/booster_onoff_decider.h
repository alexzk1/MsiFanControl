#pragma once

#include "device.h"
#include "running_avr.h"

#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

/// @brief This is "smart logic" to decide if we should switch fan's booster.
template <std::size_t AvrSamplesCount>
class BoosterOnOffDecider
{
  public:
    /// @returns state which should be set since now.
    /// @param newInfo new state received from the daemon if any.
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

    template <typename taLeft, typename taRight>
    static bool greater(const std::optional<taLeft> &left, taRight right)
    {
        static_assert(std::is_arithmetic_v<taRight> && std::is_arithmetic_v<taLeft>,
                      "Only arithemetic types are supported.");
        return left.has_value() && *left > right;
    }

    bool IsHotNow()
    {
        // celsium, nvidia gpu max is 93C.
        static constexpr int kDegreeLimitBoth = 80;
        // if cpu is such hot - boost, even if gpu is off
        static constexpr int kCpuOnlyDegree = 91;

        static_assert(kDegreeLimitBoth < kCpuOnlyDegree, "Revise here.");

        const auto avrCpu = cpuAvrTemp.GetCurrent(storedInfo.info.cpu.temperature);
        const auto avrGpu = gpuAvrTemp.GetCurrent(storedInfo.info.gpu.temperature);

        return greater(avrCpu, kCpuOnlyDegree)
               || (greater(avrCpu, kDegreeLimitBoth) && greater(avrGpu, kDegreeLimitBoth));
    }
};
