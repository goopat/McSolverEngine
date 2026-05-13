#pragma once

#include <vector>

#include "McSolverEngine/CompatModel.h"
#include "McSolverEngine/Export.h"

namespace McSolverEngine::Compat
{

enum class SolveStatus
{
    Success = 0,
    Converged = 1,
    Failed = 2,
    Invalid = 3,
    Unsupported = 4,
};

struct SolveResult
{
    SolveStatus status {SolveStatus::Failed};
    int degreesOfFreedom {-1};
    std::vector<int> conflicting;
    std::vector<int> redundant;
    std::vector<int> partiallyRedundant;

    [[nodiscard]] bool solved() const noexcept
    {
        return status == SolveStatus::Success;
    }
};

[[nodiscard]] MCSOLVERENGINE_EXPORT SolveResult solveSketch(SketchModel& model);
[[nodiscard]] MCSOLVERENGINE_EXPORT SolveResult solveSketch(
    SketchModel& model,
    const McSolverEngine::ParameterMap& parameters
);

}  // namespace McSolverEngine::Compat
