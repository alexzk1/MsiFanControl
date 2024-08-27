#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <numeric>

template <typename T, std::size_t Counts>
class RunningAvr
{
public:
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
    std::array<T, Counts> lastValues;
    std::size_t nextIndex{0};

    T Calculate() const
    {
        return std::accumulate(lastValues.begin(), lastValues.end(), T(0))
               / lastValues.size();
    }
};
