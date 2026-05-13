#include <iostream>
#include <string_view>

#include "McSolverEngine/Engine.h"
#include "WindowsAssertMode.h"

namespace
{

void printUsage()
{
    std::cout << "Usage: mcsolverengine [--version]\n";
}

}

int main(int argc, char** argv)
{
    McSolverEngine::Detail::configureWindowsAssertMode();

    if (argc > 1) {
        std::string_view arg {argv[1]};
        if (arg == "--version") {
            std::cout << McSolverEngine::Engine::version() << '\n';
            return 0;
        }
        if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        }
    }

    McSolverEngine::Engine engine;
    std::cout << engine.describe() << '\n';
    return 0;
}
