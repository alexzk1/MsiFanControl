#pragma once

#include "cm_ctors.h" // IWYU pragma: keep
#include "device.h"   // IWYU pragma: keep
#include "running_avr.h"
#include "tabular_derivative.h"

#include <chrono>
#include <cstddef>
#include <optional>
#include <tuple>
#include <type_traits>

/**
 * @brief Controller for managing CPU turbo-boost state based on temperature trends.
 *
 * This class uses temperature derivative calculations to dynamically adjust the
 * CPU turbo boost. The turbo boost can be enabled or disabled based on the current
 * temperature and its rate of change (acceleration).
 */
class CpuTurboBoostController
{
  public:
    /**
     * @brief Constructs a CpuTurboBoostController instance.
     *
     * @param alpha_temp Smoothing factor for temperature derivative (rate of temperature change)
     * [0.0 – 1.0].
     * @param alpha_derivative Smoothing factor for temperature acceleration (second derivative)
     * [0.0 – 1.0].
     */
    explicit CpuTurboBoostController(float alpha_temp = 0.3f, float alpha_derivative = 0.5f) :
        dT(alpha_temp),
        d2T(alpha_derivative)
    {
    }

    /**
     * @brief Updates the CPU temperature and returns a new turbo-boost state.
     *
     * This method calculates the rate of temperature change and its acceleration,
     * and determines if the turbo-boost feature should be enabled, disabled, or left unchanged.
     *
     * @param currentTemperature The current CPU temperature in Celsius.
     * @param currentState The current state of the turbo-boost feature (ON, OFF, or NO_CHANGE).
     * @return CpuTurboBoostState The new turbo-boost state: ON, OFF, or NO_CHANGE.
     */
    CpuTurboBoostState Update(const float currentTemperature,
                              const CpuTurboBoostState currentState) noexcept
    {
        dT.Update(currentTemperature);

        const auto tempDerivative = dT.Result();
        if (!tempDerivative.has_value())
            return CpuTurboBoostState::NO_CHANGE;

        d2T.Update(*tempDerivative);
        const auto accel = d2T.Result();
        if (!accel.has_value())
            return CpuTurboBoostState::NO_CHANGE;

        // Decision logic.
        const float temp = currentTemperature;
        const float rate = *tempDerivative;
        const float acceleration = *accel;

        static constexpr float kCpuOnlyHotDegree =
          85.0; ///< Temperature threshold to consider disabling turbo-boost.
        static constexpr float kCpuOnlyColdDegree =
          70.0; ///< Temperature threshold to consider enabling turbo-boost.

        if (currentState == CpuTurboBoostState::ON)
        {
            if (temp >= kCpuOnlyHotDegree && rate > 0.5f && acceleration > 0.0f)
            {
                return CpuTurboBoostState::OFF;
            }
        }
        else if (currentState == CpuTurboBoostState::OFF)
        {
            if (temp <= kCpuOnlyColdDegree && rate < 0.0f && acceleration < 0.0f)
            {
                return CpuTurboBoostState::ON;
            }
        }

        return CpuTurboBoostState::NO_CHANGE;
    }

  private:
    TabularDerivative dT;  ///< First derivative (rate of temperature change)
    TabularDerivative d2T; ///< Second derivative (temperature acceleration)
};

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

/// @brief This is "smart logic" to decide if we should switch boosters (fan's, cpu turboboost,
/// etc.).
template <std::size_t AvrSamplesCount>
class BoostersOnOffDecider
{
  public:
    /// @brief  Computes updated state with new info from daemon.
    /// It's safe to call this method even if there is no new info, but in that case it won't
    /// update anything. This method should be called periodically (every second or so).
    /// @param newInfo new state received from the daemon if any.
    /// @returns States which should be passed to daemon based on last call(s) to UpdateState().
    [[nodiscard]]
    BoostersStates ComputeUpdatedBoosterStates(const std::optional<FullInfoBlock> &newInfo) noexcept
    {
        BoostersStates res;
        if (newInfo)
        {
            cpuAvrTemp.OfferValue(newInfo->info.cpu.temperature);
            gpuAvrTemp.OfferValue(newInfo->info.gpu.temperature);
            lastStates = *newInfo;
        }

        // Updating CPU turboboost state, it has own complex decider.
        res.cpuTurboBoostState =
          cpuTurboBoost.Update(newInfo->info.cpu.temperature, lastStates.cpuTurboBoostState);

        // Fan's booster must be on when CPU is hot.
        const bool isSystemHot = IsSystemHot();
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

        return res;
    }

  private:
    BoostersStates lastStates;
    RunningAvr<float, AvrSamplesCount> cpuAvrTemp;
    RunningAvr<float, AvrSamplesCount> gpuAvrTemp;
    CpuTurboBoostController cpuTurboBoost;

    template <typename taLeft, typename taRight>
    [[nodiscard]]
    static bool greater(const std::optional<taLeft> &left, taRight right) noexcept
    {
        static_assert(std::is_arithmetic_v<taRight> && std::is_arithmetic_v<taLeft>,
                      "Only arithemetic types are supported.");
        return left.has_value() && *left > right;
    }

    bool IsSystemHot() const noexcept
    {
        // Celsium, nvidia gpu max is 93C.
        static constexpr int kDegreeLimitBoth = 80;
        static constexpr int kCpuOnlyHotDegree = 91;
        static_assert(kDegreeLimitBoth < kCpuOnlyHotDegree, "Revise here.");

        const auto avrCpu = cpuAvrTemp.GetCurrent();
        const auto avrGpu = gpuAvrTemp.GetCurrent();
        return greater(avrCpu, kCpuOnlyHotDegree)
               || (greater(avrCpu, kDegreeLimitBoth) && greater(avrGpu, kDegreeLimitBoth));
    }
};
