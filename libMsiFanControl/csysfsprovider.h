#pragma once

#include "readwrite.h"          // IWYU pragma: keep
#include "readwrite_provider.h" // IWYU pragma: keep

#include <filesystem>
#include <memory>

//! @note It requires acpi/irq working to access BIOS.
//! @brief Creates CReadWrite abstraction to work on sysfs OR /tmp/ file (dry-run).
class CSysFsProvider
{
  public:
    //! @param dryRun if true, then it will generated temp file with temp name, initially containing
    //! 256 zeroes.
    static CReadWrite CreateIoObject(BackupProviderPtr backupProvider, bool dryRun = true);

    //! @brief this gives direct access to modified files. It should not be used without real
    //! reason. use CReadWrite by CreateIoObject() instead.
    static std::shared_ptr<IReadWriteProvider> CreateIoDirect(bool dryRun);
};

/// @brief Expects that file contains single line with 0 or 1 as text string.
bool ReadFsBool(const std::filesystem::path &file);
void WriteFsBool(const std::filesystem::path &file, bool value);

// NOLINTNEXTLINE
inline static const std::filesystem::path
  kIntelPStateNoTurbo("/sys/devices/system/cpu/intel_pstate/no_turbo");
