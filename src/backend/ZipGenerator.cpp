#include "ZipGenerator.hpp"

#include <algorithm>
#include <iterator>
#include <limits>
#include <system_error>

namespace fs = std::filesystem;

namespace DecompileX
{
    ZipGenerator::ZipGenerator()
        : opened(false)
    {
    }

    ZipGenerator::~ZipGenerator()
    {
        if (opened)
            close();
    }

    void ZipGenerator::write16(std::uint16_t value)
    {
        char bytes[2] = {
            static_cast<char>(value & 0xFF),
            static_cast<char>((value >> 8) & 0xFF)
        };

        stream.write(bytes, 2);
    }

    void ZipGenerator::write32(std::uint32_t value)
    {
        char bytes[4] = {
            static_cast<char>(value & 0xFF),
            static_cast<char>((value >> 8) & 0xFF),
            static_cast<char>((value >> 16) & 0xFF),
            static_cast<char>((value >> 24) & 0xFF)
        };

        stream.write(bytes, 4);
    }

    std::uint32_t ZipGenerator::calculateCRC32(
        const std::vector<std::uint8_t>& data
    )
    {
        std::uint32_t crc = 0xFFFFFFFF;

        for (std::uint8_t byte : data)
        {
            crc ^= byte;

            for (int i = 0; i < 8; ++i)
            {
                std::uint32_t mask = -(crc & 1u);
                crc = (crc >> 1) ^ (0xEDB88320u & mask);
            }
        }

        return ~crc;
    }

    std::string ZipGenerator::normalizePath(
        const std::string& path
    )
    {
        std::string result = path;

        std::replace(
            result.begin(),
            result.end(),
            '\\',
            '/'
        );

        while (!result.empty() && result.front() == '/')
            result.erase(result.begin());

        return result;
    }

    bool ZipGenerator::create(const fs::path& outputPath)
    {
        if (opened)
            return false;

        stream.open(
            outputPath,
            std::ios::binary | std::ios::trunc
        );

        if (!stream)
            return false;

        entries.clear();
        opened = true;

        return true;
    }

    bool ZipGenerator::writeEntry(
        const std::string& archivePath,
        const std::vector<std::uint8_t>& data
    )
    {
        if (!opened || !stream)
            return false;

        std::string name = normalizePath(archivePath);

        if (name.empty() ||
            name.size() > std::numeric_limits<std::uint16_t>::max() ||
            data.size() > std::numeric_limits<std::uint32_t>::max() ||
            entries.size() >= std::numeric_limits<std::uint16_t>::max())
            return false;

        for (const Entry& entry : entries)
        {
            if (entry.name == name)
                return false;
        }

        std::streampos position = stream.tellp();

        if (position < 0 ||
            static_cast<std::uint64_t>(position) >
            std::numeric_limits<std::uint32_t>::max())
            return false;

        std::uint32_t offset =
            static_cast<std::uint32_t>(position);

        std::uint32_t size =
            static_cast<std::uint32_t>(data.size());

        std::uint32_t crc = calculateCRC32(data);

        write32(0x04034B50);
        write16(20);
        write16(0);
        write16(0);
        write16(0);
        write16(0);
        write32(crc);
        write32(size);
        write32(size);
        write16(static_cast<std::uint16_t>(name.size()));
        write16(0);

        stream.write(
            name.data(),
            static_cast<std::streamsize>(name.size())
        );

        if (!data.empty())
        {
            stream.write(
                reinterpret_cast<const char*>(data.data()),
                static_cast<std::streamsize>(data.size())
            );
        }

        if (!stream)
            return false;

        entries.push_back({
            name,
            crc,
            size,
            offset
        });

        return true;
    }

    bool ZipGenerator::addFile(
        const fs::path& filePath,
        const std::string& archivePath
    )
    {
        if (!opened)
            return false;

        std::error_code ec;

        if (!fs::is_regular_file(filePath, ec) || ec)
            return false;

        std::uintmax_t size = fs::file_size(filePath, ec);

        if (ec ||
            size > std::numeric_limits<std::uint32_t>::max())
            return false;

        std::ifstream file(filePath, std::ios::binary);

        if (!file)
            return false;

        std::vector<std::uint8_t> data(
            static_cast<std::size_t>(size)
        );

        if (!data.empty())
        {
            file.read(
                reinterpret_cast<char*>(data.data()),
                static_cast<std::streamsize>(data.size())
            );

            if (!file)
                return false;
        }

        std::string name = archivePath.empty()
            ? filePath.filename().generic_string()
            : archivePath;

        return writeEntry(name, data);
    }

    bool ZipGenerator::addData(
        const std::string& archivePath,
        const std::vector<std::uint8_t>& data
    )
    {
        return writeEntry(archivePath, data);
    }

    bool ZipGenerator::addText(
        const std::string& archivePath,
        const std::string& text
    )
    {
        std::vector<std::uint8_t> data(
            text.begin(),
            text.end()
        );

        return writeEntry(archivePath, data);
    }

    bool ZipGenerator::addDirectory(
        const fs::path& directoryPath,
        const std::string& archivePrefix
    )
    {
        if (!opened)
            return false;

        std::error_code ec;

        if (!fs::is_directory(directoryPath, ec) || ec)
            return false;

        fs::recursive_directory_iterator iterator(
            directoryPath,
            fs::directory_options::skip_permission_denied,
            ec
        );

        if (ec)
            return false;

        fs::recursive_directory_iterator end;

        for (; iterator != end; iterator.increment(ec))
        {
            if (ec)
                return false;

            if (!iterator->is_regular_file(ec))
            {
                if (ec)
                    return false;

                continue;
            }

            fs::path relative = fs::relative(
                iterator->path(),
                directoryPath,
                ec
            );

            if (ec)
                return false;

            std::string name = relative.generic_string();

            if (!archivePrefix.empty())
                name = normalizePath(archivePrefix) + "/" + name;

            if (!addFile(iterator->path(), name))
                return false;
        }

        return true;
    }

    bool ZipGenerator::close()
    {
        if (!opened)
            return false;

        std::streampos centralOffsetPosition = stream.tellp();

        if (centralOffsetPosition < 0 ||
            static_cast<std::uint64_t>(centralOffsetPosition) >
            std::numeric_limits<std::uint32_t>::max())
            return false;

        std::uint32_t centralOffset =
            static_cast<std::uint32_t>(centralOffsetPosition);

        for (const Entry& entry : entries)
        {
            write32(0x02014B50);
            write16(20);
            write16(20);
            write16(0);
            write16(0);
            write16(0);
            write16(0);
            write32(entry.crc);
            write32(entry.size);
            write32(entry.size);
            write16(static_cast<std::uint16_t>(entry.name.size()));
            write16(0);
            write16(0);
            write16(0);
            write16(0);
            write32(0);
            write32(entry.offset);

            stream.write(
                entry.name.data(),
                static_cast<std::streamsize>(entry.name.size())
            );
        }

        std::streampos centralEndPosition = stream.tellp();

        if (centralEndPosition < 0)
            return false;

        std::uint64_t centralSize64 =
            static_cast<std::uint64_t>(centralEndPosition) -
            centralOffset;

        if (centralSize64 >
            std::numeric_limits<std::uint32_t>::max())
            return false;

        std::uint32_t centralSize =
            static_cast<std::uint32_t>(centralSize64);

        std::uint16_t count =
            static_cast<std::uint16_t>(entries.size());

        write32(0x06054B50);
        write16(0);
        write16(0);
        write16(count);
        write16(count);
        write32(centralSize);
        write32(centralOffset);
        write16(0);

        stream.flush();
        bool success = stream.good();
        stream.close();

        opened = false;
        entries.clear();

        return success;
    }

    bool ZipGenerator::isOpen() const
    {
        return opened;
    }
}
