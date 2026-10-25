#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace DecompileX
{
    struct BinResult
    {
        bool success = false;
        std::string message;
        std::filesystem::path output;
        std::uint64_t bytesWritten = 0;
    };

    class BinCompiler
    {
    public:
        static BinResult compile(
            const std::vector<std::uint8_t>& data,
            const std::filesystem::path& output
        );

        static BinResult compileHex(
            const std::string& hex,
            const std::filesystem::path& output
        );

        static BinResult compileText(
            const std::string& text,
            const std::filesystem::path& output
        );

        static BinResult merge(
            const std::vector<std::filesystem::path>& inputs,
            const std::filesystem::path& output
        );

        static bool parseHex(
            const std::string& hex,
            std::vector<std::uint8_t>& data
        );

        static std::string toHex(
            const std::vector<std::uint8_t>& data,
            bool spaced = true
        );

        static std::uint32_t calculateCRC32(
            const std::vector<std::uint8_t>& data
        );

        static bool readBinary(
            const std::filesystem::path& path,
            std::vector<std::uint8_t>& data,
            std::size_t maxBytes = 268435456
        );

    private:
        static bool prepareOutput(
            const std::filesystem::path& output,
            std::string& error
        );
    };
}
