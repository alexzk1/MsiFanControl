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

    RunningAvr()
    {
        std::fill(lastValues.begin(), lastValues.end(), static_cast<T>(0));
    }

    /// @brief Adds value to the list, limiting it by last taCounts elements.
    /// @returns Avr. value if list has taCounts elements or std::nullopt otherwise.
    std::optional<T> GetCurrent(T newValue)
    {
        lastValues[nextIndex % lastValues.size()] = newValue;
        ++nextIndex;
        if (nextIndex >= lastValues.size())
        {
            return Calculate();
        }
        return std::nullopt;
    }

  private:
    std::array<T, taCounts> lastValues;
    std::size_t nextIndex{0};

    T Calculate() const
    {
        return std::accumulate(lastValues.begin(), lastValues.end(), T(0)) / lastValues.size();
    }
};
