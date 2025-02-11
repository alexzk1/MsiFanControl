#pragma once

#include "cm_ctors.h"
#include "device.h"
#include "readwrite.h"

class CIntelBeforeGen10 : public CDevice
{
  public:
    explicit CIntelBeforeGen10(CReadWrite readWrite);
    NO_COPYMOVE(CIntelBeforeGen10);
    ~CIntelBeforeGen10() override;

  protected:
    BehaveStates GetCmdBehaveStates() const override;
};
