#pragma once

#include "cm_ctors.h"
#include "device.h"
#include "readwrite.h"

/// @brief It is used when laptop based on Gen10 is detected.
/// Different laptops may have different offsets in the BIOS "file" for the commands.
class CIntelGen10 : public CDevice
{
  public:
    explicit CIntelGen10(CReadWrite readWrite);
    NO_COPYMOVE(CIntelGen10);
    ~CIntelGen10() override;

  protected:
    BehaveStates GetCmdBehaveStates() const override;
};
