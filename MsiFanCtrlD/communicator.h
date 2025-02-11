#pragma once

#include "cm_ctors.h"
#include "communicator_common.h"
#include "device.h"

#include <cstdint>
#include <memory>
#include <set>

// This is daemon side communicator.

namespace boost::interprocess {
class shared_memory_object;
class named_mutex;
class mapped_region;
} // namespace boost::interprocess

class CDevice;

class CSharedDevice
{
  public:
    CSharedDevice();
    NO_COPYMOVE(CSharedDevice);
    ~CSharedDevice();

    void Communicate();

  private:
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
