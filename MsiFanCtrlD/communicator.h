#pragma once

#include "cm_ctors.h"
#include "communicator_common.h"
#include "device.h"

#include <cstdint>
#include <memory>
#include <set>

/// @brief This is daemon side communicator.

namespace boost::interprocess {
class shared_memory_object;
class named_mutex;
class mapped_region;
} // namespace boost::interprocess

class CDevice;

/// @brief Main daemon's logic.
/// Also it keeps backup of BIOS' "file", so any changes can be reverted out of backup.
class CSharedDevice
{
  public:
    CSharedDevice();
    NO_COPYMOVE(CSharedDevice);
    ~CSharedDevice();

    /// @brief Do 1 step if I/O communication with GUI. Process it's orders, make proper responses.
    void Communicate();

  private:
    /// @brief This object removes shared memory block when created and destroyed.
    struct CleanSharedMemory
    {
        NO_COPYMOVE(CleanSharedMemory);
        CleanSharedMemory()
        {
            Clean();
        }

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
    friend struct BackupExecutorImpl;
    void RestoreOffsets(const std::set<int64_t> &offsetsToRestoreFromBackup) const;
    bool MakeBackupBlock();

    CleanSharedMemory memoryCleaner;
    FullInfoBlock lastReadInfo;
    std::shared_ptr<CDevice> device;
    std::shared_ptr<SharedMemoryWithMutex> sharedMem;

    std::shared_ptr<SharedMemory> sharedBackup;
};
