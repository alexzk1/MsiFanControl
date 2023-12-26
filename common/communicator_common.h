#pragma once

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>

#include <cstddef>
#include <streambuf>

inline const char*  GetMemoryName()
{
    static const char* const ptr = "MSICoolersSharedControlMem9";
    return ptr;
}

struct MemBuf : std::streambuf
{
    MemBuf(char* base, std::size_t size)
    {
        this->setp(base, base + size);
        this->setg(base, base, base + size);
    }
    std::size_t written() const
    {
        return this->pptr() - this->pbase();
    }
    std::size_t read() const
    {
        return this->gptr() - this->eback();
    }
};

inline constexpr std::size_t kWholeSharedMemSize = 4096;

class SharedMemoryWithMutex
{
public:
    SharedMemoryWithMutex() = delete;
    SharedMemoryWithMutex(boost::interprocess::shared_memory_object&& shm)
        :shm(std::move(shm)),
         region(this->shm, boost::interprocess::read_write)
    {
        void *addr       = region.get_address();
        mutex = new (addr) boost::interprocess::interprocess_mutex;

        constexpr std::size_t a = 64;
        constexpr auto x = sizeof(boost::interprocess::interprocess_mutex);

        const auto r =  x % a;
        offset = r ? x + (a - r) : x;
    }

    boost::interprocess::interprocess_mutex& Mutex() const
    {
        return *mutex;
    }

    MemBuf Daemon2UI() const
    {
        return MemBuf(Ptr(), Size());
    }

    MemBuf UI2Daemon() const
    {
        const auto sz = Size();
        return MemBuf(Ptr() + sz, sz);
    }
private:

    char * Ptr() const
    {
        return static_cast<char*>(region.get_address()) + offset;
    }

    std::size_t Size() const
    {
        std::size_t sz = (region.get_size() - offset)  / 2;
        if (sz % 2)
        {
            --sz;
        }
        return sz;
    }

    boost::interprocess::shared_memory_object shm;
    boost::interprocess::mapped_region region;

    boost::interprocess::interprocess_mutex *mutex;
    std::size_t offset;
};
