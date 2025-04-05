#pragma once

#include "device.h" // IWYU pragma: keep
#include "running_avr.h"

#include <cstddef>
#include <optional>
#include <type_traits>

/// @brief This is "smart logic" to decide if we should switch fan's booster.
template <std::size_t AvrSamplesCount>
class BoosterOnOffDecider
{
  public:
    /// @brief Updates state with new info from daemon. If no info - it will use previous one.
    /// It's safe to call this method even if there is no new info, but in that case it won't update
    /// anything. This method should be called periodically (every second or so).
    /// @param newInfo new state received from the daemon if any.
    /// @note it is methematically important to keep period time fixed.
    void UpdateState(const std::optional<FullInfoBlock> &newInfo)
    {
        if (newInfo)
        {
            cpuAvrTemp.OfferValue(newInfo->info.cpu.temperature);
            gpuAvrTemp.OfferValue(newInfo->info.gpu.temperature);
            lastStates.boosterState = newInfo->boosterState;
        }
    }

    /// @brief Computes booster's state using latest values set by UpdateState().
    /// @returns State which should be passed to daemon based on last call(s) to UpdateState().
    [[nodiscard]]
    BoosterState GetUpdatedBoosterState() const
    {
        const bool isHot = IsHotNow();

        if (lastStates.boosterState == BoosterState::OFF)
        {
            return isHot ? BoosterState::ON : BoosterState::NO_CHANGE;
        }

        return !isHot ? BoosterState::OFF : BoosterState::NO_CHANGE;
    }

  private:
    struct LastStates
    {
        BoosterState boosterState;
    } lastStates;

    RunningAvr<float, AvrSamplesCount> cpuAvrTemp;
    RunningAvr<float, AvrSamplesCount> gpuAvrTemp;

    template <typename taLeft, typename taRight>
    [[nodiscard]]
    static bool greater(const std::optional<taLeft> &left, taRight right)
    {
        static_assert(std::is_arithmetic_v<taRight> && std::is_arithmetic_v<taLeft>,
                      "Only arithemetic types are supported.");
        return left.has_value() && *left > right;
    }

    [[nodiscard]]
    bool IsHotNow() const
    {
        // Celsium, nvidia gpu max is 93C.
        static constexpr int kDegreeLimitBoth = 80;
        // If cpu is such hot - boost, even if gpu is off
        static constexpr int kCpuOnlyDegree = 91;

        static_assert(kDegreeLimitBoth < kCpuOnlyDegree, "Revise here.");

        const auto avrCpu = cpuAvrTemp.GetCurrent();
        const auto avrGpu = gpuAvrTemp.GetCurrent();

        return greater(avrCpu, kCpuOnlyDegree)
               || (greater(avrCpu, kDegreeLimitBoth) && greater(avrGpu, kDegreeLimitBoth));
    }
};
