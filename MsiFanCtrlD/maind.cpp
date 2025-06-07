#include "communicator.h"
#include "messages_types.h"
#include "runners.h"
#include "seccomp_wrapper.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <exception>
#include <iostream>
#include <mutex>
#include <optional>
#include <ostream>
#include <string>
#include <thread>

#include <systemd/sd-daemon.h>

// NOLINTNEXTLINE
#include <signal.h>
#include <unistd.h>

#include <bits/types/sigset_t.h>

namespace {
// Note, security blocks thread creation calls (which includes it would block forks()).
// So we have to wait thread launched, than engage security.
std::mutex runThreadAfterSecurity;
} // namespace

// NOLINTNEXTLINE
void threadBody(const utility::runnerint_t shouldStop)
{
    const std::lock_guard delayedStart(runThreadAfterSecurity);
    try
    {
        CSharedDevice sharedDevice;
        sd_notify(0, "READY=1");

        while (!(*shouldStop))
        {
            sharedDevice.Communicate();
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(kMinimumServiceDelay);
        }
    }
    catch (std::exception &l_exception)
    {
        auto l_resultStatus = errno;
        std::cerr << "Communication error: " << l_exception.what() << std::endl << std::flush;
        // NOLINTNEXTLINE
        sd_notifyf(0, "STATUS=Failed: %s\n ERRNO=%i", l_exception.what(), l_resultStatus);
        kill(getpid(), SIGTERM);
    }
}

int main(int argc, const char **argv)
{
    constexpr auto kRestrict = "--restrict";
    (void)argc;
    (void)argv;

    int l_resultStatus = 1;
    try
    {
        sigset_t l_waitedSignals;
        sigemptyset(&l_waitedSignals);
        sigaddset(&l_waitedSignals, SIGTERM);
        // NOLINTNEXTLINE
        sigprocmask(SIG_BLOCK, &l_waitedSignals, nullptr);

        std::optional<std::lock_guard<std::mutex>> delayedStart;
        delayedStart.emplace(runThreadAfterSecurity);
        auto thread = utility::startNewRunner(threadBody);

        const bool isSecurityEnabled =
          argc > 1 && std::any_of(argv, argv + argc, [](const char *const param) {
              return strcmp(param, kRestrict) == 0;
          });
        auto kernelSecurity = isSecurityEnabled ? CSecCompWrapper::Allocate() : nullptr;
        const bool securityEngaged = kernelSecurity && kernelSecurity->Engage();
        delayedStart.reset();

        if (securityEngaged)
        {
            std::cerr << std::string("MSI fans control daemon has successfully started up with "
                                     "kernel enforced restrictions.")
                      << std::endl
                      << std::flush;
        }
        else
        {
            std::cerr << std::string("MSI fans control daemon has started up but kernel securiy "
                                     "was not applied.\nIt is weaker for potential attacks.")
                      << std::endl
                      << std::flush;
            if (!isSecurityEnabled)
            {
                std::cerr << "To enable restriction add " << kRestrict << " parameter."
                          << std::endl;
            }
        }

        int l_signal = 0;
        sigwait(&l_waitedSignals, &l_signal);
        sd_notify(0, "STOPPING=1");

        // Stop all threading operations
        thread.reset();

        sd_notify(0, "STATUS=STOPPED");
        std::cerr << std::string("MSI fans control has been successfully shut down.") << std::endl
                  << std::flush;

        l_resultStatus = 0;
    }
    catch (std::exception &l_exception)
    {
        // NOLINTNEXTLINE
        sd_notifyf(0, "STATUS=Failed to start up: %s\n ERRNO=%i", l_exception.what(),
                   l_resultStatus);
    }
    return l_resultStatus;
}
