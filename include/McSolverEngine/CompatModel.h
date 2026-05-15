#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "McSolverEngine/Export.h"

namespace McSolverEngine
{

using ParameterMap = std::map<std::string, std::string>;

}

namespace McSolverEngine::Compat
{

struct Point2
{
    double x {};
    double y {};
};

struct Placement
{
    double px {};
    double py {};
    double pz {};
    double qx {};
    double qy {};
    double qz {};
    double qw {1.0};
};

struct PointGeometry
{
    Point2 point {};
};

struct LineSegmentGeometry
{
    Point2 start {};
    Point2 end {};
};

struct CircleGeometry
{
    Point2 center {};
    double radius {};
};

struct ArcGeometry
{
    Point2 center {};
    double radius {};
    double startAngle {};
    double endAngle {};
};

struct EllipseGeometry
{
    Point2 center {};
    Point2 focus1 {};
    double minorRadius {};
};

struct ArcOfEllipseGeometry
{
    Point2 center {};
    Point2 focus1 {};
    double minorRadius {};
    double startAngle {};
    double endAngle {};
};

struct ArcOfHyperbolaGeometry
{
    Point2 center {};
    Point2 focus1 {};
    double minorRadius {};
    double startAngle {};
    double endAngle {};
};

struct ArcOfParabolaGeometry
{
    Point2 vertex {};
    Point2 focus1 {};
    double startAngle {};
    double endAngle {};
};

struct BSplinePole
{
    Point2 point {};
    double weight {1.0};
};

struct BSplineKnot
{
    double value {};
    int multiplicity {1};
};

struct BSplineGeometry
{
    std::vector<BSplinePole> poles;
    std::vector<BSplineKnot> knots;
    int degree {2};
    bool periodic {false};
};

enum class GeometryKind
{
    Point,
    LineSegment,
    Circle,
    Arc,
    Ellipse,
    ArcOfEllipse,
    ArcOfHyperbola,
    ArcOfParabola,
    BSpline
};

using GeometryData = std::variant<
    PointGeometry,
    LineSegmentGeometry,
    CircleGeometry,
    ArcGeometry,
    EllipseGeometry,
    ArcOfEllipseGeometry,
    ArcOfHyperbolaGeometry,
    ArcOfParabolaGeometry,
    BSplineGeometry>;

struct Geometry
{
    GeometryKind kind {GeometryKind::Point};
    GeometryData data {PointGeometry {}};
    bool construction {false};
    bool external {false};
    bool blocked {false};
    int originalId {-99999999};
};

enum class PointRole
{
    None,
    Start,
    End,
    Mid,
    Center
};

struct ElementRef
{
    int geometryIndex {-1};
    PointRole role {PointRole::None};
};

enum class InternalAlignmentType
{
    Undef = 0,
    EllipseMajorDiameter = 1,
    EllipseMinorDiameter = 2,
    EllipseFocus1 = 3,
    EllipseFocus2 = 4,
    HyperbolaMajor = 5,
    HyperbolaMinor = 6,
    HyperbolaFocus = 7,
    ParabolaFocus = 8,
    BSplineControlPoint = 9,
    BSplineKnotPoint = 10,
    ParabolaFocalAxis = 11
};

enum class ConstraintKind
{
    Coincident,
    Horizontal,
    Vertical,
    DistanceX,
    DistanceY,
    Distance,
    Parallel,
    Tangent,
    Perpendicular,
    Angle,
    Radius,
    Diameter,
    Equal,
    Symmetric,
    PointOnObject,
    InternalAlignment,
    SnellsLaw,
    Block,
    Weight
};

struct Constraint
{
    ConstraintKind kind {ConstraintKind::Coincident};
    ElementRef first {};
    ElementRef second {};
    ElementRef third {};
    double value {};
    bool hasValue {false};
    bool driving {true};
    InternalAlignmentType alignmentType {InternalAlignmentType::Undef};
    int internalAlignmentIndex {-1};
    std::string parameterName;
    std::string parameterKey;
    std::string parameterExpression;
    double parameterDefaultValue {};
    bool hasParameterDefaultValue {false};
};

class MCSOLVERENGINE_EXPORT SketchModel
{
public:
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t geometryCount() const noexcept;
    [[nodiscard]] std::size_t constraintCount() const noexcept;

    int addPoint(Point2 point, bool construction = false, bool external = false, bool blocked = false);
    int addLineSegment(
        Point2 start,
        Point2 end,
        bool construction = false,
        bool external = false,
        bool blocked = false
    );
    int addCircle(
        Point2 center,
        double radius,
        bool construction = false,
        bool external = false,
        bool blocked = false
    );
    int addArc(
        Point2 center,
        double radius,
        double startAngle,
        double endAngle,
        bool construction = false,
        bool external = false,
        bool blocked = false
    );
    int addEllipse(
        Point2 center,
        Point2 focus1,
        double minorRadius,
        bool construction = false,
        bool external = false,
        bool blocked = false
    );
    int addArcOfEllipse(
        Point2 center,
        Point2 focus1,
        double minorRadius,
        double startAngle,
        double endAngle,
        bool construction = false,
        bool external = false,
        bool blocked = false
    );
    int addArcOfHyperbola(
        Point2 center,
        Point2 focus1,
        double minorRadius,
        double startAngle,
        double endAngle,
        bool construction = false,
        bool external = false,
        bool blocked = false
    );
    int addArcOfParabola(
        Point2 vertex,
        Point2 focus1,
        double startAngle,
        double endAngle,
        bool construction = false,
        bool external = false,
        bool blocked = false
    );
    int addBSpline(
        std::vector<BSplinePole> poles,
        std::vector<BSplineKnot> knots,
        int degree,
        bool periodic = false,
        bool construction = false,
        bool external = false,
        bool blocked = false
    );
    int addConstraint(Constraint constraint);

    [[nodiscard]] std::vector<Geometry>& geometries() noexcept;
    [[nodiscard]] const std::vector<Geometry>& geometries() const noexcept;
    [[nodiscard]] std::vector<Constraint>& constraints() noexcept;
    [[nodiscard]] const std::vector<Constraint>& constraints() const noexcept;
    [[nodiscard]] Placement& placement() noexcept;
    [[nodiscard]] const Placement& placement() const noexcept;
    void setPlacement(Placement placement) noexcept;

private:
    std::vector<Geometry> geometries_;
    std::vector<Constraint> constraints_;
    Placement placement_ {};
};

[[nodiscard]] MCSOLVERENGINE_EXPORT bool isPhase1ConstraintSupported(ConstraintKind kind) noexcept;
[[nodiscard]] MCSOLVERENGINE_EXPORT std::string_view toString(GeometryKind kind) noexcept;
[[nodiscard]] MCSOLVERENGINE_EXPORT std::string_view toString(ConstraintKind kind) noexcept;
[[nodiscard]] MCSOLVERENGINE_EXPORT std::string_view toString(InternalAlignmentType kind) noexcept;

}  // namespace McSolverEngine::Compat
