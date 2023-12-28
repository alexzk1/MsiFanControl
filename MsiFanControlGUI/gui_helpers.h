#pragma once

#include <QObject>
#include <tuple>

template <typename ...Ptrs>
auto BlockGuard(Ptrs ...args)
{
    class SignalBlocker
    {
    private:
        std::tuple<Ptrs...> pointers;
    public:
        SignalBlocker(Ptrs ...args)
            :pointers(args...)
        {
        }
        ~SignalBlocker()
        {
            std::apply([](Ptrs& ...args)
            {
                (...,args->blockSignals(false));
            }, pointers);
        }
    };
    return SignalBlocker(args...);
}
