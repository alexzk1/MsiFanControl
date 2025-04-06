#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <optional>
#include <type_traits>

/// @brief Running avr value of the multiply values. It keeps last taCounts values.
template <typename T, std::size_t taCounts>
class RunningAvr
{
  public:
    static_assert(std::is_arithmetic_v<T>, "Only arithmetic types are supported.");

    RunningAvr() noexcept
    {
        std::fill(lastValues.begin(), lastValues.end(), static_cast<T>(0));
    }

    /// @brief Adds value to the list, limiting it by last taCounts elements.
    /// @returns Avr. value if list has taCounts elements or std::nullopt otherwise.
    std::optional<T> GetCurrent(T newValue) noexcept
    {
        OfferValue(newValue);
        return GetCurrent();
    }

    /// @brief Adds value to the list, limiting it by last taCounts elements.
    void OfferValue(T newValue) noexcept
    {
        lastValues[nextIndex % lastValues.size()] = newValue;
        ++nextIndex;
    }

    /// @returns Avr. value if list has taCounts elements or std::nullopt otherwise.
    std::optional<T> GetCurrent() const noexcept
    {
        if (nextIndex >= lastValues.size())
        {
            return Calculate();
        }
        return std::nullopt;
    }

  private:
    std::array<T, taCounts> lastValues;
    std::size_t nextIndex{0};

    T Calculate() const noexcept
    {
        return std::accumulate(lastValues.begin(), lastValues.end(), T(0)) / lastValues.size();
    }
};
