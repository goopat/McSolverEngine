#include "McSolverEngine/Engine.h"

#include "McSolverEngine/Version.h"

namespace McSolverEngine
{

const char* Engine::version() noexcept
{
    return versionString();
}

std::string Engine::describe() const
{
    return "McSolverEngine scaffold (" + std::string(version()) + ")";
}

}
