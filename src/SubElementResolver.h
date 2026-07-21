#pragma once

#include <string>
#include <string_view>

#include "McSolverEngine/CompatModel.h"

namespace McSolverEngine::Compat
{

/// Extract the simple element name from FreeCAD's sub-element string.
/// Handles both old-style and new-style (topological) naming:
///   "Edge1"           → "Edge1"
///   ";:H1234:5,Edge5" → "Edge5"
[[nodiscard]] std::string extractSimpleElementName(std::string_view subElement);

/// Maps a FreeCAD Sketcher sub-element name to a geometry index.
///
/// In the Sketcher, every non-construction geometry IS an edge:
///   "Edge1" → 1st non-construction geometry (0-based index 0)
///   "Edge2" → 2nd non-construction geometry (0-based index 1)
///
/// Iterates model.geometries() in order, skipping construction=true
/// and external=true elements.
///
/// Returns the geometry index in the model, or -1 if not found.
[[nodiscard]] int resolveSubElementToGeoIndex(
    const SketchModel& model,
    const std::string& subElement
);

/// Result of resolving a Vertex sub-element reference.
struct ResolvedVertexRef
{
    int geometryIndex {-1};   // index in model, or -1 if not found
    PointRole pointRole {PointRole::Start};
};

/// Resolve a "VertexN" sub-element name.
/// Follows FreeCAD's rebuildVertexIndex vertex numbering:
/// vertices are numbered sequentially across non-construction, non-external
/// geometry in model order, each geometry contributing:
///   Point: 1 (start), LineSegment: 2 (start,end), Circle: 1 (mid),
///   Arc: 3 (start,end,mid), ArcOfEllipse/Hyperbola/Parabola: 3, Ellipse: 1 (mid)
[[nodiscard]] ResolvedVertexRef resolveVertexSubElement(
    const SketchModel& model,
    const std::string& subElement
);

/// Extract a 2D point from a geometry at the given point role.
/// Returns true on success, false if the role is not applicable to the kind.
[[nodiscard]] bool extractVertexPoint(
    const Geometry& geometry,
    PointRole role,
    Point2& outPoint
);

}  // namespace McSolverEngine::Compat
