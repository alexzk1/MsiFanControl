#pragma once

#include <memory>
#include <set>
#include <cstdint>
#include "communicator_common.h"

//This is daemon side communicator.

namespace boost {
namespace interprocess {
class shared_memory_object;
class named_mutex;
class mapped_region;
}
}
class CDevice;

class CSharedDevice
{
    struct CleanSharedMemory
    {
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
public:
    CSharedDevice();
    ~CSharedDevice();

    void Communicate();

private:
    friend struct BackupExecutorImpl;
    void RestoreOffsets(std::set<int64_t> offsetsToRestoreFromBackup) const;
    void MakeBackupBlock();

    CleanSharedMemory memoryCleaner;
    std::shared_ptr<CDevice> device;
    std::shared_ptr<SharedMemoryWithMutex> sharedMem;

    std::shared_ptr<SharedMemory> sharedBackup;
};
