#pragma once

#include "device.h"

class CIntelBeforeGen10 : public CDevice
{
public:
    explicit CIntelBeforeGen10(CReadWrite readWrite);
    ~CIntelBeforeGen10();

protected:
    BoosterStates GetCmdBoosterStates() const;
    BehaveStates GetCmdBehaveStates() const;
};
