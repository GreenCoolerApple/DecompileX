#include "Terminal.hpp"
#include <iostream>

int main() {
    DecompileX::Terminal terminal;
    terminal.banner("D E C O M P I L E X  v0.0.1", "Terminal Service / Binary Toolkit");
    terminal.menu("TOOLS", {{"1", "Binary analysis"}, {"2", "Archive studio"}, {"3", "Build studio"}, {"0", "Exit"}});
    terminal.table({"Module", "Status"}, {{"Terminal", "Ready"}, {"Backend", "Available separately"}});
    terminal.success("Terminal service initialized.");
    terminal.progress(100, 100, 24, "Initialization");
    return 0;
}
