#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace DecompileX {
struct IsoCodeEntry {
    std::string path;
    std::string kind;
    std::uint64_t size = 0;
    std::uint64_t offset = 0;
    bool executable = false;
};
struct IsoCodeReport {
    bool success = false;
    std::string message;
    std::string volume;
    std::uint64_t imageBytes = 0;
    std::size_t filesScanned = 0;
    std::vector<IsoCodeEntry> matches;
    std::string formatText() const;
};
class IsoCodeScanner {
public:
    static IsoCodeReport scan(const std::filesystem::path& iso, std::size_t maxEntries = 100000);
};
}
