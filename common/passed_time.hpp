#pragma once

#include "cm_ctors.h"

#include <chrono>

/// @brief Simple time passed measure. It records clock on creations,
/// and evalutes to true when recorded time + delay set < now().
class CPassedTime
{
  public:
    using clock_t = std::chrono::steady_clock;
    CPassedTime() = delete;
    ~CPassedTime() = default;
    DEFAULT_COPYMOVE(CPassedTime);

    template <typename taDuration>
    explicit CPassedTime(taDuration duration) :
        passed_at_(clock_t::now() + duration)
    {
    }

    [[nodiscard]]
    bool IsPassed() const
    {
        return std::chrono::steady_clock::now() < passed_at_;
    }

    operator bool() const
    {
        return IsPassed();
    }

  private:
    std::chrono::time_point<clock_t> passed_at_;
};
