#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace DecompileX
{
    struct Tool
    {
        std::string name;
        std::string command;
        bool available = false;
    };

    class Setup
    {
    public:
        explicit Setup(fs::path root)
            : projectRoot(std::move(root))
        {
        }

        int run()
        {
            printBanner();

            while (true)
            {
                printMenu();

                std::string input;
                std::getline(std::cin, input);

                if (!std::cin)
                    return 0;

                if (input == "1")
                    checkEnvironment();
                else if (input == "2")
                    createDirectories();
                else if (input == "3")
                    generateConfiguration();
                else if (input == "4")
                    showProjectInformation();
                else if (input == "5")
                    fullSetup();
                else if (input == "0")
                    return 0;
                else
                    std::cout << "\nInvalid option.\n";
            }
        }

    private:
        fs::path projectRoot;

        static void printBanner()
        {
            std::cout
                << "\n"
                << "============================================\n"
                << "           DecompileX Setup\n"
                << "============================================\n"
                << "       Development Environment Tool\n"
                << "============================================\n";
        }

        static void printMenu()
        {
            std::cout
                << "\n[1] Check development environment\n"
                << "[2] Create project directories\n"
                << "[3] Generate configuration\n"
                << "[4] Show project information\n"
                << "[5] Run full setup\n"
                << "[0] Exit\n"
                << "\nSetup> ";
        }

        static bool commandExists(const std::string& command)
        {
#if defined(_WIN32)
            const std::string query =
                "where " + command + " >NUL 2>&1";
#else
            const std::string query =
                "command -v " + command + " >/dev/null 2>&1";
#endif

            return std::system(query.c_str()) == 0;
        }

        static std::vector<Tool> detectTools()
        {
            std::vector<Tool> tools = {
                {"CMake", "cmake"},
                {"Ninja", "ninja"},
                {"Git", "git"},
                {"GCC", "g++"},
                {"Clang", "clang++"},
                {"Java", "java"},
                {"Gradle", "gradle"}
            };

#if defined(_WIN32)
            tools.push_back({"MSVC", "cl"});
#endif

            for (auto& tool : tools)
                tool.available = commandExists(tool.command);

            return tools;
        }

        bool checkEnvironment() const
        {
            std::cout << "\nChecking development tools...\n\n";

            const auto tools = detectTools();

            for (const auto& tool : tools)
            {
                std::cout
                    << (tool.available ? "[OK] " : "[--] ")
                    << tool.name
                    << '\n';
            }

            const bool cmake = commandExists("cmake");
            const bool compiler =
                commandExists("g++") ||
                commandExists("clang++")
#if defined(_WIN32)
                || commandExists("cl")
#endif
                ;

            std::cout << "\n";

            if (cmake && compiler)
            {
                std::cout
                    << "Basic C++ build environment is available.\n";

                return true;
            }

            std::cout
                << "Some required development tools are missing.\n";

            return false;
        }

        bool createDirectories() const
        {
            const std::vector<fs::path> directories = {
                "build",
                "dist",
                "output",
                "output/reports",
                "output/exports",
                "output/compiled",
                "temp"
            };

            bool success = true;

            std::cout << "\nPreparing directories...\n\n";

            for (const auto& directory : directories)
            {
                const fs::path target = projectRoot / directory;
                std::error_code ec;

                fs::create_directories(target, ec);

                if (ec)
                {
                    std::cout
                        << "[ERROR] "
                        << target.string()
                        << ": "
                        << ec.message()
                        << '\n';

                    success = false;
                }
                else
                {
                    std::cout
                        << "[OK] "
                        << target.string()
                        << '\n';
                }
            }

            return success;
        }

        bool generateConfiguration() const
        {
            const fs::path configPath =
                projectRoot / "decompilex.ini";

            if (fs::exists(configPath))
            {
                std::cout
                    << "\nConfiguration already exists: "
                    << configPath.string()
                    << "\nExisting settings were preserved.\n";

                return true;
            }

            std::ofstream file(configPath, std::ios::binary);

            if (!file)
            {
                std::cout
                    << "\nFailed to create configuration.\n";

                return false;
            }

            file
                << "[Application]\n"
                << "name=DecompileX\n"
                << "version=0.2.0\n"
                << "language=en\n"
                << "\n"
                << "[Analysis]\n"
                << "max_file_size_mb=256\n"
                << "hex_dump_bytes=512\n"
                << "detect_architecture=true\n"
                << "\n"
                << "[Export]\n"
                << "directory=output/exports\n"
                << "reports=output/reports\n"
                << "zip_enabled=true\n"
                << "\n"
                << "[Compiler]\n"
                << "standard=c++17\n"
                << "optimization=true\n"
                << "output=output/compiled\n";

            file.flush();

            if (!file)
            {
                std::cout
                    << "\nFailed to write configuration.\n";

                return false;
            }

            std::cout
                << "\nConfiguration created: "
                << configPath.string()
                << '\n';

            return true;
        }

        void showProjectInformation() const
        {
            std::cout
                << "\nProject: DecompileX\n"
                << "Version: 0.2.0\n"
                << "Language: C++17\n"
                << "Root: " << projectRoot.string() << '\n';

            const std::vector<fs::path> files = {
                "CMakeLists.txt",
                "src/Main.cpp",
                "src/backend/ZipGenerator.cpp",
                "src/backend/ExeCompiler.cpp",
                "tools/Setup.cpp",
                "android/app/build.gradle"
            };

            std::cout << "\nProject files:\n";

            for (const auto& path : files)
            {
                std::cout
                    << (fs::exists(projectRoot / path)
                        ? "[OK] "
                        : "[--] ")
                    << path.generic_string()
                    << '\n';
            }
        }

        void fullSetup() const
        {
            std::cout << "\nStarting full setup...\n";

            const bool directories = createDirectories();
            const bool configuration =
                directories && generateConfiguration();
            const bool environment = checkEnvironment();

            std::cout << "\nSetup summary:\n"
                      << "Directories: "
                      << (directories ? "OK" : "FAILED") << '\n'
                      << "Configuration: "
                      << (configuration ? "OK" : "FAILED") << '\n'
                      << "Build environment: "
                      << (environment ? "AVAILABLE" : "INCOMPLETE")
                      << '\n';

            if (directories && configuration && environment)
                std::cout << "\nDecompileX setup completed!\n";
            else
                std::cout << "\nSetup requires attention.\n";
        }
    };
}

int main(int argc, char* argv[])
{
    fs::path root = fs::current_path();

    if (argc > 1)
        root = fs::absolute(argv[1]);

    std::error_code ec;

    if (!fs::is_directory(root, ec) || ec)
    {
        std::cerr << "Invalid project directory.\n";
        return 1;
    }

    DecompileX::Setup setup(root);

    return setup.run();
}
