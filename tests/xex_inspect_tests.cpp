#include "x360port/xex_inspect.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

void WriteBe32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value)
{
    bytes[offset] = static_cast<std::byte>(value >> 24U);
    bytes[offset + 1U] = static_cast<std::byte>(value >> 16U);
    bytes[offset + 2U] = static_cast<std::byte>(value >> 8U);
    bytes[offset + 3U] = static_cast<std::byte>(value);
}

std::vector<std::byte> ValidatedHeader(std::size_t size)
{
    std::vector<std::byte> xex(size);
    WriteBe32(xex, 0x00U, 0x58455832U);
    WriteBe32(xex, 0x08U, 0x300U);
    WriteBe32(xex, 0x10U, 0x100U);
    WriteBe32(xex, 0x14U, 3U);
    WriteBe32(xex, 0x18U, 0x000003ffU);
    WriteBe32(xex, 0x1cU, 0x200U);
    WriteBe32(xex, 0x100U, 0x184U);
    WriteBe32(xex, 0x104U, 0x1000U);
    WriteBe32(xex, 0x210U, 0x82000000U);
    WriteBe32(xex, 0x280U, 0U);
    WriteBe32(xex, 0x200U, 0x14U);
    return xex;
}

bool Refuses(std::vector<std::byte> xex, std::string_view expected)
{
    const x360port::XexInspectionResult result = x360port::InspectXex(xex);
    return !result && result.error.find(expected) != std::string::npos;
}

} // namespace

int main()
{
    if (!Refuses(std::vector<std::byte>(8), "fixed header"))
    {
        std::cerr << "truncated XEX was accepted\n";
        return 1;
    }
    {
        std::vector<std::byte> xex(24);
        WriteBe32(xex, 0x00U, 0x12345678U);
        if (!Refuses(std::move(xex), "only XEX2"))
        {
            std::cerr << "non-XEX2 input was accepted\n";
            return 1;
        }
    }
    if (!Refuses(ValidatedHeader(0x300U), "no encrypted or compressed image payload"))
    {
        std::cerr << "payload-less XEX was accepted\n";
        return 1;
    }
    {
        std::vector<std::byte> xex = ValidatedHeader(0x310U);
        xex[0x207U] = static_cast<std::byte>(2U);
        if (!Refuses(std::move(xex), "no LZX window"))
        {
            std::cerr << "window-less compressed XEX was accepted\n";
            return 1;
        }
    }
    {
        std::vector<std::byte> xex = ValidatedHeader(0x301U);
        xex[0x205U] = static_cast<std::byte>(1U);
        if (!Refuses(std::move(xex), "AES-block aligned"))
        {
            std::cerr << "misaligned encrypted XEX was accepted\n";
            return 1;
        }
    }
    std::cout << "xex inspector preflight tests: PASS\n";
    return 0;
}
