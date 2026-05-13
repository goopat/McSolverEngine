#pragma once

#if defined(MCSOLVERENGINE_STATIC_DEFINE)
#    define MCSOLVERENGINE_EXPORT
#elif defined(_WIN32)
#    if defined(MCSOLVERENGINE_CORE_BUILD)
#        define MCSOLVERENGINE_EXPORT __declspec(dllexport)
#    else
#        define MCSOLVERENGINE_EXPORT __declspec(dllimport)
#    endif
#else
#    define MCSOLVERENGINE_EXPORT
#endif

#ifndef MCSOLVERENGINE_WITH_OCCT
#    define MCSOLVERENGINE_WITH_OCCT 0
#endif
