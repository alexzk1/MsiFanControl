#pragma once

#include <cstdint>
#include <cassert>
#include <unordered_map>
#include <variant>
#include "readwrite.h"
#include "device_commands.h"

enum class BoosterState : uint8_t {ON, OFF};
enum class BehaveState  : uint8_t {AUTO, ADVANCED};

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

//! @brief this is combined information of all readers to use with IPC.
struct FullInfoBlock
{
    CpuGpuInfo info;
    BoosterState boosterState;
    BehaveState behaveState;

    //support for Cereal
    template <class Archive>
    void serialize( Archive & ar )
    {
        ar(info, boosterState, behaveState);
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

    FullInfoBlock ReadFullInformation() const;
protected:
    using BoosterStates = AddressedValueStates<BoosterState>;
    using BehaveStates  = AddressedValueStates<BehaveState>;

    //Override methods below to provide access for different devices.
    //returns temp getter, then RPM getter for CPU, then for GPU.
    virtual AddressedValueAnyList GetCmdTempRPM() const;

    virtual BoosterStates GetCmdBoosterStates() const = 0;
    virtual BehaveStates  GetCmdBehaveStates() const = 0;

    void         SetBooster(CReadWrite::WriteHandle& handle, const BoosterState what) const;
private:
    CReadWrite readWriteAccess;
};
