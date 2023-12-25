#include "intelbeforegen10.h"

CIntelBeforeGen10::CIntelBeforeGen10(CReadWrite readWrite) : CDevice(std::move(readWrite))
{
}

CIntelBeforeGen10::~CIntelBeforeGen10()
{
}

CDevice::BoosterStates CIntelBeforeGen10::GetCmdBoosterStates() const
{
    //COOLER_BOOSTER_OFF_ON_VALUES in python example code.
    static const AddressedValue1B kBoosterOff{0x98, 0};
    static const AddressedValue1B kBoosterOn{0x98, 128};

    static const BoosterStates boosterBeforeGen10 =
    {
        //std::map
        {
            {BoosterState::OFF, kBoosterOff},
            {BoosterState::ON, kBoosterOn},
        }
    };

    return boosterBeforeGen10;
}

CDevice::BehaveStates CIntelBeforeGen10::GetCmdBehaveStates() const
{
    //AUTO_ADV_VALUES in python example code.
    static const AddressedValue1B kAuto{0xF4, 12};
    static const AddressedValue1B kAdvanced{0xF4, 140};

    static const BehaveStates behaveBeforeGen10 =
    {
        {
            {BehaveState::AUTO, kAuto},
            {BehaveState::ADVANCED, kAdvanced},
        }
    };

    return behaveBeforeGen10;
}
