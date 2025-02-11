#pragma once

#include "cm_ctors.h"
#include "device.h"
#include "readwrite.h"

class CIntelGen10 : public CDevice
{
  public:
    explicit CIntelGen10(CReadWrite readWrite);
    NO_COPYMOVE(CIntelGen10);
    ~CIntelGen10() override;

  protected:
    BehaveStates GetCmdBehaveStates() const override;
};
