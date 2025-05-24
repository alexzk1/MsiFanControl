#include "communicator.h"
#include "messages_types.h"
#include "runners.h"
#include "seccomp_wrapper.hpp"

#include <cerrno>
#include <exception>
#include <iostream>
#include <ostream>
#include <string>
#include <thread>

#include <systemd/sd-daemon.h>

// NOLINTNEXTLINE
#include <signal.h>
#include <unistd.h>

#include <bits/types/sigset_t.h>

// NOLINTNEXTLINE
void threadBody(const utility::runnerint_t shouldStop)
{
    try
    {
        CSharedDevice sharedDevice;
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

        auto thread = utility::startNewRunner(threadBody);

        sd_notify(0, "READY=1");
        auto kernelSecurity = CSecCompWrapper::Allocate();
        if (kernelSecurity->Engage())
        {
            std::cerr << std::string("MSI fans control daemon has successfully started up with "
                                     "kernel enforced restrictions.")
                      << std::endl
                      << std::flush;
        }
        else
        {
            std::cerr << std::string(
              "MSI fans control daemon has started up but kernel securiy was not applied. This "
              "instance is weaker for potential attacks.")
                      << std::endl
                      << std::flush;
        }

        int l_signal = 0;
        sigwait(&l_waitedSignals, &l_signal);
        sd_notify(0, "STOPPING=1");

        // Stop all threading operations
        thread.reset();
        kernelSecurity.reset();

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
