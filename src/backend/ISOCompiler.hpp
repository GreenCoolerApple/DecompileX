#pragma once
#include <filesystem>
#include <string>
#include <cstdint>
namespace DecompileX {
struct ISOResult { bool success=false; std::string message; std::uint64_t bytesWritten=0; std::uint32_t filesAdded=0; };
class ISOCompiler { public: static ISOResult compile(const std::filesystem::path& sourceDirectory,const std::filesystem::path& outputISO,const std::string& volumeLabel="DECOMPILEX"); };
}
