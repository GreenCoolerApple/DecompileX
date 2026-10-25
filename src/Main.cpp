#include "backend/IsoCodeScanner.hpp"
#include "backend/ExeCompiler.hpp"
#include "backend/ZipGenerator.hpp"
#include "backend/ISOCompiler.hpp"
#include "backend/BinCompiler.hpp"
#include "backend/PspAnalyzer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <chrono>
#include <memory>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    struct BinaryInfo
    {
        fs::path path;
        std::uintmax_t size = 0;
        std::string format = "Unknown";
        std::string architecture = "Unknown";
        std::string details;
        std::vector<std::uint8_t> preview;
    };

    std::string trim(const std::string& value)
    {
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return {};
        const auto last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    std::string prompt(const std::string& label)
    {
        std::cout << label;
        std::string value;
        std::getline(std::cin, value);
        return trim(value);
    }

    std::optional<std::uint64_t> parseUnsigned(const std::string& value)
    {
        try
        {
            std::size_t used = 0;
            const auto number = std::stoull(value, &used, 0);
            if (used != value.size())
                return std::nullopt;
            return number;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    std::uint16_t read16(const std::vector<std::uint8_t>& data, std::size_t pos, bool bigEndian = false)
    {
        if (pos > data.size() || data.size() - pos < 2)
            return 0;
        if (bigEndian)
            return static_cast<std::uint16_t>((data[pos] << 8) | data[pos + 1]);
        return static_cast<std::uint16_t>(data[pos] | (data[pos + 1] << 8));
    }

    std::uint32_t read32(const std::vector<std::uint8_t>& data, std::size_t pos, bool bigEndian = false)
    {
        if (pos > data.size() || data.size() - pos < 4)
            return 0;
        if (bigEndian)
            return (static_cast<std::uint32_t>(data[pos]) << 24) |
                   (static_cast<std::uint32_t>(data[pos + 1]) << 16) |
                   (static_cast<std::uint32_t>(data[pos + 2]) << 8) |
                   static_cast<std::uint32_t>(data[pos + 3]);
        return static_cast<std::uint32_t>(data[pos]) |
               (static_cast<std::uint32_t>(data[pos + 1]) << 8) |
               (static_cast<std::uint32_t>(data[pos + 2]) << 16) |
               (static_cast<std::uint32_t>(data[pos + 3]) << 24);
    }

    std::string machineName(std::uint32_t machine)
    {
        switch (machine)
        {
            case 0x03: case 0x014c: return "x86";
            case 0x3e: case 0x8664: return "x86-64";
            case 0x28: case 0x01c0: case 0x01c4: return "ARM";
            case 0xb7: case 0xaa64: return "ARM64";
            case 0x08: return "MIPS";
            case 0xf3: return "RISC-V";
            default: return "Unknown";
        }
    }

    BinaryInfo inspect(const fs::path& path)
    {
        BinaryInfo info;
        info.path = path;
        std::error_code ec;
        if (!fs::is_regular_file(path, ec) || ec)
            throw std::runtime_error("File not found or not a regular file.");
        info.size = fs::file_size(path, ec);
        if (ec)
            throw std::runtime_error("Cannot read file size.");
        std::ifstream file(path, std::ios::binary);
        if (!file)
            throw std::runtime_error("Cannot open file.");
        const auto length = static_cast<std::size_t>(std::min<std::uintmax_t>(info.size, 4096));
        info.preview.resize(length);
        if (length && !file.read(reinterpret_cast<char*>(info.preview.data()), static_cast<std::streamsize>(length)))
            throw std::runtime_error("Cannot read file header.");
        const auto& b = info.preview;
        if (b.size() >= 4 && b[0] == 0x7f && b[1] == 'E' && b[2] == 'L' && b[3] == 'F')
        {
            info.format = "ELF";
            const bool big = b.size() > 5 && b[5] == 2;
            info.architecture = machineName(read16(b, 18, big));
            info.details = b.size() > 4 && b[4] == 2 ? "64-bit" : "32-bit";
        }
        else if (b.size() >= 2 && b[0] == 'M' && b[1] == 'Z')
        {
            info.format = "DOS / PE candidate";
            if (b.size() >= 0x40)
            {
                const auto pe = read32(b, 0x3c);
                if (pe <= b.size() && b.size() - pe >= 26 && read32(b, pe) == 0x00004550)
                {
                    info.format = "Windows PE";
                    info.architecture = machineName(read16(b, pe + 4));
                    const auto magic = read16(b, pe + 24);
                    info.details = magic == 0x20b ? "PE32+" : magic == 0x10b ? "PE32" : "Unknown optional header";
                }
                else
                {
                    info.details = "PE header not found in initial 4 KiB";
                }
            }
        }
        else if (b.size() >= 4 && b[0] == 'P' && b[1] == 'K' &&
                 ((b[2] == 3 && b[3] == 4) || (b[2] == 5 && b[3] == 6) || (b[2] == 7 && b[3] == 8)))
        {
            info.format = "ZIP / APK / JAR";
            info.details = "ZIP container";
        }
        else if (b.size() >= 4 && b[0] == 0xca && b[1] == 0xfe && b[2] == 0xba && b[3] == 0xbe)
        {
            info.format = "Java class / Mach-O universal";
            info.details = "Ambiguous magic; deeper parsing required";
        }
        else if (b.size() >= 4)
        {
            const auto magic = read32(b, 0);
            if (magic == 0xfeedface || magic == 0xfeedfacf || magic == 0xcefaedfe || magic == 0xcffaedfe)
            {
                info.format = "Mach-O";
                const bool big = magic == 0xcefaedfe || magic == 0xcffaedfe;
                const auto cpu = read32(b, 4, big);
                switch (cpu)
                {
                    case 7: info.architecture = "x86"; break;
                    case 0x01000007: info.architecture = "x86-64"; break;
                    case 12: info.architecture = "ARM"; break;
                    case 0x0100000c: info.architecture = "ARM64"; break;
                    default: break;
                }
                info.details = (magic == 0xfeedfacf || magic == 0xcffaedfe) ? "64-bit" : "32-bit";
            }
        }
        return info;
    }

    std::string describe(const BinaryInfo& info)
    {
        std::ostringstream out;
        out << "File: " << info.path.string() << '\n';
        out << "Size: " << info.size << " bytes\n";
        out << "Format: " << info.format << '\n';
        out << "Architecture: " << info.architecture << '\n';
        if (!info.details.empty())
            out << "Details: " << info.details << '\n';
        return out.str();
    }

    void hexDump(const fs::path& path, std::uint64_t offset, std::size_t count, std::ostream& out)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            throw std::runtime_error("Cannot open file.");
        std::error_code ec;
        const auto fileSize = fs::file_size(path, ec);
        if (ec || offset > fileSize)
            throw std::runtime_error("Offset is outside the file.");
        if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
            throw std::runtime_error("Offset is too large.");
        file.seekg(static_cast<std::streamoff>(offset));
        std::array<unsigned char, 16> buffer{};
        std::size_t remaining = count;
        while (remaining > 0 && file)
        {
            const auto take = static_cast<std::streamsize>(std::min<std::size_t>(remaining, buffer.size()));
            file.read(reinterpret_cast<char*>(buffer.data()), take);
            const auto got = file.gcount();
            if (got <= 0)
                break;
            out << std::hex << std::setfill('0') << std::setw(8) << offset << "  ";
            for (int i = 0; i < 16; ++i)
            {
                if (i < got)
                    out << std::setw(2) << static_cast<unsigned>(buffer[static_cast<std::size_t>(i)]) << ' ';
                else
                    out << "   ";
                if (i == 7)
                    out << ' ';
            }
            out << " |";
            for (std::streamsize i = 0; i < got; ++i)
            {
                const auto c = buffer[static_cast<std::size_t>(i)];
                out << (c >= 32 && c <= 126 ? static_cast<char>(c) : '.');
            }
            out << "|\n" << std::dec;
            offset += static_cast<std::uint64_t>(got);
            remaining -= static_cast<std::size_t>(got);
        }
    }

    bool hasEnvironment(const char* name)
    {
#if defined(_WIN32)
        char* value = nullptr;
        std::size_t size = 0;
        if (_dupenv_s(&value, &size, name) != 0) return false;
        const bool present = value != nullptr;
        std::free(value);
        return present;
#else
        return std::getenv(name) != nullptr;
#endif
    }

    bool useColor()
    {
        static const bool enabled = [] {
            if (hasEnvironment("NO_COLOR")) return false;
#if defined(_WIN32)
            return hasEnvironment("WT_SESSION") || hasEnvironment("ANSICON") || hasEnvironment("TERM");
#else
            const char* term = std::getenv("TERM");
            return term && std::string(term) != "dumb";
#endif
        }();
        return enabled;
    }

    std::string color(const char* code)
    {
        return useColor() ? std::string("\033[") + code + "m" : "";
    }

    void heading(const std::string& text)
    {
        std::cout << "\n" << color("1;92") << "  >> " << text
                  << color("0") << "\n" << std::string(54, '-') << "\n";
    }

    void printBanner()
    {
        std::cout << color("1;92")
                  << "\n  +============================================================+\n"
                  << "  |  D E C O M P I L E X                         v0.0.1     |\n"
                  << "  |  PSP ANALYSIS  /  BINARY LAB  /  ARCHIVE STUDIO        |\n"
                  << "  +============================================================+\n"
                  << color("0")
                  << "  C++17  |  Offline analysis  |  Windows / Linux / Android\n";
    }

    void printMenu(const std::optional<BinaryInfo>& current)
    {
        std::cout << "\n" << color("1;96") << "  WORKSPACE  " << color("0")
                  << (current ? current->path.filename().string() : "No file selected") << "\n"
                  << "  ------------------------------------------------------------\n"
                  << color("1;92") << "  [ BINARY FORENSICS ]" << color("0") << "\n"
                  << "   1  Open binary                 2  File information\n"
                  << "   3  Hex viewer                  4  Extract ASCII strings\n"
                  << "   5  Entropy analysis            6  Export analysis ZIP\n"
                  << "  13  CRC32 checksum             14  Compare two binaries\n"
                  << "  15  Search hexadecimal bytes\n\n"
                  << color("1;92") << "  [ ARCHIVE LAB ]" << color("0") << "\n"
                  << "   7  Create ZIP                  8  Create ISO 9660\n"
                  << "  17  Hex to BIN                  18  Text to BIN\n"
                  << "  19  Merge binary files          20  BIN to hex preview\n\n"
                  << color("1;92") << "  [ BUILD STUDIO ]" << color("0") << "\n"
                  << "   9  Compile C++                10  Detect compilers\n\n"
                  << color("1;92") << "  [ UTILITIES ]" << color("0") << "\n"
                  << "  11  Export hex dump            12  CLI reference\n"
                  << "  16  Workspace dashboard         0  Exit\n\n"
                  << color("1;92") << "  [ PSP GAME CODE ANALYZER ]" << color("0") << "\n"
                  << "  21  Analyze PSP executable     22  Save PSP assembly report\n"
                  << "  23  Scan extracted PSP folder\n\n";
    }

    std::uint32_t crc32File(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open file for checksum.");
        std::uint32_t crc = 0xffffffffu;
        std::array<char, 65536> buffer{};
        while (file)
        {
            file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const auto count = file.gcount();
            for (std::streamsize i = 0; i < count; ++i)
            {
                crc ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);
                for (int bit = 0; bit < 8; ++bit)
                    crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
            }
        }
        if (file.bad()) throw std::runtime_error("Failed reading file for checksum.");
        return ~crc;
    }

    void showCRC32(const fs::path& path)
    {
        std::cout << "  CRC32: " << std::uppercase << std::hex
                  << std::setw(8) << std::setfill('0') << crc32File(path)
                  << std::dec << std::nouppercase << std::setfill(' ') << "\n";
    }

    void compareFiles(const fs::path& first, const fs::path& second)
    {
        std::ifstream a(first, std::ios::binary), b(second, std::ios::binary);
        if (!a || !b) throw std::runtime_error("Cannot open one or both files.");
        std::array<char, 65536> x{}, y{};
        std::uint64_t offset = 0, differences = 0;
        std::vector<std::uint64_t> examples;
        while (a || b)
        {
            a.read(x.data(), static_cast<std::streamsize>(x.size()));
            b.read(y.data(), static_cast<std::streamsize>(y.size()));
            const auto na = a.gcount(), nb = b.gcount();
            const auto total = std::max(na, nb);
            if (total == 0) break;
            for (std::streamsize i = 0; i < total; ++i)
            {
                if (i >= na || i >= nb || x[static_cast<std::size_t>(i)] != y[static_cast<std::size_t>(i)])
                {
                    ++differences;
                    if (examples.size() < 16) examples.push_back(offset + static_cast<std::uint64_t>(i));
                }
            }
            offset += static_cast<std::uint64_t>(total);
        }
        if (a.bad() || b.bad()) throw std::runtime_error("Error comparing files.");
        std::cout << "  Compared bytes: " << offset << "\n  Different bytes: " << differences << "\n";
        for (auto pos : examples) std::cout << "    Difference at 0x" << std::hex << pos << std::dec << "\n";
        if (differences == 0) std::cout << "  Files are identical.\n";
    }

    std::vector<std::uint8_t> parseHexBytes(const std::string& input)
    {
        std::string hex;
        for (char c : input)
        {
            if (std::isspace(static_cast<unsigned char>(c))) continue;
            if (!std::isxdigit(static_cast<unsigned char>(c))) throw std::runtime_error("Invalid hex character.");
            hex.push_back(c);
        }
        if (hex.empty() || hex.size() % 2 != 0 || hex.size() > 512)
            throw std::runtime_error("Use 1 to 256 bytes, e.g. 4D 5A 90 00.");
        std::vector<std::uint8_t> bytes;
        for (std::size_t i = 0; i < hex.size(); i += 2)
            bytes.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
        return bytes;
    }

    void searchBytes(const fs::path& path, const std::vector<std::uint8_t>& pattern)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open binary.");
        std::vector<std::size_t> prefix(pattern.size(), 0);
        for (std::size_t i = 1, j = 0; i < pattern.size(); ++i)
        {
            while (j && pattern[i] != pattern[j]) j = prefix[j - 1];
            if (pattern[i] == pattern[j]) ++j;
            prefix[i] = j;
        }
        std::uint64_t offset = 0, found = 0;
        std::size_t j = 0;
        char ch;
        while (file.get(ch))
        {
            const auto byte = static_cast<std::uint8_t>(static_cast<unsigned char>(ch));
            while (j && byte != pattern[j]) j = prefix[j - 1];
            if (byte == pattern[j]) ++j;
            if (j == pattern.size())
            {
                if (found < 32) std::cout << "  Match at 0x" << std::hex << (offset + 1 - pattern.size()) << std::dec << "\n";
                ++found;
                j = prefix[j - 1];
            }
            ++offset;
        }
        if (file.bad()) throw std::runtime_error("Error searching binary.");
        std::cout << "  Total matches: " << found << " (showing up to 32)\n";
    }

    std::string extractStrings(const fs::path& path, std::size_t minLength, std::size_t maxResults)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open binary.");
        std::ostringstream out;
        std::string current;
        std::uint64_t position = 0;
        std::uint64_t start = 0;
        std::size_t found = 0;
        char c;
        while (file.get(c))
        {
            const auto b = static_cast<unsigned char>(c);
            if (b >= 32 && b <= 126)
            {
                if (current.empty()) start = position;
                if (current.size() < 2048) current.push_back(c);
            }
            else
            {
                if (current.size() >= minLength)
                {
                    out << "0x" << std::hex << start << std::dec << "  " << current << '\n';
                    if (++found >= maxResults) break;
                }
                current.clear();
            }
            ++position;
        }
        if (found < maxResults && current.size() >= minLength)
            out << "0x" << std::hex << start << std::dec << "  " << current << '\n';
        return out.str();
    }

    void showEntropy(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open binary.");
        std::array<std::uint64_t, 256> frequency{};
        std::array<char, 65536> buffer{};
        std::uint64_t total = 0;
        while (file)
        {
            file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const auto read = file.gcount();
            for (std::streamsize i = 0; i < read; ++i)
                ++frequency[static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)])];
            total += static_cast<std::uint64_t>(read);
        }
        if (total == 0) { std::cout << "Empty file.\n"; return; }
        double entropy = 0;
        for (auto count : frequency)
        {
            if (!count) continue;
            const double probability = static_cast<double>(count) / static_cast<double>(total);
            entropy -= probability * std::log2(probability);
        }
        std::cout << std::fixed << std::setprecision(4)
                  << "  Shannon entropy: " << entropy << " / 8.0000 bits per byte\n"
                  << "  Analyzed bytes:  " << total << "\n"
                  << "  High entropy may indicate compression or encryption; it is not proof.\n";
    }

    void createISO()
    {
        const auto source = prompt("Source directory: ");
        const auto target = prompt("Output ISO path: ");
        const auto volume = prompt("Volume label [DECOMPILEX]: ");
        if (source.empty() || target.empty())
            throw std::runtime_error("Source directory and ISO output are required.");
        const auto result = DecompileX::ISOCompiler::compile(
            source, target, volume.empty() ? "DECOMPILEX" : volume);
        if (!result.success) throw std::runtime_error(result.message);
        std::cout << "  ISO created: " << target << "\n"
                  << "  Files: " << result.filesAdded << " | Bytes: " << result.bytesWritten << "\n";
    }

    void saveHex(const BinaryInfo& info)
    {
        const auto path = prompt("Output TXT path [hex-dump.txt]: ");
        const auto countText = prompt("Bytes [4096, max 1048576]: ");
        const auto count = countText.empty() ? std::optional<std::uint64_t>(4096) : parseUnsigned(countText);
        if (!count || *count > 1048576) throw std::runtime_error("Invalid byte count.");
        std::ofstream output(path.empty() ? "hex-dump.txt" : path);
        if (!output) throw std::runtime_error("Cannot create output file.");
        hexDump(info.path, 0, static_cast<std::size_t>(*count), output);
        if (!output) throw std::runtime_error("Failed to write hex dump.");
        std::cout << "  Hex dump saved.\n";
    }

    void showHelp()
    {
        std::cout << "  DecompileX --help\n"
                  << "  DecompileX --info <file>\n"
                  << "  DecompileX --hex <file> [offset] [count]\n"
                  << "  DecompileX --strings <file> [min-length]\n"
                  << "  DecompileX --entropy <file>\n"
                  << "  DecompileX --iso <directory> <output.iso> [volume]\n"
                  << "  DecompileX --zip <directory> <output.zip>\n"
                  << "  DecompileX --crc32 <file>\n"
                  << "  DecompileX --compare <file-a> <file-b>\n"
                  << "  DecompileX --findhex <file> <hex-pattern>\n"
                  << "  DecompileX --bin-hex <hex> <output.bin>\n"
                  << "  DecompileX --bin-text <text> <output.bin>\n"
                  << "  DecompileX --bin-merge <output.bin> <input1> <input2> [...]\n"
                  << "  DecompileX --bin-view <file> [max-bytes]\n"
                  << "  DecompileX --psp-analyze <EBOOT.PBP|EBOOT.BIN|ELF|PRX> [instructions]\n"
                  << "  DecompileX --psp-report <file> <report.txt> [instructions]\n"
                  << "  DecompileX --psp-scan <extracted-directory>\n"
                  << "  DecompileX --iso-code-scan <game.iso>\n"
                  << "  PSP analysis reads at most 64 MiB per executable.\n"
                  << "  ISO output must be outside its source directory.\n";
    }

    void exportAnalysis(const BinaryInfo& info)
    {
        const auto value = prompt("Output ZIP path [DecompileX-Report.zip]: ");
        const fs::path output = value.empty() ? fs::path("DecompileX-Report.zip") : fs::path(value);
        std::ostringstream dump;
        hexDump(info.path, 0, 512, dump);
        DecompileX::ZipGenerator zip;
        if (!zip.create(output))
            throw std::runtime_error("Cannot create ZIP file.");
        const bool addedReport = zip.addText("report/info.txt", describe(info));
        const bool addedHex = addedReport && zip.addText("report/hex-dump.txt", dump.str());
        const bool closed = zip.close();
        if (!addedReport || !addedHex || !closed)
            throw std::runtime_error("ZIP export failed or is incomplete.");
        std::cout << "Exported: " << output.string() << '\n';
    }

    void createDirectoryZip()
    {
        const auto source = prompt("Directory to archive: ");
        const auto target = prompt("Output ZIP path: ");
        if (source.empty() || target.empty())
            throw std::runtime_error("Directory and output are required.");
        DecompileX::ZipGenerator zip;
        if (!zip.create(target))
            throw std::runtime_error("Cannot create ZIP file.");
        const bool added = zip.addDirectory(source);
        const bool closed = zip.close();
        if (!added || !closed)
            throw std::runtime_error("Directory ZIP failed or is incomplete.");
        std::cout << "Created: " << target << '\n';
    }

    void showCompilers()
    {
        const std::array<DecompileX::CompilerType, 3> types = {
            DecompileX::CompilerType::MSVC,
            DecompileX::CompilerType::MinGW,
            DecompileX::CompilerType::Clang
        };
        for (auto type : types)
        {
            std::cout << "  " << DecompileX::ExeCompiler::getCompilerName(type)
                      << ": " << (DecompileX::ExeCompiler::isCompilerAvailable(type) ? "Available" : "Unavailable") << '\n';
        }
    }

    void compileCpp()
    {
        DecompileX::CompilerOptions options;
        std::cout << "Enter C++ source paths, one per line. Empty line to finish.\n";
        while (true)
        {
            const auto source = prompt("Source: ");
            if (source.empty())
                break;
            options.sources.emplace_back(source);
        }
        if (options.sources.empty())
            throw std::runtime_error("No source files selected.");
        const auto output = prompt("Output executable path: ");
        if (output.empty())
            throw std::runtime_error("Output path is required.");
        options.output = output;
        const auto choice = prompt("Compiler [auto/msvc/mingw/clang]: ");
        if (choice == "msvc")
            options.compiler = DecompileX::CompilerType::MSVC;
        else if (choice == "mingw")
            options.compiler = DecompileX::CompilerType::MinGW;
        else if (choice == "clang")
            options.compiler = DecompileX::CompilerType::Clang;
        else if (!choice.empty() && choice != "auto")
            throw std::runtime_error("Unknown compiler selection.");
        const auto standard = prompt("C++ standard [c++17/c++20] (default c++17): ");
        if (!standard.empty())
        {
            if (standard != "c++17" && standard != "c++20")
                throw std::runtime_error("Unsupported C++ standard.");
            options.standard = standard;
        }
        std::cout << "Compiling...\n";
        const auto result = DecompileX::ExeCompiler::compile(options);
        std::cout << result.message << '\n';
        if (!result.success)
            std::cout << "Compiler exit code: " << result.exitCode << '\n';
        else
            std::cout << "Output: " << result.output.string() << '\n';
    }

    void printBinResult(const DecompileX::BinResult& result)
    {
        if (!result.success)
            throw std::runtime_error(result.message);
        std::cout << "  [OK] " << result.message << "\n"
                  << "  Output: " << result.output.string() << "\n"
                  << "  Bytes written: " << result.bytesWritten << "\n";
    }

    void createBinHex()
    {
        const auto hex = prompt("Hexadecimal bytes: ");
        const auto output = prompt("Output .bin path: ");
        if (output.empty()) throw std::runtime_error("Output path required.");
        printBinResult(DecompileX::BinCompiler::compileHex(hex, output));
    }

    void createBinText()
    {
        const auto value = prompt("Text to encode as bytes: ");
        const auto output = prompt("Output .bin path: ");
        if (output.empty()) throw std::runtime_error("Output path required.");
        printBinResult(DecompileX::BinCompiler::compileText(value, output));
    }

    void mergeBinaries()
    {
        const auto output = prompt("Output .bin path: ");
        if (output.empty()) throw std::runtime_error("Output path required.");
        std::vector<fs::path> inputs;
        std::cout << "Enter input paths in order; empty line to finish.\n";
        while (true)
        {
            const auto path = prompt("Input: ");
            if (path.empty()) break;
            inputs.emplace_back(path);
        }
        if (inputs.empty()) throw std::runtime_error("No input files provided.");
        printBinResult(DecompileX::BinCompiler::merge(inputs, output));
    }

    void previewBin(const fs::path& path, std::size_t maxBytes)
    {
        if (maxBytes == 0 || maxBytes > 4096)
            throw std::runtime_error("Byte count must be between 1 and 4096.");
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot open input file.");
        std::vector<std::uint8_t> bytes(maxBytes);
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (input.bad()) throw std::runtime_error("Failed to read input file.");
        bytes.resize(static_cast<std::size_t>(input.gcount()));
        std::cout << "  Bytes read: " << bytes.size() << "\n"
                  << DecompileX::BinCompiler::toHex(bytes) << "\n";
    }

    constexpr std::uintmax_t PSP_MAX_BYTES = 64ull * 1024ull * 1024ull;

    std::vector<std::uint8_t> readPspBinary(const fs::path& path)
    {
        std::error_code ec;
        if (!fs::is_regular_file(path, ec) || ec)
            throw std::runtime_error("PSP input is not a regular file.");
        const auto size = fs::file_size(path, ec);
        if (ec || size > PSP_MAX_BYTES)
            throw std::runtime_error("PSP input exceeds 64 MiB or cannot be measured.");
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot open PSP executable.");
        std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
        if (!data.empty())
        {
            input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
            if (!input) throw std::runtime_error("Failed to read PSP executable.");
        }
        return data;
    }

    std::size_t pspInstructionLimit(const std::string& input)
    {
        if (input.empty()) return 160;
        const auto count = parseUnsigned(input);
        if (!count || *count < 1 || *count > 10000)
            throw std::runtime_error("Instruction limit must be 1 to 10000.");
        return static_cast<std::size_t>(*count);
    }

    std::string pspAnalysis(const fs::path& path, std::size_t instructions)
    {
        const auto data = readPspBinary(path);
        std::ostringstream out;
        out << "DecompileX PSP Game Code Analyzer v0.0.1\n"
            << "Input: " << path.string() << "\n"
            << "Bytes: " << data.size() << "\n"
            << "----------------------------------------\n"
            << DecompileX::PspAnalyzer::analyze(data, instructions) << '\n';
        return out.str();
    }

    void savePspReport(const fs::path& input, const fs::path& output, std::size_t instructions)
    {
        std::error_code ec;
        if (fs::exists(output, ec) && !ec && fs::equivalent(input, output, ec) && !ec)
            throw std::runtime_error("Report cannot overwrite its input.");
        const auto report = pspAnalysis(input, instructions);
        std::ofstream file(output, std::ios::binary | std::ios::trunc);
        if (!file) throw std::runtime_error("Cannot create report file.");
        file << report;
        file.close();
        if (!file) throw std::runtime_error("Failed to write report.");
        std::cout << "PSP report saved: " << output.string() << '\n';
    }

    void scanPspFolder(const fs::path& root)
    {
        std::error_code ec;
        if (!fs::is_directory(root, ec) || ec)
            throw std::runtime_error("PSP scan directory not found.");
        std::size_t examined = 0, recognized = 0;
        fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
        if (ec) throw std::runtime_error("Cannot enumerate directory.");
        while (it != end && examined < 10000)
        {
            const auto entry = *it;
            std::error_code statusError;
            if (entry.is_symlink(statusError))
            {
                it.disable_recursion_pending();
            }
            else if (entry.is_regular_file(statusError))
            {
                ++examined;
                const auto size = entry.file_size(statusError);
                if (!statusError && size >= 4 && size <= PSP_MAX_BYTES)
                {
                    std::ifstream file(entry.path(), std::ios::binary);
                    char header[4]{};
                    if (file.read(header, 4))
                    {
                        const bool elf = header[0] == '\x7f' && header[1] == 'E' && header[2] == 'L' && header[3] == 'F';
                        const bool pbp = header[0] == '\0' && header[1] == 'P' && header[2] == 'B' && header[3] == 'P';
                        const bool psp = header[0] == '~' && header[1] == 'P' && header[2] == 'S' && header[3] == 'P';
                        if (elf || pbp || psp)
                        {
                            ++recognized;
                            std::cout << (elf ? "ELF " : pbp ? "PBP " : "PSP ") << entry.path().string() << '\n';
                        }
                    }
                }
            }
            it.increment(ec);
            if (ec) { ec.clear(); break; }
        }
        std::cout << "Files inspected: " << examined << ", PSP candidates: " << recognized << '\n';
        if (examined >= 10000) std::cout << "Scan stopped at 10000 files.\n";
    }

    int runInteractive()
    {
        std::optional<BinaryInfo> current;
        printBanner();
        while (std::cin)
        {
            printMenu(current);
            const auto choice = prompt("DecompileX > ");
            if (!std::cin || choice == "0" || choice == "exit" || choice == "quit") break;
            try
            {
                if (choice == "1")
                {
                    const auto path = prompt("Binary path: ");
                    if (path.empty()) continue;
                    current = inspect(path);
                    heading("BINARY LOADED");
                    std::cout << describe(*current);
                }
                else if (choice == "2")
                {
                    if (!current) throw std::runtime_error("Open a binary first.");
                    heading("FILE INFORMATION");
                    current = inspect(current->path);
                    std::cout << describe(*current);
                }
                else if (choice == "3")
                {
                    if (!current) throw std::runtime_error("Open a binary first.");
                    const auto offsetText = prompt("Offset [0]: ");
                    const auto countText = prompt("Bytes [256, max 4096]: ");
                    const auto offset = offsetText.empty() ? std::optional<std::uint64_t>(0) : parseUnsigned(offsetText);
                    const auto count = countText.empty() ? std::optional<std::uint64_t>(256) : parseUnsigned(countText);
                    if (!offset || !count || *count > 4096) throw std::runtime_error("Invalid offset or byte count.");
                    heading("HEX VIEWER");
                    hexDump(current->path, *offset, static_cast<std::size_t>(*count), std::cout);
                }
                else if (choice == "4")
                {
                    if (!current) throw std::runtime_error("Open a binary first.");
                    const auto length = prompt("Minimum string length [5]: ");
                    const auto value = length.empty() ? std::optional<std::uint64_t>(5) : parseUnsigned(length);
                    if (!value || *value < 3 || *value > 1024) throw std::runtime_error("Invalid string length.");
                    heading("ASCII STRINGS (FIRST 200)");
                    std::cout << extractStrings(current->path, static_cast<std::size_t>(*value), 200);
                }
                else if (choice == "5")
                {
                    if (!current) throw std::runtime_error("Open a binary first.");
                    heading("ENTROPY ANALYSIS");
                    showEntropy(current->path);
                }
                else if (choice == "6")
                {
                    if (!current) throw std::runtime_error("Open a binary first.");
                    heading("EXPORT ANALYSIS");
                    exportAnalysis(*current);
                }
                else if (choice == "7") { heading("ZIP GENERATOR"); createDirectoryZip(); }
                else if (choice == "8") { heading("ISO 9660 GENERATOR"); createISO(); }
                else if (choice == "9") { heading("C++ COMPILER"); compileCpp(); }
                else if (choice == "10") { heading("AVAILABLE COMPILERS"); showCompilers(); }
                else if (choice == "11")
                {
                    if (!current) throw std::runtime_error("Open a binary first.");
                    heading("EXPORT HEX DUMP");
                    saveHex(*current);
                }
                else if (choice == "12") { heading("COMMAND LINE HELP"); showHelp(); }
                else if (choice == "13")
                {
                    if (!current) throw std::runtime_error("Open a binary first.");
                    heading("CRC32 CHECKSUM"); showCRC32(current->path);
                }
                else if (choice == "14")
                {
                    const auto first = current ? current->path.string() : prompt("First binary: ");
                    const auto second = prompt("Second binary: ");
                    heading("BINARY COMPARISON"); compareFiles(first, second);
                }
                else if (choice == "15")
                {
                    if (!current) throw std::runtime_error("Open a binary first.");
                    const auto pattern = parseHexBytes(prompt("Hex pattern (e.g. 4D 5A): "));
                    heading("HEX PATTERN SEARCH"); searchBytes(current->path, pattern);
                }
                else if (choice == "17") { heading("HEX TO BINARY"); createBinHex(); }
                else if (choice == "18") { heading("TEXT TO BINARY"); createBinText(); }
                else if (choice == "19") { heading("MERGE BINARIES"); mergeBinaries(); }
                else if (choice == "20")
                {
                    heading("BINARY HEX PREVIEW");
                    const auto file = prompt("Input file: ");
                    const auto count = prompt("Max bytes [256]: ");
                    const auto parsed = count.empty() ? std::optional<std::uint64_t>(256) : parseUnsigned(count);
                    if (!parsed || *parsed == 0 || *parsed > 4096) throw std::runtime_error("Invalid byte count.");
                    previewBin(file, static_cast<std::size_t>(*parsed));
                }
                else if (choice == "21")
                {
                    heading("PSP EXECUTABLE ANALYZER");
                    const auto path = prompt("PSP EBOOT.PBP / EBOOT.BIN / ELF / PRX path: ");
                    const auto limit = pspInstructionLimit(prompt("Instructions [160, max 10000]: "));
                    std::cout << pspAnalysis(path, limit);
                }
                else if (choice == "22")
                {
                    heading("EXPORT PSP ASSEMBLY REPORT");
                    const auto path = prompt("PSP executable path: ");
                    const auto output = prompt("Output TXT path: ");
                    if (path.empty() || output.empty()) throw std::runtime_error("Both paths are required.");
                    savePspReport(path, output, pspInstructionLimit(prompt("Instructions [160]: ")));
                }
                else if (choice == "23")
                {
                    heading("SCAN EXTRACTED PSP GAME FOLDER");
                    scanPspFolder(prompt("Extracted game directory: "));
                }
                else if (choice == "16")
                {
                    heading("WORKSPACE DASHBOARD");
                    if (current) { std::cout << describe(*current); showCRC32(current->path); }
                    else std::cout << "  No binary selected.\n";
                    showCompilers();
                }
                else std::cout << "Unknown option.\n";
            }
            catch (const std::exception& error)
            {
                std::cout << color("1;91") << "  [ERROR] " << error.what() << color("0") << '\n';
            }
        }
        std::cout << "Goodbye!\n";
        return 0;
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc < 2) return runInteractive();
        const std::string command = argv[1];
        if (command == "--iso-code-scan" && argc == 3)
        {
            const auto report = DecompileX::IsoCodeScanner::scan(argv[2]);
            std::cout << report.formatText();
            return report.success ? 0 : 1;
        }
        if (command == "--psp-analyze" && (argc == 3 || argc == 4))
        {
            std::cout << pspAnalysis(argv[2], argc == 4 ? pspInstructionLimit(argv[3]) : 160);
            return 0;
        }
        if (command == "--psp-report" && (argc == 4 || argc == 5))
        {
            savePspReport(argv[2], argv[3], argc == 5 ? pspInstructionLimit(argv[4]) : 160);
            return 0;
        }
        if (command == "--psp-scan" && argc == 3)
        {
            scanPspFolder(argv[2]);
            return 0;
        }
        if (command == "--help" || command == "-h") { showHelp(); return 0; }
        if (command == "--crc32" && argc == 3) { showCRC32(argv[2]); return 0; }
        if (command == "--compare" && argc == 4) { compareFiles(argv[2], argv[3]); return 0; }
        if (command == "--findhex" && argc == 4) { searchBytes(argv[2], parseHexBytes(argv[3])); return 0; }
        if (command == "--info" && argc == 3)
        {
            std::cout << describe(inspect(argv[2]));
            return 0;
        }
        if (command == "--hex" && argc >= 3 && argc <= 5)
        {
            const auto offset = argc >= 4 ? parseUnsigned(argv[3]) : std::optional<std::uint64_t>(0);
            const auto count = argc >= 5 ? parseUnsigned(argv[4]) : std::optional<std::uint64_t>(256);
            if (!offset || !count || *count > 4096) throw std::runtime_error("Invalid offset or count.");
            hexDump(argv[2], *offset, static_cast<std::size_t>(*count), std::cout);
            return 0;
        }
        if (command == "--strings" && argc >= 3 && argc <= 4)
        {
            const auto minLength = argc == 4 ? parseUnsigned(argv[3]) : std::optional<std::uint64_t>(5);
            if (!minLength || *minLength < 3 || *minLength > 1024) throw std::runtime_error("Invalid minimum string length.");
            std::cout << extractStrings(argv[2], static_cast<std::size_t>(*minLength), 500);
            return 0;
        }
        if (command == "--entropy" && argc == 3) { showEntropy(argv[2]); return 0; }
        if (command == "--bin-hex" && argc == 4)
        {
            printBinResult(DecompileX::BinCompiler::compileHex(argv[2], argv[3]));
            return 0;
        }
        if (command == "--bin-text" && argc == 4)
        {
            printBinResult(DecompileX::BinCompiler::compileText(argv[2], argv[3]));
            return 0;
        }
        if (command == "--bin-merge" && argc >= 5)
        {
            std::vector<fs::path> inputs;
            for (int i = 3; i < argc; ++i) inputs.emplace_back(argv[i]);
            printBinResult(DecompileX::BinCompiler::merge(inputs, argv[2]));
            return 0;
        }
        if (command == "--bin-view" && argc >= 3 && argc <= 4)
        {
            const auto count = argc == 4 ? parseUnsigned(argv[3]) : std::optional<std::uint64_t>(256);
            if (!count || *count == 0 || *count > 4096) throw std::runtime_error("Invalid byte count.");
            previewBin(argv[2], static_cast<std::size_t>(*count));
            return 0;
        }
        if (command == "--iso" && argc >= 4 && argc <= 5)
        {
            const auto result = DecompileX::ISOCompiler::compile(argv[2], argv[3], argc == 5 ? argv[4] : "DECOMPILEX");
            if (!result.success) throw std::runtime_error(result.message);
            std::cout << "ISO created: " << argv[3] << " (" << result.filesAdded << " files)\n";
            return 0;
        }
        if (command == "--zip" && argc == 4)
        {
            DecompileX::ZipGenerator zip;
            if (!zip.create(argv[3])) throw std::runtime_error("Cannot create ZIP.");
            const bool added = zip.addDirectory(argv[2]);
            const bool closed = zip.close();
            if (!added || !closed) throw std::runtime_error("ZIP generation failed.");
            std::cout << "ZIP created: " << argv[3] << '\n';
            return 0;
        }
        std::cerr << "Invalid arguments. Use --help.\n";
        return 2;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
