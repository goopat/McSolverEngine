#pragma once

#include <cstdio>
#include <mutex>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <Windows.h>
#    ifdef max
#        undef max
#    endif
#    ifdef min
#        undef min
#    endif
#endif

#if defined(_MSC_VER)
#    include <crtdbg.h>
#    include <cstdlib>
#endif

namespace McSolverEngine::Detail
{

#if defined(_MSC_VER)
inline void __cdecl runtimeInvalidParameterHandler(
    const wchar_t* expression,
    const wchar_t* functionName,
    const wchar_t* fileName,
    unsigned int lineNumber,
    uintptr_t
)
{
    std::fwprintf(
        stderr,
        L"Invalid parameter: expression=%ls function=%ls file=%ls line=%u\n",
        expression ? expression : L"(null)",
        functionName ? functionName : L"(null)",
        fileName ? fileName : L"(null)",
        lineNumber
    );
    std::fflush(stderr);
}
#endif

inline void configureWindowsAssertMode()
{
    static std::once_flag once;
    std::call_once(once, [] {
#if defined(_WIN32)
        const UINT currentMode = ::SetErrorMode(0);
        ::SetErrorMode(currentMode | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif

#if defined(_MSC_VER)
        _set_error_mode(_OUT_TO_STDERR);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        _set_invalid_parameter_handler(&runtimeInvalidParameterHandler);

#    if defined(_DEBUG)
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#    endif
#endif
    });
}

}  // namespace McSolverEngine::Detail
