#pragma once

#include "cm_ctors.h"
#include "communicator_common.h"
#include "device.h"
#include "runners.h"

#include <memory>

/// @brief This is GUI side communicator
namespace boost::interprocess {
class shared_memory_object;
class named_mutex;
} // namespace boost::interprocess

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

/// @brief GUI side of the daemon. It is used to communicate with the daemon and read/write data
/// from/to it.
/// @note This class is not thread-safe, but can be used in multiple threads if each thread has its
/// own instance of this class.
class CSharedDevice
{
  public:
    explicit CSharedDevice(utility::runnerint_t should_stop);
    NO_COPYMOVE(CSharedDevice);
    ~CSharedDevice();

    //! @brief Returns last known data read from the daemon.
    [[nodiscard]]
    const FullInfoBlock &LastKnownInfo() const;

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
    bool SetBattery(Battery newState);
    bool SetCpuBooster(CpuTurboBoostState newState);

    //! @brief This triggers BIOS reading and IRQ-9 than updates LastKnownInfo() local copy.
    //! Try to avoid too often usage of it.
    //! @note Call is blocking for at least kMinimumServiceDelay.
    //! @returns true if daemon responds properly.
    bool RefreshData();

  private:
    void SendRequest(const RequestFromUi &request) const;

    [[nodiscard]]
    bool UpdateInfoFromDaemon();

    [[nodiscard]]
    bool WaitDaemonRead() const;

    utility::runnerint_t should_stop;
    std::shared_ptr<SharedMemoryWithMutex> sharedMem;
    FullInfoBlock lastKnownInfo;
};
