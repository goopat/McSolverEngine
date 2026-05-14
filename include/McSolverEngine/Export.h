#pragma once

#if defined(MCSOLVERENGINE_STATIC_DEFINE)
#    define MCSOLVERENGINE_EXPORT
#elif defined(_WIN32)
#    if defined(MCSOLVERENGINE_CORE_BUILD)
#        define MCSOLVERENGINE_EXPORT __declspec(dllexport)
#    else
#        define MCSOLVERENGINE_EXPORT __declspec(dllimport)
#    endif
#elif defined(__GNUC__) || defined(__clang__)
#    define MCSOLVERENGINE_EXPORT __attribute__((visibility("default")))
#else
#    define MCSOLVERENGINE_EXPORT
#endif

#ifndef MCSOLVERENGINE_WITH_OCCT
#    define MCSOLVERENGINE_WITH_OCCT 0
#endif
