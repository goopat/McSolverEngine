#include "McSolverEngine/GeometryExport.h"

#include <unordered_set>

namespace McSolverEngine::Geometry
{

ExportResult exportSketchGeometry(const Compat::SketchModel& model)
{
    ExportResult result;
    result.placement = model.placement();

    const auto geoCount = model.geometries().size();
    result.geometries.reserve(geoCount);
    for (std::size_t geometryIndex = 0; geometryIndex < geoCount; ++geometryIndex) {
        const auto& geometry = model.geometries()[geometryIndex];

        result.geometries.push_back({
            .geometryIndex = static_cast<int>(geometryIndex),
            .originalId = geometry.originalId,
            .geometry = geometry,
            .constraints = {},
        });
    }

    for (const auto& constraint : model.constraints()) {
        if (constraint.parameterExpression.empty()) {
            continue;
        }
        std::unordered_set<int> seenGeos;
        for (int geoIdx : {constraint.first.geometryIndex,
                           constraint.second.geometryIndex,
                           constraint.third.geometryIndex}) {
            if (geoIdx < 0 || static_cast<std::size_t>(geoIdx) >= geoCount) {
                continue;
            }
            if (!seenGeos.insert(geoIdx).second) {
                continue;  // deduplicate when same constraint refs same geometry twice
            }
            result.geometries[static_cast<std::size_t>(geoIdx)].constraints.push_back({
                .kind = constraint.kind,
                .originalIndex = constraint.originalIndex,
                .expression = constraint.parameterExpression,
            });
        }
    }

    result.status = ExportStatus::Success;
    return result;
}

}  // namespace McSolverEngine::Geometry
