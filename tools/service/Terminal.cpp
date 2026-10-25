#include "Terminal.hpp"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace DecompileX {
namespace {

bool isTerminalOutput(std::ostream* stream) {
    if (stream != &std::cout && stream != &std::cerr) return false;
#ifdef _WIN32
    return _isatty(_fileno(stream == &std::cout ? stdout : stderr)) != 0;
#else
    return isatty(fileno(stream == &std::cout ? stdout : stderr)) != 0;
#endif
}

bool isTerminalInput(std::istream* stream) {
    if (stream != &std::cin) return false;
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(fileno(stdin)) != 0;
#endif
}

int colorCode(TerminalColor color) {
    switch (color) {
        case TerminalColor::Black: return 30;
        case TerminalColor::Red: return 31;
        case TerminalColor::Green: return 32;
        case TerminalColor::Yellow: return 33;
        case TerminalColor::Blue: return 34;
        case TerminalColor::Magenta: return 35;
        case TerminalColor::Cyan: return 36;
        case TerminalColor::White: return 37;
        case TerminalColor::BrightBlack: return 90;
        case TerminalColor::BrightRed: return 91;
        case TerminalColor::BrightGreen: return 92;
        case TerminalColor::BrightYellow: return 93;
        case TerminalColor::BrightBlue: return 94;
        case TerminalColor::BrightMagenta: return 95;
        case TerminalColor::BrightCyan: return 96;
        case TerminalColor::BrightWhite: return 97;
        default: return 0;
    }
}

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

}

Terminal::Terminal(std::ostream& output, std::istream& input)
    : output_(&output), input_(&input), colorsEnabled_(false),
      outputIsTerminal_(isTerminalOutput(&output)), inputIsTerminal_(isTerminalInput(&input))
#ifdef _WIN32
      , windowsHandle_(nullptr), originalMode_(0), restoreMode_(false)
#endif
{
    colorsEnabled_ = outputIsTerminal_ && std::getenv("NO_COLOR") == nullptr;
#ifdef _WIN32
    if (colorsEnabled_) {
        HANDLE handle = GetStdHandle(output_ == &std::cerr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (handle == INVALID_HANDLE_VALUE || !GetConsoleMode(handle, &mode)) {
            colorsEnabled_ = false;
        } else {
            windowsHandle_ = handle;
            originalMode_ = static_cast<unsigned long>(mode);
            if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0) {
                if (SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
                    restoreMode_ = true;
                } else {
                    colorsEnabled_ = false;
                }
            }
        }
    }
#endif
}

Terminal::Terminal() : Terminal(std::cout, std::cin) {}

Terminal::~Terminal() {
    reset();
#ifdef _WIN32
    if (restoreMode_ && windowsHandle_) {
        SetConsoleMode(static_cast<HANDLE>(windowsHandle_), static_cast<DWORD>(originalMode_));
    }
#endif
}

void Terminal::setColorsEnabled(bool enabled) { colorsEnabled_ = enabled && outputIsTerminal_; }
bool Terminal::colorsEnabled() const { return colorsEnabled_; }
bool Terminal::interactive() const { return inputIsTerminal_; }

void Terminal::writeColor(TerminalColor color) {
    if (colorsEnabled_) *output_ << "\x1b[" << colorCode(color) << 'm';
}

void Terminal::reset() {
    if (colorsEnabled_) *output_ << "\x1b[0m";
    output_->flush();
}

void Terminal::clear() {
    if (outputIsTerminal_) {
        *output_ << "\x1b[2J\x1b[H";
        output_->flush();
    }
}

void Terminal::print(const std::string& text, TerminalColor color) {
    writeColor(color);
    *output_ << text;
    if (color != TerminalColor::Default) reset();
    output_->flush();
}

void Terminal::println(const std::string& text, TerminalColor color) {
    print(text, color);
    *output_ << '\n';
    output_->flush();
}

std::string Terminal::fit(const std::string& text, std::size_t width) {
    if (width == 0) return "";
    if (text.size() <= width) return text + std::string(width - text.size(), ' ');
    if (width <= 3) return text.substr(0, width);
    return text.substr(0, width - 3) + "...";
}

void Terminal::banner(const std::string& title, const std::string& subtitle, std::size_t width) {
    width = std::max<std::size_t>(width, 8);
    println("+" + std::string(width - 2, '=') + "+", TerminalColor::BrightGreen);
    println("| " + fit(title, width - 4) + " |", TerminalColor::BrightGreen);
    if (!subtitle.empty()) println("| " + fit(subtitle, width - 4) + " |", TerminalColor::Cyan);
    println("+" + std::string(width - 2, '=') + "+", TerminalColor::BrightGreen);
}

void Terminal::section(const std::string& title, std::size_t width) {
    width = std::max<std::size_t>(width, 4);
    println("[ " + title + " ]", TerminalColor::BrightCyan);
    println(std::string(width, '-'), TerminalColor::BrightBlack);
}

void Terminal::message(const std::string& label, const std::string& text, TerminalColor color) {
    print("[" + label + "] ", color);
    println(text);
}

void Terminal::success(const std::string& text) { message("OK", text, TerminalColor::BrightGreen); }
void Terminal::warning(const std::string& text) { message("WARN", text, TerminalColor::BrightYellow); }
void Terminal::error(const std::string& text) { message("ERROR", text, TerminalColor::BrightRed); }
void Terminal::info(const std::string& text) { message("INFO", text, TerminalColor::BrightCyan); }

void Terminal::menu(const std::string& title, const std::vector<TerminalMenuItem>& items) {
    section(title);
    for (const auto& item : items) {
        print("  [" + item.key + "] ", TerminalColor::BrightGreen);
        println(item.label);
    }
    println();
}

void Terminal::table(const std::vector<std::string>& headers,
                     const std::vector<std::vector<std::string>>& rows,
                     std::size_t maxColumnWidth) {
    if (headers.empty()) return;
    maxColumnWidth = std::max<std::size_t>(1, maxColumnWidth);
    std::vector<std::size_t> widths;
    widths.reserve(headers.size());
    for (std::size_t col = 0; col < headers.size(); ++col) {
        std::size_t width = std::min(headers[col].size(), maxColumnWidth);
        for (const auto& row : rows) {
            if (col < row.size()) width = std::max(width, std::min(row[col].size(), maxColumnWidth));
        }
        widths.push_back(std::max<std::size_t>(1, width));
    }
    auto separator = [&]() {
        *output_ << '+';
        for (std::size_t width : widths) *output_ << std::string(width + 2, '-') << '+';
        *output_ << '\n';
    };
    auto printRow = [&](const std::vector<std::string>& cells) {
        *output_ << '|';
        for (std::size_t col = 0; col < widths.size(); ++col) {
            const std::string value = col < cells.size() ? cells[col] : "";
            *output_ << ' ' << fit(value, widths[col]) << " |";
        }
        *output_ << '\n';
    };
    separator();
    printRow(headers);
    separator();
    for (const auto& row : rows) printRow(row);
    separator();
    output_->flush();
}

void Terminal::progress(std::uint64_t current, std::uint64_t total,
                        std::size_t width, const std::string& label) {
    width = std::min<std::size_t>(width, 120);
    const std::uint64_t capped = total == 0 ? 0 : std::min(current, total);
    const double ratio = total == 0 ? 0.0 : static_cast<double>(capped) / static_cast<double>(total);
    const auto filled = static_cast<std::size_t>(ratio * static_cast<double>(width));
    std::ostringstream line;
    if (!label.empty()) line << label << ' ';
    line << '[' << std::string(filled, '#') << std::string(width - filled, '.') << "] "
         << static_cast<int>(ratio * 100.0) << '%';
    if (outputIsTerminal_) {
        print("\r" + line.str(), TerminalColor::BrightGreen);
        if (total != 0 && current >= total) println();
    } else {
        println(line.str());
    }
}

std::string Terminal::ask(const std::string& prompt, const std::string& fallback) {
    print(prompt + (fallback.empty() ? ": " : " [" + fallback + "]: "), TerminalColor::BrightCyan);
    std::string answer;
    if (!std::getline(*input_, answer)) return fallback;
    answer = trim(answer);
    return answer.empty() ? fallback : answer;
}

bool Terminal::confirm(const std::string& prompt, bool defaultYes) {
    while (true) {
        const std::string answer = ask(prompt + (defaultYes ? " (Y/n)" : " (y/N)"));
        if (answer.empty()) return defaultYes;
        if (answer == "y" || answer == "Y" || answer == "yes" || answer == "YES") return true;
        if (answer == "n" || answer == "N" || answer == "no" || answer == "NO") return false;
        warning("Please enter y or n.");
        if (!*input_) return defaultYes;
    }
}

int Terminal::choose(const std::string& prompt, int minimum, int maximum, int fallback) {
    if (minimum > maximum || fallback < minimum || fallback > maximum)
        throw std::invalid_argument("Invalid selection range or fallback.");
    while (true) {
        const std::string answer = ask(prompt, std::to_string(fallback));
        try {
            std::size_t used = 0;
            const long long value = std::stoll(answer, &used, 10);
            if (used == answer.size() && value >= minimum && value <= maximum)
                return static_cast<int>(value);
        } catch (const std::exception&) {
        }
        warning("Choose a number between " + std::to_string(minimum) + " and " + std::to_string(maximum) + ".");
        if (!*input_) return fallback;
    }
}

}
