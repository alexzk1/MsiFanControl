#include "device.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>

bool ___GLOBAL_DRY_RUN___ = false;

//Cannot use assert! Because it does not call destructors and shared memory remains allocated in daemon.
#ifndef NDEBUG
static void Throw(bool cond, std::string text)
{
    if (!cond && !___GLOBAL_DRY_RUN___)
    {
        throw std::runtime_error(text);
    }
}
#else
#define Throw(COND, TEXT)
#endif

Info::Info(const AddressedValueAny &temp, const AddressedValueAny &rpm)
{
    Throw(std::holds_alternative<AddressedValue1B>(temp),
          "We expect 1 byte request for the temperature.");
    Throw(std::holds_alternative<AddressedValue2B>(rpm),
          "We expect 2 bytes request for the rpm.");

    std::visit([this](const auto& val)
    {
        temperature = val.value;
    }, temp);

    std::visit([this](const auto& val)
    {
        if (val.value)
        {
            fanRPM = static_cast<std::uint16_t>(478000.0 / val.value);
        }
    }, rpm);
}

CDevice::CDevice(CReadWrite readWrite)
    : readWriteAccess(std::move(readWrite))
{
}

CDevice::~CDevice()
{

}

CpuGpuInfo CDevice::ReadInfo() const
{
    auto cmd = GetCmdTempRPM();
    Throw(cmd.size() == 4,
          "We expect 4 commands: temp getter, then RPM getter for CPU, then for GPU.");
    readWriteAccess.Read(cmd);
    Info cpu(cmd.at(0), cmd.at(1));
    Info gpu(cmd.at(2), cmd.at(3));

    return {std::move(cpu), std::move(gpu)};
}

BoosterState CDevice::ReadBoosterState() const
{
    auto cmd   = GetCmdBoosterStates();
    const auto clone = cmd;
    readWriteAccess.Read(cmd);

    const auto diff = cmd.GetOneDifference(clone);
    Throw(diff != std::nullopt, "Something went wrong. Read should indicate BOOSTER's changed state.");

    //We read OFF state different, that means there is ON state in device.
    return diff->first == BoosterState::OFF ? BoosterState::ON : BoosterState::OFF;
}

void CDevice::SetBooster(CReadWrite::WriteHandle &handle, const BoosterState what) const
{
    auto cmd   = GetCmdBoosterStates();
    readWriteAccess.Write(handle, {std::move(cmd.at(what))});
}

void CDevice::SetBooster(const BoosterState what) const
{
    auto handle = readWriteAccess.StartWritting();
    SetBooster(handle, what);
}

BehaveWithCurve CDevice::ReadBehaveState() const
{
    auto cmd = GetCmdBehaveStates();
    const auto clone = cmd;
    readWriteAccess.Read(cmd);

    const auto diff = cmd.GetOneDifference(clone);
    Throw(diff != std::nullopt, "Something went wrong. Read should indicate BEHAVE's changed state.");

    //Same logic as in booster, if "auto" is different, then "advanced" is set there.
    BehaveWithCurve res;
    res.behaveState = diff->first == BehaveState::AUTO ? BehaveState::ADVANCED :
                      BehaveState::AUTO;

    readWriteAccess.Read(res.curve.cpu);
    readWriteAccess.Read(res.curve.gpu);
    return res;
}

void CDevice::SetBehaveState(BehaveWithCurve behaveWithCurve) const
{
    auto cmd = GetCmdBehaveStates();
    auto handle = readWriteAccess.StartWritting();

    if (BehaveState::NO_CHANGE != behaveWithCurve.behaveState)
    {
        behaveWithCurve.curve.Validate();
        readWriteAccess.Write(handle,  std::move(behaveWithCurve.curve.cpu));
        readWriteAccess.Write(handle,  std::move(behaveWithCurve.curve.gpu));
        readWriteAccess.Write(handle, {std::move(cmd.at(behaveWithCurve.behaveState))});
    }
}

FullInfoBlock CDevice::ReadFullInformation(std::size_t aTag) const
{
    return {aTag, ReadInfo(), ReadBoosterState(), ReadBehaveState(), {}};
}

AddressedValueAnyList CDevice::GetCmdTempRPM() const
{
    //CPU_GPU_TEMP_ADDRESS, CPU_GPU_RPM_ADDRESS in python sample code.
    static const AddressedValueAny cpuGetters[] =
    {
        AddressedValue1B{0x68, 0}, //temperature
        AddressedValue2B{0xC8, 0}, //rpm
    };

    static const AddressedValueAny gpuGetters[] =
    {
        AddressedValue1B{0x80, 0}, //temperature
        AddressedValue2B{0xCA, 0}, //rpm
    };

    static const AddressedValueAnyList combined =
    {
        cpuGetters[0], cpuGetters[1],
        gpuGetters[0], gpuGetters[1],
    };

    return combined;
}

CDevice::BoosterStates CDevice::GetCmdBoosterStates() const
{
    //COOLER_BOOSTER_OFF_ON_VALUES in python example code.
    static const AddressedBits kBoosterOff{0x98, 0x80, 0};
    static const AddressedBits kBoosterOn {0x98, 0x80, 0x80};

    static const BoosterStates booster =
    {
        //std::map
        {
            {BoosterState::OFF, kBoosterOff},
            {BoosterState::ON, kBoosterOn},
            {BoosterState::NO_CHANGE, TagIgnore{}},
        }
    };

    return booster;
}


