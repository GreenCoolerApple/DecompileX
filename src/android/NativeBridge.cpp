#include <jni.h>
#include <string>
#include <cstdint>
#include <vector>

namespace
{
    std::string detectFormat(const std::vector<unsigned char>& data)
    {
        if (data.size() >= 4)
        {
            if (data[0] == 0x7F &&
                data[1] == 'E' &&
                data[2] == 'L' &&
                data[3] == 'F')
                return "ELF Binary";

            if (data[0] == 'P' &&
                data[1] == 'K' &&
                data[2] == 0x03 &&
                data[3] == 0x04)
                return "ZIP / APK / JAR";

            if (data[0] == 0xFE &&
                data[1] == 0xED &&
                data[2] == 0xFA &&
                (data[3] == 0xCE || data[3] == 0xCF))
                return "Mach-O Binary";

            if (data[0] == 0xCA &&
                data[1] == 0xFE &&
                data[2] == 0xBA &&
                data[3] == 0xBE)
                return "Java Class / Universal Mach-O";
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
                return "Windows PE Binary";

            return "DOS MZ Binary";
        }

        return "Unknown Binary";
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_decompilex_app_MainActivity_getNativeVersion(
    JNIEnv* env,
    jobject
)
{
    return env->NewStringUTF("DecompileX 0.1.0 - C++17");
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_decompilex_app_MainActivity_analyzeBytes(
    JNIEnv* env,
    jobject,
    jbyteArray bytes
)
{
    if (bytes == nullptr)
        return env->NewStringUTF("No data received");

    jsize length = env->GetArrayLength(bytes);

    std::vector<unsigned char> data(
        static_cast<std::size_t>(length)
    );

    if (length > 0)
    {
        env->GetByteArrayRegion(
            bytes,
            0,
            length,
            reinterpret_cast<jbyte*>(data.data())
        );

        if (env->ExceptionCheck())
            return nullptr;
    }

    std::string result = detectFormat(data);

    return env->NewStringUTF(result.c_str());
}
