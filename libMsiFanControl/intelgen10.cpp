#include "intelgen10.h"

CIntelGen10::CIntelGen10(CReadWrite readWrite) : CDevice(std::move(readWrite))
{
}

CIntelGen10::~CIntelGen10()
{
}

CDevice::BoosterStates CIntelGen10::GetCmdBoosterStates() const
{
    //COOLER_BOOSTER_OFF_ON_VALUES in python example code.
    static const AddressedValue1B kBoosterOff{0x98, 2};
    static const AddressedValue1B kBoosterOn{0x98, 130};

    static const BoosterStates boosterGen10 =
    {
        //std::map
        {
            {BoosterState::OFF, kBoosterOff},
            {BoosterState::ON, kBoosterOn},
        }
    };

    return boosterGen10;
}

CDevice::BehaveStates CIntelGen10::GetCmdBehaveStates() const
{
    //AUTO_ADV_VALUES in python example code.
    static const AddressedValue1B kAuto{0xD4, 13};
    static const AddressedValue1B kAdvanced{0xD4, 141};

    static const BehaveStates behaveGen10 =
    {
        {
            {BehaveState::AUTO, kAuto},
            {BehaveState::ADVANCED, kAdvanced},
        }
    };

    return behaveGen10;
}
