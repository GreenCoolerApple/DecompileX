#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <iomanip>
#include <cstdint>
#include <limits>
#include <algorithm>

namespace fs = std::filesystem;

namespace DecompileX
{
    struct BinaryFile
    {
        std::string path;
        std::string format;
        std::uintmax_t size = 0;
        std::vector<unsigned char> data;
    };

    std::string detectFormat(const std::vector<unsigned char>& data)
    {
        if (data.size() >= 4)
        {
            if (data[0] == 0x7F &&
                data[1] == 'E' &&
                data[2] == 'L' &&
                data[3] == 'F')
                return "ELF Executable";

            if (data[0] == 'P' &&
                data[1] == 'K' &&
                data[2] == 0x03 &&
                data[3] == 0x04)
                return "ZIP Archive";

            if (data[0] == 0xCA &&
                data[1] == 0xFE &&
                data[2] == 0xBA &&
                data[3] == 0xBE)
                return "Java Class / Mach-O Fat Binary";
        }

        if (data.size() >= 64 &&
            data[0] == 'M' &&
            data[1] == 'Z')
        {
            std::uint32_t offset =
                static_cast<std::uint32_t>(data[0x3C]) |
                (static_cast<std::uint32_t>(data[0x3D]) << 8) |
                (static_cast<std::uint32_t>(data[0x3E]) << 16) |
                (static_cast<std::uint32_t>(data[0x3F]) << 24);

            if (offset <= data.size() &&
                data.size() - offset >= 4 &&
                data[offset] == 'P' &&
                data[offset + 1] == 'E' &&
                data[offset + 2] == 0 &&
                data[offset + 3] == 0)
                return "Windows PE Executable";

            return "DOS MZ Executable";
        }

        return "Unknown Binary";
    }

    bool loadFile(const std::string& path, BinaryFile& file)
    {
        std::error_code ec;

        if (!fs::is_regular_file(path, ec))
            return false;

        auto size = fs::file_size(path, ec);

        if (ec || size > 256ULL * 1024 * 1024)
        {
            std::cout << "File is too large or inaccessible.\n";
            return false;
        }

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
        std::cout << "Size: " << file.size << " bytes\n";
        std::cout << "======================================\n";
    }

    void hexDump(const BinaryFile& file)
    {
        std::size_t limit = std::min<std::size_t>(
            file.data.size(), 512
        );

        std::cout << "\n========== HEX VIEWER ==========\n";

        for (std::size_t i = 0; i < limit; i += 16)
        {
            std::cout << std::hex << std::setw(8)
                      << std::setfill('0') << i << "  ";

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
        std::cout << "             DecompileX                 \n";
        std::cout << "         Game Binary Analyzer           \n";
        std::cout << "             Version 0.1.0              \n";
        std::cout << "========================================\n";
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
                {
                    if (loaded)
                        showInformation(currentFile);
                    else
                        std::cout << "\nNo file loaded.\n";

                    break;
                }

                case 3:
                {
                    if (loaded)
                        hexDump(currentFile);
                    else
                        std::cout << "\nNo file loaded.\n";

                    break;
                }

                case 4:
                {
                    if (loaded)
                        std::cout << "\nDetected: "
                                  << currentFile.format << '\n';
                    else
                        std::cout << "\nNo file loaded.\n";

                    break;
                }

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
