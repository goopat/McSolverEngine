#include "SubElementResolver.h"

#include <cctype>
#include <climits>
#include <cstdlib>

namespace McSolverEngine::Compat
{

std::string extractSimpleElementName(const std::string_view subElement)
{
    if (subElement.empty()) {
        return {};
    }

    // If the name starts with ';' (FreeCAD topological naming prefix),
    // extract the simple name after the last comma.
    if (subElement.front() == ';') {
        const auto comma = subElement.rfind(',');
        if (comma != std::string_view::npos && comma + 1 < subElement.size()) {
            return std::string(subElement.substr(comma + 1));
        }
        // Malformed: starts with ';' but no comma. Return as-is.
        return std::string(subElement);
    }

    return std::string(subElement);
}

namespace
{

// Parse a positive integer from a string_view that consists of digits only.
// Returns the parsed value, or -1 on failure (empty, trailing junk,
// non-positive, or out of int range).
int parseElementIndex(const std::string_view name)
{
    if (name.empty()) {
        return -1;
    }

    std::size_t pos = 0;
    while (pos < name.size() && std::isdigit(static_cast<unsigned char>(name[pos]))) {
        ++pos;
    }

    if (pos == 0 || pos != name.size()) {
        return -1;
    }

    // Parse the numeric prefix
    const auto numStr = std::string(name.substr(0, pos));
    char* end = nullptr;
    const long val = std::strtol(numStr.c_str(), &end, 10);
    if (end != numStr.c_str() + pos || val <= 0 || val > INT_MAX) {
        return -1;
    }

    return static_cast<int>(val);
}

}  // namespace

int resolveSubElementToGeoIndex(
    const SketchModel& model,
    const std::string& subElement
)
{
    const auto simpleName = extractSimpleElementName(subElement);

    // Currently only Edge references are supported.
    // Format: "EdgeN" where N is 1-based.
    if (simpleName.size() < 5 || simpleName.substr(0, 4) != "Edge") {
        return -1;
    }

    const int edgeNum = parseElementIndex(std::string_view(simpleName).substr(4));
    if (edgeNum <= 0) {
        return -1;
    }

    // Count non-construction, non-external geometries.
    // Each such geometry corresponds to one edge.
    const auto& geos = model.geometries();
    int edgeIdx = 0;
    for (std::size_t i = 0; i < geos.size(); ++i) {
        if (geos[i].construction || geos[i].external) {
            continue;
        }
        ++edgeIdx;
        if (edgeIdx == edgeNum) {
            return static_cast<int>(i);
        }
    }

    return -1;  // Edge N not found
}

// ──────────────────────────────────────────────────────────────────────────
// Vertex sub-element resolution
//
// FreeCAD numbers vertices sequentially across all non-construction,
// non-external geometry in the order returned by getCompleteGeometry().
// The mapping is defined in SketchObject::rebuildVertexIndex().
//
//   Geometry            Vertex 1         Vertex 2         Vertex 3
//   ────────            ────────         ────────         ────────
//   Point               the point itself  —                 —
//   LineSegment         start             end               —
//   Circle              center (mid)      —                 —
//   Arc                 start             end              center (mid)
//   ArcOfEllipse        start             end              center (mid)
//   ArcOfHyperbola      start             end              center (mid)
//   ArcOfParabola       start             end              center (mid)
//   Ellipse             center (mid)      —                 —
//   BSpline             start (1st pole)  end (last pole)   —
// ──────────────────────────────────────────────────────────────────────────

namespace
{

// Number of vertices that a single geometry contributes.
// Mirrors FreeCAD's SketchObject::rebuildVertexIndex().
int vertexCountForKind(GeometryKind kind)
{
    switch (kind) {
        case GeometryKind::Point:           return 1;   // start
        case GeometryKind::LineSegment:      return 2;   // start, end
        case GeometryKind::Circle:          return 1;   // mid (center)
        case GeometryKind::Arc:             return 3;   // start, end, mid
        case GeometryKind::Ellipse:         return 1;   // mid
        case GeometryKind::ArcOfEllipse:    return 3;   // start, end, mid
        case GeometryKind::ArcOfHyperbola:  return 3;   // start, end, mid
        case GeometryKind::ArcOfParabola:   return 3;   // start, end, mid
        case GeometryKind::BSpline:         return 2;   // start, end (FreeCAD rebuildVertexIndex)
    }
    return 0;
}

// Map a relative vertex offset within a geometry to a PointRole.
PointRole pointRoleForVertexOffset(const Geometry& geo, int offset)
{
    switch (geo.kind) {
        case GeometryKind::Point:
            return offset == 0 ? PointRole::Start : PointRole::None;
        case GeometryKind::LineSegment:
            return offset == 0 ? PointRole::Start : (offset == 1 ? PointRole::End : PointRole::None);
        case GeometryKind::Circle:
            return offset == 0 ? PointRole::Mid : PointRole::None;
        case GeometryKind::Arc:
            return offset == 0 ? PointRole::Start : (offset == 1 ? PointRole::End : (offset == 2 ? PointRole::Mid : PointRole::None));
        case GeometryKind::Ellipse:
            return offset == 0 ? PointRole::Mid : PointRole::None;
        case GeometryKind::ArcOfEllipse:
        case GeometryKind::ArcOfHyperbola:
        case GeometryKind::ArcOfParabola:
            return offset == 0 ? PointRole::Start : (offset == 1 ? PointRole::End : (offset == 2 ? PointRole::Mid : PointRole::None));
        case GeometryKind::BSpline:
            return offset == 0 ? PointRole::Start : (offset == 1 ? PointRole::End : PointRole::None);
    }
    return PointRole::None;
}

}  // namespace

ResolvedVertexRef resolveVertexSubElement(
    const SketchModel& model,
    const std::string& subElement
)
{
    const auto simpleName = extractSimpleElementName(subElement);

    if (simpleName.size() < 7 || simpleName.substr(0, 6) != "Vertex") {
        return {};
    }

    const int vertexNum = parseElementIndex(std::string_view(simpleName).substr(6));
    if (vertexNum <= 0) {
        return {};
    }

    // Walk geometries, accumulating vertex count
    const auto& geos = model.geometries();
    int vertexIdx = 0;
    for (std::size_t i = 0; i < geos.size(); ++i) {
        if (geos[i].construction || geos[i].external) {
            continue;
        }
        const int vc = vertexCountForKind(geos[i].kind);
        if (vc <= 0) {
            continue;
        }
        if (vertexIdx + vc >= vertexNum) {
            // The requested vertex is within this geometry
            const int offset = vertexNum - vertexIdx - 1;
            return {
                .geometryIndex = static_cast<int>(i),
                .pointRole = pointRoleForVertexOffset(geos[i], offset),
            };
        }
        vertexIdx += vc;
    }

    return {};  // Vertex N not found
}

bool extractVertexPoint(
    const Geometry& geometry,
    const PointRole role,
    Point2& outPoint
)
{
    switch (geometry.kind) {
        case GeometryKind::Point: {
            if (role != PointRole::Start) return false;
            outPoint = std::get<PointGeometry>(geometry.data).point;
            return true;
        }
        case GeometryKind::LineSegment: {
            const auto& line = std::get<LineSegmentGeometry>(geometry.data);
            if (role == PointRole::Start) { outPoint = line.start; return true; }
            if (role == PointRole::End)   { outPoint = line.end;   return true; }
            return false;
        }
        case GeometryKind::Circle: {
            if (role != PointRole::Mid) return false;
            outPoint = std::get<CircleGeometry>(geometry.data).center;
            return true;
        }
        case GeometryKind::Arc: {
            const auto& arc = std::get<ArcGeometry>(geometry.data);
            // Arc start/end are computed from center, radius, and angle.
            // The angles are in radians, matching FreeCAD's convention.
            if (role == PointRole::Start) {
                outPoint = {arc.center.x + arc.radius * std::cos(arc.startAngle),
                            arc.center.y + arc.radius * std::sin(arc.startAngle)};
                return true;
            }
            if (role == PointRole::End) {
                outPoint = {arc.center.x + arc.radius * std::cos(arc.endAngle),
                            arc.center.y + arc.radius * std::sin(arc.endAngle)};
                return true;
            }
            if (role == PointRole::Mid) { outPoint = arc.center; return true; }
            return false;
        }
        case GeometryKind::Ellipse: {
            if (role != PointRole::Mid) return false;
            outPoint = std::get<EllipseGeometry>(geometry.data).center;
            return true;
        }
        case GeometryKind::ArcOfEllipse: {
            const auto& a = std::get<ArcOfEllipseGeometry>(geometry.data);
            if (role == PointRole::Mid) { outPoint = a.center; return true; }
            return false;  // start/end require parametric evaluation
        }
        case GeometryKind::ArcOfHyperbola: {
            const auto& h = std::get<ArcOfHyperbolaGeometry>(geometry.data);
            if (role == PointRole::Mid) { outPoint = h.center; return true; }
            return false;
        }
        case GeometryKind::ArcOfParabola: {
            const auto& p = std::get<ArcOfParabolaGeometry>(geometry.data);
            if (role == PointRole::Mid) { outPoint = p.vertex; return true; }
            return false;
        }
        case GeometryKind::BSpline: {
            // Start/end are the first/last poles, matching FreeCAD's
            // rebuildVertexIndex() which assigns two vertex ids to a
            // BSpline curve.
            const auto& b = std::get<BSplineGeometry>(geometry.data);
            if (b.poles.empty()) return false;
            if (role == PointRole::Start) { outPoint = b.poles.front().point; return true; }
            if (role == PointRole::End)   { outPoint = b.poles.back().point;  return true; }
            return false;
        }
    }
    return false;
}

}  // namespace McSolverEngine::Compat
