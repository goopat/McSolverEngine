#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "McSolverEngine/CompatModel.h"
#include "McSolverEngine/Export.h"

namespace McSolverEngine::BRep
{

enum class ExportStatus
{
    Success = 0,
    Failed = 1,
    OpenCascadeUnavailable = 2,
};

struct ExportResult
{
    ExportStatus status {ExportStatus::Failed};
    std::optional<std::string> brep;
    std::vector<std::string> messages;

    [[nodiscard]] bool exported() const noexcept
    {
        return status == ExportStatus::Success;
    }
};

[[nodiscard]] MCSOLVERENGINE_EXPORT ExportResult exportSketchToBRep(const Compat::SketchModel& model);
[[nodiscard]] MCSOLVERENGINE_EXPORT ExportStatus exportSketchToBRepFile(
    const Compat::SketchModel& model,
    std::string_view path
);

}  // namespace McSolverEngine::BRep
