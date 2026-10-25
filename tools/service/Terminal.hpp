#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace DecompileX {

enum class TerminalColor {
    Default, Black, Red, Green, Yellow, Blue, Magenta, Cyan, White,
    BrightBlack, BrightRed, BrightGreen, BrightYellow, BrightBlue,
    BrightMagenta, BrightCyan, BrightWhite
};

struct TerminalMenuItem {
    std::string key;
    std::string label;
};

class Terminal {
public:
    explicit Terminal(std::ostream& output, std::istream& input);
    Terminal();
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    void setColorsEnabled(bool enabled);
    bool colorsEnabled() const;
    bool interactive() const;
    void clear();
    void reset();
    void print(const std::string& text, TerminalColor color = TerminalColor::Default);
    void println(const std::string& text = "", TerminalColor color = TerminalColor::Default);
    void banner(const std::string& title, const std::string& subtitle = "", std::size_t width = 62);
    void section(const std::string& title, std::size_t width = 62);
    void message(const std::string& label, const std::string& text, TerminalColor color);
    void success(const std::string& text);
    void warning(const std::string& text);
    void error(const std::string& text);
    void info(const std::string& text);
    void menu(const std::string& title, const std::vector<TerminalMenuItem>& items);
    void table(const std::vector<std::string>& headers,
               const std::vector<std::vector<std::string>>& rows,
               std::size_t maxColumnWidth = 42);
    void progress(std::uint64_t current, std::uint64_t total,
                  std::size_t width = 30, const std::string& label = "");
    std::string ask(const std::string& prompt, const std::string& fallback = "");
    bool confirm(const std::string& prompt, bool defaultYes = false);
    int choose(const std::string& prompt, int minimum, int maximum, int fallback);

private:
    std::ostream* output_;
    std::istream* input_;
    bool colorsEnabled_;
    bool outputIsTerminal_;
    bool inputIsTerminal_;
#ifdef _WIN32
    void* windowsHandle_;
    unsigned long originalMode_;
    bool restoreMode_;
#endif
    void writeColor(TerminalColor color);
    static std::string fit(const std::string& text, std::size_t width);
};

}
