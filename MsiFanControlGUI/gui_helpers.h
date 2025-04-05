#pragma once

#include "cm_ctors.h"

#include <QObject>

#include <tuple>

/// @brief Blocks signals for the given QObject pointers until the guard is destroyed.
/// @param args A list of QObject pointers. The signals for these objects will be blocked.
/// @return A RAII object that unblocks the signals when it goes out of scope.
template <typename... Ptrs>
auto BlockGuard(Ptrs... args)
{
    class SignalBlocker
    {
      private:
        std::tuple<Ptrs...> pointers;

      public:
        NO_COPYMOVE(SignalBlocker);
        explicit SignalBlocker(Ptrs... args) :
            pointers(args...)
        {
        }
        ~SignalBlocker()
        {
            std::apply(
              [](Ptrs &...args) {
                  (..., args->blockSignals(false));
              },
              pointers);
        }
    };
    return SignalBlocker(args...);
}
