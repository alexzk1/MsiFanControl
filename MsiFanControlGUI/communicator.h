#pragma once

#include <cstddef>
#include <memory>
#include <limits>

#include "device.h"
#include "cm_ctors.h"
#include "communicator_common.h"
#include "runners.h"

//This is GUI side communicator
namespace boost::interprocess {
class shared_memory_object;
class named_mutex;
}

struct CleanSharedMemory
{
    NO_COPYMOVE(CleanSharedMemory);
    CleanSharedMemory() = default;

    ~CleanSharedMemory()
    {
        Clean();
    }

    static void Clean()
    {
        using namespace boost::interprocess;
        shared_memory_object::remove(GetMemoryName());
    }
};

class CSharedDevice
{
public:
    CSharedDevice(utility::runnerint_t should_stop);
    NO_COPYMOVE(CSharedDevice);
    ~CSharedDevice();

    //! @brief Returns last known data read from the daemon.
    [[nodiscard]]
    const FullInfoBlock& LastKnownInfo() const;

    //! @brief Blocking call to read if daemon alive, does not update values from the BIOS,
    //! but updates LastKnownInfo() local copy.
    //! @note Call is blocking for at least kMinimumServiceDelay.
    //! @returns true if daemon responds properly.
    [[nodiscard]]
    bool PingDaemon();

    //! @brief Writes desired booster state, than triggers BIOS reading,
    //! than updates LastKnownInfo() local copy.
    //! @note Call is blocking for at least 2 * kMinimumServiceDelay.
    //! @returns true if daemon responds properly.
    bool SetBooster(BoosterState newState);

    //! @brief This triggers BIOS reading and IRQ-9 than updates LastKnownInfo() local copy.
    //! Try to avoid too often usage of it.
    //! @note Call is blocking for at least kMinimumServiceDelay.
    //! @returns true if daemon responds properly.
    bool RefreshData();
private:
    void SendRequest(const RequestFromUi& request) const;

    [[nodiscard]]
    bool UpdateInfoFromDaemon();

    bool WaitDaemonRead() const;

    utility::runnerint_t should_stop;
    std::shared_ptr<SharedMemoryWithMutex> sharedMem;
    FullInfoBlock lastKnownInfo;
};
