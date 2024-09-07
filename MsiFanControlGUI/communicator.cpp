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

#include <bits/chrono.h>
#include <istream>
#include <memory>
#include <ostream>
#include <utility>
#include <thread>

#include "communicator.h"
#include "device.h"

//This is GUI side communicator

static_assert(kWholeSharedMemSize % 2 == 0, "Wrong size.");

//Ok, idea is, on 1st half of the memory we will put cereal serialized current state like temperature / rpm.
//From the 2nd half we will read contol if any.

CSharedDevice::CSharedDevice(utility::runnerint_t should_stop):
    should_stop(std::move(should_stop))
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

const FullInfoBlock& CSharedDevice::LastKnownInfo() const
{
    return lastKnownInfo;
}

bool CSharedDevice::PingDaemon()
{
    static const RequestFromUi ping{RequestFromUi::RequestType::PING_DAEMON};

    SendRequest(ping);
    return UpdateInfoFromDaemon();
}

bool CSharedDevice::SetBooster(BoosterState newState)
{
    const RequestFromUi writeBooster{RequestFromUi::RequestType::WRITE_DATA, newState};

    SendRequest(writeBooster);
    return UpdateInfoFromDaemon();
}

bool CSharedDevice::RefreshData()
{
    static const RequestFromUi readRequest{RequestFromUi::RequestType::READ_FRESH_DATA};

    SendRequest(readRequest);
    return UpdateInfoFromDaemon();
}

void CSharedDevice::SendRequest(const RequestFromUi& request) const
{
    using namespace boost::interprocess;
    const scoped_lock<interprocess_mutex> grd(sharedMem->Mutex());
    auto buffer = sharedMem->UI2Daemon();
    std::ostream ss(&buffer);
    cereal::BinaryOutputArchive oarchive(ss);
    oarchive(request);
    sharedMem->UIPushedForDaemon();
}

bool CSharedDevice::UpdateInfoFromDaemon()
{
    using namespace boost::interprocess;

    if (!WaitDaemonRead())
    {
        return false;
    }

    const auto old_tag = lastKnownInfo.tag;
    const scoped_lock<interprocess_mutex> grd(sharedMem->Mutex());
    auto buffer = sharedMem->Daemon2UI();
    std::istream ss(&buffer);
    cereal::BinaryInputArchive iarchive(ss);
    iarchive(lastKnownInfo);

    return old_tag < lastKnownInfo.tag;
}

bool CSharedDevice::WaitDaemonRead() const
{
    using namespace boost::interprocess;

    constexpr auto half = kMinimumServiceDelay / 2;
    std::this_thread::sleep_for(half);

    bool ok = false;
    for (int i = 0; !ok && i < 15 && !(*should_stop); ++i)
    {
        std::this_thread::sleep_for(half);

        const scoped_lock<interprocess_mutex> grd(sharedMem->Mutex());
        ok = !sharedMem->IsUiPushed();
    }

    return ok;
}
