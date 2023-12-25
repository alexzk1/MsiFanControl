#pragma once

#include <fstream>
#include <memory>

class IReadWriteProvider
{
public:
    virtual std::ofstream WriteStream() const = 0;
    virtual std::ifstream ReadStream() const = 0;

    virtual ~IReadWriteProvider() = default;
};

using ReadWriteProviderPtr = std::shared_ptr<IReadWriteProvider>;
