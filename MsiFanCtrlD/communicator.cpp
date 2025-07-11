
#include "communicator.h" // IWYU pragma: keep

#include "cm_ctors.h" // IWYU pragma: keep
#include "communicator_common.h"
#include "csysfsprovider.h" // IWYU pragma: keep
#include "device.h"         // IWYU pragma: keep
#include "messages_types.h"
#include "msi_fan_control.h"
#include "readwrite_provider.h" // IWYU pragma: keep

#include <boost/interprocess/creation_tags.hpp>            // IWYU pragma: keep
#include <boost/interprocess/detail/os_file_functions.hpp> // IWYU pragma: keep
#include <boost/interprocess/permissions.hpp>              // IWYU pragma: keep
#include <boost/interprocess/sync/interprocess_mutex.hpp>  // IWYU pragma: keep
#include <boost/interprocess/sync/scoped_lock.hpp>         // IWYU pragma: keep

#include <cereal/archives/binary.hpp>
#include <cereal/types/array.hpp>   // IWYU pragma: keep
#include <cereal/types/map.hpp>     // IWYU pragma: keep
#include <cereal/types/string.hpp>  // IWYU pragma: keep
#include <cereal/types/variant.hpp> // IWYU pragma: keep
#include <cereal/types/vector.hpp>  // IWYU pragma: keep

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <utility>

// This is daemon side communicator.

namespace {
// Ok, idea is, on 1st half of the memory we will put cereal serialized current state like
// temperature / rpm. From the 2nd half we will read contol if any.

/// @brief Does backup of 1 liner files (should be used on sysfs).
class BackupOneLiner
{
  public:
    explicit BackupOneLiner(std::filesystem::path aFile) :
        file(std::move(aFile))
    {
        try
        {
            std::ifstream inp(file);
            std::string tmp;
            inp >> tmp;
            old_value = std::move(tmp);
        }
        catch (std::exception &ex)
        {
            std::cerr << "Failed to backup file: " << file << ". Reason: " << ex.what() << std::endl
                      << std::flush;
            old_value = std::nullopt;
        }
    }
    BackupOneLiner() = delete;
    NO_COPYMOVE(BackupOneLiner);

    ~BackupOneLiner()
    {
        try
        {
            if (old_value)
            {
                std::ofstream out(file, std::ios_base::trunc);
                out << *old_value;
            }
        }
        catch (std::exception &ex)
        {
            std::cerr << "Failed to restore file: " << file << ". Reason: " << ex.what()
                      << std::endl
                      << std::flush;
        }
    }

  private:
    std::optional<std::string> old_value;
    std::filesystem::path file;
};

// Kernel does not allow to set 0x666 on shared memory.
struct RelaxKernel : public BackupOneLiner
{
    // NOLINTNEXTLINE
    std::filesystem::path file;
    // NOLINTNEXTLINE
    std::string old_value;

    NO_COPYMOVE(RelaxKernel);
    explicit RelaxKernel(std::filesystem::path aFile) :
        BackupOneLiner(std::move(aFile))
    {
        try
        {
            WriteFsBool(file, false);
        }
        catch (std::exception &ex)
        {
            std::cerr << "Failed to relax kernel. GUI may not connect: " << ex.what() << std::endl
                      << std::flush;
        }
    }

    RelaxKernel() :
        RelaxKernel({"/proc/sys/fs/protected_regular"})
    {
    }

    ~RelaxKernel() = default;
};

constexpr bool kDryRun = false;
static_assert(kWholeSharedMemSize % 2 == 0, "Wrong size.");

} // namespace

// Making this separated class because we need to pass shared_ptr.
class BackupExecutorImpl final : public IBackupProvider
{
  public:
    ~BackupExecutorImpl() override = default;
    BackupExecutorImpl() = delete;
    NO_COPYMOVE(BackupExecutorImpl);
    explicit BackupExecutorImpl(CSharedDevice *owner) :
        owner(owner)
    {
    }

    void RestoreOffsets(const std::set<int64_t> &offsetsToRestoreFromBackup) const final
    {
        if (owner)
        {
            owner->RestoreOffsets(offsetsToRestoreFromBackup);
        }
    }

  private:
    // NOLINTNEXTLINE
    CSharedDevice *owner{nullptr};
    const BackupOneLiner backupTurboBoost{kIntelPStateNoTurbo};
};

CSharedDevice::CSharedDevice() :
    memoryCleaner(),
    lastReadInfo()
{
    // Must be 1st to create.
    MakeBackupBlock();

    device = CreateDeviceController(std::make_shared<BackupExecutorImpl>(this), kDryRun);
    using namespace boost::interprocess;

    const RelaxKernel relax;
    permissions unrestricted_permissions;
    unrestricted_permissions.set_unrestricted();

    shared_memory_object shm(open_or_create, GetMemoryName(), read_write, unrestricted_permissions);
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
    // NOLINTNEXTLINE
    catch (...)
    {
    }

    try
    {
        sharedMem.reset();
    }
    // NOLINTNEXTLINE
    catch (...)
    {
    }

    // must be destroyed after device
    try
    {
        sharedBackup.reset();
    }
    // NOLINTNEXTLINE
    catch (...)
    {
    }
}

void CSharedDevice::Communicate()
{
    using namespace boost::interprocess;

    RequestFromUi fromUI;
    {
        const scoped_lock<interprocess_mutex> grd(sharedMem->Mutex());
        if (!sharedMem->IsUiPushed())
        {
            return;
        }

        auto readUiBuffer = sharedMem->UI2Daemon();
        std::istream ss(&readUiBuffer);
        try
        {
            cereal::BinaryInputArchive iarchive(ss);
            iarchive(fromUI);
        }
        catch (std::exception &ex)
        {
            sharedMem->DaemonReadUI();
            std::cerr << "Failed to read/parse  UI command: " << ex.what() << std::endl
                      << std::flush;
            return;
        }
    }

    // We have some request from UI so we must respond with at least incremented tag.
    ++lastReadInfo.tag;

    if (fromUI.request != RequestFromUi::RequestType::PING_DAEMON)
    {
        if (fromUI.request == RequestFromUi::RequestType::WRITE_DATA)
        {
            // Write data sent by UI.
            device->SetBoosters(fromUI.boostersStates);
            device->SetBattery(fromUI.battery);
        }

        // Read fresh data from BIOS
        try
        {
            lastReadInfo = device->ReadFullInformation(lastReadInfo.tag);
        }
        catch (std::exception &ex)
        {
            lastReadInfo.daemonDeviceException = ex.what();
            std::cerr << "Failure reading info: " << ex.what() << std::endl << ::std::flush;
        }
    }

    const scoped_lock<interprocess_mutex> grd(sharedMem->Mutex());
    auto daemonWritebuffer = sharedMem->Daemon2UI();
    std::ostream ss(&daemonWritebuffer);
    cereal::BinaryOutputArchive oarchive(ss);
    oarchive(lastReadInfo);

    sharedMem->DaemonReadUI();
}

void CSharedDevice::RestoreOffsets(const std::set<int64_t> &offsetsToRestoreFromBackup) const
{
    // This will be called when destructor does device.reset()
    if (sharedBackup)
    {
        try
        {
            // copy ACPI file to shared memory which we just allocated. It should persist between
            // runs and keep 1st run copy, i.e. original data.
            auto stream = CSysFsProvider::CreateIoDirect(kDryRun)->WriteStream();
            for (const auto &offset : offsetsToRestoreFromBackup)
            {
                try
                {
                    stream.seekp(offset);
                    // NOLINTNEXTLINE
                    stream.write(sharedBackup->Ptr() + offset, 1);
                }
                catch (std::exception &ex)
                {
                    std::cerr << "Failed to restore backup on offset " << offset
                              << "(decimal): " << ex.what() << std::endl
                              << std::flush;
                }
            }
        }
        catch (std::exception &ex)
        {
            std::cerr << "Failed IO during restoring backup. Backup was not restored: " << ex.what()
                      << std::endl
                      << std::flush;
        }
    }
}

bool CSharedDevice::MakeBackupBlock()
{
    // This block is created ONLY on 1st run after reboot. If daemon is restarted, memory remains
    // "leaked" until reboot. It keeps original ACPI data, not modified.
    using namespace boost::interprocess;
    static const char *kBackupName = "MSIFansACPIBackup";

    try
    {
        auto shm = shared_memory_object(create_only, kBackupName, read_write);
        shm.truncate(kBackupSharedSize);

        sharedBackup = std::make_shared<SharedMemory>(std::move(shm));
        try
        {
            // copy ACPI file to shared memory which we just allocated. It should persist between
            // runs and keep 1st run copy, i.e. original data.
            const auto io = CSysFsProvider::CreateIoDirect(kDryRun);
            io->ReadStream().read(sharedBackup->Ptr(),
                                  static_cast<std::int64_t>(sharedBackup->Size()));
        }
        catch (std::exception &ex)
        {
            std::cerr << "Creating IO failed for backup. Backup was disabled: " << ex.what()
                      << std::endl
                      << std::flush;

            sharedBackup.reset();
            shared_memory_object::remove(kBackupName);
            return false;
        }
        return true;
    }
    // NOLINTNEXTLINE
    catch (...)
    {
        // This is expected fail, means this is 2nd+ run after reboot and backup exists already.
    }

    auto shm = shared_memory_object(open_only, kBackupName, read_write);
    sharedBackup = std::make_shared<SharedMemory>(std::move(shm));
    return false;
}
