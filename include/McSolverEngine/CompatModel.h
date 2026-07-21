#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "McSolverEngine/Export.h"

namespace McSolverEngine
{

using ParameterMap = std::map<std::string, std::string>;

}

namespace McSolverEngine::Compat
{

// Forward declarations for friend declarations in SketchModel
enum class SolveStatus;
struct SolveResult;

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

struct ExternalGeometrySource
{
    std::string sourceObject;   // source sketch object name, e.g. "SketchB"
    std::string sourceSub;      // sub-element name, e.g. "Edge1"
};

struct Geometry
{
    GeometryKind kind {GeometryKind::Point};
    GeometryData data {PointGeometry {}};
    bool construction {false};
    bool external {false};
    bool blocked {false};
    int originalId {-99999999};
    std::optional<ExternalGeometrySource> externalSource;
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
    int originalIndex {-99999999};
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

    // ──── Cross-sketch cascade context ────
    // Dependency sketches imported from the same Document.xml.
    // Key: source object name (e.g. "SketchB").
    // Populated by importSketchFromDocumentXml when ExternalGeometry
    // links reference other sketches in the same document.
    std::unordered_map<std::string, SketchModel> dependencyModels_;

    // Topologically sorted dependency names (DFS post-order).
    // Empty when there are no cross-sketch references or a cycle
    // was detected during import.
    std::vector<std::string> dependencySolveOrder_;

    // Set when a dependency cycle is detected during import.
    bool hasDependencyCycle_ {false};

    friend SolveResult solveSketch(SketchModel& model);
    friend SolveResult solveSketch(SketchModel& model, const McSolverEngine::ParameterMap& parameters);
    friend class SketchModelInternalAccess;
};

[[nodiscard]] MCSOLVERENGINE_EXPORT bool isPhase1ConstraintSupported(ConstraintKind kind) noexcept;
[[nodiscard]] MCSOLVERENGINE_EXPORT std::string_view toString(GeometryKind kind) noexcept;
[[nodiscard]] MCSOLVERENGINE_EXPORT std::string_view toString(ConstraintKind kind) noexcept;
[[nodiscard]] MCSOLVERENGINE_EXPORT std::string_view toString(InternalAlignmentType kind) noexcept;

// Internal accessor for DocumentXml import functions.
// Not part of the public API.
class SketchModelInternalAccess
{
public:
    static void emplaceDependency(
        SketchModel& model,
        const std::string& name,
        SketchModel&& dep
    )
    {
        model.dependencyModels_.emplace(name, std::move(dep));
    }

    static void setDependencySolveOrder(
        SketchModel& model,
        std::vector<std::string>&& order
    )
    {
        model.dependencySolveOrder_ = std::move(order);
    }

    [[nodiscard]] static SketchModel* findDependency(
        SketchModel& model,
        const std::string& name
    )
    {
        auto it = model.dependencyModels_.find(name);
        return it != model.dependencyModels_.end() ? &it->second : nullptr;
    }

    static void setHasDependencyCycle(SketchModel& model, bool value)
    {
        model.hasDependencyCycle_ = value;
    }

    [[nodiscard]] static bool hasDependencyCycle(const SketchModel& model)
    {
        return model.hasDependencyCycle_;
    }
};

}  // namespace McSolverEngine::Compat
