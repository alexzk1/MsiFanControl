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

#include "msi_fan_control.h"
#include "communicator.h"

constexpr std::size_t kWholeSharedMemSize = 4096;
static_assert(kWholeSharedMemSize % 2 == 0, "Wrong size.");

//Ok, idea is, on 1st half of the memory we will put cereal serialized current state like temperature / rpm.
//From the 2nd half we will read contol if any.

struct MemBuf : std::streambuf
{
    MemBuf(char* base, std::size_t size)
    {
        this->setp(base, base + size);
        this->setg(base, base, base + size);
    }
    std::size_t written() const
    {
        return this->pptr() - this->pbase();
    }
    std::size_t read() const
    {
        return this->gptr() - this->eback();
    }
};

CSharedDevice::CSharedDevice()
{
    static const std::string kSharedMemoryName = "MSICoolersSharedControlMem6";
    static const std::string kSharedMutexName  = "MSICoolersSharedControlMtx6";

    device = CreateDeviceController(true);
    using namespace boost::interprocess;

    sharedMutex = std::shared_ptr<named_mutex>(new named_mutex(create_only,
                  kSharedMutexName.c_str()), [](auto ptr)
    {
        if (ptr)
        {
            named_mutex::remove(kSharedMutexName.c_str());
            delete ptr;
        }
    });

    sharedMemory = std::shared_ptr<shared_memory_object>(new shared_memory_object(create_only,
                   kSharedMemoryName.c_str(), read_write), [](auto* ptr)
    {
        if (ptr)
        {
            shared_memory_object::remove(kSharedMemoryName.c_str());
            delete ptr;
        }
    });
    sharedMemory->truncate(kWholeSharedMemSize);

    if (!sharedMutex || !sharedMemory || !device)
    {
        throw std::runtime_error("Failed to create at least one important object.");
    }
}

CSharedDevice::~CSharedDevice()
{
    {
        using namespace boost::interprocess;
        scoped_lock<named_mutex> grd(*sharedMutex);
        if (device)
        {
            //when we done - switch to AUTO most common things.
            try
            {
                device->SetBehaveState(BehaveState::AUTO);
            }
            catch(std::exception& ex)
            {
                std::cerr << "MSI: Failed to restore AUTO behave on exit: " << ex.what();
            }
            catch(...)
            {
                std::cerr << "MSI: Failed to restore AUTO behave on exit.";
            }
        }
        try
        {
            device.reset();
        }
        catch(...) {}

        try
        {
            sharedMemory.reset();
        }
        catch(...) {}
    }
    try
    {
        sharedMutex.reset();
    }
    catch(...)
    {
    }
}

void CSharedDevice::Communicate()
{
    using namespace boost::interprocess;

    mapped_region region{*sharedMemory, read_write};
    scoped_lock<named_mutex> grd(*sharedMutex);
    {
        const auto info = device->ReadFullInformation();
        MemBuf buffer(static_cast<char*>(region.get_address()), region.get_size() / 2);
        std::ostream ss(&buffer);
        cereal::BinaryOutputArchive oarchive(ss);
        oarchive(info);
    }

#ifndef NDEBUG
    //std::cerr << "Updated..."<<std::endl;
#endif
}
