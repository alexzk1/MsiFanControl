#pragma once

#include <type_traits>

template <typename ValueT>
class LowPassFilter
{
private:
    const double alpha;
    double value;
public:
    explicit LowPassFilter(double alpha)
        : alpha(alpha),
          value(0.0)
    {
        static_assert(std::is_arithmetic_v<ValueT>, "Only arithmetic types are allowed.");
    }

    void AddValue(ValueT newValue)
    {
        const auto a1 = 1 - alpha;
        value = a1 * value + newValue * alpha;
    }

    ValueT Get() const
    {
        return static_cast<ValueT>(value.load());
    }
};
