#pragma once

#include "cm_ctors.h"
#include "device_commands.h"
#include "messages_types.h"
#include "readwrite.h"

/// @brief This represents physical device we're on. Like whole laptop, with fans, CPU, GPU etc.
/// This is supposed to be used from the daemon with root access.
class CDevice
{
  public:
    CDevice() = delete;
    NO_COPYMOVE(CDevice);

    explicit CDevice(CReadWrite readWrite);
    virtual ~CDevice();

    CpuGpuInfo ReadInfo() const;

    BoostersStates ReadBoostersStates() const;
    void SetBoosters(const BoostersStates what) const;

    BehaveWithCurve ReadBehaveState() const;
    void SetBehaveState(const BehaveWithCurve &behaveWithCurve) const;

    Battery ReadBattery() const;
    void SetBattery(const Battery &battery) const;

    FullInfoBlock ReadFullInformation(std::size_t aTag) const;

  protected:
    using BoosterStates = AddressedValueStates<BoosterState>;
    using BehaveStates = AddressedValueStates<BehaveState>;

    // Override methods below to provide access for different devices.
    // returns temp getter, then RPM getter for CPU, then for GPU.
    virtual AddressedValueAnyList GetCmdTempRPM() const;

    virtual BoosterStates GetCmdBoosterStates() const;
    virtual BehaveStates GetCmdBehaveStates() const = 0;

    /// @brief Tries to detect valid offset to read/write battery command to BIOS. It is different
    /// on different models.
    /// @returns Command for r/w. It may be wrong detected which can cause exceptions later
    /// ("nothing works").
    virtual AddressedValue1B GetBatteryThreshold() const;

  private:
    CReadWrite readWriteAccess;
};
