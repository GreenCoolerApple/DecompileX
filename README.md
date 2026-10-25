<div align="center"><img src="arts/icon.png" alt="DecompileX Icon" width="180">DecompileX

A modern, cross-platform binary analysis and development toolkit written in C++17.

Explore binaries. Inspect executable formats. Build projects. Create archives.

"C++" (https://img.shields.io/badge/C++-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
"Windows" (https://img.shields.io/badge/Windows-Supported-0078D4?style=for-the-badge&logo=windows&logoColor=white)
"Android" (https://img.shields.io/badge/Android-APK-3DDC84?style=for-the-badge&logo=android&logoColor=white)
"Version" (https://img.shields.io/badge/Version-0.2.0-22C55E?style=for-the-badge)
"License" (https://img.shields.io/github/license/RivenChill98/DecompileX?style=for-the-badge)

"Repository" (https://github.com/RivenChill98/DecompileX) · "Releases" (https://github.com/RivenChill98/DecompileX/releases) · "Actions" (https://github.com/RivenChill98/DecompileX/actions)

</div>---

Overview

DecompileX is an open-source binary analysis and development toolkit built with modern C++.

It provides tools for examining executable files, exploring binary data, creating archives, and compiling C++ applications through a unified command-line interface.

DecompileX is designed to be lightweight, extensible, and cross-platform.

«Note: DecompileX is currently a binary analysis toolkit. Full machine-code disassembly and high-level source-code decompilation are planned features.»

Features

Binary Analysis

- PE, ELF, Mach-O, and ZIP format identification
- CPU architecture detection
- Interactive hexadecimal viewer
- ASCII string extraction
- Shannon entropy analysis
- Binary file information
- Analysis report export
- Command-line operations

Archive Tools

- ZIP archive generation
- CRC32 integrity calculation
- Directory packaging
- ISO 9660 image generation
- Custom ISO volume names

Build Tools

- C++ executable compilation
- MSVC, MinGW-w64, and Clang support
- Compiler availability detection
- Configurable C++ language standards
- Build environment setup utility

Platform Support

Platform| Support
Windows x64| CLI executable
Android ARM64| APK with native C++ integration
Linux| C++ source build
macOS| C++ source build, subject to toolchain compatibility

Interface

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

Getting Started

Requirements

- C++17-compatible compiler
- CMake 3.16 or newer
- Ninja, Visual Studio, or another supported CMake build system

Build on Windows

git clone https://github.com/RivenChill98/DecompileX.git
cd DecompileX
cmake -S . -B build
cmake --build build --config Release

The executable will be generated under "build/bin/".

Build on Linux

git clone https://github.com/RivenChill98/DecompileX.git
cd DecompileX
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

Android

The Android application uses Java and JNI to communicate with the native C++ library.

Android APK builds are handled through GitHub Actions.

Visit the "Actions page" (https://github.com/RivenChill98/DecompileX/actions) to access available build artifacts.

Command-Line Usage

DecompileX --help
DecompileX --info game.exe
DecompileX --hex game.exe 0x100 512
DecompileX --strings game.exe 6
DecompileX --entropy game.exe
DecompileX --zip assets assets.zip
DecompileX --iso assets assets.iso GAME_DATA

Project Structure

DecompileX/
├── .github/
│   └── workflows/
│       └── build.yml
├── android/
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
└── README.md

Roadmap

- [x] Binary format detection
- [x] Hexadecimal viewer
- [x] ASCII string extraction
- [x] Entropy analysis
- [x] ZIP archive generation
- [x] ISO 9660 image generation
- [x] C++ compilation backend
- [x] Windows executable builds
- [x] Android APK builds
- [ ] x86/x64 disassembler
- [ ] ARM64 instruction decoding
- [ ] Symbol and function analysis
- [ ] Control-flow graph generation
- [ ] High-level pseudocode generation
- [ ] Windows graphical interface
- [ ] Advanced Android analysis interface

Contributing

Contributions, bug reports, feature suggestions, and pull requests are welcome.

If you want to contribute, fork the repository, create a feature branch, and submit a pull request with a clear description of your changes.

License

See the "LICENSE" (LICENSE) file for licensing information.

---

<div align="center">Made with C++17 and passion.

DecompileX — Explore what is inside.

</div>
