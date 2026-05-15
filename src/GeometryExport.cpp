#include "McSolverEngine/GeometryExport.h"

namespace McSolverEngine::Geometry
{

ExportResult exportSketchGeometry(const Compat::SketchModel& model)
{
    ExportResult result;
    result.placement = model.placement();

    for (std::size_t geometryIndex = 0; geometryIndex < model.geometries().size(); ++geometryIndex) {
        const auto& geometry = model.geometries()[geometryIndex];

        result.geometries.push_back({
            .geometryIndex = static_cast<int>(geometryIndex),
            .originalId = geometry.originalId,
            .geometry = geometry,
        });
    }

    result.status = ExportStatus::Success;
    return result;
}

}  // namespace McSolverEngine::Geometry
