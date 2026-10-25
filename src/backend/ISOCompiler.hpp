#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

namespace DecompileX {
struct ISOResult {
    bool success = false;
    std::string message;
    std::uint64_t bytesWritten = 0;
    std::uint32_t filesAdded = 0;
    std::uint32_t directoriesAdded = 0;
};
struct ISOOptions {
    std::string volumeLabel = "DECOMPILEX";
    bool overwrite = true;
    bool verifyOutput = true;
};
class ISOCompiler {
public:
    static ISOResult compile(const std::filesystem::path& sourceDirectory,
                             const std::filesystem::path& outputISO,
                             const std::string& volumeLabel = "DECOMPILEX");
    static ISOResult compile(const std::filesystem::path& sourceDirectory,
                             const std::filesystem::path& outputISO,
                             const ISOOptions& options);
};
}
