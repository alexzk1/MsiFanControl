#pragma once

#include "cm_ctors.h"

#include <QObject>

#include <tuple>

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
