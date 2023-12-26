#include "csysfsprovider.h"
#include "msi_fan_control.h"
#include "intelbeforegen10.h"
#include "intelgen10.h"

#include <cstddef>
#include <libcpuid/libcpuid.h>
#include <stdexcept>
#include <iostream>

DevicePtr CreateDeviceController(bool dryRun)
{
    if (!cpuid_present())
    {
        throw std::runtime_error("Unrecognized CPU. We cannot proceed.");
    }
    cpu_raw_data_t raw;
    cpu_id_t data;

    if (cpuid_get_raw_data(&raw) < 0)
    {
        const auto err = std::string("Sorry, cannot get the CPUID raw data: ") + std::string(cpuid_error());
        throw std::runtime_error(err);
    }

    if (cpu_identify(&raw, &data) < 0)
    {
        const auto err = std::string("Sorry, CPU identification failed: ") + std::string(cpuid_error());
        throw std::runtime_error(err);
    }

    const std::string brand = std::string(data.brand_str);
    if (data.vendor != cpu_vendor_t::VENDOR_INTEL)
    {
        throw std::runtime_error("We support only Intel CPUs into MSI laptops. Detected CPU: " + brand);
    }
    const auto till = brand.find("th");
    if (till != std::string::npos)
    {
        const auto gen = std::stoi(brand.substr(0, till));

        std::cerr << "CPU Gen detected: " << gen << std::endl << std::flush;
        if (gen > 9)
        {
            return std::make_shared<CIntelGen10>(CSysFsProvider::CreateIoObject(dryRun));
        }
    }
    else
    {
        std::cerr << "Didn't find tag \"th\" into brand string. Assuming it is old model.";
    }

    return std::make_shared<CIntelBeforeGen10>(CSysFsProvider::CreateIoObject(dryRun));
}
