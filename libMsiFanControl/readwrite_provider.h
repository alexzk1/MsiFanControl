#pragma once

#include <cstdint>
#include <fstream>
#include <memory>
#include <set>

#include "cm_ctors.h"
//! @brief provides IO access to the system data which should be changed.
class IReadWriteProvider
{
public:
    DEFAULT_COPYMOVE(IReadWriteProvider);
    IReadWriteProvider() = default;
    virtual ~IReadWriteProvider() = default;

    [[nodiscard]]
    virtual std::ofstream WriteStream() const = 0;

    [[nodiscard]]
    virtual std::ifstream ReadStream() const = 0;
};

using ReadWriteProviderPtr = std::shared_ptr<IReadWriteProvider>;

//! @brief Caller supplies list of the offsets into WriteStream which were changed.
//! Provider implementation must restore original system values at those offets.
class IBackupProvider
{
public:
    DEFAULT_COPYMOVE(IBackupProvider);
    IBackupProvider() = default;
    virtual ~IBackupProvider() = default;

    virtual void RestoreOffsets(const std::set<std::int64_t>&
                                offsetsToRestoreFromBackup)const = 0;
};

using BackupProviderPtr = std::shared_ptr<IBackupProvider>;
