#pragma once

#include "device_commands.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

/// @brief This class check contained command by the predicate once.
/// It will return command which had predicate's result "true" always.
class ProperCommandDetector
{
  public:
    ProperCommandDetector() = delete;
    explicit ProperCommandDetector(AddressedValueAnyList commandsToChooseFrom) :
        commandsToChooseFrom(std::move(commandsToChooseFrom))
    {
    }

    /// @brief Predicated is called for each element, one which returned true will be kept.
    template <typename taPredicate>
    void DetectProperOneByOne(const taPredicate &predicate)
    {
        if (commandsToChooseFrom.empty())
        {
            throw std::runtime_error("Detector was called with empty commands list.");
        }

        if (commandsToChooseFrom.size() > 1)
        {
            const auto it =
              std::find_if(commandsToChooseFrom.begin(), commandsToChooseFrom.end(), predicate);
            if (it == commandsToChooseFrom.end())
            {
                throw std::runtime_error("Could not detect proper command.");
            }
            auto tmp = std::move(*it);
            commandsToChooseFrom.clear();
            commandsToChooseFrom.emplace_back(std::move(tmp));
        }
        validateSingleElement();
    }

    /// @brief Filter is called once, it should leave only one element.
    /// @note This method assumes that filter will always leave exactly one element.
    ///       If there are more or less elements left after filtering,
    ///       std::logic_error will be thrown.
    /// @warning Be careful with the filter implementation. It must not leave
    ///          zero elements, as this would lead to infinite exceptions.
    template <typename taFilter>
    void DetectProperAtOnce(const taFilter &filter)
    {
        if (commandsToChooseFrom.empty())
        {
            throw std::logic_error("Detector was called with empty commands list.");
        }
        if (commandsToChooseFrom.size() > 1)
        {
            filter(commandsToChooseFrom);
        }
        validateSingleElement();
    }

    [[nodiscard]]
    const AddressedValueAny &get() const
    {
        validateSingleElement();
        return commandsToChooseFrom.front();
    }

    // Yes, I love implicit conversions ...
    // NOLINTNEXTLINE
    operator const AddressedValueAny &() const
    {
        return get();
    }

    bool isValid() const
    {
        return commandsToChooseFrom.size() == 1;
    }

  private:
    void validateSingleElement() const
    {
        if (!isValid())
        {
            throw std::logic_error("Command should be detected before use.");
        }
    }

    AddressedValueAnyList commandsToChooseFrom;
};
