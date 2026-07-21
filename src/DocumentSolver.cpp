// ──────────────────────────────────────────────────────────────────────────
// Cross-sketch cascade: external geometry update
//
// After a source sketch is solved, its geometry coordinates are projected
// onto the dependent sketch's plane (via PlaneTransform) and written into
// the dependent's external geometry elements.  This replaces the stale
// coordinate copies that were embedded in the Document.xml at save time.
//
// Handles two sub-element reference flavours:
//   EdgeN   – copies the whole geometry (same-kind, e.g. Line→Line)
//   VertexN – extracts a single point from the source geometry and
//             writes it into a target Point geometry.
// ──────────────────────────────────────────────────────────────────────────

#include "DocumentSolver.h"

#include "PlaneTransform.h"
#include "SubElementResolver.h"

namespace McSolverEngine::Compat
{

namespace
{

// ── Per-kind coordinate-copy helpers ──
// Each helper receives the target geometry variant, the solved source
// geometry variant, the plane-to-plane transform, and optional skip/message
// out-parameters.  Returns true if the update was applied.

bool updatePoint(PointGeometry& geo, const PointGeometry& src, const PlaneTransform& xf)
{
    geo.point = xf.apply(src.point);
    return true;
}

bool updateLineSegment(LineSegmentGeometry& geo, const LineSegmentGeometry& src, const PlaneTransform& xf)
{
    geo.start = xf.apply(src.start);
    geo.end = xf.apply(src.end);
    return true;
}

bool updateCircle(
    CircleGeometry& geo,
    const CircleGeometry& src,
    const PlaneTransform& xf,
    int& skippedCount,
    std::vector<std::string>& messages,
    const std::string& refDescription
)
{
    if (!xf.isConformal()) {
        ++skippedCount;
        messages.push_back(
            "Skipped Circle external geometry update for " + refDescription
            + ": source and target sketch planes are not parallel (circle would become ellipse)."
        );
        return false;
    }
    geo.center = xf.apply(src.center);
    geo.radius = src.radius * xf.scaleFactor();
    return true;
}

bool updateArc(
    ArcGeometry& geo,
    const ArcGeometry& src,
    const PlaneTransform& xf,
    int& skippedCount,
    std::vector<std::string>& messages,
    const std::string& refDescription
)
{
    if (!xf.isConformal()) {
        ++skippedCount;
        messages.push_back(
            "Skipped Arc external geometry update for " + refDescription
            + ": source and target sketch planes are not parallel (arc would become arc-of-ellipse)."
        );
        return false;
    }
    geo.center = xf.apply(src.center);
    geo.radius = src.radius * xf.scaleFactor();
    // Arc angles are polar angles around the center, so they are NOT
    // preserved under a general conformal transform:
    //   rotation   (det > 0): α → α + θ
    //   reflection (det < 0): α → φ − α  (arc direction flips, so the
    //                          start/end angles are swapped as well)
    const double phi = xf.referenceAngle();
    if (!xf.isReflection()) {
        geo.startAngle = src.startAngle + phi;
        geo.endAngle = src.endAngle + phi;
    } else {
        geo.startAngle = phi - src.endAngle;
        geo.endAngle = phi - src.startAngle;
    }
    return true;
}

// Conic-arc angles are measured relative to the major axis (the focus1
// direction), which is transformed together with the geometry.  They are
// therefore preserved under rotation, but flip sign under reflection —
// with start/end swapped to keep the arc oriented consistently.
void transformConicAngles(
    const PlaneTransform& xf,
    const double srcStart,
    const double srcEnd,
    double& outStart,
    double& outEnd
)
{
    if (!xf.isReflection()) {
        outStart = srcStart;
        outEnd = srcEnd;
    } else {
        outStart = -srcEnd;
        outEnd = -srcStart;
    }
}

bool updateEllipse(
    EllipseGeometry& geo,
    const EllipseGeometry& src,
    const PlaneTransform& xf,
    int& skippedCount,
    std::vector<std::string>& messages,
    const std::string& refDescription
)
{
    if (!xf.isConformal()) {
        ++skippedCount;
        messages.push_back(
            "Skipped Ellipse external geometry update for " + refDescription
            + ": non-conformal transform not yet handled."
        );
        return false;
    }
    geo.center = xf.apply(src.center);
    geo.focus1 = xf.apply(src.focus1);
    geo.minorRadius = src.minorRadius * xf.scaleFactor();
    return true;
}

bool updateArcOfEllipse(
    ArcOfEllipseGeometry& geo,
    const ArcOfEllipseGeometry& src,
    const PlaneTransform& xf,
    int& skippedCount,
    std::vector<std::string>& messages,
    const std::string& refDescription
)
{
    if (!xf.isConformal()) {
        ++skippedCount;
        messages.push_back(
            "Skipped ArcOfEllipse external geometry update for " + refDescription
            + ": non-conformal transform not yet handled."
        );
        return false;
    }
    geo.center = xf.apply(src.center);
    geo.focus1 = xf.apply(src.focus1);
    geo.minorRadius = src.minorRadius * xf.scaleFactor();
    transformConicAngles(xf, src.startAngle, src.endAngle, geo.startAngle, geo.endAngle);
    return true;
}

bool updateArcOfHyperbola(
    ArcOfHyperbolaGeometry& geo,
    const ArcOfHyperbolaGeometry& src,
    const PlaneTransform& xf,
    int& skippedCount,
    std::vector<std::string>& messages,
    const std::string& refDescription
)
{
    if (!xf.isConformal()) {
        ++skippedCount;
        messages.push_back(
            "Skipped ArcOfHyperbola external geometry update for " + refDescription
            + ": non-conformal transform not yet handled."
        );
        return false;
    }
    geo.center = xf.apply(src.center);
    geo.focus1 = xf.apply(src.focus1);
    geo.minorRadius = src.minorRadius * xf.scaleFactor();
    transformConicAngles(xf, src.startAngle, src.endAngle, geo.startAngle, geo.endAngle);
    return true;
}

bool updateArcOfParabola(
    ArcOfParabolaGeometry& geo,
    const ArcOfParabolaGeometry& src,
    const PlaneTransform& xf,
    int& skippedCount,
    std::vector<std::string>& messages,
    const std::string& refDescription
)
{
    if (!xf.isConformal()) {
        ++skippedCount;
        messages.push_back(
            "Skipped ArcOfParabola external geometry update for " + refDescription
            + ": non-conformal transform not yet handled."
        );
        return false;
    }
    geo.vertex = xf.apply(src.vertex);
    geo.focus1 = xf.apply(src.focus1);
    transformConicAngles(xf, src.startAngle, src.endAngle, geo.startAngle, geo.endAngle);
    return true;
}

bool updateBSpline(
    BSplineGeometry& geo,
    const BSplineGeometry& src,
    const PlaneTransform& xf
)
{
    if (geo.poles.size() != src.poles.size()) {
        return false;
    }
    for (std::size_t i = 0; i < src.poles.size(); ++i) {
        geo.poles[i].point = xf.apply(src.poles[i].point);
        geo.poles[i].weight = src.poles[i].weight;
    }
    geo.knots = src.knots;  // knot vector unchanged
    geo.degree = src.degree;
    geo.periodic = src.periodic;
    return true;
}

// Build a human-readable reference description for log messages.
std::string refDescription(
    const std::string& sourceObject,
    const std::string& sourceSub
)
{
    return sourceObject + "." + sourceSub;
}

}  // namespace

ExternalGeometryUpdateResult updateExternalGeometry(
    SketchModel& dependent,
    const std::string& sourceObjectName,
    const SketchModel& solvedSource
)
{
    ExternalGeometryUpdateResult result;

    const auto xf = PlaneTransform::between(
        solvedSource.placement(),
        dependent.placement()
    );

    for (auto& geo : dependent.geometries()) {
        if (!geo.external) {
            continue;
        }
        if (!geo.externalSource.has_value()) {
            continue;
        }
        if (geo.externalSource->sourceObject != sourceObjectName) {
            continue;
        }

        const auto desc = refDescription(
            sourceObjectName,
            geo.externalSource->sourceSub
        );

        const auto simpleName = extractSimpleElementName(geo.externalSource->sourceSub);

        // ── Vertex reference ────────────────────────────────────────────
        // e.g. "Vertex1" or "Vertex2".  The source geometry is a curve
        // (LineSegment, Arc, …) but the external geometry is a Point that
        // was projected from that curve's endpoint.  We resolve the vertex
        // index to a concrete (geometryIndex, PointRole) pair, extract the
        // 2D point, project it between sketch planes, and write it into the
        // target Point.
        if (isVertexSubElementName(simpleName)) {
            auto vertexRef = resolveVertexSubElement(solvedSource, geo.externalSource->sourceSub);
            if (vertexRef.geometryIndex < 0) {
                ++result.missingRefCount;
                result.messages.push_back(
                    "External geometry references missing source element: " + desc);
                continue;
            }

            const auto& srcGeo =
                solvedSource.geometries()[static_cast<std::size_t>(vertexRef.geometryIndex)];

            Point2 srcPoint;
            if (!extractVertexPoint(srcGeo, vertexRef.pointRole, srcPoint)) {
                result.messages.push_back(
                    "Cannot extract vertex from source geometry for " + desc);
                continue;
            }

            if (geo.kind == GeometryKind::Point) {
                std::get<PointGeometry>(geo.data).point = xf.apply(srcPoint);
                ++result.updatedCount;
            } else {
                result.messages.push_back(
                    "Vertex reference target is not a Point for " + desc);
            }
            continue;
        }

        // ── Edge reference ────────────────────────────────────────────
        // e.g. "Edge1" or "Edge2".  Both the source and the external
        // geometry have the same kind (LineSegment, Circle, Arc, …).
        // We copy the source coordinates through the plane-to-plane
        // transform.  Circle / Arc additionally require the transform to
        // be conformal (parallel sketch planes).
        const int srcIdx = resolveSubElementToGeoIndex(
            solvedSource,
            geo.externalSource->sourceSub
        );

        if (srcIdx < 0) {
            ++result.missingRefCount;
            result.messages.push_back(
                "External geometry references missing source element: " + desc);
            continue;
        }

        const auto& srcGeo = solvedSource.geometries()[static_cast<std::size_t>(srcIdx)];

        bool ok = false;

        // Both source and target must have the same geometry kind for the
        // coordinate copy to be meaningful.
        if (geo.kind != srcGeo.kind) {
            result.messages.push_back(
                "Skipped external geometry update for " + desc
                + ": geometry kind mismatch between source and external."
            );
            continue;
        }

        switch (geo.kind) {
            case GeometryKind::Point:
                ok = updatePoint(
                    std::get<PointGeometry>(geo.data),
                    std::get<PointGeometry>(srcGeo.data),
                    xf
                );
                break;
            case GeometryKind::LineSegment:
                ok = updateLineSegment(
                    std::get<LineSegmentGeometry>(geo.data),
                    std::get<LineSegmentGeometry>(srcGeo.data),
                    xf
                );
                break;
            case GeometryKind::Circle:
                ok = updateCircle(
                    std::get<CircleGeometry>(geo.data),
                    std::get<CircleGeometry>(srcGeo.data),
                    xf,
                    result.skippedCount,
                    result.messages,
                    desc
                );
                break;
            case GeometryKind::Arc:
                ok = updateArc(
                    std::get<ArcGeometry>(geo.data),
                    std::get<ArcGeometry>(srcGeo.data),
                    xf,
                    result.skippedCount,
                    result.messages,
                    desc
                );
                break;
            case GeometryKind::Ellipse:
                ok = updateEllipse(
                    std::get<EllipseGeometry>(geo.data),
                    std::get<EllipseGeometry>(srcGeo.data),
                    xf,
                    result.skippedCount,
                    result.messages,
                    desc
                );
                break;
            case GeometryKind::ArcOfEllipse:
                ok = updateArcOfEllipse(
                    std::get<ArcOfEllipseGeometry>(geo.data),
                    std::get<ArcOfEllipseGeometry>(srcGeo.data),
                    xf,
                    result.skippedCount,
                    result.messages,
                    desc
                );
                break;
            case GeometryKind::ArcOfHyperbola:
                ok = updateArcOfHyperbola(
                    std::get<ArcOfHyperbolaGeometry>(geo.data),
                    std::get<ArcOfHyperbolaGeometry>(srcGeo.data),
                    xf,
                    result.skippedCount,
                    result.messages,
                    desc
                );
                break;
            case GeometryKind::ArcOfParabola:
                ok = updateArcOfParabola(
                    std::get<ArcOfParabolaGeometry>(geo.data),
                    std::get<ArcOfParabolaGeometry>(srcGeo.data),
                    xf,
                    result.skippedCount,
                    result.messages,
                    desc
                );
                break;
            case GeometryKind::BSpline:
                ok = updateBSpline(
                    std::get<BSplineGeometry>(geo.data),
                    std::get<BSplineGeometry>(srcGeo.data),
                    xf
                );
                break;
        }

        if (ok) {
            ++result.updatedCount;
        }
    }

    return result;
}

}  // namespace McSolverEngine::Compat
