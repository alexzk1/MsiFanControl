#pragma once

#include "cm_ctors.h"
#include "device_commands.h"
#include "readwrite.h"

#include <cereal/cereal.hpp>

#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

// Rosetta stone: https://github.com/YoyPa/isw/blob/master/wiki/msi%20ec.png
//  More information:
//  https://github.com/dmitry-s93/MControlCenter/blob/main/src/operate.cpp
// https://github.com/BeardOverflow/msi-ec/blob/main/msi-ec.c#L2613

/// @brief Current state of fan's boost, value is passed/read to/from system fs.
enum class BoosterState : std::uint8_t {
    ON,
    OFF,
    NO_CHANGE
};

/// @brief Behave state of the driver/BIOS. This value is passed/read to/from system fs.
enum class BehaveState : std::uint8_t {
    AUTO,
    ADVANCED,
    NO_CHANGE
};

/// @brief Current state of CPU turbo-boost feature.
enum class CpuTurboBoostState : std::uint8_t {
    ON,
    OFF,
    NO_CHANGE
};

/// @brief At least delay between 2 sequental communications sessions of the daemon / GUI (it is
/// poll-time of the daemon).
using namespace std::chrono_literals;
constexpr inline auto kMinimumServiceDelay = 500ms;

///@brief Contains temperature and RPM of the CPU or GPU.
struct Info
{
    // NOLINTNEXTLINE
    std::uint16_t temperature{0};
    // NOLINTNEXTLINE
    std::uint16_t fanRPM{0};

    Info() = default;
    // NOLINTNEXTLINE
    Info(const AddressedValueAny &temp, const AddressedValueAny &rpm);

    // Those statics can be re-used from GUI.
    static std::uint16_t parseTemp(const AddressedValueAny &temp)
    {
        assert(std::holds_alternative<AddressedValue1B>(temp)
               && "We expect 1 byte request for the temperature.");
        return std::visit(
          [](const auto &val) -> std::uint16_t {
              return val.value;
          },
          temp);
    }

    static std::uint16_t parseRPM(const AddressedValueAny &rpm)
    {
        assert(std::holds_alternative<AddressedValue2B>(rpm)
               && "We expect 2 bytes request for the rpm.");
        return std::visit(
          [](const auto &val) -> std::uint16_t {
              if (val.value)
              {
                  return 478000.0 / val.value;
              }
              return 0u;
          },
          rpm);
    }

    // support for Cereal
    template <class Archive>
    void serialize(Archive &ar, const std::uint32_t /*version*/)
    {
        ar(temperature, fanRPM);
    }
};
CEREAL_CLASS_VERSION(Info, 1)

/// @brief Contains both CPU and GPU struct Info.
struct CpuGpuInfo
{
    Info cpu;
    Info gpu;

    // support for Cereal
    template <class Archive>
    void serialize(Archive &ar, const std::uint32_t /*version*/)
    {
        ar(cpu, gpu);
    }
};
CEREAL_CLASS_VERSION(CpuGpuInfo, 1)

/// @brief Fan's curves for CPU/GPU.
/// @note TODO: Real use of it is not implemented in GUI, and not tested in daemon.
/// @note Lists must contain 1 byte values only.
struct CpuGpuFanCurve
{
    // NOLINTNEXTLINE
    AddressedValueAnyList cpu{};
    // NOLINTNEXTLINE
    AddressedValueAnyList gpu{};

    // Daemon only
    void Validate() const;

    bool operator==(const CpuGpuFanCurve &another) const
    {
        return std::tie(cpu, gpu) == std::tie(another.cpu, another.gpu);
    }

    bool operator!=(const CpuGpuFanCurve &another) const
    {
        return !(*this == another);
    }

    /// Make default fan curve. Address depends on device, and in 99% it will remain the same.
    /// Curve itself, looks like we have 7 speed steps, each step (index into vector)
    /// is activated at given temperature.
    ///
    /// Daemon uses those as example to check incoming addresses from the GUI.
    /// @note Only mentioned here are allowed for security reasons. If any other address will be
    /// given, except those, it will be ignored by the daemon.
    static CpuGpuFanCurve MakeDefault()
    {

        static const AddressedValueAnyList cpuCurve = {
          AddressedValue1B{0x72, 0},  AddressedValue1B{0x73, 40}, AddressedValue1B{0x74, 48},
          AddressedValue1B{0x75, 56}, AddressedValue1B{0x76, 64}, AddressedValue1B{0x77, 72},
          AddressedValue1B{0x78, 80},
        };

        static const AddressedValueAnyList gpuCurve = {
          AddressedValue1B{0x8A, 0},  AddressedValue1B{0x8B, 48}, AddressedValue1B{0x8C, 56},
          AddressedValue1B{0x8D, 64}, AddressedValue1B{0x8E, 72}, AddressedValue1B{0x8F, 79},
          AddressedValue1B{0x90, 86},
        };

        return {cpuCurve, gpuCurve};
    }

    // support for Cereal
    template <class Archive>
    void serialize(Archive &ar, const std::uint32_t /*version*/)
    {
        ar(cpu, gpu);
    }
};
CEREAL_CLASS_VERSION(CpuGpuFanCurve, 1)

struct BehaveWithCurve
{
    // NOLINTNEXTLINE
    BehaveState behaveState;
    // NOLINTNEXTLINE
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

    bool operator==(const BehaveWithCurve &another) const
    {
        return std::tie(behaveState, curve) == std::tie(another.behaveState, another.curve);
    }

    bool operator!=(const BehaveWithCurve &another) const
    {
        return !(*this == another);
    }

    // support for Cereal
    template <class Archive>
    void serialize(Archive &ar, const std::uint32_t /*version*/)
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

/// @brief Defines possibly baterry behaves.
enum class BatteryLevels : std::uint8_t {
    BestForBattery,
    Balanced,
    BestForMobility,
    NotKnown
};

/// @brief Battery behave and addressed command for the BIOS.
/// @note @var _debugRead is updated by the daemon, it is either read from BIOS and updates @var
/// maxLevel, or depend on @var maxLevel proper value is composed and sent to BIOS.
struct Battery
{
    BatteryLevels maxLevel{BatteryLevels::NotKnown};

    Battery() = default;
    DEFAULT_COPYMOVE(Battery);
    ~Battery() = default;
    explicit Battery(BatteryLevels level) :
        maxLevel(level)
    {
    }

    // support for Cereal
    template <class Archive>
    void serialize(Archive &ar, const std::uint32_t /*version*/)
    {
        ar(maxLevel, _debugRead);
    }

  private:
    friend class CDevice;

    explicit Battery(AddressedValue1B value) :
        _debugRead(value)
    {
        if (_debugRead.value >= 0x80 && _debugRead.value <= 0xE4)
        {
            if (_debugRead.value == 0x80 + 60)
            {
                maxLevel = BatteryLevels::BestForBattery;
            }
            if (_debugRead.value == 0x80 + 80)
            {
                maxLevel = BatteryLevels::Balanced;
            }
            if (_debugRead.value > 0x80 + 80)
            {
                maxLevel = BatteryLevels::BestForMobility;
            }
        }
    }
    AddressedValue1B _debugRead{};
};
CEREAL_CLASS_VERSION(Battery, 1)

struct RequestFromUi;
struct FullInfoBlock;

/// @brief Different boosters' states bound together. This result of what should be send to the
/// daemon.
struct BoostersStates
{
    BoosterState fanBoosterState{BoosterState::NO_CHANGE};
    CpuTurboBoostState cpuTurboBoostState{CpuTurboBoostState::NO_CHANGE};

    BoostersStates() = default;
    ~BoostersStates() = default;
    DEFAULT_COPYMOVE(BoostersStates);

    bool operator==(const BoostersStates &other) const noexcept
    {
        const auto tie = [](const auto &v) {
            return std::tie(v.fanBoosterState, v.cpuTurboBoostState);
        };
        return tie(*this) == tie(other);
    }

    bool operator!=(const BoostersStates &other) const noexcept
    {
        return !(*this == other);
    }

    /// @returns true if this object should be sent to daemon.
    [[nodiscard]]
    bool HasAnyChange() const noexcept
    {
        static const BoostersStates kDefault{};
        return *this != kDefault;
    }

    // support for Cereal
    template <class Archive>
    void serialize(Archive &ar, const std::uint32_t /*version*/)
    {
        ar(fanBoosterState, cpuTurboBoostState);
    }
};
CEREAL_CLASS_VERSION(BoostersStates, 3)

//! @brief this is combined information passed from daemon to UI.
struct FullInfoBlock
{
    static inline constexpr std::size_t signature = 0xABBACDDCDEFEEF01u;

    // tag is strictly incremented by daemon, used by GUI to detect disconnect or so.
    std::size_t tag{0};
    CpuGpuInfo info;
    BoostersStates boostersStates;
    BehaveWithCurve behaveAndCurve;
    std::string daemonDeviceException;
    Battery battery;

    // support for Cereal
    template <class Archive>
    void save(Archive &ar, const std::uint32_t version) const
    {
        if (version < 4)
        {
            throw std::runtime_error("Recompile. It is not compatible binary with older code.");
        }

        ar(signature, tag, info, boostersStates, behaveAndCurve, daemonDeviceException, battery);
        return;
    }

    template <class Archive>
    void load(Archive &ar, const std::uint32_t version)
    {
        if (version < 4)
        {
            throw std::runtime_error("Recompile. It is not compatible binary with older code.");
        }

        std::size_t signatureRead = 0u;
        ar(signatureRead, tag, info, boostersStates, behaveAndCurve, daemonDeviceException,
           battery);
        if (signatureRead != signature)
        {
            throw std::runtime_error("Wrong signature detected on reading FullInfoBlock.");
        }
    }
};
CEREAL_CLASS_VERSION(FullInfoBlock, 4)

/// @brief Request sent by GUI to daemon. It can be ping, action to execute, etc.
struct RequestFromUi
{
    /// PING_DAEMON - daemon just updates tag field, other data are last read values.
    /// READ_FRESH_DATA - daemon does actual read of the data like current temperatures and updates
    /// last read data.
    /// WRITE_DATA - daemon writes data present in this request and updates last
    /// read data by following reading.
    enum class RequestType : std::uint8_t {
        PING_DAEMON,
        READ_FRESH_DATA,
        WRITE_DATA
    };

    RequestType request;
    BoostersStates boostersStates{};
    BehaveWithCurve behaveAndCurve{};
    Battery battery{BatteryLevels::NotKnown};

    // support for Cereal
    template <class Archive>
    void serialize(Archive &ar, const std::uint32_t version)
    {
        if (version < 4)
        {
            throw std::runtime_error("Recompile. It is not compatible binary with older code.");
        }
        ar(boostersStates, behaveAndCurve, request, battery);
    }

    /// @returns true if this request from GUI to daemon contains some action requested by user (or
    /// "smart" algorithm) to execute by daemon.
    /// @note TODO: currently we support only simpliest things, like switch boost speed of the fans.
    bool HasUserAction() const
    {
        return boostersStates.HasAnyChange() || battery.maxLevel != BatteryLevels::NotKnown;
    }
};
CEREAL_CLASS_VERSION(RequestFromUi, 4)

/// @brief This represents physical device we're on. Like whole laptop, with fans, CPU, GPU etc.
class CDevice
{
  public:
    CDevice() = delete;
    NO_COPYMOVE(CDevice);

    explicit CDevice(CReadWrite readWrite);
    virtual ~CDevice();

    CpuGpuInfo ReadInfo() const;

    BoosterState ReadBoosterState() const;
    CpuTurboBoostState ReadCpuTurboBoostState() const;
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
