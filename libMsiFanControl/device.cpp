#include "device.h" // IWYU pragma: keep

#include "command_detector.h"
#include "csysfsprovider.h"
#include "device_commands.h" // IWYU pragma: keep
#include "readwrite.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

// NOLINTNEXTLINE
bool GLOBAL_DRY_RUN = false;

// Rosetta stone: https://github.com/YoyPa/isw/blob/master/wiki/msi%20ec.png
//  More information:
//  https://github.com/dmitry-s93/MControlCenter/blob/main/src/operate.cpp
// https://github.com/BeardOverflow/msi-ec/blob/main/msi-ec.c#L2613

namespace {
// Cannot use assert! Because it does not call destructors and shared memory remains allocated in
// daemon.
#ifndef NDEBUG
void Throw(bool cond, const std::string &text)
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
    static const auto makeSingle = [](const AddressedValueAnyList &src) {
        AddressedValueAllowedAddresses res;
        std::transform(src.begin(), src.end(), std::inserter(res, res.end()),
                       [](const auto &value) {
                           const auto &vb = std::get<AddressedValue1B>(value);
                           return vb.address;
                       });
        return res;
    };
    const auto curves = CpuGpuFanCurve::MakeDefault();
    return {makeSingle(curves.cpu), makeSingle(curves.gpu)};
}
} // namespace

// NOLINTNEXTLINE
Info::Info(const AddressedValueAny &temp, const AddressedValueAny &rpm) :
    temperature(parseTemp(temp)),
    fanRPM(parseRPM(rpm))
{
}

CDevice::CDevice(CReadWrite readWrite) :
    readWriteAccess(std::move(readWrite))
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

BoostersStates CDevice::ReadBoostersStates() const
{
    auto cmd = GetCmdBoosterStates();
    const auto clone = cmd;
    readWriteAccess.Read(cmd);

    const auto diff = cmd.GetOneDifference(clone);
    Throw(diff != std::nullopt,
          "Something went wrong. Read should indicate BOOSTER's changed state.");

    const bool isTurboEnabled = !ReadFsBool(kIntelPStateNoTurbo);

    // We read OFF state different, that means there is ON state in device.
    return {!diff || diff->first == BoosterState::OFF ? BoosterState::ON : BoosterState::OFF,
            isTurboEnabled ? CpuTurboBoostState::ON : CpuTurboBoostState::OFF};
}

void CDevice::SetBoosters(const BoostersStates what) const
{
    auto handle = readWriteAccess.StartWritting();
    auto cmd = GetCmdBoosterStates();
    readWriteAccess.Write(handle, {cmd.at(what.fanBoosterState)});

    switch (what.cpuTurboBoostState)
    {
        case CpuTurboBoostState::OFF:
            WriteFsBool(kIntelPStateNoTurbo, true);
            break;
        case CpuTurboBoostState::ON:
            WriteFsBool(kIntelPStateNoTurbo, false);
            break;
        case CpuTurboBoostState::NO_CHANGE:
            break;
    }
}

BehaveWithCurve CDevice::ReadBehaveState() const
{
    auto cmd = GetCmdBehaveStates();
    const auto clone = cmd;
    readWriteAccess.Read(cmd);

    const auto diff = cmd.GetOneDifference(clone);
    Throw(diff != std::nullopt,
          "Something went wrong. Read should indicate BEHAVE's changed state.");

    // Same logic as in booster, if "auto" is different, then "advanced" is set there.
    BehaveWithCurve res;
    res.behaveState =
      diff && diff->first == BehaveState::AUTO ? BehaveState::ADVANCED : BehaveState::AUTO;

    readWriteAccess.Read(res.curve.cpu);
    readWriteAccess.Read(res.curve.gpu);
    return res;
}

void CDevice::SetBehaveState(const BehaveWithCurve &behaveWithCurve) const
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

Battery CDevice::ReadBattery() const
{
    AddressedValue1B cmd = GetBatteryThreshold();
    readWriteAccess.ReadOne(cmd);
    return Battery{cmd};
}
/*
    if (streq(buf, "max"))
        result = ec_write(conf.charge_control.address,
                  conf.charge_control.range_max);

    else if (streq(buf, "medium")) // up to 80%
        result = ec_write(conf.charge_control.address,
                  conf.charge_control.offset_end + 80);

    else if (streq(buf, "min")) // up to 60%
        result = ec_write(conf.charge_control.address,
                  conf.charge_control.offset_end + 60);
*/
void CDevice::SetBattery(const Battery &battery) const
{
    static const std::map<BatteryLevels, std::uint8_t> enum2value = {
      {BatteryLevels::BestForMobility, 0xE4},
      {BatteryLevels::Balanced, 0x80 + 80},
      {BatteryLevels::BestForBattery, 0x80 + 60},
    };
    if (battery.maxLevel != BatteryLevels::NotKnown)
    {
        auto cmd = GetBatteryThreshold();
        cmd.value = enum2value.at(battery.maxLevel);
        auto handle = readWriteAccess.StartWritting();
        readWriteAccess.Write(handle, {cmd});
    }
}

FullInfoBlock CDevice::ReadFullInformation(std::size_t aTag) const
{
    return {aTag,          ReadInfo(),   ReadBoostersStates(), ReadBehaveState(),
            std::string{}, ReadBattery()};
}

AddressedValueAnyList CDevice::GetCmdTempRPM() const
{
    static ProperCommandDetector cpuRpmDetector({
      AddressedValue2B{0xC8, 0},
      AddressedValue2B{0xCC, 0},
    });

    cpuRpmDetector.DetectProperAtOnce([this](auto &commandsList) {
        Throw(commandsList.size() == 2, "Something went wrong. cpuRpmDetector.size() == 2.");
        // Order above is important here for the check
        AddressedValueAnyList clone = commandsList;
        readWriteAccess.Read(clone);
        auto toErase = std::prev(commandsList.end());

        if (std::get<AddressedValue2B>(clone.at(1)).value > 0)
        {
            toErase = commandsList.begin();
        }
        else
        {
            // Sample code I took it from checks 1 byte though ... should I (& 0xFF) here ?
            const auto c9 = std::get<AddressedValue2B>(clone.at(0)).value;
            if (c9 > 0 && c9 < 50)
            {
                toErase = commandsList.begin();
            }
        }
        commandsList.erase(toErase);
    });

    // CPU_GPU_TEMP_ADDRESS, CPU_GPU_RPM_ADDRESS in python sample code.
    return {
      // cpu: temperature, rpm
      AddressedValue1B{0x68, 0},
      cpuRpmDetector,
      // gpu: temperature, rpm
      AddressedValue1B{0x80, 0},
      AddressedValue2B{0xCA, 0},
    };
}

CDevice::BoosterStates CDevice::GetCmdBoosterStates() const
{
    // COOLER_BOOSTER_OFF_ON_VALUES in python example code.
    static const AddressedBits kBoosterOff{0x98, 0x80, 0};
    static const AddressedBits kBoosterOn{0x98, 0x80, 0x80};

    static const BoosterStates booster = {// std::map
                                          {
                                            {BoosterState::OFF, kBoosterOff},
                                            {BoosterState::ON, kBoosterOn},
                                            {BoosterState::NO_CHANGE, TagIgnore{}},
                                          }};

    return booster;
}

AddressedValue1B CDevice::GetBatteryThreshold() const
{
    static ProperCommandDetector addressDetector({
      AddressedValue1B{0xEF, 0},
      AddressedValue1B{0xD7, 0},
    });

    // Trying to detect 1 of the addresses.
    addressDetector.DetectProperAtOnce([this](auto &commandsList) {
        Throw(commandsList.size() == 2, "Something went wrong. cpuRpmDetector.size() == 2.");
        // Order above is important here for the check
        AddressedValueAnyList clone = commandsList;
        readWriteAccess.Read(clone);

        const auto &vEF = std::get<AddressedValue1B>(clone.at(0)).value;
        const auto &vD7 = std::get<AddressedValue1B>(clone.at(1)).value;
        if (vEF < 0x80 || vEF > 0xE4)
        {
            commandsList.erase(commandsList.begin());
        }
        if (vD7 < 0x80 || vD7 > 0xE4)
        {
            commandsList.erase(std::prev(commandsList.end()));
        }

        // FIXME: We may have excepton on lambda exit if both addresses failed the check.
        // And everything will not work than.
        if (commandsList.size() == 1)
        {
            readWriteAccess.CancelBackupOn(commandsList.front());
        }
    });
    return std::get<AddressedValue1B>(addressDetector.get());
}

void CpuGpuFanCurve::Validate() const
{
    static const auto validateCurve = [](const AddressedValueAnyList &src) {
        if (src.size() < 2)
        {
            throw std::invalid_argument("Curve must contain at least 2 points.");
        }

        (void)std::adjacent_find(src.begin(), src.end(), [](const auto &v1, const auto &v2) {
            if (!std::holds_alternative<AddressedValue1B>(v1)
                || !std::holds_alternative<AddressedValue1B>(v2))
            {
                throw std::invalid_argument("Curve must contain 1 byte values only!!!");
            }
            const auto &vb1 = std::get<AddressedValue1B>(v1);
            const auto &vb2 = std::get<AddressedValue1B>(v2);

            const bool valid = vb1.address < vb2.address && vb1.value <= vb2.value;
            if (!valid)
            {
                throw std::runtime_error(
                  "Invalid fan's curve detected. It must increase or remain the same.");
            }
            return false;
        });
    };
    static const auto validateAddresses = [](const AddressedValueAnyList &src,
                                             const AddressedValueAllowedAddresses &example) {
        for (const auto &value : src)
        {
            const auto &vb = std::get<AddressedValue1B>(value);
            if (example.count(vb.address) == 0)
            {
                throw std::invalid_argument(
                  "CpuGpuFanCurve contains unknown address. Rejected for security reasons.");
            }
        }
    };

    validateCurve(cpu);
    validateCurve(gpu);

    // If any other address is present than we generate ourself - raise for security reasons.
    static const auto kAddressesChecker = BuildAllowedAdressesFromDefaults();

    validateAddresses(cpu, kAddressesChecker.cpu);
    validateAddresses(gpu, kAddressesChecker.gpu);
}
