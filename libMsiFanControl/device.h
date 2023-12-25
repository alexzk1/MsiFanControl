#pragma once

#include <cstdint>
#include <cassert>
#include <unordered_map>
#include <variant>
#include "readwrite.h"
#include "device_commands.h"

struct Info
{
    std::int16_t temperature{0};
    std::int16_t fanRPM{0};
    Info() = delete;
    Info(const AddressedValueAny& temp, const AddressedValueAny& rpm);
};

struct CpuGpuInfo
{
    Info cpu;
    Info gpu;
};

//Lists must contain 1 byte values only.
struct CpuGpuFanCurve
{
    AddressedValueAnyList cpu;
    AddressedValueAnyList gpu;

    static CpuGpuFanCurve MakeDefault();

    void Validate() const;
};

class CDevice
{
public:
    enum class BoosterState {ON, OFF};
    enum class BehaveState  {AUTO, ADVANCED};

    CDevice() = delete;
    explicit CDevice(CReadWrite readWrite);
    virtual ~CDevice();

    CpuGpuInfo   ReadInfo() const;

    BoosterState GetBoosterState() const;
    void         SetBooster(const BoosterState what) const;

    BehaveState  ReadBehaveState() const;
    void         SetBehaveState(const BehaveState what,
                                CpuGpuFanCurve fanCurve = CpuGpuFanCurve::MakeDefault()) const;
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
