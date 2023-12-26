#pragma once

#include <memory>

namespace boost
{
    namespace interprocess
    {
        class shared_memory_object;
        class named_mutex;
    }
}
class CDevice;

class CSharedDevice
{
public:
    CSharedDevice();
    ~CSharedDevice();

    void Communicate();
private:
    std::shared_ptr<CDevice> device;

    std::shared_ptr<boost::interprocess::named_mutex> sharedMutex;
    std::shared_ptr<boost::interprocess::shared_memory_object> sharedMemory;
};
