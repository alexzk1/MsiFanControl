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
    using TComputedValue = std::optional<float>;

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
        history.emplace_back(value);
        if (history.size() > 3)
        {
            history.pop_front();
        }
        auto derivative = ComputeDerivativeBasedOnHistory();
        if (!derivative)
        {
            return;
        }

        // EMA (exponential smoothing).
        if (!smoothed)
        {
            smoothed = std::move(derivative);
            return;
        }
        *smoothed = alpha * derivative.value() + (1.0f - alpha) * smoothed.value();
    }

    /// @returns Smoothed function by real time derivative since last Update(). Or std::nullopt if
    /// it is not possible to compute yet.
    [[nodiscard]]
    const std::optional<float> &Result() const
    {
        return smoothed;
    }

  private:
    struct Measure
    {
        TimePoint time; // x
        float value;    // f(x)

        explicit Measure(float value) :
            time(Clock::now()),
            value(value)
        {
        }
    };

    TComputedValue ComputeDerivativeBasedOnHistory() const
    {
        if (history.size() < 2)
        {
            return std::nullopt;
        }
        const auto &right = history.back();
        const auto &left = history.front();

        const float dtSeconds =
          std::chrono::duration_cast<std::chrono::milliseconds>(right.time - left.time).count()
          / 1000.f;

        if (dtSeconds <= 0.0f)
        {
            throw std::logic_error(
              "Non-positive time slice happened. Chech the clocks in the system.");
        }

        return (right.value - left.value) / dtSeconds;
    }

    float alpha; // smoothing coefficient [0..1]
    TComputedValue smoothed;
    std::deque<Measure> history;
};
