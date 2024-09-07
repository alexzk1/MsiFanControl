#pragma once

#include <memory>
#include <set>
#include <cstdint>

#include "communicator_common.h"
#include "cm_ctors.h"
#include "device.h"

//This is daemon side communicator.

namespace boost::interprocess {
class shared_memory_object;
class named_mutex;
class mapped_region;
}

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
