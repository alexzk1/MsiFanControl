#pragma once

#include <cereal/cereal.hpp>

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <chrono>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

#include "readwrite.h"
#include "device_commands.h"
#include "cm_ctors.h"

//Rosetta stone: https://github.com/YoyPa/isw/blob/master/wiki/msi%20ec.png
// More information:
// https://github.com/dmitry-s93/MControlCenter/blob/main/src/operate.cpp

enum class BoosterState : std::uint8_t {ON, OFF, NO_CHANGE};
enum class BehaveState : std::uint8_t {AUTO, ADVANCED, NO_CHANGE};

using namespace std::chrono_literals;
constexpr inline auto kMinimumServiceDelay = 500ms;

struct Info
{
    //NOLINTNEXTLINE
    std::uint16_t temperature{0};
    //NOLINTNEXTLINE
    std::uint16_t fanRPM{0};

    Info() = default;
    //NOLINTNEXTLINE
    Info(const AddressedValueAny& temp, const AddressedValueAny& rpm);

    //Those statics can be re-used from GUI.
    static std::uint16_t parseTemp(const AddressedValueAny& temp)
    {
        assert(std::holds_alternative<AddressedValue1B>(temp)
               && "We expect 1 byte request for the temperature.");
        return std::visit([](const auto& val) -> std::uint16_t
        {
            return val.value;
        }, temp);
    }

    static std::uint16_t parseRPM(const AddressedValueAny& rpm)
    {
        assert(std::holds_alternative<AddressedValue2B>(rpm)
               && "We expect 2 bytes request for the rpm.");
        return std::visit([](const auto& val) -> std::uint16_t
        {
            if (val.value)
            {
                return 478000.0 / val.value;
            }
            return 0u;
        }, rpm);
    }

    //support for Cereal
    template <class Archive>
    void serialize(Archive& ar, const std::uint32_t /*version*/)
    {
        ar(temperature, fanRPM);
    }
};
CEREAL_CLASS_VERSION(Info, 1)

struct CpuGpuInfo
{
    Info cpu;
    Info gpu;

    //support for Cereal
    template <class Archive>
    void serialize(Archive& ar, const std::uint32_t /*version*/)
    {
        ar(cpu, gpu);
    }
};
CEREAL_CLASS_VERSION(CpuGpuInfo, 1)

//Lists must contain 1 byte values only.
struct CpuGpuFanCurve
{
    //NOLINTNEXTLINE
    AddressedValueAnyList cpu{};
    //NOLINTNEXTLINE
    AddressedValueAnyList gpu{};

    //Daemon only
    void Validate() const;

    bool operator==(const CpuGpuFanCurve& another) const
    {
        return std::tie(cpu, gpu) == std::tie(another.cpu, another.gpu);
    }

    bool operator!=(const CpuGpuFanCurve& another) const
    {
        return !(*this == another);
    }

    static CpuGpuFanCurve MakeDefault()
    {
        //Make default fan curve. Address depends on device, and in 99% it will remain the same.
        //Curve itself, looks like we have 7 speed steps, each step (index into vector)
        //is activated at given temperature.

        //Daemon uses those as example to check incoming addresses from the GUI.
        //Only mentioned here are allowed for security reasons.

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

    //support for Cereal
    template <class Archive>
    void serialize(Archive& ar, const std::uint32_t /*version*/)
    {
        ar(cpu, gpu);
    }
};
CEREAL_CLASS_VERSION(CpuGpuFanCurve, 1)

struct BehaveWithCurve
{
    //NOLINTNEXTLINE
    BehaveState behaveState;
    //NOLINTNEXTLINE
    CpuGpuFanCurve curve;

    BehaveWithCurve() :
        behaveState{BehaveState::NO_CHANGE},
        curve{CpuGpuFanCurve::MakeDefault()}
    {
    }

    BehaveWithCurve(BehaveState behaveState, CpuGpuFanCurve curve) :
        behaveState{behaveState},
        curve{std::move(curve)}
    {
    }

    bool operator==(const BehaveWithCurve& another) const
    {
        return std::tie(behaveState, curve) == std::tie(another.behaveState, another.curve);
    }

    bool operator!=(const BehaveWithCurve& another) const
    {
        return !(*this == another);
    }

    //support for Cereal
    template <class Archive>
    void serialize(Archive& ar, const std::uint32_t /*version*/)
    {
        ar(behaveState, curve);
    }

    static BehaveWithCurve EmptyForGUI()
    {
        BehaveWithCurve res;
        res.curve.cpu.clear();
        res.curve.gpu.clear();
        return res;
    }
};
CEREAL_CLASS_VERSION(BehaveWithCurve, 1)

//! @brief this is combined information from daemon to UI.
struct FullInfoBlock
{
    static inline constexpr std::size_t signature = 0xABBACDDCDEFEEF01u;

    //tag is strictly incremented by daemon, used by GUI to detect disconnect or so.
    std::size_t tag{0};
    CpuGpuInfo info;
    BoosterState boosterState;
    BehaveWithCurve behaveAndCurve;

    std::string daemonDeviceException;

    //support for Cereal
    template <class Archive>
    void save(Archive& ar, const std::uint32_t /*version*/) const
    {
        ar(signature, tag, info, boosterState, behaveAndCurve, daemonDeviceException);
    }

    template <class Archive>
    void load(Archive& ar, const std::uint32_t /*version*/)
    {
        std::size_t signatureRead = 0u;
        ar(signatureRead, tag, info, boosterState, behaveAndCurve, daemonDeviceException);
        if (signatureRead != signature)
        {
            throw std::runtime_error("Wrong signature detected on reading FullInfoBlock.");
        }
    }
};
CEREAL_CLASS_VERSION(FullInfoBlock, 1)

struct RequestFromUi
{
    //PING_DAEMON - daemon just updates tag field, other data are last read values.
    //READ_FRESH_DATA - daemon does actual read of the data like current temperatures and updates last read data.
    //WRITE_DATA - daemon writes data present in this request and updates last read data by following reading.
    enum class RequestType : std::uint8_t {PING_DAEMON, READ_FRESH_DATA, WRITE_DATA};

    RequestType request;
    BoosterState boosterState{BoosterState::NO_CHANGE};
    BehaveWithCurve behaveAndCurve{};

    //support for Cereal
    template <class Archive>
    void serialize(Archive& ar, const std::uint32_t /*version*/)
    {
        ar(boosterState, behaveAndCurve, request);
    }
};
CEREAL_CLASS_VERSION(RequestFromUi, 1)

class CDevice
{
public:
    CDevice() = delete;
    NO_COPYMOVE(CDevice);

    explicit CDevice(CReadWrite readWrite);
    virtual ~CDevice();

    CpuGpuInfo ReadInfo() const;

    BoosterState ReadBoosterState() const;
    void SetBooster(const BoosterState what) const;

    BehaveWithCurve ReadBehaveState() const;
    void SetBehaveState(const BehaveWithCurve& behaveWithCurve) const;

    FullInfoBlock ReadFullInformation(std::size_t aTag) const;
protected:
    using BoosterStates = AddressedValueStates<BoosterState>;
    using BehaveStates = AddressedValueStates<BehaveState>;

    //Override methods below to provide access for different devices.
    //returns temp getter, then RPM getter for CPU, then for GPU.
    virtual AddressedValueAnyList GetCmdTempRPM() const;

    virtual BoosterStates GetCmdBoosterStates() const;
    virtual BehaveStates GetCmdBehaveStates() const = 0;

    void SetBooster(CReadWrite::WriteHandle& handle,
                    const BoosterState what) const;
private:
    CReadWrite readWriteAccess;
};
