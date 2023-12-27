#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <ostream>
#include <type_traits>
#include <array>
#include <variant>
#include <iostream>
#include <vector>
#include <type_traits>
#include "readwrite_provider.h"
#include "device_commands.h"

class CReadWrite
{
public:
    class WriteHandle
    {
    public:
        ~WriteHandle() = default;
        WriteHandle() = delete;
    private:
        friend class CReadWrite;
        explicit WriteHandle(std::ofstream&& stream):stream(std::move(stream))
        {}
        std::ofstream stream;
    };
public:
    CReadWrite(ReadWriteProviderPtr ioProvider, BackupProviderPtr backupProvider)
        : ioProvider(std::move(ioProvider)),
          backupProvider(std::move(backupProvider))
    {
    }
    CReadWrite() = delete;

    ~CReadWrite()
    {
        if (backupProvider)
        {
            try
            {
                backupProvider->RestoreOffsets(std::move(backupOffsets));
            }
            catch(std::exception& ex)
            {
                std::cerr << "Exception on calling backup from ~CReadWrite(): " << ex.what()
                          << std::endl << std::flush;
            }
        }
    }

    //!@brief starts writting. Writting ends when returned WriteHandle is going out of scope.
    WriteHandle StartWritting() const
    {
        return WriteHandle(ioProvider->WriteStream());
    }

    //! @brief writes multiply commands to the handle.
    //! @param handle - result of StartWritting()
    //! @param toWrite - values to write.
    void Write(WriteHandle& handle, const AddressedValueAnyList& toWrite) const
    {
        for (const auto& value : toWrite)
        {
            std::visit([&handle, this](const auto& element)
            {
                using stored_type = typename std::decay<decltype(element)>::type::value_type;
                Write<stored_type>(handle.stream, element.address, element.value);
            }, value);
        }
    }

    //! @brief optimized version which opens file once for many elements to read.
    //! It uses address field to seek and fills value field of the toFill object.
    template <typename Container>
    void Read(Container& toFill) const
    {
        auto stream = ioProvider->ReadStream();
        for (auto& containedValue : toFill)
        {
            std::visit([&stream](auto& element)
            {

                using stored_type = typename std::decay<decltype(element)>::type::value_type;
                element.value = Read<stored_type>(stream, element.address);
            }, GetCommandFromContained(containedValue));
        }
    }

private:
    ReadWriteProviderPtr           ioProvider;
    BackupProviderPtr              backupProvider;
    mutable std::set<std::int64_t> backupOffsets;

    static AddressedValueAny& GetCommandFromContained(AddressedValueAnyList::value_type& elem)
    {
        return elem;
    }

    template<typename TheState>
    static AddressedValueAny& GetCommandFromContained(std::pair<TheState, AddressedValueAny> &elem)
    {
        return elem.second;
    }

    template <typename T>
    static T Read(std::ifstream& readStream, const std::streampos offset)
    {
        //using intermedial array to deal with optimization: -fstrict-aliasing
        static_assert(std::is_scalar_v<T>, "Only scalars are allowed.");
        std::array<char, sizeof(T)> tmp;

        readStream.seekg(offset);
        readStream.read(tmp.data(), tmp.size());

        //Guess i should understand this python code as big endian:
        // VALUE = int(file.read(2).hex(),16)
        T result;
        std::copy(tmp.rbegin(), tmp.rend(), &result);
        return result;
    }

    template <typename T>
    void Write(std::ofstream& writeStream, const std::streampos offset, const T& value) const
    {
        //installing backup
        {
            std::int64_t off = offset;
            for (std::size_t i = 0; i < sizeof(T); ++i)
            {
                backupOffsets.insert(off++);
            }
        }
        //using intermedial array to deal with optimization: -fstrict-aliasing

        static_assert(std::is_scalar_v<T>, "Only scalars are allowed.");
        std::array<char, sizeof(T)> tmp;

        //Guess this is BIG endian too:
        //file.write(bytes((VALUE,)))
        std::copy_n(&value, sizeof(value), tmp.rbegin());
        writeStream.seekp(offset);
        writeStream.write(tmp.data(), tmp.size());
    }
};
