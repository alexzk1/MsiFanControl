#include "intelbeforegen10.h"

#include "device.h" // IWYU pragma: keep
#include "device_commands.h"
#include "readwrite.h"

#include <utility>

CIntelBeforeGen10::CIntelBeforeGen10(CReadWrite readWrite) :
    CDevice(std::move(readWrite))
{
}
CIntelBeforeGen10::~CIntelBeforeGen10() = default;

CDevice::BehaveStates CIntelBeforeGen10::GetCmdBehaveStates() const
{
    // AUTO_ADV_VALUES in python example code.
    static const AddressedValue1B kAuto{0xF4, 12};
    static const AddressedValue1B kAdvanced{0xF4, 140};

    static const BehaveStates behaveBeforeGen10 = {{
      {BehaveState::AUTO, kAuto},
      {BehaveState::ADVANCED, kAdvanced},
      {BehaveState::NO_CHANGE, TagIgnore{}},
    }};

    return behaveBeforeGen10;
}
