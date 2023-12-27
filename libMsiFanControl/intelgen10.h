#pragma once

#include "device.h"

class CIntelGen10 : public CDevice
{
public:
    explicit CIntelGen10(CReadWrite readWrite);
    ~CIntelGen10();

protected:
    BehaveStates GetCmdBehaveStates() const;
};
