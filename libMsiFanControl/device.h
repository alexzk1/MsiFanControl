#pragma once

#include <cstdint>
#include <cassert>
#include <iterator>
#include <unordered_map>
#include <variant>
#include "readwrite.h"
#include "device_commands.h"

//Rosetta stone: https://github.com/YoyPa/isw/blob/master/wiki/msi%20ec.png

enum class BoosterState : uint8_t {ON, OFF, NO_CHANGE};
enum class BehaveState  : uint8_t {AUTO, ADVANCED, NO_CHANGE};

struct Info
{
    std::uint16_t temperature{0};
    std::uint16_t fanRPM{0};
    Info() = default;
    Info(const AddressedValueAny& temp, const AddressedValueAny& rpm);

    //support for Cereal
    template <class Archive>
    void serialize( Archive & ar )
    {
        ar(temperature, fanRPM);
    }
};

struct CpuGpuInfo
{
    Info cpu;
    Info gpu;

    //support for Cereal
    template <class Archive>
    void serialize( Archive & ar )
    {
        ar(cpu, gpu);
    }
};

//Lists must contain 1 byte values only.
struct CpuGpuFanCurve
{
    AddressedValueAnyList cpu;
    AddressedValueAnyList gpu;

    static CpuGpuFanCurve MakeDefault();

    void Validate() const;

    //support for Cereal
    template <class Archive>
    void serialize( Archive & ar )
    {
        ar(cpu, gpu);
    }
};

//! @brief this is combined information from daemon to UI.
struct FullInfoBlock
{
    //tag is strictly incremented by daemon, used by GUI to detect disconnect or so.
    std::size_t  tag{0};
    CpuGpuInfo   info;
    BoosterState boosterState;
    BehaveState  behaveState;
    std::string  daemonDeviceException;

    //support for Cereal
    template <class Archive>
    void serialize( Archive & ar )
    {
        ar(tag, info, boosterState, behaveState, daemonDeviceException);
    }
};

struct RequestFromUi
{
    BoosterState boosterState{BoosterState::NO_CHANGE};
    //support for Cereal
    template <class Archive>
    void serialize( Archive & ar )
    {
        ar(boosterState);
    }
};

class CDevice
{
public:
    CDevice() = delete;
    explicit CDevice(CReadWrite readWrite);
    virtual ~CDevice();

    CpuGpuInfo   ReadInfo() const;

    BoosterState ReadBoosterState() const;
    void         SetBooster(const BoosterState what) const;

    BehaveState  ReadBehaveState() const;
    void         SetBehaveState(const BehaveState what,
                                CpuGpuFanCurve fanCurve = CpuGpuFanCurve::MakeDefault()) const;

    FullInfoBlock ReadFullInformation(std::size_t aTag) const;
protected:
    using BoosterStates = AddressedValueStates<BoosterState>;
    using BehaveStates  = AddressedValueStates<BehaveState>;

    //Override methods below to provide access for different devices.
    //returns temp getter, then RPM getter for CPU, then for GPU.
    virtual AddressedValueAnyList GetCmdTempRPM() const;

    virtual BoosterStates GetCmdBoosterStates() const;
    virtual BehaveStates  GetCmdBehaveStates() const = 0;

    void         SetBooster(CReadWrite::WriteHandle& handle, const BoosterState what) const;
private:
    CReadWrite readWriteAccess;
};
