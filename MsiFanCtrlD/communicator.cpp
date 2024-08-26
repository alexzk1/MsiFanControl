#include <algorithm>
#include <boost/interprocess/creation_tags.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/types/string.hpp>
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
#include "csysfsprovider.h"

//This is daemon side communicator.

static constexpr bool kDryRun = false;

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
        try
        {
            {
                std::ifstream inp(file);
                inp >> old_value;
            }
            std::ofstream out(file, std::ios_base::trunc);
            out << "0";
        }
        catch (std::exception& ex)
        {
            std::cout << "Failed to relax kernel. GUI may not connect: " << ex.what()
                      << std::endl << std::flush;
        }
    }

    RelaxKernel() : RelaxKernel("/proc/sys/fs/protected_regular")
    {
    }

    ~RelaxKernel()
    {
        try
        {
            std::ofstream out(file, std::ios_base::trunc);
            out << old_value;
        }
        catch (std::exception& ex)
        {
            std::cout << "Failed restore security on files. Reboot to restore: " << ex.what()
                      << std::endl << std::flush;
        }
    }
};

//Making this separated class because we need to pass shared_ptr.
struct BackupExecutorImpl final : public IBackupProvider
{
    ~BackupExecutorImpl() override = default;
    BackupExecutorImpl() = delete;
    explicit BackupExecutorImpl(CSharedDevice* owner)
        :owner(owner)
    {
    }

    void RestoreOffsets(std::set<int64_t> offsetsToRestoreFromBackup) const final
    {
        if (owner)
        {
            owner->RestoreOffsets(std::move(offsetsToRestoreFromBackup));
        }
    }

    CSharedDevice* owner{nullptr};
};

CSharedDevice::CSharedDevice()
    : memoryCleaner()
{
    //Must be 1st to create.
    MakeBackupBlock();

    device = CreateDeviceController(std::make_shared<BackupExecutorImpl>(this), kDryRun);
    using namespace boost::interprocess;

    RelaxKernel relax;
    permissions unrestricted_permissions;
    unrestricted_permissions.set_unrestricted();

    shared_memory_object shm(open_or_create,
                             GetMemoryName(), read_write, unrestricted_permissions);
    shm.truncate(kWholeSharedMemSize);
    sharedMem = std::make_shared<SharedMemoryWithMutex>(std::move(shm));
    sharedMem->DaemonReadUI();
}

CSharedDevice::~CSharedDevice()
{
    try
    {
        device.reset();
    }
    catch (...) {}

    try
    {
        sharedMem.reset();
    }
    catch (...) {}

    //must be destroyed after device
    try
    {
        sharedBackup.reset();
    }
    catch (...) {}
}

void CSharedDevice::Communicate()
{
    using namespace boost::interprocess;
    static std::size_t tag = 0;

    FullInfoBlock info;
    try
    {
        info = device->ReadFullInformation(++tag);
    }
    catch (std::exception& ex)
    {
        info.daemonDeviceException = ex.what();
        std::cerr << "Failure reading info: " << ex.what() << std::endl << ::std::flush;
    }

    RequestFromUi fromUI;
    {
        scoped_lock<interprocess_mutex> grd(sharedMem->Mutex());

        auto buffer = sharedMem->Daemon2UI();
        std::ostream ss(&buffer);
        cereal::BinaryOutputArchive oarchive(ss);
        oarchive(info);

        if (sharedMem->IsUiPushed())
        {
            auto buffer = sharedMem->UI2Daemon();
            std::istream ss(&buffer);
            try
            {
                cereal::BinaryInputArchive iarchive(ss);
                iarchive(fromUI);
            }
            catch (std::exception& ex)
            {
                sharedMem->DaemonReadUI();
                std::cerr << "Failed to read/parse  UI command: " << ex.what() << std::endl <<
                          std::flush;
                return;
            }
            sharedMem->DaemonReadUI();
        }
    }

    device->SetBooster(fromUI.boosterState);
}

void CSharedDevice::RestoreOffsets(std::set<int64_t> offsetsToRestoreFromBackup)
const
{
    //This will be called when destructor does device.reset()
    if (sharedBackup)
    {
        try
        {
            //copy ACPI file to shared memory which we just allocated. It should persist between runs
            //and keep 1st run copy, i.e. original data.
            auto stream = CSysFsProvider::CreateIoDirect(kDryRun)->WriteStream();
            for (const auto& offset : offsetsToRestoreFromBackup)
            {
                try
                {
                    stream.seekp(offset);
                    stream.write(sharedBackup->Ptr() + offset, 1);
                }
                catch (std::exception& ex)
                {
                    std::cerr << "Failed to restore backup on offset " << offset << "(decimal): " <<
                              ex.what()
                              <<std::endl << std::flush;
                }
            }
        }
        catch (std::exception& ex)
        {
            std::cerr << "Failed IO during restoring backup. Backup was not restored: " <<
                      ex.what()
                      << std::endl << std::flush;
        }
    }
}

void CSharedDevice::MakeBackupBlock()
{
    //This block is created ONLY on 1st run after reboot. If daemon is restarted, memory remains "leaked" until reboot.
    //It keeps original ACPI data, not modified.
    using namespace boost::interprocess;
    static const char* kBackupName = "MSIFansACPIBackup";
    static constexpr auto kExpectedSize = 256;

    try
    {
        auto shm = shared_memory_object(create_only, kBackupName, read_write);
        shm.truncate(kExpectedSize);

        sharedBackup = std::make_shared<SharedMemory>(std::move(shm));
        try
        {
            //copy ACPI file to shared memory which we just allocated. It should persist between runs
            //and keep 1st run copy, i.e. original data.
            const auto io = CSysFsProvider::CreateIoDirect(kDryRun);
            io->ReadStream().read(sharedBackup->Ptr(), sharedBackup->Size());
        }
        catch (std::exception& ex)
        {
            std::cerr << "Creating IO failed for backup. Backup was disabled: " << ex.what()
                      << std::endl << std::flush;

            sharedBackup.reset();
            shared_memory_object::remove(kBackupName);
            return;
        }

        return;
    }
    catch (...)
    {
        //This is expected fail, means this is 2nd+ run after reboot and backup exists already.
    }

    auto shm = shared_memory_object(open_only, kBackupName, read_write);
    sharedBackup = std::make_shared<SharedMemory>(std::move(shm));
}
