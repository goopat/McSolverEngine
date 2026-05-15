#pragma once

#include <string>
#include <vector>

#include "McSolverEngine/CompatModel.h"
#include "McSolverEngine/Export.h"

namespace McSolverEngine::Geometry
{

struct GeometryRecord
{
    int geometryIndex {-1};
    int originalId {-99999999};
    Compat::Geometry geometry {};
};

enum class ExportStatus
{
    Success = 0,
    Failed = 1,
};

struct ExportResult
{
    ExportStatus status {ExportStatus::Failed};
    Compat::Placement placement {};
    std::vector<GeometryRecord> geometries;
    std::vector<std::string> messages;

    [[nodiscard]] bool exported() const noexcept
    {
        return status == ExportStatus::Success;
    }
};

[[nodiscard]] MCSOLVERENGINE_EXPORT ExportResult exportSketchGeometry(const Compat::SketchModel& model);

}  // namespace McSolverEngine::Geometry
