#pragma once

#include "readwrite.h"
#include "readwrite_provider.h"

#include <memory>

//! @note It requires acpi/irq working to access BIOS.
class CSysFsProvider
{
public:
    //! @param dryRun if true, then it will generated temp file with temp name, initially containing 256 zeroes.
    static CReadWrite CreateIoObject(BackupProviderPtr backuPovider, bool dryRun = true);

    //! @brief this gives direct access to modified files. It should not be used without real reason.
    //! use CReadWrite by CreateIoObject() instead.
    static std::shared_ptr<IReadWriteProvider> CreateIoDirect(bool dryRun);
};
