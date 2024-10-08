#include <algorithm>
#include <array>
#include <cstddef>
#include <iosfwd>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <set>
#include <utility>
#include <variant>

#include "device.h"
#include "device_commands.h"
#include "readwrite.h"

//NOLINTNEXTLINE
bool GLOBAL_DRY_RUN = false;

namespace {
//Cannot use assert! Because it does not call destructors and shared memory remains allocated in daemon.
#ifndef NDEBUG
void Throw(bool cond, const std::string& text)
{
    if (!cond && !GLOBAL_DRY_RUN)
    {
        throw std::runtime_error(text);
    }
}
#else
#define Throw(COND, TEXT)
#endif

using AddressedValueAllowedAddresses = std::set<std::streampos>;
struct AllowedAddresses
{
    AddressedValueAllowedAddresses cpu;
    AddressedValueAllowedAddresses gpu;
};

AllowedAddresses BuildAllowedAdressesFromDefaults()
{
    static const auto makeSingle = [](const AddressedValueAnyList& src)
    {
        AddressedValueAllowedAddresses res;
        std::transform(src.begin(), src.end(), std::inserter(res, res.end()),
                       [](const auto& value)
        {
            const auto& vb = std::get<AddressedValue1B>(value);
            return vb.address;
        });
        return res;
    };
    const auto curves = CpuGpuFanCurve::MakeDefault();
    return {makeSingle(curves.cpu), makeSingle(curves.gpu)};
}
} // namespace

//NOLINTNEXTLINE
Info::Info(const AddressedValueAny& temp, const AddressedValueAny& rpm) :
    temperature(parseTemp(temp)),
    fanRPM(parseRPM(rpm))
{
}

CDevice::CDevice(CReadWrite readWrite)
    : readWriteAccess(std::move(readWrite))
{
}
CDevice::~CDevice() = default;

CpuGpuInfo CDevice::ReadInfo() const
{
    auto cmd = GetCmdTempRPM();
    Throw(cmd.size() == 4,
          "We expect 4 commands: temp getter, then RPM getter for CPU, then for GPU.");
    readWriteAccess.Read(cmd);
    const Info cpu(cmd.at(0), cmd.at(1));
    const Info gpu(cmd.at(2), cmd.at(3));

    return {cpu, gpu};
}

BoosterState CDevice::ReadBoosterState() const
{
    auto cmd = GetCmdBoosterStates();
    const auto clone = cmd;
    readWriteAccess.Read(cmd);

    const auto diff = cmd.GetOneDifference(clone);
    Throw(diff != std::nullopt,
          "Something went wrong. Read should indicate BOOSTER's changed state.");

    //We read OFF state different, that means there is ON state in device.
    return !diff || diff->first == BoosterState::OFF ? BoosterState::ON :
           BoosterState::OFF;
}

void CDevice::SetBooster(CReadWrite::WriteHandle& handle,
                         const BoosterState what) const
{
    auto cmd = GetCmdBoosterStates();
    readWriteAccess.Write(handle, {cmd.at(what)});
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
    Throw(diff != std::nullopt,
          "Something went wrong. Read should indicate BEHAVE's changed state.");

    //Same logic as in booster, if "auto" is different, then "advanced" is set there.
    BehaveWithCurve res;
    res.behaveState = diff && diff->first == BehaveState::AUTO ? BehaveState::ADVANCED :
                      BehaveState::AUTO;

    readWriteAccess.Read(res.curve.cpu);
    readWriteAccess.Read(res.curve.gpu);
    return res;
}

void CDevice::SetBehaveState(const BehaveWithCurve& behaveWithCurve) const
{
    auto cmd = GetCmdBehaveStates();
    auto handle = readWriteAccess.StartWritting();

    if (BehaveState::NO_CHANGE != behaveWithCurve.behaveState)
    {
        behaveWithCurve.curve.Validate();
        readWriteAccess.Write(handle, behaveWithCurve.curve.cpu);
        readWriteAccess.Write(handle, behaveWithCurve.curve.gpu);
        readWriteAccess.Write(handle, {cmd.at(behaveWithCurve.behaveState)});
    }
}

FullInfoBlock CDevice::ReadFullInformation(std::size_t aTag) const
{
    return {aTag, ReadInfo(), ReadBoosterState(), ReadBehaveState(), {}};
}

AddressedValueAnyList CDevice::GetCmdTempRPM() const
{
    //CPU_GPU_TEMP_ADDRESS, CPU_GPU_RPM_ADDRESS in python sample code.
    static const std::array<AddressedValueAny, 2> cpuGetters =
    {
        AddressedValue1B{0x68, 0}, //temperature
        AddressedValue2B{0xC8, 0}, //rpm
    };

    static const std::array<AddressedValueAny, 2> gpuGetters =
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

void CpuGpuFanCurve::Validate() const
{
    static const auto validateCurve = [](const AddressedValueAnyList& src)
    {
        if (src.size() < 2)
        {
            throw std::invalid_argument("Curve must contain at least 2 points.");
        }

        (void)std::adjacent_find(src.begin(), src.end(), [](const auto& v1, const auto& v2)
        {
            if (!std::holds_alternative<AddressedValue1B>(v1)
                    || !std::holds_alternative<AddressedValue1B>(v2))
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
    static const auto validateAddresses = [](const AddressedValueAnyList& src,
                                             const AddressedValueAllowedAddresses& example)
    {
        for (const auto& value : src)
        {
            const auto& vb = std::get<AddressedValue1B>(value);
            if (example.count(vb.address) == 0)
            {
                throw std::invalid_argument("CpuGpuFanCurve contains unknown address. Rejected for security reasons.");
            }
        }
    };

    validateCurve(cpu);
    validateCurve(gpu);

    //If any other address is present than we generate ourself - raise for security reasons.
    static const auto kAddressesChecker = BuildAllowedAdressesFromDefaults();

    validateAddresses(cpu, kAddressesChecker.cpu);
    validateAddresses(gpu, kAddressesChecker.gpu);
}
