#include "backend/PspAnalyzer.hpp"
#include "backend/ISOCompiler.hpp"
#include <filesystem>
#include <exception>
#include <jni.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::vector<std::uint8_t> bytes(JNIEnv* env, jbyteArray input) {
    if (!input) return {};
    jsize size = env->GetArrayLength(input);
    std::vector<std::uint8_t> out(static_cast<std::size_t>(size));
    if (size) env->GetByteArrayRegion(input, 0, size, reinterpret_cast<jbyte*>(out.data()));
    return out;
}

jstring result(JNIEnv* env, const std::string& s) {
    return env->NewStringUTF(s.c_str());
}

std::string hex(std::uint64_t value, int width = 0) {
    std::ostringstream s;
    s << std::uppercase << std::hex << std::setfill('0') << std::setw(width) << value;
    return s.str();
}

std::string kind(const std::vector<std::uint8_t>& b) {
    if (b.size() >= 2 && b[0] == 'M' && b[1] == 'Z') return "PE / DOS executable";
    if (b.size() >= 4 && b[0] == 0x7f && b[1] == 'E' && b[2] == 'L' && b[3] == 'F') return "ELF executable";
    if (b.size() >= 4 && b[0] == 'P' && b[1] == 'K' && b[2] == 3 && b[3] == 4) return "ZIP archive";
    if (b.size() >= 4 && ((b[0] == 0xFE && b[1] == 0xED && b[2] == 0xFA && (b[3] == 0xCE || b[3] == 0xCF)) || (b[0] == 0xCF && b[1] == 0xFA && b[2] == 0xED && b[3] == 0xFE) || (b[0] == 0xCE && b[1] == 0xFA && b[2] == 0xED && b[3] == 0xFE))) return "Mach-O executable";
    if (b.size() >= 8 && b[0] == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G') return "PNG image";
    return "Unknown / raw binary";
}

std::uint32_t crc32(const std::vector<std::uint8_t>& b) {
    std::uint32_t crc = 0xffffffffu;
    for (auto x : b) {
        crc ^= x;
        for (int i = 0; i < 8; ++i) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

int nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_decompilex_app_MainActivity_getNativeVersion(JNIEnv* env, jobject) {
    return result(env, "0.0.1");
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_decompilex_app_MainActivity_analyzeBytes(JNIEnv* env, jobject, jbyteArray input) {
    auto b = bytes(env, input);
    std::ostringstream s;
    s << "FORMAT  " << kind(b) << "\n"
      << "LOADED  " << b.size() << " bytes\n"
      << "CRC32   " << hex(crc32(b), 8) << "\n";
    if (b.size() >= 5 && b[0] == 0x7f && b[1] == 'E' && b[2] == 'L' && b[3] == 'F') {
        s << "ELF     " << (b[4] == 2 ? "64-bit" : b[4] == 1 ? "32-bit" : "unknown class") << "\n";
    }
    return result(env, s.str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_decompilex_app_MainActivity_hexDump(JNIEnv* env, jobject, jbyteArray input, jint offset, jint length) {
    auto b = bytes(env, input);
    std::size_t start = std::min(b.size(), static_cast<std::size_t>(std::max(0, offset)));
    std::size_t end = start + std::min(b.size() - start, static_cast<std::size_t>(std::max(0, std::min(length, 4096))));
    std::ostringstream s;
    for (std::size_t i = start; i < end; i += 16) {
        s << hex(i, 8) << "  ";
        for (std::size_t j = 0; j < 16; ++j) {
            if (i + j < end) s << hex(b[i + j], 2) << ' ';
            else s << "   ";
        }
        s << " |";
        for (std::size_t j = i; j < std::min(i + 16, end); ++j) s << (b[j] >= 32 && b[j] < 127 ? static_cast<char>(b[j]) : '.');
        s << "|\n";
    }
    return result(env, s.str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_decompilex_app_MainActivity_extractStrings(JNIEnv* env, jobject, jbyteArray input, jint minimum) {
    auto b = bytes(env, input);
    std::size_t min = static_cast<std::size_t>(std::max(3, std::min(minimum, 64)));
    std::ostringstream s;
    std::size_t count = 0;
    for (std::size_t i = 0; i < b.size() && count < 150; ) {
        if (b[i] < 32 || b[i] > 126) { ++i; continue; }
        std::size_t start = i;
        while (i < b.size() && b[i] >= 32 && b[i] <= 126) ++i;
        if (i - start >= min) {
            s << "0x" << hex(start, 8) << "  ";
            for (std::size_t j = start; j < std::min(i, start + 160); ++j) s << static_cast<char>(b[j]);
            if (i - start > 160) s << "...";
            s << '\n';
            ++count;
        }
    }
    if (!count) s << "No matching ASCII strings found.";
    if (count == 150) s << "\nLimited to 150 strings.";
    return result(env, s.str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_decompilex_app_MainActivity_entropy(JNIEnv* env, jobject, jbyteArray input) {
    auto b = bytes(env, input);
    if (b.empty()) return result(env, "No data loaded.");
    std::array<std::uint64_t, 256> freq{};
    for (auto x : b) ++freq[x];
    double entropy = 0;
    for (auto count : freq) {
        if (count) {
            double p = static_cast<double>(count) / static_cast<double>(b.size());
            entropy -= p * std::log2(p);
        }
    }
    std::ostringstream s;
    s << std::fixed << std::setprecision(5) << "Shannon entropy: " << entropy << " / 8 bits per byte\n"
      << "Sample size: " << b.size() << " bytes\n\nHigh entropy can indicate compressed or encrypted data, but is not conclusive.";
    return result(env, s.str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_decompilex_app_MainActivity_compareBytes(JNIEnv* env, jobject, jbyteArray first, jbyteArray second) {
    auto a = bytes(env, first), b = bytes(env, second);
    std::size_t common = std::min(a.size(), b.size());
    std::uint64_t differences = static_cast<std::uint64_t>(std::max(a.size(), b.size()) - common);
    std::ostringstream s;
    s << "FIRST   " << a.size() << " bytes\nSECOND  " << b.size() << " bytes\n";
    std::size_t shown = 0;
    for (std::size_t i = 0; i < common; ++i) {
        if (a[i] != b[i]) {
            ++differences;
            if (shown++ < 20) s << "0x" << hex(i, 8) << ": " << hex(a[i], 2) << " -> " << hex(b[i], 2) << '\n';
        }
    }
    s << "\nDifferent byte positions: " << differences;
    return result(env, s.str());
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_decompilex_app_MainActivity_hexToBytes(JNIEnv* env, jobject, jstring input) {
    if (!input) return nullptr;
    const char* raw = env->GetStringUTFChars(input, nullptr);
    if (!raw) return nullptr;
    std::string value(raw);
    env->ReleaseStringUTFChars(input, raw);
    std::vector<jbyte> out;
    int high = -1;
    for (std::size_t i = 0; i < value.size(); ++i) {
        char c = value[i];
        if (c == ' ' || c == '\n' || c == '\t' || c == ',' || c == ':' || c == '_') {
            if (high != -1) return nullptr;
            continue;
        }
        if (high == -1 && c == '0' && i + 1 < value.size() && (value[i + 1] == 'x' || value[i + 1] == 'X')) { ++i; continue; }
        int n = nibble(c);
        if (n < 0) return nullptr;
        if (high == -1) high = n;
        else { out.push_back(static_cast<jbyte>((high << 4) | n)); high = -1; }
        if (out.size() > 1024 * 1024) return nullptr;
    }
    if (high != -1) return nullptr;
    jbyteArray array = env->NewByteArray(static_cast<jsize>(out.size()));
    if (array && !out.empty()) env->SetByteArrayRegion(array, 0, static_cast<jsize>(out.size()), out.data());
    return array;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_decompilex_app_MainActivity_findHex(JNIEnv* env, jobject, jbyteArray input, jstring pattern) {
    if (!input || !pattern) return result(env, "Select a file and enter a hex pattern.");
    const char* raw = env->GetStringUTFChars(pattern, nullptr);
    if (!raw) return result(env, "Could not read pattern.");
    std::string text(raw);
    env->ReleaseStringUTFChars(pattern, raw);
    std::vector<std::uint8_t> needle;
    int high = -1;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char ch = text[i];
        if (ch == ' ' || ch == '\n' || ch == '\t' || ch == ',' || ch == ':') {
            if (high != -1) return result(env, "Incomplete hexadecimal byte.");
            continue;
        }
        if (high == -1 && ch == '0' && i + 1 < text.size() && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
            ++i;
            continue;
        }
        int value = nibble(ch);
        if (value < 0) return result(env, "Invalid hexadecimal character.");
        if (high < 0) high = value;
        else {
            needle.push_back(static_cast<std::uint8_t>((high << 4) | value));
            high = -1;
        }
        if (needle.size() > 4096) return result(env, "Pattern exceeds 4096 bytes.");
    }
    if (high != -1 || needle.empty()) return result(env, "Enter complete hexadecimal bytes.");
    auto data = bytes(env, input);
    std::ostringstream output;
    std::size_t total = 0;
    auto it = data.begin();
    while (it != data.end()) {
        auto found = std::search(it, data.end(), needle.begin(), needle.end());
        if (found == data.end()) break;
        std::size_t offset = static_cast<std::size_t>(found - data.begin());
        if (total < 100) output << "0x" << hex(offset, 8) << '\n';
        ++total;
        it = found + 1;
    }
    if (!total) return result(env, "No matches found.");
    output << "\nTotal matches: " << total;
    if (total > 100) output << " (first 100 shown)";
    return result(env, output.str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_decompilex_app_MainActivity_compileIsoNative(JNIEnv* env, jobject,
    jstring source, jstring destination, jstring label) {
    if (!source || !destination || !label) return env->NewStringUTF("Missing ISO arguments.");
    const char* s = env->GetStringUTFChars(source, nullptr);
    if (!s) return nullptr;
    const char* d = env->GetStringUTFChars(destination, nullptr);
    if (!d) { env->ReleaseStringUTFChars(source, s); return nullptr; }
    const char* l = env->GetStringUTFChars(label, nullptr);
    if (!l) { env->ReleaseStringUTFChars(destination, d); env->ReleaseStringUTFChars(source, s); return nullptr; }
    std::string src(s), dst(d), volume(l);
    env->ReleaseStringUTFChars(label, l);
    env->ReleaseStringUTFChars(destination, d);
    env->ReleaseStringUTFChars(source, s);
    try {
        DecompileX::ISOOptions options;
        options.volumeLabel = volume;
        options.overwrite = false;
        options.verifyOutput = true;
        auto r = DecompileX::ISOCompiler::compile(src, dst, options);
        std::string response = r.success
            ? "OK|ISO generated: " + std::to_string(r.bytesWritten) + " bytes, " +
              std::to_string(r.filesAdded) + " files, " + std::to_string(r.directoriesAdded) + " directories."
            : "ISO failed: " + r.message;
        return env->NewStringUTF(response.c_str());
    } catch (const std::exception& e) {
        std::string response = std::string("ISO exception: ") + e.what();
        return env->NewStringUTF(response.c_str());
    } catch (...) {
        return env->NewStringUTF("Unknown native ISO error.");
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_decompilex_app_MainActivity_analyzePspBytes(JNIEnv* env, jobject, jbyteArray input) {
    if (!input) return env->NewStringUTF("No PSP executable selected.");
    if (env->GetArrayLength(input) > 64 * 1024 * 1024)
        return env->NewStringUTF("PSP executable exceeds 64 MiB in-memory analysis limit.");
    try {
        auto report = DecompileX::PspAnalyzer::analyze(bytes(env, input), 160);
        return result(env, report);
    } catch (const std::exception& e) {
        return result(env, std::string("PSP analysis failed: ") + e.what());
    }
}
