#include "McSolverEngine/Version.h"

namespace McSolverEngine
{

const char* versionString() noexcept
{
#ifdef MCSOLVERENGINE_VERSION
    return MCSOLVERENGINE_VERSION;
#else
    return "0.1.0-dev";
#endif
}

}
