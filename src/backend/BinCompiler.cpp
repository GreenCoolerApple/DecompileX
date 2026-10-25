#include "BinCompiler.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace DecompileX
{
    namespace
    {
        int hexValue(char value)
        {
            if (value >= '0' && value <= '9')
                return value - '0';

            if (value >= 'a' && value <= 'f')
                return value - 'a' + 10;

            if (value >= 'A' && value <= 'F')
                return value - 'A' + 10;

            return -1;
        }

        bool sameFile(
            const fs::path& first,
            const fs::path& second
        )
        {
            std::error_code ec;

            bool firstExists = fs::exists(first, ec);

            if (ec || !firstExists)
                return false;

            bool secondExists = fs::exists(second, ec);

            if (ec || !secondExists)
                return false;

            return fs::equivalent(first, second, ec) && !ec;
        }
    }

    bool BinCompiler::prepareOutput(
        const fs::path& output,
        std::string& error
    )
    {
        if (output.empty() || output.filename().empty())
        {
            error = "Invalid output path.";
            return false;
        }

        fs::path parent = output.parent_path();

        if (parent.empty())
            return true;

        std::error_code ec;

        fs::create_directories(parent, ec);

        if (ec)
        {
            error = "Failed to create output directory: " +
                ec.message();

            return false;
        }

        return true;
    }

    bool BinCompiler::parseHex(
        const std::string& hex,
        std::vector<std::uint8_t>& data
    )
    {
        std::vector<std::uint8_t> result;
        int high = -1;

        for (std::size_t i = 0; i < hex.size(); ++i)
        {
            unsigned char current =
                static_cast<unsigned char>(hex[i]);

            if (std::isspace(current) ||
                hex[i] == ',' ||
                hex[i] == ':' ||
                hex[i] == '-' ||
                hex[i] == '_')
            {
                if (high != -1)
                    return false;

                continue;
            }

            if (high == -1 &&
                hex[i] == '0' &&
                i + 1 < hex.size() &&
                (hex[i + 1] == 'x' || hex[i + 1] == 'X'))
            {
                ++i;
                continue;
            }

            int value = hexValue(hex[i]);

            if (value < 0)
                return false;

            if (high == -1)
            {
                high = value;
            }
            else
            {
                result.push_back(
                    static_cast<std::uint8_t>(
                        (high << 4) | value
                    )
                );

                high = -1;
            }
        }

        if (high != -1)
            return false;

        data = std::move(result);

        return true;
    }

    std::string BinCompiler::toHex(
        const std::vector<std::uint8_t>& data,
        bool spaced
    )
    {
        std::ostringstream output;

        output << std::uppercase
               << std::hex
               << std::setfill('0');

        for (std::size_t i = 0; i < data.size(); ++i)
        {
            if (spaced && i != 0)
                output << ' ';

            output << std::setw(2)
                   << static_cast<unsigned int>(data[i]);
        }

        return output.str();
    }

    std::uint32_t BinCompiler::calculateCRC32(
        const std::vector<std::uint8_t>& data
    )
    {
        std::uint32_t crc = 0xFFFFFFFFu;

        for (std::uint8_t byte : data)
        {
            crc ^= byte;

            for (int bit = 0; bit < 8; ++bit)
            {
                std::uint32_t mask = 0u - (crc & 1u);

                crc = (crc >> 1) ^
                    (0xEDB88320u & mask);
            }
        }

        return ~crc;
    }

    bool BinCompiler::readBinary(
        const fs::path& path,
        std::vector<std::uint8_t>& data,
        std::size_t maxBytes
    )
    {
        std::error_code ec;

        if (!fs::is_regular_file(path, ec) || ec)
            return false;

        std::uintmax_t fileSize = fs::file_size(path, ec);

        if (ec || fileSize > maxBytes)
            return false;

        std::ifstream input(path, std::ios::binary);

        if (!input)
            return false;

        std::vector<std::uint8_t> result(
            static_cast<std::size_t>(fileSize)
        );

        if (!result.empty())
        {
            input.read(
                reinterpret_cast<char*>(result.data()),
                static_cast<std::streamsize>(result.size())
            );

            if (!input)
                return false;
        }

        data = std::move(result);

        return true;
    }

    BinResult BinCompiler::compile(
        const std::vector<std::uint8_t>& data,
        const fs::path& output
    )
    {
        BinResult result;
        result.output = output;

        if (!prepareOutput(output, result.message))
            return result;

        std::ofstream file(
            output,
            std::ios::binary | std::ios::trunc
        );

        if (!file)
        {
            result.message = "Failed to open output file.";
            return result;
        }

        if (!data.empty())
        {
            file.write(
                reinterpret_cast<const char*>(data.data()),
                static_cast<std::streamsize>(data.size())
            );
        }

        file.flush();

        if (!file)
        {
            result.message = "Failed to write binary data.";
            return result;
        }

        file.close();

        if (!file)
        {
            result.message = "Failed to close output file.";
            return result;
        }

        result.success = true;
        result.bytesWritten =
            static_cast<std::uint64_t>(data.size());

        result.message = "Binary generated successfully.";

        return result;
    }

    BinResult BinCompiler::compileHex(
        const std::string& hex,
        const fs::path& output
    )
    {
        std::vector<std::uint8_t> data;

        if (!parseHex(hex, data))
        {
            BinResult result;
            result.output = output;
            result.message = "Invalid hexadecimal input.";

            return result;
        }

        return compile(data, output);
    }

    BinResult BinCompiler::compileText(
        const std::string& text,
        const fs::path& output
    )
    {
        std::vector<std::uint8_t> data(
            text.begin(),
            text.end()
        );

        return compile(data, output);
    }

    BinResult BinCompiler::merge(
        const std::vector<fs::path>& inputs,
        const fs::path& output
    )
    {
        BinResult result;
        result.output = output;

        if (inputs.empty())
        {
            result.message = "No input files provided.";
            return result;
        }

        for (const auto& input : inputs)
        {
            std::error_code ec;

            if (!fs::is_regular_file(input, ec) || ec)
            {
                result.message =
                    "Input file not found: " + input.string();

                return result;
            }

            if (sameFile(input, output))
            {
                result.message =
                    "Output cannot overwrite an input file.";

                return result;
            }
        }

        if (!prepareOutput(output, result.message))
            return result;

        std::ofstream destination(
            output,
            std::ios::binary | std::ios::trunc
        );

        if (!destination)
        {
            result.message = "Failed to create output file.";
            return result;
        }

        std::array<char, 65536> buffer{};

        for (const auto& inputPath : inputs)
        {
            std::ifstream source(
                inputPath,
                std::ios::binary
            );

            if (!source)
            {
                result.message =
                    "Failed to open input: " +
                    inputPath.string();

                return result;
            }

            while (source)
            {
                source.read(
                    buffer.data(),
                    static_cast<std::streamsize>(buffer.size())
                );

                std::streamsize count = source.gcount();

                if (count > 0)
                {
                    destination.write(
                        buffer.data(),
                        count
                    );

                    if (!destination)
                    {
                        result.message =
                            "Failed to write merged binary.";

                        return result;
                    }

                    result.bytesWritten +=
                        static_cast<std::uint64_t>(count);
                }
            }

            if (source.bad())
            {
                result.message =
                    "Failed to read input: " +
                    inputPath.string();

                return result;
            }
        }

        destination.flush();

        if (!destination)
        {
            result.message = "Failed to finalize binary.";
            return result;
        }

        destination.close();

        if (!destination)
        {
            result.message = "Failed to close merged binary.";
            return result;
        }

        result.success = true;
        result.message = "Binary files merged successfully.";

        return result;
    }
}
