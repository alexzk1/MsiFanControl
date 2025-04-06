#pragma once

#include <chrono>
#include <deque>
#include <optional>
#include <stdexcept>

/// @brief Computes running tabular derivative of the function, based on real time passed.
class TabularDerivative
{
  public:
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::duration<float>;

    /**
     * @brief Constructs a TabularDerivative instance.
     *
     * Initializes the object for approximating the derivative of a time-dependent signal.
     * The derivative is smoothed using Exponential Moving Average (EMA) to reduce noise and make
     * the trend more stable.
     *
     * @param smoothing_alpha Smoothing factor in the range [0.0 – 1.0]:
     *  - 1.0 — no smoothing (reacts instantly, but very noisy),
     *  - 0.0 — ignores all new input (not practical),
     *  - 0.1–0.3 — heavy smoothing, good for noisy signals (slow to react, but stable),
     *  - 0.6–0.9 — light smoothing, faster reaction (may respond to noise).
     *
     * The smaller the alpha, the more "inertial" the derivative becomes:
     * fewer false triggers, but delayed reaction.
     * At alpha = 1.0, no smoothing is applied and raw derivatives are used.
     */
    explicit TabularDerivative(float smoothing_alpha) :
        alpha(smoothing_alpha)
    {
    }
    TabularDerivative() = delete;

    /// @brief Offers new function value, time passed is measured between 2 calls of it.
    /// @note It must be used inside pereodical loop where real time process is measured.
    void Update(float value)
    {
        const TimePoint now = Clock::now();

        if (last_time.has_value() && last_value.has_value())
        {
            const Duration dt = now - *last_time;
            const float dy = value - *last_value;
            const float derivative = dy / dt.count();

            if (dt.count() <= 0.0f)
            {
                throw std::logic_error(
                  "Non-positive time slice happened. Chech the clocks in the system.");
            }

            // Keeping last 3 values at most for the central difference method.
            history.push_back({dt.count(), dy});
            if (history.size() > 3)
            {
                history.pop_front();
            }

            // EMA (exponential smoothing).
            if (!has_smoothed)
            {
                smoothed = derivative;
                has_smoothed = true;
            }
            else
            {
                smoothed = alpha * derivative + (1.0f - alpha) * smoothed;
            }
        }

        last_time = now;
        last_value = value;
    }

    /// @returns Smoothed function by real time derivative since last Update(). Or std::nullopt if
    /// it is not possible to compute yet.
    [[nodiscard]]
    std::optional<float> Result() const
    {
        if (!has_smoothed)
        {
            return std::nullopt;
        }
        return smoothed;
    }

  private:
    struct Step
    {
        float dt;
        float dy;
    };

    float alpha; // smoothing coefficient [0..1]
    float smoothed = 0.0f;
    bool has_smoothed = false;

    std::optional<TimePoint> last_time;
    std::optional<float> last_value;

    std::deque<Step> history;
};
