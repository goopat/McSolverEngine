#include "McSolverEngine/CompatModel.h"

namespace McSolverEngine::Compat
{

namespace
{

Geometry makeGeometry(GeometryKind kind, GeometryData data, bool construction, bool external, bool blocked)
{
    return Geometry {
        .kind = kind,
        .data = std::move(data),
        .construction = construction,
        .external = external,
        .blocked = blocked,
    };
}

}  // namespace

bool SketchModel::empty() const noexcept
{
    return geometries_.empty() && constraints_.empty();
}

std::size_t SketchModel::geometryCount() const noexcept
{
    return geometries_.size();
}

std::size_t SketchModel::constraintCount() const noexcept
{
    return constraints_.size();
}

int SketchModel::addPoint(Point2 point, bool construction, bool external, bool blocked)
{
    geometries_.push_back(makeGeometry(
        GeometryKind::Point,
        PointGeometry {.point = point},
        construction,
        external,
        blocked
    ));
    return static_cast<int>(geometries_.size() - 1);
}

int SketchModel::addLineSegment(Point2 start, Point2 end, bool construction, bool external, bool blocked)
{
    geometries_.push_back(makeGeometry(
        GeometryKind::LineSegment,
        LineSegmentGeometry {.start = start, .end = end},
        construction,
        external,
        blocked
    ));
    return static_cast<int>(geometries_.size() - 1);
}

int SketchModel::addCircle(Point2 center, double radius, bool construction, bool external, bool blocked)
{
    geometries_.push_back(makeGeometry(
        GeometryKind::Circle,
        CircleGeometry {.center = center, .radius = radius},
        construction,
        external,
        blocked
    ));
    return static_cast<int>(geometries_.size() - 1);
}

int SketchModel::addArc(
    Point2 center,
    double radius,
    double startAngle,
    double endAngle,
    bool construction,
    bool external,
    bool blocked
)
{
    geometries_.push_back(makeGeometry(
        GeometryKind::Arc,
        ArcGeometry {
            .center = center,
            .radius = radius,
            .startAngle = startAngle,
            .endAngle = endAngle,
        },
        construction,
        external,
        blocked
    ));
    return static_cast<int>(geometries_.size() - 1);
}

int SketchModel::addEllipse(
    Point2 center,
    Point2 focus1,
    double minorRadius,
    bool construction,
    bool external,
    bool blocked
)
{
    geometries_.push_back(makeGeometry(
        GeometryKind::Ellipse,
        EllipseGeometry {.center = center, .focus1 = focus1, .minorRadius = minorRadius},
        construction,
        external,
        blocked
    ));
    return static_cast<int>(geometries_.size() - 1);
}

int SketchModel::addArcOfEllipse(
    Point2 center,
    Point2 focus1,
    double minorRadius,
    double startAngle,
    double endAngle,
    bool construction,
    bool external,
    bool blocked
)
{
    geometries_.push_back(makeGeometry(
        GeometryKind::ArcOfEllipse,
        ArcOfEllipseGeometry {
            .center = center,
            .focus1 = focus1,
            .minorRadius = minorRadius,
            .startAngle = startAngle,
            .endAngle = endAngle,
        },
        construction,
        external,
        blocked
    ));
    return static_cast<int>(geometries_.size() - 1);
}

int SketchModel::addArcOfHyperbola(
    Point2 center,
    Point2 focus1,
    double minorRadius,
    double startAngle,
    double endAngle,
    bool construction,
    bool external,
    bool blocked
)
{
    geometries_.push_back(makeGeometry(
        GeometryKind::ArcOfHyperbola,
        ArcOfHyperbolaGeometry {
            .center = center,
            .focus1 = focus1,
            .minorRadius = minorRadius,
            .startAngle = startAngle,
            .endAngle = endAngle,
        },
        construction,
        external,
        blocked
    ));
    return static_cast<int>(geometries_.size() - 1);
}

int SketchModel::addArcOfParabola(
    Point2 vertex,
    Point2 focus1,
    double startAngle,
    double endAngle,
    bool construction,
    bool external,
    bool blocked
)
{
    geometries_.push_back(makeGeometry(
        GeometryKind::ArcOfParabola,
        ArcOfParabolaGeometry {
            .vertex = vertex,
            .focus1 = focus1,
            .startAngle = startAngle,
            .endAngle = endAngle,
        },
        construction,
        external,
        blocked
    ));
    return static_cast<int>(geometries_.size() - 1);
}

int SketchModel::addBSpline(
    std::vector<BSplinePole> poles,
    std::vector<BSplineKnot> knots,
    int degree,
    bool periodic,
    bool construction,
    bool external,
    bool blocked
)
{
    geometries_.push_back(makeGeometry(
        GeometryKind::BSpline,
        BSplineGeometry {
            .poles = std::move(poles),
            .knots = std::move(knots),
            .degree = degree,
            .periodic = periodic,
        },
        construction,
        external,
        blocked
    ));
    return static_cast<int>(geometries_.size() - 1);
}

int SketchModel::addConstraint(Constraint constraint)
{
    constraints_.push_back(std::move(constraint));
    return static_cast<int>(constraints_.size() - 1);
}

std::vector<Geometry>& SketchModel::geometries() noexcept
{
    return geometries_;
}

const std::vector<Geometry>& SketchModel::geometries() const noexcept
{
    return geometries_;
}

std::vector<Constraint>& SketchModel::constraints() noexcept
{
    return constraints_;
}

const std::vector<Constraint>& SketchModel::constraints() const noexcept
{
    return constraints_;
}

Placement& SketchModel::placement() noexcept
{
    return placement_;
}

const Placement& SketchModel::placement() const noexcept
{
    return placement_;
}

void SketchModel::setPlacement(Placement placement) noexcept
{
    placement_ = placement;
}

bool isPhase1ConstraintSupported(ConstraintKind kind) noexcept
{
    switch (kind) {
        case ConstraintKind::Coincident:
        case ConstraintKind::Horizontal:
        case ConstraintKind::Vertical:
        case ConstraintKind::DistanceX:
        case ConstraintKind::DistanceY:
        case ConstraintKind::Distance:
        case ConstraintKind::Parallel:
        case ConstraintKind::Tangent:
        case ConstraintKind::Perpendicular:
        case ConstraintKind::Angle:
        case ConstraintKind::Radius:
        case ConstraintKind::Diameter:
        case ConstraintKind::Equal:
        case ConstraintKind::Symmetric:
        case ConstraintKind::PointOnObject:
        case ConstraintKind::InternalAlignment:
        case ConstraintKind::SnellsLaw:
        case ConstraintKind::Block:
        case ConstraintKind::Weight:
            return true;
    }

    return false;
}

std::string_view toString(GeometryKind kind) noexcept
{
    switch (kind) {
        case GeometryKind::Point:
            return "Point";
        case GeometryKind::LineSegment:
            return "LineSegment";
        case GeometryKind::Circle:
            return "Circle";
        case GeometryKind::Arc:
            return "Arc";
        case GeometryKind::Ellipse:
            return "Ellipse";
        case GeometryKind::ArcOfEllipse:
            return "ArcOfEllipse";
        case GeometryKind::ArcOfHyperbola:
            return "ArcOfHyperbola";
        case GeometryKind::ArcOfParabola:
            return "ArcOfParabola";
        case GeometryKind::BSpline:
            return "BSpline";
    }

    return "UnknownGeometry";
}

std::string_view toString(ConstraintKind kind) noexcept
{
    switch (kind) {
        case ConstraintKind::Coincident:
            return "Coincident";
        case ConstraintKind::Horizontal:
            return "Horizontal";
        case ConstraintKind::Vertical:
            return "Vertical";
        case ConstraintKind::DistanceX:
            return "DistanceX";
        case ConstraintKind::DistanceY:
            return "DistanceY";
        case ConstraintKind::Distance:
            return "Distance";
        case ConstraintKind::Parallel:
            return "Parallel";
        case ConstraintKind::Tangent:
            return "Tangent";
        case ConstraintKind::Perpendicular:
            return "Perpendicular";
        case ConstraintKind::Angle:
            return "Angle";
        case ConstraintKind::Radius:
            return "Radius";
        case ConstraintKind::Diameter:
            return "Diameter";
        case ConstraintKind::Equal:
            return "Equal";
        case ConstraintKind::Symmetric:
            return "Symmetric";
        case ConstraintKind::PointOnObject:
            return "PointOnObject";
        case ConstraintKind::InternalAlignment:
            return "InternalAlignment";
        case ConstraintKind::SnellsLaw:
            return "SnellsLaw";
        case ConstraintKind::Block:
            return "Block";
        case ConstraintKind::Weight:
            return "Weight";
    }

    return "UnknownConstraint";
}

std::string_view toString(InternalAlignmentType kind) noexcept
{
    switch (kind) {
        case InternalAlignmentType::Undef:
            return "Undef";
        case InternalAlignmentType::EllipseMajorDiameter:
            return "EllipseMajorDiameter";
        case InternalAlignmentType::EllipseMinorDiameter:
            return "EllipseMinorDiameter";
        case InternalAlignmentType::EllipseFocus1:
            return "EllipseFocus1";
        case InternalAlignmentType::EllipseFocus2:
            return "EllipseFocus2";
        case InternalAlignmentType::HyperbolaMajor:
            return "HyperbolaMajor";
        case InternalAlignmentType::HyperbolaMinor:
            return "HyperbolaMinor";
        case InternalAlignmentType::HyperbolaFocus:
            return "HyperbolaFocus";
        case InternalAlignmentType::ParabolaFocus:
            return "ParabolaFocus";
        case InternalAlignmentType::BSplineControlPoint:
            return "BSplineControlPoint";
        case InternalAlignmentType::BSplineKnotPoint:
            return "BSplineKnotPoint";
        case InternalAlignmentType::ParabolaFocalAxis:
            return "ParabolaFocalAxis";
    }

    return "UnknownInternalAlignment";
}

}  // namespace McSolverEngine::Compat
