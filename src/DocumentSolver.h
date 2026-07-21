#pragma once

#include <string>
#include <vector>

#include "McSolverEngine/CompatModel.h"

namespace McSolverEngine::Compat
{

/// Result of updating external geometry from a solved source sketch.
struct ExternalGeometryUpdateResult
{
    int updatedCount {0};
    int skippedCount {0};        // non-conformal curves skipped
    int missingRefCount {0};
    std::vector<std::string> messages;
};

/// Update external geometry in `dependent` from a solved `source` sketch.
///
/// For each geometry in `dependent` where `external = true` and
/// `externalSource.sourceObject` matches `sourceObjectName`:
///   1. Parse `externalSource.sourceSub` to determine the reference kind
///      (Edge → whole geometry, Vertex → a single point on the curve).
///   2. Resolve the sub-element name to a concrete geometry index (+ PointRole
///      for vertices) in `source`.
///   3. Compute PlaneTransform::between(source.placement, dependent.placement)
///      to project between sketch planes.
///   4. Apply the transform:
///      - Edge:  copy/paste the full geometry (same-kind requirement).
///      - Vertex: extract a single 2D point from the source curve and write
///        it into a target Point geometry.
///
/// Geometry-type-specific behaviour:
///   Point / LineSegment / BSpline — always works (affine transform)
///   Circle / Arc / Ellipse / conic arcs — only when planes are parallel
///     (isConformal); else skip. Arc polar angles are adjusted for
///     in-plane rotation and flipped under reflection.
///
/// The dependent sketch must be re-solved after this call.
ExternalGeometryUpdateResult updateExternalGeometry(
    SketchModel& dependent,
    const std::string& sourceObjectName,
    const SketchModel& solvedSource
);

}  // namespace McSolverEngine::Compat
