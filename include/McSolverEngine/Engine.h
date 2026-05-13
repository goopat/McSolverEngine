#pragma once

#include <string>

#include "McSolverEngine/Export.h"

namespace McSolverEngine
{

class MCSOLVERENGINE_EXPORT Engine
{
public:
    [[nodiscard]] static const char* version() noexcept;
    [[nodiscard]] std::string describe() const;
};

}
