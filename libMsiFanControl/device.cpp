#include "device.h"
#include <algorithm>
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
        Throw((temperature == 0 || (temperature > 10 && temperature < 150)),
              "Something is messed up. Temperature is weird.");
    }, temp);

    std::visit([this](const auto& val)
    {
        if (val.value)
        {
            fanRPM = static_cast<std::uint16_t>(478000.0 / val.value);
        }
        Throw((fanRPM == 0 || (fanRPM > 500 && fanRPM < 12000)),
              "Something is messed up. RPM is weird.");
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
    Throw(diff != std::nullopt, "Something went wrong. Read should indicate changed state.");

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

BehaveState CDevice::ReadBehaveState() const
{
    auto cmd = GetCmdBehaveStates();
    const auto clone = cmd;
    readWriteAccess.Read(cmd);

    const auto diff = cmd.GetOneDifference(clone);
    Throw(diff != std::nullopt, "Something went wrong. Read should indicate changed state.");

    //Same logic as in booster, if "auto" is different, then "advanced" is set there.
    return diff->first == BehaveState::AUTO ? BehaveState::ADVANCED : BehaveState::AUTO;
}

void CDevice::SetBehaveState(const BehaveState what, CpuGpuFanCurve fanCurve) const
{
    fanCurve.Validate();

    auto cmd = GetCmdBehaveStates();
    auto handle = readWriteAccess.StartWritting();

    SetBooster(handle, BoosterState::OFF);
    readWriteAccess.Write(handle, fanCurve.cpu);
    readWriteAccess.Write(handle, fanCurve.gpu);
    readWriteAccess.Write(handle, {std::move(cmd.at(what))});
}

FullInfoBlock CDevice::ReadFullInformation() const
{
    return {ReadInfo(), ReadBoosterState(), ReadBehaveState()};
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

CpuGpuFanCurve CpuGpuFanCurve::MakeDefault()
{
    //Make default fan curve. Address depends on device, and in 99% it will remain the same.
    //Curve itself, looks like we have 7 speed steps, each step (index into vector) is activated at given temperature.
    static const AddressedValueAnyList cpuCurve =
    {
        AddressedValue1B{0x72, 0},
        AddressedValue1B{0x73, 40},
        AddressedValue1B{0x74, 48},
        AddressedValue1B{0x75, 56},
        AddressedValue1B{0x76, 64},
        AddressedValue1B{0x77, 72},
        AddressedValue1B{0x78, 80},
    };

    static const AddressedValueAnyList gpuCurve =
    {
        AddressedValue1B{0x8A, 0},
        AddressedValue1B{0x8B, 48},
        AddressedValue1B{0x8C, 56},
        AddressedValue1B{0x8D, 64},
        AddressedValue1B{0x8E, 72},
        AddressedValue1B{0x8F, 79},
        AddressedValue1B{0x90, 86},
    };

    return {cpuCurve, gpuCurve};
}

void CpuGpuFanCurve::Validate() const
{
    static const auto validateCurve = [](const AddressedValueAnyList& src)
    {
        std::adjacent_find(src.begin(), src.end(), [](const auto& v1, const auto& v2)
        {
            if (!std::holds_alternative<AddressedValue1B>(v1) || !std::holds_alternative<AddressedValue1B>(v2))
            {
                throw std::invalid_argument("Curve must contain 1 byte values only!!!");
            }
            const auto& vb1 = std::get<AddressedValue1B>(v1);
            const auto& vb2 = std::get<AddressedValue1B>(v2);

            const bool valid = vb1.address < vb2.address && vb1.value <= vb2.value;
            if (!valid)
            {
                throw std::runtime_error("Invalid fan's curve detected. It must increase or remain the same.");
            }
            return false;
        });
    };

    validateCurve(cpu);
    validateCurve(gpu);
}
