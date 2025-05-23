#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

namespace utility {
using runnerint_t = std::shared_ptr<std::atomic<bool>>;
using runner_f_t = std::function<void(const runnerint_t should_int)>;

// simple way to execute lambda in thread, in case when shared_ptr is cleared it will send
// stop notify and join(), so I can ensure 1 pointer has only 1 running thread always for the same
// task

inline auto startNewRunner(runner_f_t func)
{
    using res_t = std::shared_ptr<std::thread>;
    const auto stop = runnerint_t(new std::atomic<bool>(false));
    const auto threadException =
      std::shared_ptr<std::exception_ptr>(new std::exception_ptr(nullptr));
    return res_t(new std::thread(func, stop), [stop, threadException](auto p) {
        stop->store(true);
        if (p)
        {
            if (p->joinable())
            {
                p->join();
            }
            delete p;
        }
    });
}

inline size_t currentThreadId()
{
    return std::hash<std::thread::id>{}(std::this_thread::get_id());
}
} // namespace utility
