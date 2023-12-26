#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/archives/binary.hpp>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include <cstddef>
#include <exception>
#include <memory>

#include <stdexcept>
#include <string>
#include <ostream>

#include "device.h"
#include "communicator.h"
#include "communicator_common.h"

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

void CSharedDevice::Communicate()
{
    using namespace boost::interprocess;
    FullInfoBlock info;

    scoped_lock<interprocess_mutex> grd(sharedMem->Mutex());
    {
        auto buffer = sharedMem->Daemon2UI();
        std::istream ss(&buffer);
        cereal::BinaryInputArchive iarchive(ss);
        iarchive(info);
    }

#ifndef NDEBUG
    std::cerr << "CPU Temp: "<< info.info.cpu.temperature << ", cpu rpm: " << info.info.cpu.fanRPM <<
              std::endl;
#endif
}
