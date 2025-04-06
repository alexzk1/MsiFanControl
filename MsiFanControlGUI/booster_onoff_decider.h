#pragma once

#include "cm_ctors.h" // IWYU pragma: keep
#include "device.h"   // IWYU pragma: keep
#include "running_avr.h"

#include <chrono>
#include <cstddef>
#include <optional>
#include <tuple>
#include <type_traits>

/// @brief Different boosters' states bound together.
struct BoostersStates
{
    BoosterState fanBoosterState{BoosterState::NO_CHANGE};
    CpuTurboBoostState cpuTurboBoostState{CpuTurboBoostState::NO_CHANGE};
    std::chrono::system_clock::time_point cpuTurboBoostOffAt{std::chrono::system_clock::now()};

    BoostersStates() = default;
    ~BoostersStates() = default;
    explicit BoostersStates(const FullInfoBlock &info) :
        fanBoosterState(info.boosterState),
        cpuTurboBoostState(info.cpuTurboBoost)
    {
    }
    DEFAULT_COPYMOVE(BoostersStates);

    bool operator==(const BoostersStates &other) const noexcept
    {
        const auto tie = [](const auto &v) {
            return std::tie(v.fanBoosterState, v.cpuTurboBoostState);
        };
        return tie(*this) == tie(other);
    }

    bool operator!=(const BoostersStates &other) const noexcept
    {
        return !(*this == other);
    }

    BoostersStates &operator=(const FullInfoBlock &info) noexcept
    {
        fanBoosterState = info.boosterState;
        if (cpuTurboBoostState != info.cpuTurboBoost
            && info.cpuTurboBoost == CpuTurboBoostState::OFF)
        {
            cpuTurboBoostOffAt = std::chrono::system_clock::now();
        }
        cpuTurboBoostState = info.cpuTurboBoost;

        return *this;
    }

    /// @returns true if this object should be sent to daemon.
    [[nodiscard]]
    bool HasAnyChange() const noexcept
    {
        static const BoostersStates kDefault{};
        return *this != kDefault;
    }

    /// @brief Writes current state into @p request.
    void UpdateRequest(RequestFromUi &request) const noexcept
    {
        request.boosterState = fanBoosterState;
        request.cpuTurboBoost = cpuTurboBoostState;
    }

    [[nodiscard]]
    std::chrono::milliseconds PassedSinceCpuBoostOff() const noexcept
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now() - cpuTurboBoostOffAt);
    }
};

/// @brief This is "smart logic" to decide if we should switch fan's booster.
template <std::size_t AvrSamplesCount>
class BoosterOnOffDecider
{
  public:
    /// @brief Updates state with new info from daemon. If no info - it will use previous one.
    /// It's safe to call this method even if there is no new info, but in that case it won't
    /// update anything. This method should be called periodically (every second or so).
    /// @param newInfo new state received from the daemon if any.
    /// @note it is methematically important to keep period time fixed.
    void UpdateState(const std::optional<FullInfoBlock> &newInfo) noexcept
    {
        if (newInfo)
        {
            cpuAvrTemp.OfferValue(newInfo->info.cpu.temperature);
            gpuAvrTemp.OfferValue(newInfo->info.gpu.temperature);
            lastStates = *newInfo;
        }
    }

    /// @brief Computes boosters states using latest values set by UpdateState().
    /// @returns States which should be passed to daemon based on last call(s) to UpdateState().
    [[nodiscard]]
    BoostersStates GetUpdatedBoosterStates() const noexcept
    {
        BoostersStates res;
        const bool isSystemHot = IsSystemHotNow();

        // This booster must be on when CPU is hot.
        switch (lastStates.fanBoosterState)
        {
            case BoosterState::NO_CHANGE:
                // This is something which should not happen. But throwing exception does not help
                // here too. Let's just pass, and see what will happen on the next cycle.
                break;
            case BoosterState::OFF:
                res.fanBoosterState = isSystemHot ? BoosterState::ON : BoosterState::NO_CHANGE;
                break;
            case BoosterState::ON:
                res.fanBoosterState = !isSystemHot ? BoosterState::OFF : BoosterState::NO_CHANGE;
                break;
        };

        // This booster can be on when CPU is cold.
        switch (lastStates.cpuTurboBoostState)
        {
            case CpuTurboBoostState::NO_CHANGE:
                // This is something which should not happen. But throwing exception does not help
                // here too. Let's just pass, and see what will happen on the next cycle.
                break;
            case CpuTurboBoostState::OFF:
                res.cpuTurboBoostState =
                  !isSystemHot && lastStates.PassedSinceCpuBoostOff() > 5000ms
                    ? CpuTurboBoostState::ON
                    : CpuTurboBoostState::NO_CHANGE;
                break;
            case CpuTurboBoostState::ON:
                res.cpuTurboBoostState =
                  isSystemHot ? CpuTurboBoostState::OFF : CpuTurboBoostState::NO_CHANGE;
                break;
        };

        return res;
    }

  private:
    BoostersStates lastStates;

    RunningAvr<float, AvrSamplesCount> cpuAvrTemp;
    RunningAvr<float, AvrSamplesCount> gpuAvrTemp;

    template <typename taLeft, typename taRight>
    [[nodiscard]]
    static bool greater(const std::optional<taLeft> &left, taRight right) noexcept
    {
        static_assert(std::is_arithmetic_v<taRight> && std::is_arithmetic_v<taLeft>,
                      "Only arithemetic types are supported.");
        return left.has_value() && *left > right;
    }

    [[nodiscard]]
    bool IsSystemHotNow() const noexcept
    {
        const auto avrCpu = cpuAvrTemp.GetCurrent();
        const auto avrGpu = gpuAvrTemp.GetCurrent();
        return (greater(avrCpu, kDegreeLimitBoth) && greater(avrGpu, kDegreeLimitBoth))
               || IsCpuHotNow();
    }

    [[nodiscard]]
    bool IsCpuHotNow() const noexcept
    {
        const auto avrCpu = cpuAvrTemp.GetCurrent();
        return greater(avrCpu, kCpuOnlyDegree);
    }

    // Celsium, nvidia gpu max is 93C.
    inline static constexpr int kDegreeLimitBoth = 80;
    // If cpu is such hot - boost, even if gpu is off
    inline static constexpr int kCpuOnlyDegree = 91;
    static_assert(kDegreeLimitBoth < kCpuOnlyDegree, "Revise here.");
};
