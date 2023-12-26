#pragma once

#include <memory>

#include "communicator_common.h"

//This is GUI side communicator
namespace boost
{
    namespace interprocess
    {
        class shared_memory_object;
        class named_mutex;
    }
}

class CSharedDevice
{
public:
    CSharedDevice();
    ~CSharedDevice();

    void Communicate();
private:
    CleanSharedMemory memoryCleaner;
    std::shared_ptr<SharedMemoryWithMutex> sharedMem;
};
