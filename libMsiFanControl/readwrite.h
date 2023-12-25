#pragma once

#include <fstream>
#include <type_traits>
#include <memory.h>
#include <array>
#include <variant>
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
    explicit CReadWrite(ReadWriteProviderPtr aProvider)
        : iProvider(std::move(aProvider))
    {
    }
    CReadWrite() = delete;

    //!@brief starts writting. Writting ends when returned WriteHandle is going out of scope.
    WriteHandle StartWritting() const
    {
        return WriteHandle(iProvider->WriteStream());
    }

    //! @brief writes multiply commands to the handle.
    //! @param handle - result of StartWritting()
    //! @param toWrite - values to write.
    void Write(WriteHandle& handle, const AddressedValueAnyList& toWrite) const
    {
        for (const auto& value : toWrite)
        {
            std::visit([&handle](const auto& element)
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
        auto stream = iProvider->ReadStream();
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
    ReadWriteProviderPtr iProvider;

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

        T result;
        memcpy(&result, tmp.data(), sizeof(T));
        return result;
    }

    template <typename T>
    static void Write(std::ofstream& writeStream, const std::streampos offset, const T& value)
    {
        //using intermedial array to deal with optimization: -fstrict-aliasing

        static_assert(std::is_scalar_v<T>, "Only scalars are allowed.");
        std::array<char, sizeof(T)> tmp;
        memcpy(tmp.data(), std::addressof(value), sizeof(value));

        writeStream.seekp(offset);
        writeStream.write(tmp.data(), tmp.size());
    }
};
