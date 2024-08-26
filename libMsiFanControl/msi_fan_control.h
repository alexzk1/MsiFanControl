#pragma once

#include <memory>
#include "device.h"

using DevicePtr = std::shared_ptr<CDevice>;

//! @brief main entry point to the library. Call this function to acquire controller.
//! @throws if cpu is not recognized, wrong vendor, missing access to debugfs (no root) etc.
//! @param dryRun if true, then temporary file will be created with 256 bytes size and filled by
//!        zeroes to operate on it instead access to debugfs.
//!
DevicePtr CreateDeviceController(BackupProviderPtr backuPovider,
                                 bool dryRun = false);
