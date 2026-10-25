#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace DecompileX
{
    enum class CompilerType
    {
        Auto,
        MSVC,
        MinGW,
        Clang
    };

    struct CompilerOptions
    {
        CompilerType compiler = CompilerType::Auto;
        std::vector<std::filesystem::path> sources;
        std::vector<std::filesystem::path> includeDirectories;
        std::vector<std::string> arguments;
        std::filesystem::path output;
        std::string standard = "c++17";
        bool optimize = true;
        bool debug = false;
    };

    struct CompilerResult
    {
        bool success = false;
        int exitCode = -1;
        std::string command;
        std::string message;
        std::filesystem::path output;
    };

    class ExeCompiler
    {
    public:
        static CompilerResult compile(const CompilerOptions& options);

        static bool isCompilerAvailable(CompilerType type);

        static CompilerType detectCompiler();

        static std::string getCompilerName(CompilerType type);

    private:
        static std::string quote(const std::string& value);

        static std::string buildCommand(
            const CompilerOptions& options,
            CompilerType type
        );

        static std::string getExecutable(CompilerType type);
    };
}
