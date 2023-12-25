#pragma once

#include "readwrite.h"

class CSysFsProvider
{
public:
    //! @param dryRun if true, then it will generated temp file with temp name, initially containing 256 zeroes.
    static CReadWrite CreateIoObject(bool dryRun = true);
};
