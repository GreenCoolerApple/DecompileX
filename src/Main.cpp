#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <iomanip>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <utility>
#include <system_error>

namespace fs = std::filesystem;

namespace DecompileX
{
    constexpr std::uintmax_t MAX_FILE_SIZE = 256ULL * 1024 * 1024;
    constexpr std::size_t HEX_DUMP_LIMIT = 512;

    struct BinaryFile
    {
        std::string path;
        std::string format;
        std::string architecture;
        std::uintmax_t size = 0;
        std::vector<unsigned char> data;
    };

    std::string getPlatform()
    {
#if defined(__ANDROID__)
        return "Android";
#elif defined(_WIN32)
        return "Windows";
#elif defined(__APPLE__)
        return "macOS";
#elif defined(__linux__)
        return "Linux";
#else
        return "Unknown";
#endif
    }

    std::string getArchitecture()
    {
#if defined(__aarch64__) || defined(_M_ARM64)
        return "ARM64";
#elif defined(__arm__) || defined(_M_ARM)
        return "ARM";
#elif defined(__x86_64__) || defined(_M_X64)
        return "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
        return "x86";
#else
        return "Unknown";
#endif
    }

    std::uint16_t read16(
        const std::vector<unsigned char>& data,
        std::size_t offset,
        bool littleEndian = true
    )
    {
        if (offset > data.size() ||
            data.size() - offset < 2)
            return 0;

        if (littleEndian)
        {
            return static_cast<std::uint16_t>(
                data[offset] |
                (static_cast<std::uint16_t>(data[offset + 1]) << 8)
            );
        }

        return static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(data[offset]) << 8) |
            data[offset + 1]
        );
    }

    std::uint32_t read32(
        const std::vector<unsigned char>& data,
        std::size_t offset
    )
    {
        if (offset > data.size() ||
            data.size() - offset < 4)
            return 0;

        return static_cast<std::uint32_t>(data[offset]) |
               (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(data[offset + 3]) << 24);
    }

    std::string detectFormat(
        const std::vector<unsigned char>& data
    )
    {
        if (data.size() >= 4)
        {
            if (data[0] == 0x7F &&
                data[1] == 'E' &&
                data[2] == 'L' &&
                data[3] == 'F')
                return "ELF";

            if (data[0] == 'P' &&
                data[1] == 'K' &&
                data[2] == 0x03 &&
                data[3] == 0x04)
                return "ZIP / APK / JAR";

            if (data[0] == 0xCA &&
                data[1] == 0xFE &&
                data[2] == 0xBA &&
                data[3] == 0xBE)
                return "Java Class / Mach-O Universal";

            if ((data[0] == 0xFE && data[1] == 0xED &&
                 data[2] == 0xFA && data[3] == 0xCE) ||
                (data[0] == 0xCE && data[1] == 0xFA &&
                 data[2] == 0xED && data[3] == 0xFE) ||
                (data[0] == 0xFE && data[1] == 0xED &&
                 data[2] == 0xFA && data[3] == 0xCF) ||
                (data[0] == 0xCF && data[1] == 0xFA &&
                 data[2] == 0xED && data[3] == 0xFE))
                return "Mach-O";
        }

        if (data.size() >= 64 &&
            data[0] == 'M' &&
            data[1] == 'Z')
        {
            std::uint32_t offset = read32(data, 0x3C);

            if (offset <= data.size() &&
                data.size() - offset >= 4 &&
                data[offset] == 'P' &&
                data[offset + 1] == 'E' &&
                data[offset + 2] == 0 &&
                data[offset + 3] == 0)
                return "Windows PE";

            return "DOS MZ";
        }

        return "Unknown Binary";
    }

    std::string detectArchitecture(
        const std::vector<unsigned char>& data,
        const std::string& format
    )
    {
        if (format == "ELF" && data.size() >= 20)
        {
            bool littleEndian = data[5] != 2;
            std::uint16_t machine = read16(data, 18, littleEndian);

            switch (machine)
            {
                case 3: return "x86";
                case 40: return "ARM";
                case 62: return "x86_64";
                case 183: return "ARM64";
                case 243: return "RISC-V";
                default: return "Unknown ELF Architecture";
            }
        }

        if (format == "Windows PE" && data.size() >= 64)
        {
            std::uint32_t offset = read32(data, 0x3C);
            std::uint16_t machine = read16(data, offset + 4);

            switch (machine)
            {
                case 0x014C: return "x86";
                case 0x8664: return "x86_64";
                case 0x01C0: return "ARM";
                case 0xAA64: return "ARM64";
                default: return "Unknown PE Architecture";
            }
        }

        return "Unknown";
    }

    bool loadFile(
        const std::string& path,
        BinaryFile& file
    )
    {
        std::error_code ec;

        if (!fs::is_regular_file(path, ec) || ec)
            return false;

        std::uintmax_t size = fs::file_size(path, ec);

        if (ec || size > MAX_FILE_SIZE)
            return false;

        std::ifstream stream(path, std::ios::binary);

        if (!stream)
            return false;

        std::vector<unsigned char> buffer(
            static_cast<std::size_t>(size)
        );

        if (!buffer.empty())
        {
            stream.read(
                reinterpret_cast<char*>(buffer.data()),
                static_cast<std::streamsize>(buffer.size())
            );

            if (!stream)
                return false;
        }

        file.path = path;
        file.size = size;
        file.format = detectFormat(buffer);
        file.architecture = detectArchitecture(buffer, file.format);
        file.data = std::move(buffer);

        return true;
    }

    void showInformation(const BinaryFile& file)
    {
        std::cout << "\n========== FILE INFORMATION ==========\n";
        std::cout << "Name: "
                  << fs::path(file.path).filename().string() << '\n';
        std::cout << "Path: " << file.path << '\n';
        std::cout << "Format: " << file.format << '\n';
        std::cout << "Architecture: " << file.architecture << '\n';
        std::cout << "Size: " << file.size << " bytes\n";
        std::cout << "======================================\n";
    }

    void hexDump(const BinaryFile& file)
    {
        std::size_t limit = std::min(
            file.data.size(),
            HEX_DUMP_LIMIT
        );

        std::cout << "\n========== HEX VIEWER ==========\n";

        for (std::size_t i = 0; i < limit; i += 16)
        {
            std::cout << std::hex
                      << std::setw(8)
                      << std::setfill('0')
                      << i << "  ";

            for (std::size_t j = 0; j < 16; ++j)
            {
                if (i + j < limit)
                {
                    std::cout << std::setw(2)
                              << static_cast<unsigned int>(
                                  file.data[i + j]
                              ) << ' ';
                }
                else
                {
                    std::cout << "   ";
                }
            }

            std::cout << " |";

            for (std::size_t j = 0;
                 j < 16 && i + j < limit; ++j)
            {
                unsigned char c = file.data[i + j];

                std::cout << (
                    c >= 32 && c <= 126
                    ? static_cast<char>(c)
                    : '.'
                );
            }

            std::cout << "|\n";
        }

        std::cout << std::dec << std::setfill(' ');
        std::cout << "================================\n";
    }

    void printBanner()
    {
        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "               DecompileX               \n";
        std::cout << "           Game Binary Analyzer         \n";
        std::cout << "               Version 0.1.0            \n";
        std::cout << "========================================\n";
        std::cout << "Platform: " << getPlatform() << '\n';
        std::cout << "Architecture: " << getArchitecture() << '\n';
    }

    void run()
    {
        BinaryFile currentFile;
        bool loaded = false;
        bool running = true;

        while (running)
        {
            printBanner();

            std::cout << "\n[1] Open Binary File\n";
            std::cout << "[2] File Information\n";
            std::cout << "[3] Hex Viewer\n";
            std::cout << "[4] Detect File Format\n";
            std::cout << "[5] System Information\n";
            std::cout << "[0] Exit\n";
            std::cout << "\nSelect: ";

            int option;

            if (!(std::cin >> option))
            {
                if (std::cin.eof())
                    break;

                std::cin.clear();
                std::cin.ignore(
                    std::numeric_limits<std::streamsize>::max(),
                    '\n'
                );

                std::cout << "Invalid input.\n";
                continue;
            }

            std::cin.ignore(
                std::numeric_limits<std::streamsize>::max(),
                '\n'
            );

            switch (option)
            {
                case 1:
                {
                    std::string path;

                    std::cout << "\nEnter file path: ";
                    std::getline(std::cin, path);

                    if (path.size() >= 2 &&
                        path.front() == '"' &&
                        path.back() == '"')
                    {
                        path = path.substr(1, path.size() - 2);
                    }

                    BinaryFile newFile;

                    if (loadFile(path, newFile))
                    {
                        currentFile = std::move(newFile);
                        loaded = true;

                        std::cout << "\nFile loaded successfully!\n";
                        showInformation(currentFile);
                    }
                    else
                    {
                        std::cout << "\nFailed to load file.\n";
                    }

                    break;
                }

                case 2:
                    if (loaded)
                        showInformation(currentFile);
                    else
                        std::cout << "\nNo file loaded.\n";
                    break;

                case 3:
                    if (loaded)
                        hexDump(currentFile);
                    else
                        std::cout << "\nNo file loaded.\n";
                    break;

                case 4:
                    if (loaded)
                    {
                        std::cout << "\nFormat: "
                                  << currentFile.format << '\n';
                        std::cout << "Architecture: "
                                  << currentFile.architecture << '\n';
                    }
                    else
                    {
                        std::cout << "\nNo file loaded.\n";
                    }
                    break;

                case 5:
                    std::cout << "\nPlatform: "
                              << getPlatform() << '\n';
                    std::cout << "Architecture: "
                              << getArchitecture() << '\n';
                    std::cout << "C++ Standard: "
                              << __cplusplus << '\n';
                    break;

                case 0:
                    running = false;
                    break;

                default:
                    std::cout << "\nUnknown option.\n";
                    break;
            }
        }

        std::cout << "\nDecompileX closed.\n";
    }
}

int main()
{
    DecompileX::run();
    return 0;
}
