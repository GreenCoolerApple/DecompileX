#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace DecompileX {
class PspAnalyzer {
public:
    static std::string analyze(const std::vector<std::uint8_t>& input, std::size_t maxInstructions = 160);
};
}
