#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace DecompileX
{
    class ZipGenerator
    {
    public:
        ZipGenerator();
        ~ZipGenerator();

        bool create(const std::filesystem::path& outputPath);

        bool addFile(
            const std::filesystem::path& filePath,
            const std::string& archivePath = ""
        );

        bool addData(
            const std::string& archivePath,
            const std::vector<std::uint8_t>& data
        );

        bool addText(
            const std::string& archivePath,
            const std::string& text
        );

        bool addDirectory(
            const std::filesystem::path& directoryPath,
            const std::string& archivePrefix = ""
        );

        bool close();

        bool isOpen() const;

    private:
        struct Entry
        {
            std::string name;
            std::uint32_t crc;
            std::uint32_t size;
            std::uint32_t offset;
        };

        std::ofstream stream;
        std::vector<Entry> entries;
        bool opened;

        static std::uint32_t calculateCRC32(
            const std::vector<std::uint8_t>& data
        );

        static std::string normalizePath(
            const std::string& path
        );

        bool writeEntry(
            const std::string& archivePath,
            const std::vector<std::uint8_t>& data
        );

        void write16(std::uint16_t value);
        void write32(std::uint32_t value);
    };
}
