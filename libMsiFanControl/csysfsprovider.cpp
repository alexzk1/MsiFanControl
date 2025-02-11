#include "csysfsprovider.h"

#include "readwrite.h"
#include "readwrite_provider.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <utility>

// NOLINTNEXTLINE
extern bool GLOBAL_DRY_RUN;

class ReadWriteProviderImpl : public IReadWriteProvider
{
  private:
    std::filesystem::path fileName;

  public:
    explicit ReadWriteProviderImpl(std::filesystem::path fileName) :
        fileName(std::move(fileName))
    {
    }

    [[nodiscard]]
    std::ofstream WriteStream() const final
    {
        return {fileName, std::ios_base::out | std::ios_base::binary | std::ios_base::app};
    }

    [[nodiscard]]
    std::ifstream ReadStream() const final
    {
        return {fileName, std::ios_base::in | std::ios_base::binary};
    }
};

std::shared_ptr<IReadWriteProvider> CSysFsProvider::CreateIoDirect(bool dryRun)
{
    GLOBAL_DRY_RUN = dryRun;
    static const auto genDryRun = []() -> std::filesystem::path {
        auto name = std::filesystem::temp_directory_path();
        name /= std::filesystem::path("msiDryRun.bin");

        std::array<char, 256> zeroes{};
        std::fill(zeroes.begin(), zeroes.end(), 0);
        std::ofstream ofs(name, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
        ofs.write(zeroes.data(), zeroes.size());
        std::cerr << "Path to dry-run file: " << name << std::endl;
        return name;
    };

    return std::make_shared<ReadWriteProviderImpl>(dryRun ? genDryRun()
                                                          : "/sys/kernel/debug/ec/ec0/io");
}

CReadWrite CSysFsProvider::CreateIoObject(BackupProviderPtr backuPovider, bool dryRun)
{
    return {CreateIoDirect(dryRun), std::move(backuPovider)};
}
