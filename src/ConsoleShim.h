#pragma once

#include <cstdarg>
#include <cstdio>

namespace Base
{

class ConsoleShim
{
public:
    void log(const char* format, ...) const
    {
        va_list args;
        va_start(args, format);
        std::vfprintf(stdout, format, args);
        va_end(args);
    }

    void warning(const char* format, ...) const
    {
        va_list args;
        va_start(args, format);
        std::vfprintf(stderr, format, args);
        va_end(args);
    }
};

inline const ConsoleShim& Console()
{
    static const ConsoleShim console {};
    return console;
}

}  // namespace Base
