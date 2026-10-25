#include "ExeCompiler.hpp"

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <system_error>

namespace fs = std::filesystem;

namespace DecompileX
{
    std::string ExeCompiler::quote(const std::string& value)
    {
        std::string result = "\"";

        for (char c : value)
        {
            if (c == '"')
                result += "\\\"";
            else
                result += c;
        }

        result += "\"";
        return result;
    }

    std::string ExeCompiler::getExecutable(CompilerType type)
    {
        switch (type)
        {
            case CompilerType::MSVC:
                return "cl";

            case CompilerType::MinGW:
#if defined(_WIN32)
                return "g++";
#else
                return "x86_64-w64-mingw32-g++";
#endif

            case CompilerType::Clang:
                return "clang++";

            default:
                return "";
        }
    }

    std::string ExeCompiler::getCompilerName(CompilerType type)
    {
        switch (type)
        {
            case CompilerType::MSVC:
                return "Microsoft Visual C++";

            case CompilerType::MinGW:
                return "MinGW-w64";

            case CompilerType::Clang:
                return "Clang";

            default:
                return "Automatic";
        }
    }

    bool ExeCompiler::isCompilerAvailable(CompilerType type)
    {
        if (type == CompilerType::Auto)
            return detectCompiler() != CompilerType::Auto;

        std::string executable = getExecutable(type);

        if (executable.empty())
            return false;

        std::string command;

        if (type == CompilerType::MSVC)
            command = executable + " /? > ";
        else
            command = executable + " --version > ";

#if defined(_WIN32)
        command += "NUL 2>&1";
#else
        command += "/dev/null 2>&1";
#endif

        return std::system(command.c_str()) == 0;
    }

    CompilerType ExeCompiler::detectCompiler()
    {
#if defined(_WIN32)
        if (isCompilerAvailable(CompilerType::MSVC))
            return CompilerType::MSVC;
#endif

        if (isCompilerAvailable(CompilerType::MinGW))
            return CompilerType::MinGW;

        if (isCompilerAvailable(CompilerType::Clang))
            return CompilerType::Clang;

        return CompilerType::Auto;
    }

    std::string ExeCompiler::buildCommand(
        const CompilerOptions& options,
        CompilerType type
    )
    {
        std::ostringstream command;

        command << getExecutable(type) << " ";

        if (type == CompilerType::MSVC)
        {
            command << "/nologo /EHsc ";

            if (options.standard == "c++20")
                command << "/std:c++20 ";
            else if (options.standard == "c++17")
                command << "/std:c++17 ";
            else
                command << "/std:c++14 ";

            if (options.optimize)
                command << "/O2 ";

            if (options.debug)
                command << "/Zi ";

            for (const auto& directory : options.includeDirectories)
                command << "/I" << quote(directory.string()) << " ";

            for (const auto& source : options.sources)
                command << quote(source.string()) << " ";

            for (const auto& argument : options.arguments)
                command << argument << " ";

            command << "/Fe:" << quote(options.output.string());
        }
        else
        {
            command << "-std=" << options.standard << " ";

            if (options.optimize)
                command << "-O2 ";

            if (options.debug)
                command << "-g ";

            for (const auto& directory : options.includeDirectories)
                command << "-I" << quote(directory.string()) << " ";

            for (const auto& source : options.sources)
                command << quote(source.string()) << " ";

            for (const auto& argument : options.arguments)
                command << argument << " ";

            command << "-o " << quote(options.output.string());
        }

        return command.str();
    }

    CompilerResult ExeCompiler::compile(
        const CompilerOptions& options
    )
    {
        CompilerResult result;
        result.output = options.output;

        if (options.sources.empty())
        {
            result.message = "No source files provided.";
            return result;
        }

        if (options.output.empty())
        {
            result.message = "Output path is empty.";
            return result;
        }

        for (const auto& source : options.sources)
        {
            std::error_code ec;

            if (!fs::is_regular_file(source, ec) || ec)
            {
                result.message =
                    "Source file not found: " + source.string();

                return result;
            }
        }

        CompilerType type = options.compiler;

        if (type == CompilerType::Auto)
            type = detectCompiler();

        if (type == CompilerType::Auto)
        {
            result.message = "No supported compiler found.";
            return result;
        }

        if (!isCompilerAvailable(type))
        {
            result.message = "Selected compiler is not available.";
            return result;
        }

        std::error_code ec;

        if (!options.output.parent_path().empty())
        {
            fs::create_directories(
                options.output.parent_path(),
                ec
            );

            if (ec)
            {
                result.message =
                    "Failed to create output directory.";

                return result;
            }
        }

        result.command = buildCommand(options, type);

        result.exitCode = std::system(result.command.c_str());

        if (result.exitCode == 0 &&
            fs::is_regular_file(options.output))
        {
            result.success = true;
            result.message = "Compilation completed successfully.";
        }
        else
        {
            result.message = "Compilation failed.";
        }

        return result;
    }
}
