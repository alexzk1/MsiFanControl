#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <limits>

#include "device.h"
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

struct CleanSharedMemory
{
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
    CSharedDevice();
    ~CSharedDevice();

    FullInfoBlock Communicate(const RequestFromUi &request);

    bool PossiblyBroken() const;
private:
    std::shared_ptr<SharedMemoryWithMutex> sharedMem;
    std::size_t lastTag{std::numeric_limits<std::size_t>::max()};
    std::size_t wrongTagCntr{std::numeric_limits<std::size_t>::max() / 3};
    bool justCreated{true};
};
