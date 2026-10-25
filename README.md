<div align="center"><img src="arts/icon.png" alt="DecompileX Logo" width="220">DecompileX

A modern, cross-platform binary analysis and development toolkit built with C++17.

Explore binaries. Understand file structures. Build and package projects.

"C++17" (https://img.shields.io/badge/C++-17-00599C?style=flat-square&logo=cplusplus&logoColor=white)
"CMake" (https://img.shields.io/badge/CMake-Build_System-064F8C?style=flat-square&logo=cmake&logoColor=white)
"Windows" (https://img.shields.io/badge/Windows-x64-0078D4?style=flat-square)
"Android" (https://img.shields.io/badge/Android-ARM64-3DDC84?style=flat-square&logo=android&logoColor=white)
"Version" (https://img.shields.io/badge/Version-0.2.0-22C55E?style=flat-square)

"Repository" (https://github.com/RivenChill98/DecompileX) · "Builds" (https://github.com/RivenChill98/DecompileX/actions) · "Issues" (https://github.com/RivenChill98/DecompileX/issues) · "Releases" (https://github.com/RivenChill98/DecompileX/releases)

</div>---

Contents

- "Overview" (#overview)
- "Features" (#features)
- "Supported Platforms" (#supported-platforms)
- "Installation" (#installation)
- "Usage" (#usage)
- "Backend Documentation" (#backend-documentation)
- "Project Structure" (#project-structure)
- "Build System" (#build-system)
- "Limitations" (#limitations)
- "Roadmap" (#roadmap)
- "Contributing" (#contributing)
- "License" (#license)

---

Overview

DecompileX is an open-source toolkit for inspecting binary files, analyzing executable formats, generating archives, and compiling C++ projects.

The project focuses on providing a modular architecture that can be extended with additional binary analysis and reverse-engineering features.

Its core is written in C++17, with a command-line interface for desktop systems and an Android application using Java and JNI.

Project Goals

- Provide accessible binary inspection tools.
- Support multiple executable and archive formats.
- Offer reusable C++ backend modules.
- Simplify development and packaging workflows.
- Build a foundation for future disassembly and decompilation capabilities.

«Development status: DecompileX currently focuses on binary inspection and development utilities. Full machine-code disassembly and high-level source-code decompilation are not yet implemented.»

---

Features

Binary Analysis

Feature| Description
Format Detection| Identify PE, ELF, Mach-O and ZIP signatures
Architecture Detection| Identify supported CPU architectures
Hex Viewer| Inspect file contents in hexadecimal and ASCII
String Extraction| Find printable ASCII strings
Entropy Analysis| Calculate Shannon entropy for binary data
File Information| Display binary metadata
Report Export| Package analysis results into ZIP archives

Archive Tools

Feature| Description
ZIP Generator| Create ZIP archives from files and directories
CRC32| Calculate checksums for ZIP entries
ISO Compiler| Generate ISO 9660 data images
Directory Packaging| Package directory contents
Volume Labels| Configure ISO volume identifiers

Build Tools

Feature| Description
EXE Compiler| Compile C++ source files using an installed toolchain
Compiler Detection| Detect supported C++ compilers
Build Options| Configure optimization and debugging
Include Directories| Supply additional header search paths
Setup Utility| Prepare directories and development configuration

User Interface

The desktop application provides an interactive terminal menu with organized sections for binary inspection, archive generation, compilation, and utilities.

The Android application provides a native file-selection interface and communicates with the C++ backend through JNI.

---

Supported Platforms

Platform| Build Target| Status
Windows x64| "DecompileX.exe"| Supported
Android ARM64| "DecompileX-Android.apk"| Supported
Linux| Native CLI| Source build
macOS| Native CLI| Source build, not fully validated

Some features depend on platform-specific compilers or tools.

---

Installation

Downloading Builds

You can obtain build artifacts from the project's GitHub Actions workflow:

"View GitHub Actions" (https://github.com/RivenChill98/DecompileX/actions)

Available build artifacts include:

- "DecompileX-Windows-x64"
- "DecompileX-Android-APK"

GitHub Actions artifacts may require signing in and are retained for a limited period.

Building from Source

Requirements

- Git
- CMake 3.16 or newer
- A C++17-compatible compiler
- A supported build system such as Ninja or Visual Studio

Clone the repository:

git clone https://github.com/RivenChill98/DecompileX.git
cd DecompileX

Windows

Configure and build:

cmake -S . -B build
cmake --build build --config Release

The compiled executable is placed in:

build/bin/DecompileX.exe

Linux

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

Run the executable:

./build/bin/DecompileX

Android

The Android application uses:

- Java for the Android interface
- JNI for native integration
- CMake for native compilation
- Gradle for APK generation
- Android NDK for ARM64 compilation

Build the debug APK from the "android" directory:

gradle :app:assembleDebug

The APK is generated at:

android/app/build/outputs/apk/debug/app-debug.apk

The Android SDK, NDK, CMake, Java and Gradle versions must match the project's build configuration.

---

Usage

Interactive Mode

Start DecompileX without arguments:

DecompileX

Example interface:

+======================================================+
|  D E C O M P I L E X                    v0.2.0      |
|  Binary Lab  /  Build Studio  /  Archive Tools      |
+======================================================+

BINARY LAB
[1] Open binary            [2] File information
[3] Hex viewer             [4] Extract ASCII strings
[5] Entropy analysis       [6] Export analysis ZIP

ARCHIVE STUDIO
[7] Create ZIP             [8] Create ISO 9660

BUILD STUDIO
[9] Compile C++            [10] Detect compilers

UTILITIES
[11] Export hex dump       [12] Help / CLI commands
[0] Exit

DecompileX >

Command-Line Reference

Command| Description
"--help"| Display available commands
"--info"| Show binary information
"--hex"| Display hexadecimal data
"--strings"| Extract printable ASCII strings
"--entropy"| Calculate binary entropy
"--zip"| Create a ZIP archive
"--iso"| Generate an ISO image

Inspect a Binary

DecompileX --info game.exe

View Hexadecimal Data

DecompileX --hex game.exe 0x100 512

This reads 512 bytes starting at offset "0x100".

Extract Strings

DecompileX --strings game.exe 6

The final argument specifies the minimum string length.

Analyze Entropy

DecompileX --entropy game.exe

Entropy analysis can help identify unusual data distributions. High entropy alone does not prove that data is encrypted or compressed.

Create a ZIP Archive

DecompileX --zip assets assets.zip

Create an ISO Image

DecompileX --iso assets assets.iso GAME_DATA

The last argument is the ISO volume identifier.

---

Backend Documentation

The native backend is organized into independent modules under "src/backend/".

ZipGenerator

Files: "ZipGenerator.hpp" and "ZipGenerator.cpp"

Creates ZIP archives from files, directories, text, and binary data.

#include "backend/ZipGenerator.hpp"

DecompileX::ZipGenerator zip;

if (zip.create("output.zip"))
{
    bool success = zip.addText(
        "README.txt",
        "Generated by DecompileX"
    );

    success = zip.close() && success;
}

Current limitations: Uses ZIP STORE without DEFLATE compression and does not support ZIP64.

ISOCompiler

Files: "ISOCompiler.hpp" and "ISOCompiler.cpp"

Creates ISO 9660 data images from directories.

#include "backend/ISOCompiler.hpp"

auto result = DecompileX::ISOCompiler::compile(
    "assets",
    "output.iso",
    "DECOMPILEX"
);

Current limitations: ISO 9660 data images only. Bootable ISO images, Joliet, and UDF are not supported.

ExeCompiler

Files: "ExeCompiler.hpp" and "ExeCompiler.cpp"

Provides a common interface for invoking installed C++ compilers.

#include "backend/ExeCompiler.hpp"

DecompileX::CompilerOptions options;

options.compiler = DecompileX::CompilerType::Auto;
options.sources = {"src/Test.cpp"};
options.output = "build/Test.exe";
options.standard = "c++17";

auto result = DecompileX::ExeCompiler::compile(options);

Important: The generated executable format depends on the selected compiler and target platform. A native Linux or macOS compiler does not automatically generate Windows executables.

The current command execution approach should only be used with trusted arguments.

Setup Utility

File: "tools/Setup.cpp"

Provides development environment checks, directory creation, and configuration generation.

Its main functions include:

- Detecting available development tools
- Preparing build and output directories
- Generating "decompilex.ini"
- Displaying project information

The configuration file is intended for future integration with the main application.

---

Project Structure

DecompileX/
├── .github/
│   └── workflows/
│       └── build.yml
├── android/
│   ├── app/
│   │   ├── build.gradle
│   │   └── src/main/
│   ├── build.gradle
│   ├── gradle.properties
│   └── settings.gradle
├── arts/
│   └── icon.png
├── assets/
│   └── icons/
│       ├── DecompileX.ico
│       └── DecompileX.rc
├── src/
│   ├── Main.cpp
│   ├── android/
│   │   └── NativeBridge.cpp
│   └── backend/
│       ├── ExeCompiler.cpp
│       ├── ExeCompiler.hpp
│       ├── ISOCompiler.cpp
│       ├── ISOCompiler.hpp
│       ├── ZipGenerator.cpp
│       └── ZipGenerator.hpp
├── tools/
│   └── Setup.cpp
├── CMakeLists.txt
├── LICENSE
└── README.md

---

Build System

DecompileX uses CMake to manage native compilation.

The repository's GitHub Actions workflow includes two build jobs.

Windows Build

- Uses MSVC and Ninja.
- Compiles the C++ CLI application.
- Embeds the Windows application icon.
- Packages the executable as a ZIP artifact.

Android Build

- Uses Java 17 and Gradle.
- Builds the Android application and JNI library.
- Targets ARM64.
- Checks APK structure, signing, and native library inclusion.
- Generates a SHA-256 checksum.

The CI workflow currently produces a debug APK, not a production-signed Google Play release.

---

Limitations

DecompileX is under active development.

Current limitations include:

- No high-level source-code decompilation.
- No complete machine-code disassembler.
- Limited executable format parsing.
- ZIP generation without compression.
- ISO generation limited to ISO 9660 data images.
- Platform-dependent compiler availability.
- Android interface with fewer features than the desktop CLI.
- Some features require additional testing across operating systems.

---

Roadmap

Phase 1 — Binary Toolkit

- [x] Binary format detection
- [x] Architecture identification
- [x] Hexadecimal viewer
- [x] ASCII string extraction
- [x] Entropy analysis
- [x] ZIP generator
- [x] ISO 9660 generator
- [x] C++ compiler integration
- [x] Windows and Android build workflows

Phase 2 — Binary Inspection

- [ ] Detailed PE and ELF header parsing
- [ ] Import and export table inspection
- [ ] Section analysis
- [ ] Symbol table support
- [ ] Executable metadata export
- [ ] Improved error reporting

Phase 3 — Disassembly

- [ ] x86 instruction decoding
- [ ] x86-64 instruction decoding
- [ ] ARM64 instruction decoding
- [ ] Function boundary identification
- [ ] Basic block detection
- [ ] Control-flow graph generation

Phase 4 — Decompilation

- [ ] Intermediate representation
- [ ] Control-flow reconstruction
- [ ] Pseudocode generation
- [ ] Variable and function analysis
- [ ] Structured output export

Phase 5 — User Experience

- [ ] Desktop graphical interface
- [ ] Advanced Android interface
- [ ] Syntax-highlighted disassembly viewer
- [ ] Project workspace management
- [ ] Plugin architecture

---

Contributing

Contributions are welcome!

To contribute:

1. Fork the repository.
2. Create a branch for your changes.
3. Implement and test your feature.
4. Submit a pull request explaining the changes.

For bug reports or feature requests, use "GitHub Issues" (https://github.com/RivenChill98/DecompileX/issues).

Please keep contributions compatible with the project's C++17 architecture unless a change is explicitly approved.

---

License

See the "LICENSE" (LICENSE) file for the project's license terms.

---

<div align="center"><img src="arts/icon.png" alt="DecompileX" width="90">DecompileX

Explore what is inside.

Made with C++17 and passion. 💚

"Back to Top" (#decompilex)

</div>
