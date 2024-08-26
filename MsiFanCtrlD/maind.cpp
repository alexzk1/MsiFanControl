#include "runners.h"
#include "communicator.h"

#include <cerrno>
#include <chrono>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include<exception>
#include <signal.h>
#include <systemd/sd-daemon.h>
#include <thread>

void threadBody(const utility::runnerint_t shouldStop)
{
    try
    {
        CSharedDevice sharedDevice;
        while (!(*shouldStop))
        {
            sharedDevice.Communicate();
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1s);
        }
    }
    catch (std::exception& l_exception)
    {
        auto l_resultStatus = errno;
        std::cerr << "Communication error: " << l_exception.what() << std::endl <<
                  std::flush;
        sd_notifyf(0, "STATUS=Failed: %s\n ERRNO=%i", l_exception.what(), l_resultStatus);
        std::exit(l_resultStatus);
    }
}

int main(int argc, const char** argv)
{
    (void)argc;
    (void)argv;

    int l_resultStatus = 1;
    try
    {
        sigset_t l_waitedSignals;
        sigemptyset(&l_waitedSignals);
        sigaddset(&l_waitedSignals, SIGTERM);
        sigprocmask(SIG_BLOCK, &l_waitedSignals, nullptr);

        auto thread = utility::startNewRunner(threadBody);

        sd_notify(0, "READY=1");
        std::cerr << std::string("MSI fans control daemon has successfully started up.") <<
                  std::endl <<
                  std::flush;

        int l_signal;
        sigwait(&l_waitedSignals, &l_signal);
        sd_notify(0, "STOPPING=1");

        //Stop all threading operations
        thread.reset();
        sd_notify(0, "STATUS=STOPPED");
        std::cerr << std::string("MSI fans control has been successfully shut down.") <<
                  std::endl <<
                  std::flush;

        l_resultStatus = 0;
    }
    catch (std::exception& l_exception)
    {
        sd_notifyf(0, "STATUS=Failed to start up: %s\n ERRNO=%i", l_exception.what(),
                   l_resultStatus);
        return l_resultStatus;
    }

    return l_resultStatus;
}
