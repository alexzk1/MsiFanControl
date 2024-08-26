#pragma once

#include <cstdint>
#include <fstream>
#include <memory>
#include <set>

//! @brief provides IO access to the system data which should be changed.
class IReadWriteProvider
{
public:
    virtual std::ofstream WriteStream() const = 0;
    virtual std::ifstream ReadStream() const = 0;

    virtual ~IReadWriteProvider() = default;
};

using ReadWriteProviderPtr = std::shared_ptr<IReadWriteProvider>;

//! @brief Caller supplies list of the offsets into WriteStream which were changed.
//! Provider implementation must restore original system values at those offets.
class IBackupProvider
{
public:
    virtual void RestoreOffsets(std::set<std::int64_t> offsetsToRestoreFromBackup) const
        = 0;
    virtual ~IBackupProvider() = default;
};

using BackupProviderPtr = std::shared_ptr<IBackupProvider>;
