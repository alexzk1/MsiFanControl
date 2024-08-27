#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/detail/os_file_functions.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>

#include <istream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <ostream>
#include <utility>

#include "communicator.h"
#include "communicator_common.h"
#include "device.h"

//This is GUI side communicator

static_assert(kWholeSharedMemSize % 2 == 0, "Wrong size.");

//Ok, idea is, on 1st half of the memory we will put cereal serialized current state like temperature / rpm.
//From the 2nd half we will read contol if any.

CSharedDevice::CSharedDevice()
{
    using namespace boost::interprocess;

    shared_memory_object shm(open_only,
                             GetMemoryName(), read_write);
    shm.truncate(kWholeSharedMemSize);
    sharedMem = std::make_shared<SharedMemoryWithMutex>(std::move(shm));
}

CSharedDevice::~CSharedDevice()
{
    sharedMem.reset();
}

FullInfoBlock CSharedDevice::Communicate(const std::optional<RequestFromUi>& request)
{
    using namespace boost::interprocess;
    FullInfoBlock info;
    {
        const scoped_lock<interprocess_mutex> grd(sharedMem->Mutex());
        {
            auto buffer = sharedMem->Daemon2UI();
            std::istream ss(&buffer);
            cereal::BinaryInputArchive iarchive(ss);
            iarchive(info);
        }

        if (std::nullopt != request)
        {
            auto buffer = sharedMem->UI2Daemon();
            std::ostream ss(&buffer);
            cereal::BinaryOutputArchive oarchive(ss);
            oarchive(*request);
            sharedMem->UIPushedForDaemon();
        }
    }

    wrongTagCntr = lastTag < info.tag || justCreated? 0 : wrongTagCntr + 1;
    lastTag = info.tag;

    if (wrongTagCntr > 5)
    {
        throw std::runtime_error("Connection to the daemon looks broken.");
    }
    justCreated = false;

    return info;
}

bool CSharedDevice::PossiblyBroken() const
{
    return wrongTagCntr > 0 && !justCreated;
}
