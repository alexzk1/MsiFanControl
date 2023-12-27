#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <iterator>
#include <type_traits>
#include <variant>
#include <vector>
#include <map>
#include <stdexcept>
#include <optional>

template <typename ValueType>
struct AddressedValueTmpl
{
    using value_type = ValueType;
    static_assert(std::is_scalar_v<ValueType>, "Only scalars are allowed.");

    std::streampos address;
    ValueType value;

    bool operator==(const AddressedValueTmpl& another) const
    {
        return std::tie(address, value) == std::tie(another.address, another.value);
    }

    bool operator!=(const AddressedValueTmpl& another) const
    {
        return !(*this == another);
    }

    //support for Cereal
    template <class Archive>
    void serialize( Archive & ar )
    {
        ar(address, value);
    }
};

struct AddressedBits
{
    using value_type = uint8_t;
    std::streampos address;

    //bit mask, if bit is 1 then this is valid in byte
    uint8_t validBits;

    //actual value
    uint8_t value;

    void MaskValue()
    {
        value &= validBits;
    }

    uint8_t ValueForWritting(uint8_t existingValue) const
    {
        existingValue &= ~validBits;
        existingValue |= value;
        return existingValue;
    }

    bool operator==(const AddressedBits& another) const
    {
        return std::tie(address, value, validBits) == std::tie(another.address, another.value,
                another.validBits);
    }

    bool operator!=(const AddressedBits& another) const
    {
        return !(*this == another);
    }
};

using AddressedValue1B = AddressedValueTmpl<std::uint8_t>;
using AddressedValue2B = AddressedValueTmpl<std::uint16_t>;

using AddressedValueAny =
    std::variant<AddressedValue1B, AddressedValue2B, AddressedBits>;

using AddressedValueAnyList = std::vector<AddressedValueAny>;

//Container to be used when one of many states can be active (RadioGroup in UI).
template <typename State>
struct AddressedValueStates
{
    using DataType = std::map<State, AddressedValueAny>;
    static_assert(std::is_enum_v<State>, "Expecting enum as the key.");

    //! @brief detects if there is 1 differente element exact between this and "other".
    //! @returns different value in this object if there is 1 difference, nullopt otherwise.
    std::optional<typename DataType::value_type> GetOneDifference(
        const AddressedValueStates<State>& other) const
    {
        if (data.size() != other.data.size())
        {
            throw std::invalid_argument("Containers must have the same amount of the states to compare.");
        }

        std::vector<State> difference;
        difference.reserve(data.size());
        for (const auto& value : data)
        {
            const auto iter = other.data.find(value.first);
            if (iter == other.data.end())
            {
                throw std::invalid_argument("Different keys found. We can compare only the states with the same keys.");
            }
            if (value.second != iter->second)
            {
                difference.push_back(value.first);
            }
        }

        return 1 == difference.size() ? std::make_optional(*data.find(difference.front())) : std::nullopt;
    }

    AddressedValueAny& at(const State key)
    {
        auto iter = data.find(key);
        if (iter == data.end())
        {
            throw std::invalid_argument("Requested access to the missing key.");
        }
        return iter->second;
    }

    const AddressedValueAny& at(const State key) const
    {
        auto iter = data.find(key);
        if (iter == data.end())
        {
            throw std::invalid_argument("Requested access to the missing key.");
        }
        return iter->second;
    }

    DataType* operator->()
    {
        return &data;
    }

    const DataType* operator->() const
    {
        return &data;
    }

    DataType& operator*()
    {
        return data;
    }

    const DataType& operator*() const
    {
        return data;
    }

    auto begin()
    {
        return data.begin();
    }

    auto begin() const
    {
        return data.begin();
    }

    auto end()
    {
        return data.end();
    }

    auto end() const
    {
        return data.end();
    }

    //support for Cereal
    template <class Archive>
    void serialize( Archive & ar )
    {
        ar(data);
    }

    DataType data;
};
