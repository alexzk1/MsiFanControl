#pragma once

#include "device_commands.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

/// This class check contained command by the predicate once.
/// It will return command which had predicate's result "true" always.
class ProperCommandDetector
{
  public:
    ProperCommandDetector() = delete;
    explicit ProperCommandDetector(AddressedValueAnyList commandsToChooseFrom) :
        commandsToChooseFrom(std::move(commandsToChooseFrom))
    {
    }

    /// Predicated is called for each element, one which returned true will be kept.
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

    /// Filter should leave 1 element.
    template <typename taFilter>
    void DetectProperAtOnce(const taFilter &filter)
    {
        if (commandsToChooseFrom.empty())
        {
            throw std::runtime_error("Detector was called with empty commands list.");
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

  private:
    void validateSingleElement() const
    {
        if (commandsToChooseFrom.size() != 1)
        {
            throw std::logic_error("Command should be detected before use.");
        }
    }

    AddressedValueAnyList commandsToChooseFrom;
};
