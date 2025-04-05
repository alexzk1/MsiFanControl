#include "intelgen10.h"

#include "device.h" // IWYU pragma: keep
#include "device_commands.h"
#include "readwrite.h"

#include <utility>

CIntelGen10::CIntelGen10(CReadWrite readWrite) :
    CDevice(std::move(readWrite))
{
}
CIntelGen10::~CIntelGen10() = default;

CDevice::BehaveStates CIntelGen10::GetCmdBehaveStates() const
{
    // AUTO_ADV_VALUES in python example code.
    static const AddressedValue1B kAuto{0xD4, 13};
    static const AddressedValue1B kAdvanced{0xD4, 141};

    static const BehaveStates behaveGen10 = {{
      {BehaveState::AUTO, kAuto},
      {BehaveState::ADVANCED, kAdvanced},
      {BehaveState::NO_CHANGE, TagIgnore{}},
    }};

    return behaveGen10;
}
