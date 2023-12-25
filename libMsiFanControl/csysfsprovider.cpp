#include "csysfsprovider.h"
#include "readwrite_provider.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <array>

class ReadWriteProviderImpl : public IReadWriteProvider
{
private:
    std::filesystem::path fileName;
public:
    ReadWriteProviderImpl(std::filesystem::path fileName):
        fileName(std::move(fileName))
    {

    }
    std::ofstream WriteStream() const final
    {
        return std::ofstream(fileName, std::ios_base::out | std::ios_base::binary | std::ios_base::app);
    }

    std::ifstream ReadStream() const final
    {
        return std::ifstream(fileName, std::ios_base::in | std::ios_base::binary);
    }
};

CReadWrite CSysFsProvider::CreateIoObject(bool dryRun)
{
    static const auto genDryRun = []()->std::filesystem::path
    {
        auto name = std::filesystem::path(std::tmpnam(nullptr));
        std::array<char, 256> zeroes{};
        std::fill(zeroes.begin(), zeroes.end(), 0);
        std::ofstream ofs(name, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
        ofs.write(zeroes.data(), zeroes.size());

        return name;
    };
    return CReadWrite(std::make_shared<ReadWriteProviderImpl>(dryRun ? genDryRun() :
                      "/sys/kernel/debug/ec/ec0/io"));
}
