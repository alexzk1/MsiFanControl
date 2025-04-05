#pragma once

#include "cm_ctors.h"
#include "device_commands.h"
#include "readwrite_provider.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iosfwd>
#include <iostream>
#include <ostream>
#include <set>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

/// @brief This is read-write abstraction which accepts AddressedValueAnyList as commands and
/// operates on "file" provided by ReadWriteProvider.
///
class CReadWrite
{
  public:
    class WriteHandle;

    CReadWrite(ReadWriteProviderPtr ioProvider, BackupProviderPtr backupProvider) :
        ioProvider(std::move(ioProvider)),
        backupProvider(std::move(backupProvider))
    {
    }
    CReadWrite() = delete;
    MOVEONLY_ALLOWED(CReadWrite);

    ~CReadWrite()
    {
        if (backupProvider)
        {
            try
            {
                backupProvider->RestoreOffsets(backupOffsets);
            }
            catch (std::exception &ex)
            {
                std::cerr << "Exception on calling backup from ~CReadWrite(): " << ex.what()
                          << std::endl
                          << std::flush;
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
    void Write(WriteHandle &handle, const AddressedValueAnyList &toWrite) const
    {
        for (const auto &value : toWrite)
        {
            std::visit(
              [&handle, this](const auto &element) {
                  using stored_type = std::decay_t<decltype(element)>;
                  Write<stored_type>(handle.stream, element);
              },
              value);
        }
    }

    //! @brief optimized version which opens file once for many elements to read.
    //! It uses address field to seek and fills value field of the toFill object.
    template <typename Container>
    void Read(Container &toFill) const
    {
        auto stream = ioProvider->ReadStream();
        for (auto &containedValue : toFill)
        {
            std::visit(
              [&stream](auto &element) {
                  using stored_type = std::decay_t<decltype(element)>;
                  Read<stored_type>(stream, element);
              },
              GetCommandFromContained(containedValue));
        }
    }

    template <typename taElement>
    void ReadOne(taElement &toFill) const
    {
        auto stream = ioProvider->ReadStream();
        Read(stream, toFill);
    }

    /// @brief Cancel backup on listed objects, which means changes at those addresses
    /// will not be restored on daemon closing.
    void CancelBackupOn(const AddressedValueAnyList &aList) const
    {
        for (const auto &value : aList)
        {
            CancelBackupOn(value);
        }
    }

    /// @brief Cancel backup on object, which means changes at those addresses
    /// will not be restored on daemon closing.
    void CancelBackupOn(const AddressedValueAny &value) const
    {
        std::visit(
          [this](const auto &element) {
              ForEachByte(element, [this](const auto offset) {
                  ignoreBackupOffsets.insert(offset);
                  backupOffsets.erase(offset);
              });
          },
          value);
    }

    class WriteHandle
    {
      public:
        ~WriteHandle() = default;
        WriteHandle() = delete;
        MOVEONLY_ALLOWED(WriteHandle);

      private:
        friend class CReadWrite;
        explicit WriteHandle(std::ofstream &&stream) :
            stream(std::move(stream))
        {
        }
        std::ofstream stream;
    };

  private:
    ReadWriteProviderPtr ioProvider;
    BackupProviderPtr backupProvider;
    mutable std::set<std::int64_t> backupOffsets;
    mutable std::set<std::int64_t> ignoreBackupOffsets;

    static AddressedValueAny &GetCommandFromContained(AddressedValueAnyList::value_type &elem)
    {
        return elem;
    }

    template <typename TheState>
    static AddressedValueAny &GetCommandFromContained(std::pair<TheState, AddressedValueAny> &elem)
    {
        return elem.second;
    }

    template <typename T>
    static void Read(std::ifstream &readStream, T &element)
    {
        if constexpr (std::is_same_v<T, TagIgnore>)
        {
            return;
        }
        using value_t = decltype(element.value);
        // using intermedial array to deal with optimization: -fstrict-aliasing
        static_assert(std::is_scalar_v<value_t>, "Only scalar values are allowed.");
        std::array<char, sizeof(value_t)> tmp{};

        readStream.seekg(element.address);
        readStream.read(tmp.data(), tmp.size());

        // Guess i should understand this python code as big endian:
        //  VALUE = int(file.read(2).hex(),16)

        std::copy(tmp.rbegin(), tmp.rend(), &element.value);

        if constexpr (std::is_same_v<T, AddressedBits>)
        {
            element.MaskValue();
        }
    }

    /// Calls callabale passing to each address (offset) used to store element.value.
    template <typename taElementType, typename taCallable>
    void ForEachByte(const taElementType &element, const taCallable &func) const
    {
        using value_t = decltype(element.value);
        const auto offset = static_cast<std::int64_t>(element.address);
        for (std::size_t i = 0; i < sizeof(value_t); ++i)
        {
            func(offset + static_cast<std::int64_t>(i));
        }
    }

    template <typename taElementType>
    void Write(std::ofstream &writeStream, const taElementType &element) const
    {
        if constexpr (std::is_same_v<taElementType, TagIgnore>)
        {
            return;
        }

        // installing backup
        ForEachByte(element, [this](const auto offset) {
            if (ignoreBackupOffsets.count(offset) == 0)
            {
                backupOffsets.insert(offset);
            }
        });

        using value_t = decltype(element.value);
        const std::streampos offset = element.address;
        auto value = element.value;

        if constexpr (std::is_same_v<taElementType, AddressedBits>)
        {
            // using intermedial array to deal with optimization: -fstrict-aliasing
            AddressedValueAnyList tmp{AddressedValue1B{offset, 0}};
            Read(tmp);
            value = element.ValueForWritting(std::get<AddressedValue1B>(tmp.front()).value);
        }

        static_assert(std::is_scalar_v<value_t>, "Only scalar values are allowed.");
        std::array<char, sizeof(value_t)> tmp{};

        // Guess this is BIG endian too:
        // file.write(bytes((VALUE,)))
        std::copy_n(&value, sizeof(value), tmp.rbegin());
        writeStream.seekp(offset);
        writeStream.write(tmp.data(), tmp.size());
    }
};

TEST_MOVE_NOEX(CReadWrite);
