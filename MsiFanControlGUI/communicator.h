#pragma once

#include <cstddef>
#include <memory>
#include <limits>
#include <optional>

#include "device.h"
#include "cm_ctors.h"
#include "communicator_common.h"

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
    CSharedDevice();
    NO_COPYMOVE(CSharedDevice);
    ~CSharedDevice();

    FullInfoBlock Communicate(const std::optional<RequestFromUi> &request);

    [[nodiscard]]
    bool PossiblyBroken() const;
private:
    std::shared_ptr<SharedMemoryWithMutex> sharedMem;
    std::size_t lastTag{std::numeric_limits<std::size_t>::max()};
    std::size_t wrongTagCntr{std::numeric_limits<std::size_t>::max() / 3};
    bool justCreated{true};
};
