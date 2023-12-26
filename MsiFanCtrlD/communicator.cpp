#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/archives/binary.hpp>

#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/permissions.hpp>

#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>

#include <stdexcept>
#include <string>
#include <ostream>

#include "msi_fan_control.h"
#include "communicator.h"

//This is daemon side communicator.

static_assert(kWholeSharedMemSize % 2 == 0, "Wrong size.");

//Ok, idea is, on 1st half of the memory we will put cereal serialized current state like temperature / rpm.
//From the 2nd half we will read contol if any.

//Kernel does not allow to set 0x666 on shared memory.
struct RelaxKernel
{
    std::filesystem::path file;
    std::string old_value;
    RelaxKernel(std::filesystem::path aFile) : file(std::move(aFile))
    {
        {
            std::ifstream inp(file);
            inp >> old_value;
        }
        std::ofstream out(file, std::ios_base::trunc);
        out << "0";
    }

    RelaxKernel() : RelaxKernel("/proc/sys/fs/protected_regular")
    {
    }

    ~RelaxKernel()
    {
        std::ofstream out(file, std::ios_base::trunc);
        out << old_value;
    }
};

CSharedDevice::CSharedDevice()
    : memoryCleaner()
{
    device = CreateDeviceController(false);
    using namespace boost::interprocess;

    RelaxKernel relax;

    permissions  unrestricted_permissions;
    unrestricted_permissions.set_unrestricted();

    shared_memory_object shm(create_only,
                             GetMemoryName(), read_write, unrestricted_permissions);
    shm.truncate(kWholeSharedMemSize);
    sharedMem = std::make_shared<SharedMemoryWithMutex>(std::move(shm));
}

CSharedDevice::~CSharedDevice()
{
    try
    {
        using namespace boost::interprocess;
        scoped_lock<interprocess_mutex> grd(sharedMem->Mutex());
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
    }
    catch(...)
    {
    }

    try
    {
        sharedMem.reset();
    }
    catch(...) {}
}

void CSharedDevice::Communicate()
{
    using namespace boost::interprocess;

    scoped_lock<interprocess_mutex> grd(sharedMem->Mutex());
    {
        const auto info = device->ReadFullInformation();
        auto buffer = sharedMem->Daemon2UI();
        std::ostream ss(&buffer);
        cereal::BinaryOutputArchive oarchive(ss);
        oarchive(info);
    }

#ifndef NDEBUG
    //std::cerr << "Updated..."<<std::endl;
#endif
}
