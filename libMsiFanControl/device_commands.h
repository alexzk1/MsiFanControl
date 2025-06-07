#pragma once

#include <cstdint>
#include <ios>
#include <iosfwd>
#include <map>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

/// @brief Defines some scalar value with bound offset in some system fs binary file, which is
/// provided by drivers/BIOS. Minimal size of the value is 1 byte.
/// @note This is a template class. The taValueType must be a scalar type (int, short, char etc.).
///
/// @note When we write this object to FS, than it is acting as command to the BIOS.
/// When we read FS to this object, than it is acting as current state provider.
template <typename taValueType>
struct AddressedValueTmpl
{
    using value_type = taValueType;
    static_assert(std::is_scalar_v<taValueType>, "Only scalars are allowed.");

    ///@brief Offset in some binary file.
    // NOLINTNEXTLINE
    std::streampos address;

    ///@brief Value at @var address. Does not handle bytes order itself.
    // NOLINTNEXTLINE
    taValueType value;

    bool operator==(const AddressedValueTmpl &another) const
    {
        return std::tie(address, value) == std::tie(another.address, another.value);
    }

    bool operator!=(const AddressedValueTmpl &another) const
    {
        return !(*this == another);
    }

    // support for Cereal
    template <class Archive>
    void save(Archive &ar) const
    {
        // https://github.com/USCiLab/cereal/issues/684
        std::streamoff addr = address;
        ar(addr, value);
    }

    template <class Archive>
    void load(Archive &ar)
    {
        std::streamoff addr = -1;
        ar(addr, value);
        address = addr;
    }
};

/// @brief Shortcut for 1 byte value.
using AddressedValue1B = AddressedValueTmpl<std::uint8_t>;
/// @brief Shortcut for 2 bytes value.
using AddressedValue2B = AddressedValueTmpl<std::uint16_t>;

/// @brief Similar to @struct AddressedValueTmpl but works with individual bits. Can access 1 byet =
/// 8 bits at once.
struct AddressedBits
{
    using value_type = std::uint8_t;

    /// @brief Contains offset ins some binary file, where byte is located.
    // NOLINTNEXTLINE
    std::streampos address;

    /// @brief Bit mask to access individual bits of the byte at @var address, if bit in the mask is
    /// 1 then this is valid in byte.
    // NOLINTNEXTLINE
    std::uint8_t validBits;

    /// @brief actual 1 byte value
    // NOLINTNEXTLINE
    std::uint8_t value;

    static AddressedBits From1BValue(const AddressedValue1B &val, std::uint8_t validBits)
    {
        AddressedBits bits{val.address, validBits, val.value};
        bits.MaskValue();
        return bits;
    }

    // support for Cereal
    template <class Archive>
    void save(Archive &ar) const
    {
        // https://github.com/USCiLab/cereal/issues/684
        std::streamoff addr = address;
        ar(addr, value, validBits);
    }

    template <class Archive>
    void load(Archive &ar)
    {
        std::streamoff addr = -1;
        ar(addr, value, validBits);
        address = addr;
    }

    /// @brief Removes unused bits by @var validBits out of @var value.
    void MaskValue()
    {
        value &= validBits;
    }

    /// @brief Updates @p existingValue with @var value and returns new value.
    /// @param existingValue Existing byte in the memory.
    /// @returns Updated byte with applied bits.
    /// @note stored @var value must be masked already by call to MaskValue().
    [[nodiscard]]
    uint8_t ValueForWritting(std::uint8_t existingValue) const
    {
        existingValue &= ~validBits;
        existingValue |= value;
        return existingValue;
    }

    bool operator==(const AddressedBits &another) const
    {
        return std::tie(address, value, validBits)
               == std::tie(another.address, another.value, another.validBits);
    }

    bool operator!=(const AddressedBits &another) const
    {
        return !(*this == another);
    }
};

/// @brief Tag which just "do nothing". Usable when we want to mix commands to BIOS and to daemon.
/// Command with this tag will do nothing to the system fs.
struct TagIgnore
{
    using value_type = std::uint8_t;
    // NOLINTNEXTLINE
    std::streampos address{0};
    // NOLINTNEXTLINE
    std::uint8_t value{0};

    bool operator==(const TagIgnore &) const
    {
        return true;
    }

    bool operator!=(const TagIgnore &) const
    {
        return false;
    }

    // support for Cereal
    template <class Archive>
    void save(Archive &ar) const
    {
        // https://github.com/USCiLab/cereal/issues/684
        std::streamoff addr = address;
        ar(addr, value);
    }

    template <class Archive>
    void load(Archive &ar)
    {
        std::streamoff addr = -1;
        ar(addr, value);
        address = addr;
    }
};

/// @brief Possibly values we accepts.
using AddressedValueAny =
  std::variant<AddressedValue1B, AddressedValue2B, AddressedBits, TagIgnore>;

/// @brief List of values we accept, those make a command chain. When we write it to system fs
/// through the drivers BIOS accepts it as commands.
/// If we read those from the system fs than we get current state of the things.
using AddressedValueAnyList = std::vector<AddressedValueAny>;

/// @brief Container to be used when one of many states can be active (RadioGroup in UI).
/// Idea is that some enum's value is matched per @struct AddressedValueAny.
/// @note This is a template class. The taState must be an enum type.
template <typename taState>
struct AddressedValueStates
{
    using DataType = std::map<taState, AddressedValueAny>;
    static_assert(std::is_enum_v<taState>, "Expecting enum as the key.");

    //! @brief detects if there is 1 differente element exact between this and "other".
    //! @returns different value in this object if there is 1 difference, nullopt otherwise.
    //! @note This allows to find out what was changed.
    std::optional<typename DataType::value_type>
    GetOneDifference(const AddressedValueStates<taState> &other) const
    {
        if (data.size() != other.data.size())
        {
            throw std::invalid_argument(
              "Containers must have the same amount of the states to compare.");
        }

        std::vector<taState> difference;
        difference.reserve(data.size());
        for (const auto &value : data)
        {
            const auto iter = other.data.find(value.first);
            if (iter == other.data.end())
            {
                throw std::invalid_argument(
                  "Different keys found. We can compare only the states with the same keys.");
            }
            if (value.second != iter->second)
            {
                difference.push_back(value.first);
            }
        }

        return 1 == difference.size() ? std::make_optional(*data.find(difference.front()))
                                      : std::nullopt;
    }

    AddressedValueAny &at(const taState key)
    {
        auto iter = data.find(key);
        if (iter == data.end())
        {
            throw std::invalid_argument("Requested access to the missing key.");
        }
        return iter->second;
    }

    const AddressedValueAny &at(const taState key) const
    {
        auto iter = data.find(key);
        if (iter == data.end())
        {
            throw std::invalid_argument("Requested access to the missing key.");
        }
        return iter->second;
    }

    DataType *operator->()
    {
        return &data;
    }

    const DataType *operator->() const
    {
        return &data;
    }

    DataType &operator*()
    {
        return data;
    }

    const DataType &operator*() const
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

    // support for Cereal
    template <class Archive>
    void serialize(Archive &ar, const std::uint32_t /*version*/)
    {
        ar(data);
    }

    // NOLINTNEXTLINE
    DataType data;
};
