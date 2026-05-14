#include <cmath>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numbers>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef MCSOLVERENGINE_WITH_OCCT
#    define MCSOLVERENGINE_WITH_OCCT 0
#endif

#if MCSOLVERENGINE_WITH_OCCT
#include <BRepAdaptor_Curve.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Curve.hxx>
#include <Geom_Line.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#endif

#include "McSolverEngine/BRepExport.h"
#include "McSolverEngine/CompatModel.h"
#include "McSolverEngine/CompatSolver.h"
#include "McSolverEngine/DocumentXml.h"
#include "McSolverEngine/Engine.h"
#include "McSolverEngine/GeometryExport.h"
#include "src/WindowsAssertMode.h"
#include "src/planegcs/GCS.h"

namespace
{

struct Point3
{
    double x {};
    double y {};
    double z {};
};

bool samePoint(const Point3& point, double x, double y, double z, double tolerance = 1e-8)
{
    return std::abs(point.x - x) <= tolerance && std::abs(point.y - y) <= tolerance
        && std::abs(point.z - z) <= tolerance;
}

Point3 transformPoint(const McSolverEngine::Compat::Placement& placement, double x, double y, double z = 0.0)
{
    const double magnitude = std::sqrt(
        placement.qx * placement.qx + placement.qy * placement.qy + placement.qz * placement.qz
        + placement.qw * placement.qw
    );
    const double qx = magnitude > 0.0 ? placement.qx / magnitude : 0.0;
    const double qy = magnitude > 0.0 ? placement.qy / magnitude : 0.0;
    const double qz = magnitude > 0.0 ? placement.qz / magnitude : 0.0;
    const double qw = magnitude > 0.0 ? placement.qw / magnitude : 1.0;
    const double xx = qx * qx;
    const double yy = qy * qy;
    const double zz = qz * qz;
    const double xy = qx * qy;
    const double xz = qx * qz;
    const double yz = qy * qz;
    const double wx = qw * qx;
    const double wy = qw * qy;
    const double wz = qw * qz;

    return Point3 {
        .x = placement.px + (1.0 - 2.0 * (yy + zz)) * x + 2.0 * (xy - wz) * y + 2.0 * (xz + wy) * z,
        .y = placement.py + 2.0 * (xy + wz) * x + (1.0 - 2.0 * (xx + zz)) * y + 2.0 * (yz - wx) * z,
        .z = placement.pz + 2.0 * (xz - wy) * x + 2.0 * (yz + wx) * y + (1.0 - 2.0 * (xx + yy)) * z,
    };
}

double lineLength(const McSolverEngine::Compat::LineSegmentGeometry& line)
{
    return std::hypot(line.end.x - line.start.x, line.end.y - line.start.y);
}

double lineAngleDegrees(const McSolverEngine::Compat::LineSegmentGeometry& line)
{
    return std::atan2(line.end.y - line.start.y, line.end.x - line.start.x) * 180.0 / std::numbers::pi;
}

McSolverEngine::Compat::Point2 evaluateBSpline(
    const McSolverEngine::Compat::BSplineGeometry& splineGeometry,
    double parameter
)
{
    std::vector<double> poleXs;
    std::vector<double> poleYs;
    std::vector<double> weights;
    std::vector<double> knots;
    poleXs.reserve(splineGeometry.poles.size());
    poleYs.reserve(splineGeometry.poles.size());
    weights.reserve(splineGeometry.poles.size());
    knots.reserve(splineGeometry.knots.size());

    GCS::BSpline spline;
    spline.degree = splineGeometry.degree;
    spline.periodic = splineGeometry.periodic;
    spline.mult.reserve(splineGeometry.knots.size());

    for (const auto& pole : splineGeometry.poles) {
        poleXs.push_back(pole.point.x);
        poleYs.push_back(pole.point.y);
        weights.push_back(pole.weight);
    }
    for (std::size_t index = 0; index < splineGeometry.poles.size(); ++index) {
        spline.poles.push_back({&poleXs[index], &poleYs[index]});
        spline.weights.push_back(&weights[index]);
    }
    for (const auto& knot : splineGeometry.knots) {
        knots.push_back(knot.value);
        spline.knots.push_back(&knots.back());
        spline.mult.push_back(knot.multiplicity);
    }
    spline.setupFlattenedKnots();

    const auto value = spline.Value(parameter, 0.0);
    return {.x = value.x, .y = value.y};
}

McSolverEngine::Compat::Point2 bsplineTangentDirection(
    const McSolverEngine::Compat::BSplineGeometry& splineGeometry,
    double parameter
)
{
    constexpr double epsilon = 1e-6;
    const auto before = evaluateBSpline(splineGeometry, parameter - epsilon);
    const auto after = evaluateBSpline(splineGeometry, parameter + epsilon);
    return {.x = after.x - before.x, .y = after.y - before.y};
}

#if MCSOLVERENGINE_WITH_OCCT

std::string formatDouble(double value)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(8);
    stream << value;
    return stream.str();
}

std::string formatPoint(const gp_Pnt& point)
{
    return formatDouble(point.X()) + "," + formatDouble(point.Y()) + "," + formatDouble(point.Z());
}

std::vector<std::string> describeEdges(const TopoDS_Shape& shape)
{
    std::vector<std::string> descriptions;

    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());

        Standard_Real first = 0.0;
        Standard_Real last = 0.0;
        const Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, first, last);
        if (curve.IsNull()) {
            descriptions.push_back("NullCurve");
            continue;
        }

        TopoDS_Vertex firstVertex;
        TopoDS_Vertex lastVertex;
        TopExp::Vertices(edge, firstVertex, lastVertex);
        const gp_Pnt firstPoint = BRep_Tool::Pnt(firstVertex);
        const gp_Pnt lastPoint = BRep_Tool::Pnt(lastVertex);

        if (const Handle(Geom_Line) line = Handle(Geom_Line)::DownCast(curve)) {
            (void)line;
            descriptions.push_back("Line:" + formatPoint(firstPoint) + "->" + formatPoint(lastPoint));
            continue;
        }

        if (const Handle(Geom_Circle) circle = Handle(Geom_Circle)::DownCast(curve)) {
            descriptions.push_back(
                "Circle:" + formatPoint(circle->Position().Location())
                + ":r=" + formatDouble(circle->Radius())
                + ":p=" + formatDouble(first) + "," + formatDouble(last)
                + ":" + formatPoint(firstPoint) + "->" + formatPoint(lastPoint)
            );
            continue;
        }

        descriptions.push_back("OtherCurve");
    }

    std::sort(descriptions.begin(), descriptions.end());
    return descriptions;
}

bool readBrepFromString(const std::string& brep, TopoDS_Shape& outShape)
{
    std::istringstream stream(brep);
    BRep_Builder builder;
    BRepTools::Read(outShape, stream, builder);
    return !outShape.IsNull();
}

bool hasExplicitIdentityLocationBlock(const std::string& brep)
{
    static constexpr std::string_view block =
        "Locations 1\n"
        "1\n"
        "1.000000000000000 0.000000000000000 0.000000000000000 0.000000000000000 \n"
        "0.000000000000000 1.000000000000000 0.000000000000000 0.000000000000000 \n"
        "0.000000000000000 0.000000000000000 1.000000000000000 0.000000000000000 \n";
    return brep.find(block) != std::string::npos;
}

bool hasExplicitRootLocationBlock(const std::string& brep)
{
    return brep.find("Locations 1\n1\n") != std::string::npos;
}

bool sameTransform(const gp_Trsf& first, const gp_Trsf& second, double tolerance = 1e-12)
{
    for (int row = 1; row <= 3; ++row) {
        for (int col = 1; col <= 4; ++col) {
            if (std::abs(first.Value(row, col) - second.Value(row, col)) > tolerance) {
                return false;
            }
        }
    }
    return true;
}

std::vector<std::string> tokenizeBrep(const std::string& value)
{
    std::vector<std::string> tokens;
    std::istringstream stream(value);
    for (std::string token; stream >> token;) {
        tokens.push_back(std::move(token));
    }
    return tokens;
}

bool tryParseFloatingToken(const std::string& value, double& parsed)
{
    char* end = nullptr;
    parsed = std::strtod(value.c_str(), &end);
    return end != nullptr && *end == '\0';
}

bool sameBrepTokens(const std::string& expected, const std::string& actual, double tolerance = 1e-9)
{
    const auto expectedTokens = tokenizeBrep(expected);
    const auto actualTokens = tokenizeBrep(actual);
    if (expectedTokens.size() != actualTokens.size()) {
        return false;
    }

    for (std::size_t index = 0; index < expectedTokens.size(); ++index) {
        double expectedNumber = 0.0;
        double actualNumber = 0.0;
        if (tryParseFloatingToken(expectedTokens[index], expectedNumber)
            && tryParseFloatingToken(actualTokens[index], actualNumber)) {
            if (std::abs(expectedNumber - actualNumber) > tolerance) {
                return false;
            }
            continue;
        }
        if (expectedTokens[index] != actualTokens[index]) {
            return false;
        }
    }
    return true;
}

bool samePoint(const Point3& point, const gp_Pnt& expected, double tolerance = 1e-8)
{
    return samePoint(point, expected.X(), expected.Y(), expected.Z(), tolerance);
}

struct LineDescriptor
{
    gp_Pnt start;
    gp_Pnt end;
};

struct CircleDescriptor
{
    gp_Pnt center;
    double radius {};
};

std::vector<LineDescriptor> collectLineDescriptors(const TopoDS_Shape& shape)
{
    std::vector<LineDescriptor> lines;
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        BRepAdaptor_Curve curve(edge);
        if (curve.GetType() != GeomAbs_Line) {
            continue;
        }
        lines.push_back({
            .start = curve.Value(curve.FirstParameter()),
            .end = curve.Value(curve.LastParameter()),
        });
    }
    return lines;
}

std::vector<CircleDescriptor> collectCircleDescriptors(const TopoDS_Shape& shape)
{
    std::vector<CircleDescriptor> circles;
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        BRepAdaptor_Curve curve(edge);
        if (curve.GetType() != GeomAbs_Circle) {
            continue;
        }
        const gp_Circ circle = curve.Circle();
        circles.push_back({
            .center = circle.Location(),
            .radius = circle.Radius(),
        });
    }
    return circles;
}

bool matchesLineGeometry(
    const McSolverEngine::Geometry::GeometryRecord& record,
    const McSolverEngine::Compat::Placement& placement,
    const LineDescriptor& line,
    double tolerance = 1e-8
)
{
    if (record.geometry.kind != McSolverEngine::Compat::GeometryKind::LineSegment) {
        return false;
    }

    const auto& segment = std::get<McSolverEngine::Compat::LineSegmentGeometry>(record.geometry.data);
    const auto start = transformPoint(placement, segment.start.x, segment.start.y);
    const auto end = transformPoint(placement, segment.end.x, segment.end.y);

    return (samePoint(start, line.start, tolerance) && samePoint(end, line.end, tolerance))
        || (samePoint(start, line.end, tolerance) && samePoint(end, line.start, tolerance));
}

bool matchesCircleGeometry(
    const McSolverEngine::Geometry::GeometryRecord& record,
    const McSolverEngine::Compat::Placement& placement,
    const CircleDescriptor& circle,
    double tolerance = 1e-6
)
{
    if (record.geometry.kind != McSolverEngine::Compat::GeometryKind::Circle) {
        return false;
    }

    const auto& exportedCircle = std::get<McSolverEngine::Compat::CircleGeometry>(record.geometry.data);
    const auto center = transformPoint(placement, exportedCircle.center.x, exportedCircle.center.y);
    return samePoint(center, circle.center, tolerance)
        && std::abs(exportedCircle.radius - circle.radius) <= tolerance;
}

std::size_t countShapes(const TopoDS_Shape& shape, TopAbs_ShapeEnum shapeType)
{
    std::size_t count = 0;
    for (TopExp_Explorer explorer(shape, shapeType); explorer.More(); explorer.Next()) {
        ++count;
    }
    return count;
}

#endif

}  // namespace

int main()
{
    McSolverEngine::Detail::configureWindowsAssertMode();

    std::string_view version {McSolverEngine::Engine::version()};
    if (version.empty()) {
        std::cerr << "Expected a non-empty version string.\n";
        return 1;
    }

    McSolverEngine::Engine engine;
    if (engine.describe().find("McSolverEngine scaffold") == std::string::npos) {
        std::cerr << "Unexpected engine description.\n";
        return 1;
    }

    GCS::System system;
    if (system.getFinePrecision() <= 0.0) {
        std::cerr << "Expected a positive default GCS fine precision.\n";
        return 1;
    }

    McSolverEngine::Compat::SketchModel model;
    int lineId = model.addLineSegment({0.0, 0.0}, {10.0, 0.0});
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Horizontal,
        .first = {.geometryIndex = lineId, .role = McSolverEngine::Compat::PointRole::None},
    });

    if (model.geometryCount() != 1 || model.constraintCount() != 1) {
        std::cerr << "Unexpected compat model counts.\n";
        return 1;
    }

    if (!McSolverEngine::Compat::isPhase1ConstraintSupported(
            McSolverEngine::Compat::ConstraintKind::Horizontal)) {
        std::cerr << "Expected Horizontal to be a supported phase 1 constraint.\n";
        return 1;
    }

    McSolverEngine::Compat::SketchModel solvable;
    int solveLineId = solvable.addLineSegment({0.0, 1.0}, {5.0, 3.0});
    solvable.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Horizontal,
        .first = {.geometryIndex = solveLineId, .role = McSolverEngine::Compat::PointRole::None},
    });
    solvable.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceY,
        .first = {.geometryIndex = solveLineId, .role = McSolverEngine::Compat::PointRole::Start},
        .value = 1.0,
        .hasValue = true,
    });

    const auto solveResult = McSolverEngine::Compat::solveSketch(solvable);
    if (!solveResult.solved()) {
        std::cerr << "Expected the phase 1 sketch to solve successfully.\n";
        return 1;
    }

    const auto& solvedLine =
        std::get<McSolverEngine::Compat::LineSegmentGeometry>(solvable.geometries().front().data);
    if (std::abs(solvedLine.start.y - 1.0) > 1e-8 || std::abs(solvedLine.end.y - 1.0) > 1e-8) {
        std::cerr << "Solved line does not satisfy the expected horizontal placement.\n";
        return 1;
    }

    McSolverEngine::Compat::SketchModel circleSketch;
    int circleId = circleSketch.addCircle({2.0, 3.0}, 5.0);
    circleSketch.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Radius,
        .first = {.geometryIndex = circleId, .role = McSolverEngine::Compat::PointRole::None},
        .value = 2.5,
        .hasValue = true,
    });

    const auto circleSolveResult = McSolverEngine::Compat::solveSketch(circleSketch);
    if (!circleSolveResult.solved()) {
        std::cerr << "Expected the radius-constrained circle to solve successfully.\n";
        return 1;
    }

    const auto& solvedCircle =
        std::get<McSolverEngine::Compat::CircleGeometry>(circleSketch.geometries().front().data);
    if (std::abs(solvedCircle.radius - 2.5) > 1e-8) {
        std::cerr << "Solved circle does not satisfy the expected radius.\n";
        return 1;
    }

    constexpr std::string_view documentXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Document>
    <ObjectData Count="1">
        <Object name="Sketch">
            <Properties Count="3" TransientCount="0">
                <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                    <ConstraintList count="3">
                        <Constrain Name="" Type="1" Value="0.0" First="-1" FirstPos="1" Second="0" SecondPos="1" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="2" Value="0.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="7" Value="2.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                    </ConstraintList>
                </Property>
                <Property name="ExternalGeo" type="Part::PropertyGeometryList">
                    <GeometryList count="2">
                        <Geometry type="Part::GeomLineSegment" id="-1" migrated="1">
                            <LineSegment StartX="0.0" StartY="0.0" StartZ="0.0" EndX="1.0" EndY="0.0" EndZ="0.0"/>
                            <Construction value="1"/>
                        </Geometry>
                        <Geometry type="Part::GeomLineSegment" id="-2" migrated="1">
                            <LineSegment StartX="0.0" StartY="0.0" StartZ="0.0" EndX="0.0" EndY="1.0" EndZ="0.0"/>
                            <Construction value="1"/>
                        </Geometry>
                    </GeometryList>
                </Property>
                <Property name="Geometry" type="Part::PropertyGeometryList">
                    <GeometryList count="2">
                        <Geometry type="Part::GeomLineSegment" id="1" migrated="1">
                            <LineSegment StartX="4.0" StartY="3.0" StartZ="0.0" EndX="6.0" EndY="5.0" EndZ="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                        <Geometry type="Part::GeomLineSegment" id="2" migrated="1">
                            <LineSegment StartX="10.0" StartY="10.0" StartZ="0.0" EndX="12.0" EndY="11.0" EndZ="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                    </GeometryList>
                </Property>
            </Properties>
        </Object>
    </ObjectData>
</Document>)";

    auto imported = McSolverEngine::DocumentXml::importSketchFromDocumentXml(documentXml, "Sketch");
    if (!imported.imported() || imported.status != McSolverEngine::DocumentXml::ImportStatus::Success) {
        std::cerr << "Expected the inline Document.xml sketch to import successfully.\n";
        return 1;
    }

    const auto importedSolveResult = McSolverEngine::Compat::solveSketch(imported.model);
    if (!importedSolveResult.solved()) {
        std::cerr << "Expected the imported sketch to solve successfully.\n";
        return 1;
    }

    const auto& importedLine =
        std::get<McSolverEngine::Compat::LineSegmentGeometry>(imported.model.geometries().front().data);
    if (std::abs(importedLine.start.x) > 1e-8 || std::abs(importedLine.start.y) > 1e-8
        || std::abs(importedLine.end.x - 2.0) > 1e-8 || std::abs(importedLine.end.y) > 1e-8) {
        std::cerr << "Imported sketch did not solve to the expected axis-anchored line.\n";
        return 1;
    }

    constexpr std::string_view elementOnlyDocumentXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Document>
    <ObjectData Count="1">
        <Object name="Sketch">
            <Properties Count="2" TransientCount="0">
                <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                    <ConstraintList count="2">
                        <Constrain Name="" Type="2" Value="0.0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" ElementIds="0 -2000 -2000" ElementPositions="0 0 0" />
                        <Constrain Name="" Type="6" Value="2.0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" ElementIds="0 -2000 -2000" ElementPositions="0 0 0" />
                    </ConstraintList>
                </Property>
                <Property name="Geometry" type="Part::PropertyGeometryList">
                    <GeometryList count="1">
                        <Geometry type="Part::GeomLineSegment" id="1" migrated="1">
                            <LineSegment StartX="4.0" StartY="3.0" StartZ="0.0" EndX="6.0" EndY="5.0" EndZ="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                    </GeometryList>
                </Property>
            </Properties>
        </Object>
    </ObjectData>
</Document>)";

    auto importedElementOnly =
        McSolverEngine::DocumentXml::importSketchFromDocumentXml(elementOnlyDocumentXml, "Sketch");
    if (!importedElementOnly.imported()
        || importedElementOnly.status != McSolverEngine::DocumentXml::ImportStatus::Success) {
        std::cerr << "Expected ElementIds-only constraint import to succeed.\n";
        return 1;
    }

    const auto importedElementOnlySolve = McSolverEngine::Compat::solveSketch(importedElementOnly.model);
    if (!importedElementOnlySolve.solved()) {
        std::cerr << "Expected ElementIds-only sketch to solve successfully.\n";
        return 1;
    }

    const auto& importedElementOnlyLine =
        std::get<McSolverEngine::Compat::LineSegmentGeometry>(importedElementOnly.model.geometries().front().data);
    if (std::abs(importedElementOnlyLine.end.y - importedElementOnlyLine.start.y) > 1e-8
        || std::abs(lineLength(importedElementOnlyLine) - 2.0) > 1e-8) {
        std::cerr << "ElementIds-only constraints did not solve to the expected line.\n";
        return 1;
    }

    constexpr std::string_view legacyOverrideDocumentXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Document>
    <ObjectData Count="1">
        <Object name="Sketch">
            <Properties Count="2" TransientCount="0">
                <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                    <ConstraintList count="1">
                        <Constrain Name="" Type="13" Value="0.0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" ElementIds="1 0 -2000" ElementPositions="0 1 0" First="1" FirstPos="1" Second="0" SecondPos="0" Third="-2000" ThirdPos="0" />
                    </ConstraintList>
                </Property>
                <Property name="Geometry" type="Part::PropertyGeometryList">
                    <GeometryList count="2">
                        <Geometry type="Part::GeomLineSegment" id="1" migrated="1">
                            <LineSegment StartX="0.0" StartY="0.0" StartZ="0.0" EndX="2.0" EndY="0.0" EndZ="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                        <Geometry type="Part::GeomPoint" id="2" migrated="1">
                            <Point X="1.0" Y="3.0" Z="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                    </GeometryList>
                </Property>
            </Properties>
        </Object>
    </ObjectData>
</Document>)";

    auto importedLegacyOverride =
        McSolverEngine::DocumentXml::importSketchFromDocumentXml(legacyOverrideDocumentXml, "Sketch");
    if (!importedLegacyOverride.imported()
        || importedLegacyOverride.status != McSolverEngine::DocumentXml::ImportStatus::Success
        || importedLegacyOverride.model.constraintCount() != 1) {
        std::cerr << "Expected legacy constraint fields to override ElementIds for the first three elements.\n";
        return 1;
    }

    constexpr std::string_view parameterizedDocumentXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Document>
    <Objects Count="2">
        <Object type="App::VarSet" name="VarSet" id="1"/>
        <Object type="Sketcher::SketchObject" name="Sketch" id="2"/>
    </Objects>
    <ObjectData Count="2">
        <Object name="VarSet">
            <Properties Count="2" TransientCount="0">
                <Property name="Label" type="App::PropertyString">
                    <String value="Parameters"/>
                </Property>
                <Property name="Width" type="App::PropertyFloat">
                    <Float value="6.0"/>
                </Property>
            </Properties>
        </Object>
        <Object name="Sketch">
            <Properties Count="3" TransientCount="0">
                <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                    <ConstraintList count="2">
                        <Constrain Name="" Type="2" Value="0.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="6" Value="0.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                    </ConstraintList>
                </Property>
                <Property name="ExpressionEngine" type="App::PropertyExpressionEngine">
                    <ExpressionEngine count="1">
                        <Expression path="Constraints[1]" expression="&lt;&lt;Parameters&gt;&gt;.Width"/>
                    </ExpressionEngine>
                </Property>
                <Property name="Geometry" type="Part::PropertyGeometryList">
                    <GeometryList count="1">
                        <Geometry type="Part::GeomLineSegment" id="1" migrated="1">
                            <LineSegment StartX="0.0" StartY="0.0" StartZ="0.0" EndX="3.0" EndY="1.0" EndZ="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                    </GeometryList>
                </Property>
            </Properties>
        </Object>
    </ObjectData>
</Document>)";

    auto importedParameterized =
        McSolverEngine::DocumentXml::importSketchFromDocumentXml(parameterizedDocumentXml, "Sketch");
    if (!importedParameterized.imported()
        || importedParameterized.status != McSolverEngine::DocumentXml::ImportStatus::Success) {
        std::cerr << "Expected VarSet-backed sketch import to succeed.\n";
        return 1;
    }
    if (importedParameterized.model.constraintCount() != 2) {
        std::cerr << "Expected VarSet-backed sample to keep both constraints.\n";
        return 1;
    }
    const auto& parameterConstraint = importedParameterized.model.constraints().back();
    if (parameterConstraint.parameterName != "Width" || parameterConstraint.parameterKey != "VarSet.Width"
        || parameterConstraint.parameterExpression != "<<Parameters>>.Width"
        || !parameterConstraint.hasParameterDefaultValue
        || std::abs(parameterConstraint.parameterDefaultValue - 6.0) > 1e-8
        || std::abs(parameterConstraint.value - 6.0) > 1e-8) {
        std::cerr << "Expected importer to bind VarSet metadata onto the dimensional constraint.\n";
        return 1;
    }

    const auto parameterizedSolve = McSolverEngine::Compat::solveSketch(importedParameterized.model);
    if (!parameterizedSolve.solved()) {
        std::cerr << "Expected VarSet-backed sketch to solve with the imported default parameter value.\n";
        return 1;
    }
    const auto& defaultParameterizedLine =
        std::get<McSolverEngine::Compat::LineSegmentGeometry>(importedParameterized.model.geometries().front().data);
    if (std::abs(defaultParameterizedLine.end.y - defaultParameterizedLine.start.y) > 1e-8
        || std::abs(std::hypot(
                        defaultParameterizedLine.end.x - defaultParameterizedLine.start.x,
                        defaultParameterizedLine.end.y - defaultParameterizedLine.start.y
                    ) - 6.0)
            > 1e-8) {
        std::cerr << "VarSet default parameter did not drive the solved line length.\n";
        return 1;
    }

    auto importedWithOverride = McSolverEngine::DocumentXml::importSketchFromDocumentXml(
        parameterizedDocumentXml,
        McSolverEngine::ParameterMap {{"Width", "8.5"}},
        "Sketch"
    );
    if (!importedWithOverride.imported()
        || importedWithOverride.status != McSolverEngine::DocumentXml::ImportStatus::Success) {
        std::cerr << "Expected parameter override import to succeed.\n";
        return 1;
    }
    if (std::abs(importedWithOverride.model.constraints().back().value - 8.5) > 1e-8) {
        std::cerr << "Import-time parameter override did not update the bound constraint value.\n";
        return 1;
    }
    const auto overrideImportedSolve = McSolverEngine::Compat::solveSketch(importedWithOverride.model);
    if (!overrideImportedSolve.solved()) {
        std::cerr << "Expected import-time parameter override sketch to solve successfully.\n";
        return 1;
    }
    const auto& importOverrideLine =
        std::get<McSolverEngine::Compat::LineSegmentGeometry>(importedWithOverride.model.geometries().front().data);
    if (std::abs(importOverrideLine.end.y - importOverrideLine.start.y) > 1e-8
        || std::abs(std::hypot(
                        importOverrideLine.end.x - importOverrideLine.start.x,
                        importOverrideLine.end.y - importOverrideLine.start.y
                    ) - 8.5)
            > 1e-8) {
        std::cerr << "Import-time parameter override did not drive the expected solved length.\n";
        return 1;
    }

    auto solveOverrideModel =
        McSolverEngine::DocumentXml::importSketchFromDocumentXml(parameterizedDocumentXml, "Sketch").model;
    const auto parameterOverrideSolve = McSolverEngine::Compat::solveSketch(
        solveOverrideModel,
        McSolverEngine::ParameterMap {{"VarSet.Width", "9.5"}}
    );
    if (!parameterOverrideSolve.solved()) {
        std::cerr << "Expected solve-time parameter override sketch to solve successfully.\n";
        return 1;
    }
    const auto& solveOverrideLine =
        std::get<McSolverEngine::Compat::LineSegmentGeometry>(solveOverrideModel.geometries().front().data);
    if (std::abs(solveOverrideLine.end.y - solveOverrideLine.start.y) > 1e-8
        || std::abs(std::hypot(
                        solveOverrideLine.end.x - solveOverrideLine.start.x,
                        solveOverrideLine.end.y - solveOverrideLine.start.y
                    ) - 9.5)
            > 1e-8) {
        std::cerr << "Solve-time parameter override did not drive the expected solved length.\n";
        return 1;
    }

    constexpr std::string_view parameterizedAngleDocumentXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Document>
    <Objects Count="2">
        <Object type="App::VarSet" name="VarSet" id="1"/>
        <Object type="Sketcher::SketchObject" name="Sketch" id="2"/>
    </Objects>
    <ObjectData Count="2">
        <Object name="VarSet">
            <Properties Count="2" TransientCount="0">
                <Property name="Label" type="App::PropertyString">
                    <String value="Parameters"/>
                </Property>
                <Property name="Angle" type="App::PropertyQuantity">
                    <Quantity value="90 deg"/>
                </Property>
            </Properties>
        </Object>
        <Object name="Sketch">
            <Properties Count="3" TransientCount="0">
                <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                    <ConstraintList count="4">
                        <Constrain Name="" Type="7" Value="0.0" First="0" FirstPos="1" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="8" Value="0.0" First="0" FirstPos="1" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="6" Value="10.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="9" Value="0.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                    </ConstraintList>
                </Property>
                <Property name="ExpressionEngine" type="App::PropertyExpressionEngine">
                    <ExpressionEngine count="1">
                        <Expression path="Constraints[3]" expression="&lt;&lt;Parameters&gt;&gt;.Angle"/>
                    </ExpressionEngine>
                </Property>
                <Property name="Geometry" type="Part::PropertyGeometryList">
                    <GeometryList count="1">
                        <Geometry type="Part::GeomLineSegment" id="1" migrated="1">
                            <LineSegment StartX="1.0" StartY="2.0" StartZ="0.0" EndX="4.0" EndY="6.0" EndZ="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                    </GeometryList>
                </Property>
            </Properties>
        </Object>
    </ObjectData>
</Document>)";

    auto importedAngleParameterized =
        McSolverEngine::DocumentXml::importSketchFromDocumentXml(parameterizedAngleDocumentXml, "Sketch");
    if (!importedAngleParameterized.imported()
        || importedAngleParameterized.status != McSolverEngine::DocumentXml::ImportStatus::Success) {
        std::cerr << "Expected angle VarSet-backed sketch import to succeed.\n";
        return 1;
    }
    const auto& angleConstraint = importedAngleParameterized.model.constraints().back();
    if (!angleConstraint.hasParameterDefaultValue
        || std::abs(angleConstraint.parameterDefaultValue - (std::numbers::pi / 2.0)) > 1e-8
        || std::abs(angleConstraint.value - (std::numbers::pi / 2.0)) > 1e-8) {
        std::cerr << "Expected angle VarSet defaults to be converted from degree to radian.\n";
        return 1;
    }
    const auto angleParameterizedSolve = McSolverEngine::Compat::solveSketch(importedAngleParameterized.model);
    if (!angleParameterizedSolve.solved()) {
        std::cerr << "Expected angle VarSet-backed sketch to solve with converted default angle.\n";
        return 1;
    }
    const auto& defaultAngleLine =
        std::get<McSolverEngine::Compat::LineSegmentGeometry>(importedAngleParameterized.model.geometries().front().data);
    if (std::abs(lineLength(defaultAngleLine) - 10.0) > 1e-8
        || std::abs(defaultAngleLine.start.x) > 1e-8
        || std::abs(defaultAngleLine.start.y) > 1e-8
        || std::abs(lineAngleDegrees(defaultAngleLine) - 90.0) > 1e-8) {
        std::cerr << "Converted default angle parameter did not drive the expected solved orientation.\n";
        return 1;
    }

    auto importedAngleWithOverride = McSolverEngine::DocumentXml::importSketchFromDocumentXml(
        parameterizedAngleDocumentXml,
        McSolverEngine::ParameterMap {{"Angle", "45"}},
        "Sketch"
    );
    if (!importedAngleWithOverride.imported()
        || importedAngleWithOverride.status != McSolverEngine::DocumentXml::ImportStatus::Success) {
        std::cerr << "Expected angle parameter override import to succeed.\n";
        return 1;
    }
    if (std::abs(importedAngleWithOverride.model.constraints().back().value - (std::numbers::pi / 4.0)) > 1e-8) {
        std::cerr << "Import-time angle parameter override was not converted from degree to radian.\n";
        return 1;
    }
    const auto overrideAngleSolve = McSolverEngine::Compat::solveSketch(importedAngleWithOverride.model);
    if (!overrideAngleSolve.solved()) {
        std::cerr << "Expected import-time angle parameter override sketch to solve successfully.\n";
        return 1;
    }
    const auto& importOverrideAngleLine =
        std::get<McSolverEngine::Compat::LineSegmentGeometry>(importedAngleWithOverride.model.geometries().front().data);
    if (std::abs(lineLength(importOverrideAngleLine) - 10.0) > 1e-8
        || std::abs(lineAngleDegrees(importOverrideAngleLine) - 45.0) > 1e-8) {
        std::cerr << "Import-time angle parameter override did not drive the expected solved angle.\n";
        return 1;
    }

    const auto invalidAngleImport = McSolverEngine::DocumentXml::importSketchFromDocumentXml(
        parameterizedAngleDocumentXml,
        McSolverEngine::ParameterMap {{"Angle", "45 deg"}},
        "Sketch"
    );
    if (invalidAngleImport.imported()
        || invalidAngleImport.status != McSolverEngine::DocumentXml::ImportStatus::Failed) {
        std::cerr << "Expected non-numeric API angle parameter input to fail import.\n";
        return 1;
    }

    const auto makeSolveTimeAngleModel = [] {
        McSolverEngine::Compat::SketchModel model;
        const int lineId = model.addLineSegment({1.0, 2.0}, {4.0, 6.0});
        model.addConstraint({
            .kind = McSolverEngine::Compat::ConstraintKind::DistanceX,
            .first = {.geometryIndex = lineId, .role = McSolverEngine::Compat::PointRole::Start},
            .value = 0.0,
            .hasValue = true,
        });
        model.addConstraint({
            .kind = McSolverEngine::Compat::ConstraintKind::DistanceY,
            .first = {.geometryIndex = lineId, .role = McSolverEngine::Compat::PointRole::Start},
            .value = 0.0,
            .hasValue = true,
        });
        model.addConstraint({
            .kind = McSolverEngine::Compat::ConstraintKind::Distance,
            .first = {.geometryIndex = lineId, .role = McSolverEngine::Compat::PointRole::None},
            .value = 10.0,
            .hasValue = true,
        });
        model.addConstraint({
            .kind = McSolverEngine::Compat::ConstraintKind::Angle,
            .first = {.geometryIndex = lineId, .role = McSolverEngine::Compat::PointRole::None},
            .value = 0.0,
            .hasValue = true,
            .parameterName = "Angle",
            .parameterKey = "VarSet.Angle",
        });
        return model;
    };

    auto solveTimeAngleModel = makeSolveTimeAngleModel();
    const auto solveTimeAngleResult = McSolverEngine::Compat::solveSketch(
        solveTimeAngleModel,
        McSolverEngine::ParameterMap {{"VarSet.Angle", "90"}}
    );
    if (!solveTimeAngleResult.solved()) {
        std::cerr << "Expected solve-time angle parameter override sketch to solve successfully.\n";
        return 1;
    }
    const auto& solveTimeAngleLine =
        std::get<McSolverEngine::Compat::LineSegmentGeometry>(solveTimeAngleModel.geometries().front().data);
    if (std::abs(lineLength(solveTimeAngleLine) - 10.0) > 1e-8
        || std::abs(lineAngleDegrees(solveTimeAngleLine) - 90.0) > 1e-8) {
        std::cerr << "Solve-time angle parameter override did not convert degrees before solving.\n";
        return 1;
    }

    auto invalidSolveTimeAngleModel = makeSolveTimeAngleModel();
    const auto invalidSolveTimeAngle = McSolverEngine::Compat::solveSketch(
        invalidSolveTimeAngleModel,
        McSolverEngine::ParameterMap {{"VarSet.Angle", "90 deg"}}
    );
    if (invalidSolveTimeAngle.status != McSolverEngine::Compat::SolveStatus::Invalid) {
        std::cerr << "Expected non-numeric solve-time API angle parameter input to be rejected.\n";
        return 1;
    }

    McSolverEngine::Compat::SketchModel bsplineKnotTangentModel;
    const int splineTangentSplineId = bsplineKnotTangentModel.addBSpline(
        {
            {{.x = 0.0, .y = 0.0}, 1.0},
            {{.x = 1.0, .y = 2.0}, 1.0},
            {{.x = 2.0, .y = 2.0}, 1.0},
            {{.x = 3.0, .y = 0.0}, 1.0},
            {{.x = 4.0, .y = -1.0}, 1.0},
        },
        {
            {.value = 0.0, .multiplicity = 3},
            {.value = 1.0, .multiplicity = 1},
            {.value = 2.0, .multiplicity = 1},
            {.value = 3.0, .multiplicity = 3},
        },
        2,
        false,
        false,
        true
    );
    const int splineTangentPointId = bsplineKnotTangentModel.addPoint({1.2, 1.0});
    const int splineTangentLineId = bsplineKnotTangentModel.addLineSegment({1.0, 1.5}, {2.5, 2.0});
    bsplineKnotTangentModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::InternalAlignment,
        .first = {.geometryIndex = splineTangentPointId, .role = McSolverEngine::Compat::PointRole::Start},
        .second = {.geometryIndex = splineTangentSplineId, .role = McSolverEngine::Compat::PointRole::None},
        .alignmentType = McSolverEngine::Compat::InternalAlignmentType::BSplineKnotPoint,
        .internalAlignmentIndex = 1,
    });
    bsplineKnotTangentModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Tangent,
        .first = {.geometryIndex = splineTangentPointId, .role = McSolverEngine::Compat::PointRole::Start},
        .second = {.geometryIndex = splineTangentLineId, .role = McSolverEngine::Compat::PointRole::Start},
        .value = 0.0,
        .hasValue = true,
    });
    bsplineKnotTangentModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Distance,
        .first = {.geometryIndex = splineTangentLineId, .role = McSolverEngine::Compat::PointRole::None},
        .value = 2.0,
        .hasValue = true,
    });
    const auto bsplineKnotTangentSolve = McSolverEngine::Compat::solveSketch(bsplineKnotTangentModel);
    if (!bsplineKnotTangentSolve.solved()) {
        std::cerr << "Expected BSpline knot tangent special case to solve successfully.\n";
        return 1;
    }
    const auto& solvedTangentSpline =
        std::get<McSolverEngine::Compat::BSplineGeometry>(bsplineKnotTangentModel.geometries()[0].data);
    const auto& solvedTangentPoint =
        std::get<McSolverEngine::Compat::PointGeometry>(bsplineKnotTangentModel.geometries()[1].data);
    const auto& solvedTangentLine =
        std::get<McSolverEngine::Compat::LineSegmentGeometry>(bsplineKnotTangentModel.geometries()[2].data);
    const double knotParameter = solvedTangentSpline.knots[1].value;
    const auto knotPoint = evaluateBSpline(solvedTangentSpline, knotParameter);
    const auto tangentDirection = bsplineTangentDirection(solvedTangentSpline, knotParameter);
    const double lineDx = solvedTangentLine.end.x - solvedTangentLine.start.x;
    const double lineDy = solvedTangentLine.end.y - solvedTangentLine.start.y;
    const double tangentLength = std::hypot(tangentDirection.x, tangentDirection.y);
    const double lineDirectionLength = std::hypot(lineDx, lineDy);
    const double normalizedCross = tangentLength > 1e-12 && lineDirectionLength > 1e-12
        ? std::abs(lineDx * tangentDirection.y - lineDy * tangentDirection.x)
            / (tangentLength * lineDirectionLength)
        : std::numeric_limits<double>::infinity();
    if (std::abs(solvedTangentPoint.point.x - knotPoint.x) > 1e-6
        || std::abs(solvedTangentPoint.point.y - knotPoint.y) > 1e-6
        || std::abs(solvedTangentLine.start.x - knotPoint.x) > 1e-6
        || std::abs(solvedTangentLine.start.y - knotPoint.y) > 1e-6
        || std::abs(lineLength(solvedTangentLine) - 2.0) > 1e-8
        || normalizedCross > 1e-5) {
        std::cerr << "BSpline knot tangent special case did not match the expected tangent-at-knot semantics.\n";
        return 1;
    }

    McSolverEngine::Compat::SketchModel rejectedBsplineKnotTangentModel;
    const int rejectedSplineId = rejectedBsplineKnotTangentModel.addBSpline(
        {
            {{.x = 0.0, .y = 0.0}, 1.0},
            {{.x = 1.0, .y = 2.0}, 1.0},
            {{.x = 2.0, .y = 2.0}, 1.0},
            {{.x = 3.0, .y = 0.0}, 1.0},
            {{.x = 4.0, .y = -1.0}, 1.0},
        },
        {
            {.value = 0.0, .multiplicity = 3},
            {.value = 1.0, .multiplicity = 2},
            {.value = 2.0, .multiplicity = 3},
        },
        2,
        false,
        false,
        true
    );
    const int rejectedPointId = rejectedBsplineKnotTangentModel.addPoint({1.2, 1.0});
    const int rejectedLineId = rejectedBsplineKnotTangentModel.addLineSegment({1.0, 1.5}, {2.5, 2.0});
    rejectedBsplineKnotTangentModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::InternalAlignment,
        .first = {.geometryIndex = rejectedPointId, .role = McSolverEngine::Compat::PointRole::Start},
        .second = {.geometryIndex = rejectedSplineId, .role = McSolverEngine::Compat::PointRole::None},
        .alignmentType = McSolverEngine::Compat::InternalAlignmentType::BSplineKnotPoint,
        .internalAlignmentIndex = 1,
    });
    rejectedBsplineKnotTangentModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Tangent,
        .first = {.geometryIndex = rejectedPointId, .role = McSolverEngine::Compat::PointRole::Start},
        .second = {.geometryIndex = rejectedLineId, .role = McSolverEngine::Compat::PointRole::Start},
        .value = 0.0,
        .hasValue = true,
    });
    rejectedBsplineKnotTangentModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Distance,
        .first = {.geometryIndex = rejectedLineId, .role = McSolverEngine::Compat::PointRole::None},
        .value = 2.0,
        .hasValue = true,
    });
    const auto rejectedBsplineKnotTangentSolve =
        McSolverEngine::Compat::solveSketch(rejectedBsplineKnotTangentModel);
    if (rejectedBsplineKnotTangentSolve.status != McSolverEngine::Compat::SolveStatus::Unsupported) {
        std::cerr << "Discontinuous BSpline knot tangent should be rejected instead of falling back.\n";
        return 1;
    }

    McSolverEngine::Compat::SketchModel blockedLineModel;
    const int blockedLineId = blockedLineModel.addLineSegment({0.0, 0.0}, {5.0, 0.0});
    blockedLineModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Distance,
        .first = {.geometryIndex = blockedLineId, .role = McSolverEngine::Compat::PointRole::None},
        .value = 10.0,
        .hasValue = true,
    });
    blockedLineModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Block,
        .first = {.geometryIndex = blockedLineId, .role = McSolverEngine::Compat::PointRole::None},
    });
    const auto blockedLineSolve = McSolverEngine::Compat::solveSketch(blockedLineModel);
    if (!blockedLineSolve.solved()) {
        std::cerr << "Expected Block preanalysis to ignore earlier driving constraints on blocked geometry.\n";
        return 1;
    }
    const auto& solvedBlockedLine =
        std::get<McSolverEngine::Compat::LineSegmentGeometry>(blockedLineModel.geometries().front().data);
    if (std::abs(lineLength(solvedBlockedLine) - 5.0) > 1e-8
        || std::abs(solvedBlockedLine.start.x) > 1e-8 || std::abs(solvedBlockedLine.start.y) > 1e-8
        || std::abs(solvedBlockedLine.end.x - 5.0) > 1e-8 || std::abs(solvedBlockedLine.end.y) > 1e-8) {
        std::cerr << "Block preanalysis did not preserve blocked geometry against earlier driving constraints.\n";
        return 1;
    }

    constexpr std::string_view pointwiseAngleDocumentXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Document>
    <ObjectData Count="1">
        <Object name="Sketch">
            <Properties Count="2" TransientCount="0">
                <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                    <ConstraintList count="8">
                        <Constrain Name="" Type="7" Value="0.0" First="2" FirstPos="1" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="8" Value="0.0" First="2" FirstPos="1" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="1" Value="0.0" First="2" FirstPos="1" Second="0" SecondPos="1" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="1" Value="0.0" First="2" FirstPos="1" Second="1" SecondPos="1" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="2" Value="0.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="6" Value="4.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="6" Value="3.0" First="1" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="9" Value="0.7853981633974483" First="0" FirstPos="1" Second="1" SecondPos="1" Third="2" ThirdPos="1" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                    </ConstraintList>
                </Property>
                <Property name="Geometry" type="Part::PropertyGeometryList">
                    <GeometryList count="3">
                        <Geometry type="Part::GeomLineSegment" id="1" migrated="1">
                            <LineSegment StartX="1.0" StartY="1.0" StartZ="0.0" EndX="5.0" EndY="1.0" EndZ="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                        <Geometry type="Part::GeomLineSegment" id="2" migrated="1">
                            <LineSegment StartX="1.5" StartY="0.5" StartZ="0.0" EndX="3.0" EndY="2.0" EndZ="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                        <Geometry type="Part::GeomPoint" id="3" migrated="1">
                            <Point X="0.5" Y="-0.5" Z="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                    </GeometryList>
                </Property>
            </Properties>
        </Object>
    </ObjectData>
</Document>)";

    auto importedPointwiseAngle =
        McSolverEngine::DocumentXml::importSketchFromDocumentXml(pointwiseAngleDocumentXml, "Sketch");
    if (!importedPointwiseAngle.imported()
        || importedPointwiseAngle.status != McSolverEngine::DocumentXml::ImportStatus::Success) {
        std::cerr << "Expected point-wise Angle sketch import to succeed.\n";
        return 1;
    }
    const auto pointwiseAngleSolve = McSolverEngine::Compat::solveSketch(importedPointwiseAngle.model);
    if (!pointwiseAngleSolve.solved()) {
        std::cerr << "Expected point-wise Angle sketch to solve successfully.\n";
        return 1;
    }
    const auto& solvedAngleBaseLine =
        std::get<McSolverEngine::Compat::LineSegmentGeometry>(importedPointwiseAngle.model.geometries()[0].data);
    const auto& solvedAngleLine =
        std::get<McSolverEngine::Compat::LineSegmentGeometry>(importedPointwiseAngle.model.geometries()[1].data);
    const auto& solvedAnglePoint =
        std::get<McSolverEngine::Compat::PointGeometry>(importedPointwiseAngle.model.geometries()[2].data);
    if (std::abs(solvedAnglePoint.point.x) > 1e-8 || std::abs(solvedAnglePoint.point.y) > 1e-8
        || std::abs(solvedAngleBaseLine.start.x) > 1e-8 || std::abs(solvedAngleBaseLine.start.y) > 1e-8
        || std::abs(lineLength(solvedAngleBaseLine) - 4.0) > 1e-8
        || std::abs(lineAngleDegrees(solvedAngleBaseLine)) > 1e-8
        || std::abs(solvedAngleLine.start.x) > 1e-8 || std::abs(solvedAngleLine.start.y) > 1e-8
        || std::abs(lineLength(solvedAngleLine) - 3.0) > 1e-8
        || std::abs(lineAngleDegrees(solvedAngleLine) - 45.0) > 1e-8) {
        std::cerr << "Point-wise Angle import/solve did not preserve the expected avp semantics.\n";
        return 1;
    }

    constexpr std::string_view pointwisePerpendicularDocumentXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Document>
    <ObjectData Count="1">
        <Object name="Sketch">
            <Properties Count="2" TransientCount="0">
                <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                    <ConstraintList count="6">
                        <Constrain Name="" Type="11" Value="2.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="7" Value="0.0" First="0" FirstPos="3" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="8" Value="0.0" First="0" FirstPos="3" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="7" Value="0.0" First="1" FirstPos="1" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="6" Value="4.0" First="1" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="10" Value="0.0" First="1" FirstPos="1" Second="0" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                    </ConstraintList>
                </Property>
                <Property name="Geometry" type="Part::PropertyGeometryList">
                    <GeometryList count="2">
                        <Geometry type="Part::GeomCircle" id="1" migrated="1">
                            <Circle CenterX="1.0" CenterY="-1.0" CenterZ="0.0" NormalX="0.0" NormalY="0.0" NormalZ="1.0" Radius="3.0"/>
                            <Construction value="0"/>
                        </Geometry>
                        <Geometry type="Part::GeomLineSegment" id="2" migrated="1">
                            <LineSegment StartX="-1.0" StartY="2.5" StartZ="0.0" EndX="2.0" EndY="3.0" EndZ="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                    </GeometryList>
                </Property>
            </Properties>
        </Object>
    </ObjectData>
</Document>)";

    auto importedPointwisePerpendicular =
        McSolverEngine::DocumentXml::importSketchFromDocumentXml(pointwisePerpendicularDocumentXml, "Sketch");
    if (!importedPointwisePerpendicular.imported()
        || importedPointwisePerpendicular.status != McSolverEngine::DocumentXml::ImportStatus::Success) {
        std::cerr << "Expected point-wise Perpendicular sketch import to succeed.\n";
        return 1;
    }
    const auto pointwisePerpendicularSolve =
        McSolverEngine::Compat::solveSketch(importedPointwisePerpendicular.model);
    if (!pointwisePerpendicularSolve.solved()) {
        std::cerr << "Expected point-wise Perpendicular sketch to solve successfully.\n";
        return 1;
    }
    const auto& solvedPerpendicularLine =
        std::get<McSolverEngine::Compat::LineSegmentGeometry>(importedPointwisePerpendicular.model.geometries()[1].data);
    if (std::abs(solvedPerpendicularLine.start.x) > 1e-8
        || std::abs(std::abs(solvedPerpendicularLine.start.y) - 2.0) > 1e-8
        || std::abs(lineLength(solvedPerpendicularLine) - 4.0) > 1e-8
        || std::abs(solvedPerpendicularLine.end.x - solvedPerpendicularLine.start.x) > 1e-8) {
        std::cerr << "Point-wise Perpendicular import/solve did not preserve endpoint-to-curve semantics.\n";
        return 1;
    }

    McSolverEngine::Compat::SketchModel pointCircleDistanceModel;
    const int distanceCircleId = pointCircleDistanceModel.addCircle({1.0, 1.0}, 3.0);
    const int distancePointId = pointCircleDistanceModel.addPoint({6.0, 0.0});
    pointCircleDistanceModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Radius,
        .first = {.geometryIndex = distanceCircleId, .role = McSolverEngine::Compat::PointRole::None},
        .value = 2.0,
        .hasValue = true,
    });
    pointCircleDistanceModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceX,
        .first = {.geometryIndex = distanceCircleId, .role = McSolverEngine::Compat::PointRole::Mid},
        .value = 0.0,
        .hasValue = true,
    });
    pointCircleDistanceModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceY,
        .first = {.geometryIndex = distanceCircleId, .role = McSolverEngine::Compat::PointRole::Mid},
        .value = 0.0,
        .hasValue = true,
    });
    pointCircleDistanceModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceX,
        .first = {.geometryIndex = distancePointId, .role = McSolverEngine::Compat::PointRole::Start},
        .value = 6.0,
        .hasValue = true,
    });
    pointCircleDistanceModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Distance,
        .first = {.geometryIndex = distancePointId, .role = McSolverEngine::Compat::PointRole::Start},
        .second = {.geometryIndex = distanceCircleId, .role = McSolverEngine::Compat::PointRole::None},
        .value = 4.0,
        .hasValue = true,
    });
    const auto pointCircleDistanceSolve = McSolverEngine::Compat::solveSketch(pointCircleDistanceModel);
    if (!pointCircleDistanceSolve.solved()) {
        std::cerr << "Expected point-to-circle distance sketch to solve successfully, status="
                  << static_cast<int>(pointCircleDistanceSolve.status)
                  << ", dof=" << pointCircleDistanceSolve.degreesOfFreedom << ".\n";
        return 1;
    }
    const auto& solvedDistancePoint =
        std::get<McSolverEngine::Compat::PointGeometry>(pointCircleDistanceModel.geometries()[1].data);
    if (std::abs(solvedDistancePoint.point.x - 6.0) > 1e-8
        || std::abs(solvedDistancePoint.point.y) > 1e-4) {
        std::cerr << "Point-to-circle distance did not match the expected FreeCAD semantics.\n";
        return 1;
    }

    McSolverEngine::Compat::SketchModel circleLineDistanceModel;
    const int lineDistanceCircleId = circleLineDistanceModel.addCircle({1.0, 1.0}, 4.0);
    const int lineDistanceLineId = circleLineDistanceModel.addLineSegment({8.0, -3.0}, {8.0, 3.0});
    circleLineDistanceModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Radius,
        .first = {.geometryIndex = lineDistanceCircleId, .role = McSolverEngine::Compat::PointRole::None},
        .value = 2.0,
        .hasValue = true,
    });
    circleLineDistanceModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceX,
        .first = {.geometryIndex = lineDistanceCircleId, .role = McSolverEngine::Compat::PointRole::Mid},
        .value = 0.0,
        .hasValue = true,
    });
    circleLineDistanceModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceY,
        .first = {.geometryIndex = lineDistanceCircleId, .role = McSolverEngine::Compat::PointRole::Mid},
        .value = 0.0,
        .hasValue = true,
    });
    circleLineDistanceModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Vertical,
        .first = {.geometryIndex = lineDistanceLineId, .role = McSolverEngine::Compat::PointRole::None},
    });
    circleLineDistanceModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceY,
        .first = {.geometryIndex = lineDistanceLineId, .role = McSolverEngine::Compat::PointRole::Start},
        .value = -3.0,
        .hasValue = true,
    });
    circleLineDistanceModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceY,
        .first = {.geometryIndex = lineDistanceLineId, .role = McSolverEngine::Compat::PointRole::End},
        .value = 3.0,
        .hasValue = true,
    });
    circleLineDistanceModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Distance,
        .first = {.geometryIndex = lineDistanceCircleId, .role = McSolverEngine::Compat::PointRole::None},
        .second = {.geometryIndex = lineDistanceLineId, .role = McSolverEngine::Compat::PointRole::None},
        .value = 3.0,
        .hasValue = true,
    });
    const auto circleLineDistanceSolve = McSolverEngine::Compat::solveSketch(circleLineDistanceModel);
    if (!circleLineDistanceSolve.solved()) {
        std::cerr << "Expected circle-to-line distance sketch to solve successfully.\n";
        return 1;
    }
    const auto& solvedDistanceLine =
        std::get<McSolverEngine::Compat::LineSegmentGeometry>(circleLineDistanceModel.geometries()[1].data);
    if (std::abs(solvedDistanceLine.end.x - solvedDistanceLine.start.x) > 1e-8
        || std::abs(std::abs(solvedDistanceLine.start.x) - 5.0) > 1e-8
        || std::abs(solvedDistanceLine.start.y + 3.0) > 1e-8
        || std::abs(solvedDistanceLine.end.y - 3.0) > 1e-8) {
        std::cerr << "Circle-to-line distance did not match the expected FreeCAD semantics.\n";
        return 1;
    }

    McSolverEngine::Compat::SketchModel circleCircleDistanceModel;
    const int firstCircleId = circleCircleDistanceModel.addCircle({1.0, 1.0}, 4.0);
    const int secondCircleId = circleCircleDistanceModel.addCircle({8.0, 0.5}, 2.0);
    circleCircleDistanceModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Radius,
        .first = {.geometryIndex = firstCircleId, .role = McSolverEngine::Compat::PointRole::None},
        .value = 2.0,
        .hasValue = true,
    });
    circleCircleDistanceModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceX,
        .first = {.geometryIndex = firstCircleId, .role = McSolverEngine::Compat::PointRole::Mid},
        .value = 0.0,
        .hasValue = true,
    });
    circleCircleDistanceModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceY,
        .first = {.geometryIndex = firstCircleId, .role = McSolverEngine::Compat::PointRole::Mid},
        .value = 0.0,
        .hasValue = true,
    });
    circleCircleDistanceModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Radius,
        .first = {.geometryIndex = secondCircleId, .role = McSolverEngine::Compat::PointRole::None},
        .value = 1.0,
        .hasValue = true,
    });
    circleCircleDistanceModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceY,
        .first = {.geometryIndex = secondCircleId, .role = McSolverEngine::Compat::PointRole::Mid},
        .value = 0.0,
        .hasValue = true,
    });
    circleCircleDistanceModel.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Distance,
        .first = {.geometryIndex = firstCircleId, .role = McSolverEngine::Compat::PointRole::None},
        .second = {.geometryIndex = secondCircleId, .role = McSolverEngine::Compat::PointRole::None},
        .value = 4.0,
        .hasValue = true,
    });
    const auto circleCircleDistanceSolve = McSolverEngine::Compat::solveSketch(circleCircleDistanceModel);
    if (!circleCircleDistanceSolve.solved()) {
        std::cerr << "Expected circle-to-circle distance sketch to solve successfully.\n";
        return 1;
    }
    const auto& solvedSecondCircle =
        std::get<McSolverEngine::Compat::CircleGeometry>(circleCircleDistanceModel.geometries()[1].data);
    if (std::abs(solvedSecondCircle.center.y) > 1e-8
        || std::abs(std::abs(solvedSecondCircle.center.x) - 7.0) > 1e-8
        || std::abs(solvedSecondCircle.radius - 1.0) > 1e-8) {
        std::cerr << "Circle-to-circle distance did not match the expected FreeCAD semantics.\n";
        return 1;
    }

    const auto inlineBrep = McSolverEngine::BRep::exportSketchToBRep(imported.model);
#if MCSOLVERENGINE_WITH_OCCT
    if (!inlineBrep.exported()) {
        std::cerr << "Expected the inline imported sketch to export to BREP.\n";
        return 1;
    }
#else
    if (inlineBrep.status != McSolverEngine::BRep::ExportStatus::OpenCascadeUnavailable || inlineBrep.brep.has_value()) {
        std::cerr << "Expected BREP export to report missing OCCT support.\n";
        return 1;
    }
#endif

    const auto inlineGeometry = McSolverEngine::Geometry::exportSketchGeometry(imported.model);
    if (!inlineGeometry.exported() || inlineGeometry.geometries.size() != 2) {
        std::cerr << "Expected the inline imported sketch to export two exact geometry records.\n";
        return 1;
    }
    bool foundSolvedGeometry = false;
    bool foundUnconstrainedGeometry = false;
    for (const auto& record : inlineGeometry.geometries) {
        if (record.geometry.kind != McSolverEngine::Compat::GeometryKind::LineSegment) {
            continue;
        }
        const auto& segment = std::get<McSolverEngine::Compat::LineSegmentGeometry>(record.geometry.data);
        foundSolvedGeometry = foundSolvedGeometry
            || ((std::abs(segment.start.x) <= 1e-8 && std::abs(segment.start.y) <= 1e-8
                 && std::abs(segment.end.x - 2.0) <= 1e-8 && std::abs(segment.end.y) <= 1e-8)
                || (std::abs(segment.end.x) <= 1e-8 && std::abs(segment.end.y) <= 1e-8
                    && std::abs(segment.start.x - 2.0) <= 1e-8 && std::abs(segment.start.y) <= 1e-8));
        foundUnconstrainedGeometry = foundUnconstrainedGeometry
            || ((std::abs(segment.start.x - 10.0) <= 1e-8 && std::abs(segment.start.y - 10.0) <= 1e-8
                 && std::abs(segment.end.x - 12.0) <= 1e-8 && std::abs(segment.end.y - 11.0) <= 1e-8)
                || (std::abs(segment.end.x - 10.0) <= 1e-8 && std::abs(segment.end.y - 10.0) <= 1e-8
                    && std::abs(segment.start.x - 12.0) <= 1e-8 && std::abs(segment.start.y - 11.0) <= 1e-8));
    }
    if (!foundSolvedGeometry || !foundUnconstrainedGeometry) {
        std::cerr << "Exact geometry export did not preserve the complete inline 2D geometry set.\n";
        return 1;
    }

#if MCSOLVERENGINE_WITH_OCCT
    if (!inlineBrep.brep.has_value()) {
        std::cerr << "Expected inline BREP export to return in-memory data.\n";
        return 1;
    }
    if (!hasExplicitIdentityLocationBlock(*inlineBrep.brep) || inlineBrep.brep->find("Locations 0") != std::string::npos) {
        std::cerr << "Inline BREP export did not preserve FreeCAD-like explicit root location output.\n";
        return 1;
    }

    TopoDS_Shape inlineShape;
    if (!readBrepFromString(*inlineBrep.brep, inlineShape)) {
        std::cerr << "Failed to parse BREP exported from inline imported sketch.\n";
        return 1;
    }

    const auto inlineEdges = describeEdges(inlineShape);
    if (inlineEdges.size() != 2
        || std::find(
               inlineEdges.begin(),
               inlineEdges.end(),
               "Line:0.00000000,0.00000000,0.00000000->2.00000000,0.00000000,0.00000000"
           ) == inlineEdges.end()
        || std::find(
               inlineEdges.begin(),
               inlineEdges.end(),
               "Line:10.00000000,10.00000000,0.00000000->12.00000000,11.00000000,0.00000000"
            ) == inlineEdges.end()) {
        std::cerr << "BREP export did not preserve the complete inline 2D geometry set.\n";
        return 1;
    }
#endif

    const std::string sourceDir {MCSOLVERENGINE_SOURCE_DIR};
    const std::string fcstdDocDir = sourceDir + "/fcstdDoc/";
#if MCSOLVERENGINE_WITH_OCCT
    const std::string sampleXmlPath = fcstdDocDir + "1.xml";
    const std::string sampleBrpPath = fcstdDocDir + "1.brp";
    const std::string sampleSolverBrpPath = fcstdDocDir + "1.solver.brp";
    const McSolverEngine::ParameterMap noParameters {};

    const auto verifySampleRegression = [&](
                                           const std::string& xmlPath,
                                           const std::string& sketchName,
                                           const std::string& expectedPath,
                                           const std::string& actualPath,
                                           const std::string& label,
                                           bool expectIdentityLocation,
                                           bool allowPartialImport,
                                           const McSolverEngine::ParameterMap& parameters) -> bool {
        auto importedSample = McSolverEngine::DocumentXml::importSketchFromDocumentXmlFile(
            xmlPath,
            parameters,
            sketchName
        );
        const bool importStatusAccepted =
            importedSample.status == McSolverEngine::DocumentXml::ImportStatus::Success
            || (allowPartialImport && importedSample.status == McSolverEngine::DocumentXml::ImportStatus::Partial);
        if (!importedSample.imported() || !importStatusAccepted) {
            std::cerr << "Expected " << label << " to import without skipped constraints.\n";
            return false;
        }

        const auto importedSampleSolve = McSolverEngine::Compat::solveSketch(importedSample.model);
        if (!importedSampleSolve.solved()) {
            std::cerr << "Expected " << label << " to solve successfully.\n";
            return false;
        }

        if (McSolverEngine::BRep::exportSketchToBRepFile(importedSample.model, actualPath)
            != McSolverEngine::BRep::ExportStatus::Success) {
            std::cerr << "Expected solved sample sketch to export to " << actualPath << ".\n";
            return false;
        }

        std::ifstream actualBrpFile(actualPath, std::ios::binary);
        if (!actualBrpFile) {
            std::cerr << "Failed to open generated BREP validation file for " << label << ".\n";
            return false;
        }
        const std::string actualBrp((std::istreambuf_iterator<char>(actualBrpFile)), std::istreambuf_iterator<char>());

        if (!hasExplicitRootLocationBlock(actualBrp) || actualBrp.find("Locations 0") != std::string::npos) {
            std::cerr << "Generated sample BREP did not preserve explicit root location output for " << label << ".\n";
            return false;
        }
        if (expectIdentityLocation && !hasExplicitIdentityLocationBlock(actualBrp)) {
            std::cerr << "Generated sample BREP did not preserve an explicit identity root location for " << label << ".\n";
            return false;
        }

        std::ifstream expectedBrpFile(expectedPath, std::ios::binary);
        if (!expectedBrpFile) {
            std::cerr << "Failed to open expected BREP validation file for " << label << ".\n";
            return false;
        }
        const std::string expectedBrp((std::istreambuf_iterator<char>(expectedBrpFile)), std::istreambuf_iterator<char>());

        TopoDS_Shape actualShape;
        TopoDS_Shape expectedShape;
        if (!readBrepFromString(actualBrp, actualShape) || !readBrepFromString(expectedBrp, expectedShape)) {
            std::cerr << "Failed to parse generated or expected BREP validation data for " << label << ".\n";
            return false;
        }

        if (actualShape.ShapeType() != expectedShape.ShapeType()
            || countShapes(actualShape, TopAbs_WIRE) != countShapes(expectedShape, TopAbs_WIRE)
            || countShapes(actualShape, TopAbs_EDGE) != countShapes(expectedShape, TopAbs_EDGE)
            || countShapes(actualShape, TopAbs_VERTEX) != countShapes(expectedShape, TopAbs_VERTEX)) {
            std::cerr << "Generated BREP topology does not match expected validation data for " << label << ".\n";
            return false;
        }

        if (!sameTransform(actualShape.Location().Transformation(), expectedShape.Location().Transformation())) {
            std::cerr << "Generated BREP root location does not match expected validation data for " << label << ".\n";
            return false;
        }

        if (!sameBrepTokens(expectedBrp, actualBrp)) {
            std::cerr << "Generated BREP text does not match expected validation data for " << label << ".\n";
            return false;
        }

        return true;
    };

    if (!verifySampleRegression(
            sampleXmlPath,
            "Sketch",
            sampleBrpPath,
            sampleSolverBrpPath,
            "fcstdDoc/1.xml / Sketch",
            true,
            false,
            noParameters)) {
        return 1;
    }

    const std::string sampleV1024XmlPath = fcstdDocDir + "V102.4.xml";
    const std::string sampleV1024ExpectedPath = fcstdDocDir + "V102.4.brp";
    const std::string sampleV1024ActualPath = fcstdDocDir + "V102.4.solver.brp";
    if (!verifySampleRegression(
            sampleV1024XmlPath,
            "Sketch",
            sampleV1024ExpectedPath,
            sampleV1024ActualPath,
            "fcstdDoc/V102.4.xml / Sketch",
            true,
            true,
            noParameters)) {
        return 1;
    }

    const std::string sampleV1024Plus1ExpectedPath = fcstdDocDir + "V102.4.plus1.brp";
    const std::string sampleV1024Plus1ActualPath = fcstdDocDir + "V102.4.plus1.solver.brp";
    if (!verifySampleRegression(
            sampleV1024XmlPath,
            "Sketch",
            sampleV1024Plus1ExpectedPath,
            sampleV1024Plus1ActualPath,
            "fcstdDoc/V102.4.xml / Sketch with parameters",
            true,
            true,
            McSolverEngine::ParameterMap {
                {"D1", "61"},
                {"L1", "41"},
                {"L2", "61"},
                {"L3", "11"},
                {"L4", "16"},
                {"L5", "21"},
            })) {
        return 1;
    }

    const std::string sampleV1021XmlPath = fcstdDocDir + "V102.1.xml";
    const std::string sampleV1021ExpectedPath = fcstdDocDir + "V102.1.brp";
    const std::string sampleV1021ActualPath = fcstdDocDir + "V102.1.solver.brp";
    if (!verifySampleRegression(
            sampleV1021XmlPath,
            "Sketch",
            sampleV1021ExpectedPath,
            sampleV1021ActualPath,
            "fcstdDoc/V102.1.xml / Sketch",
            false,
            false,
            noParameters)) {
        return 1;
    }

    const std::string sampleV1025XmlPath = fcstdDocDir + "V102.5.xml";
    const std::string sampleV1025ExpectedPath = fcstdDocDir + "V102.5.brp";
    const std::string sampleV1025ActualPath = fcstdDocDir + "V102.5.solver.brp";
    if (!verifySampleRegression(
            sampleV1025XmlPath,
            "Sketch",
            sampleV1025ExpectedPath,
            sampleV1025ActualPath,
            "fcstdDoc/V102.5.xml / Sketch",
            true,
            false,
            noParameters)) {
        return 1;
    }

    const std::array<std::tuple<std::string, std::string, std::string>, 3> sampleV1022Cases {{
        {"Sketch", fcstdDocDir + "V102.2.Sketch.Shape.brp", fcstdDocDir + "V102.2.Sketch.solver.brp"},
        {
            "Sketch001",
            fcstdDocDir + "V102.2.Sketch001.Shape.brp",
            fcstdDocDir + "V102.2.Sketch001.solver.brp"
        },
        {
            "Sketch002",
            fcstdDocDir + "V102.2.Sketch002.Shape.brp",
            fcstdDocDir + "V102.2.Sketch002.solver.brp"
        },
    }};
    const std::string sampleV1022XmlPath = fcstdDocDir + "V102.2.xml";
    for (const auto& [sketchName, expectedPath, actualPath] : sampleV1022Cases) {
        if (!verifySampleRegression(
                sampleV1022XmlPath,
                sketchName,
                expectedPath,
                actualPath,
                "fcstdDoc/V102.2.xml / " + sketchName,
                false,
                false,
                noParameters)) {
            return 1;
        }
    }
#else
    const std::string sampleXmlPath = fcstdDocDir + "1.xml";
    const std::string sampleSolverBrpPath = fcstdDocDir + "1.solver.brp";
    auto importedSample = McSolverEngine::DocumentXml::importSketchFromDocumentXmlFile(sampleXmlPath, "Sketch");
    if (!importedSample.imported() || !McSolverEngine::Compat::solveSketch(importedSample.model).solved()) {
        std::cerr << "Expected sample sketch to import and solve without OCCT.\n";
        return 1;
    }
    if (McSolverEngine::BRep::exportSketchToBRepFile(importedSample.model, sampleSolverBrpPath)
        != McSolverEngine::BRep::ExportStatus::OpenCascadeUnavailable) {
        std::cerr << "Expected file BREP export to report missing OCCT support.\n";
        return 1;
    }
#endif

    const std::array<std::pair<std::string, bool>, 3> sample2Cases {{
        {"Sketch", true},
        {"Sketch001", false},
        {"Sketch002", false},
    }};

    const std::string sample2XmlPath = fcstdDocDir + "2.xml";
    for (const auto& [sketchName, expectIdentityLocation] : sample2Cases) {
        const std::string expectedPath = fcstdDocDir + "2." + sketchName + ".Shape.brp";
        const std::string actualPath = fcstdDocDir + "2." + sketchName + ".solver.brp";
#if MCSOLVERENGINE_WITH_OCCT
        if (!verifySampleRegression(
                sample2XmlPath,
                sketchName,
                expectedPath,
                actualPath,
                "fcstdDoc/2.xml / " + sketchName,
                expectIdentityLocation,
                false,
                noParameters)) {
            return 1;
        }
#endif

        auto importedSample = McSolverEngine::DocumentXml::importSketchFromDocumentXmlFile(sample2XmlPath, sketchName);
        if (!importedSample.imported() || !McSolverEngine::Compat::solveSketch(importedSample.model).solved()) {
            std::cerr << "Expected exact geometry sample " << sketchName << " to import and solve.\n";
            return 1;
        }

        const auto exportedGeometry = McSolverEngine::Geometry::exportSketchGeometry(importedSample.model);
        if (!exportedGeometry.exported() || exportedGeometry.geometries.size() != 2) {
            std::cerr << "Expected " << sketchName << " to export exactly two exact geometry records.\n";
            return 1;
        }

#if MCSOLVERENGINE_WITH_OCCT
        std::ifstream expectedBrpFile(expectedPath, std::ios::binary);
        if (!expectedBrpFile) {
            std::cerr << "Failed to open expected BREP validation file for exact geometry check of "
                      << sketchName << ".\n";
            return 1;
        }
        const std::string expectedBrp(
            (std::istreambuf_iterator<char>(expectedBrpFile)),
            std::istreambuf_iterator<char>()
        );

        TopoDS_Shape expectedShape;
        if (!readBrepFromString(expectedBrp, expectedShape)) {
            std::cerr << "Failed to parse expected BREP validation data for exact geometry check of "
                      << sketchName << ".\n";
            return 1;
        }

        const auto lines = collectLineDescriptors(expectedShape);
        const auto circles = collectCircleDescriptors(expectedShape);
        if (lines.size() != 1 || circles.size() != 1) {
            std::cerr << "Unexpected expected-shape topology for exact geometry check of " << sketchName << ".\n";
            return 1;
        }

        bool foundLine = false;
        bool foundCircle = false;
        for (const auto& record : exportedGeometry.geometries) {
            foundLine = foundLine || matchesLineGeometry(record, exportedGeometry.placement, lines.front());
            foundCircle = foundCircle || matchesCircleGeometry(record, exportedGeometry.placement, circles.front());
        }
        if (!foundLine || !foundCircle) {
            std::cerr << "Exact geometry export did not match expected line/circle geometry for "
                      << sketchName << ".\n";
            return 1;
        }
#endif
    }

    const std::string sample3XmlPath = fcstdDocDir + "3.xml";
    const std::string sample3ExpectedPath = fcstdDocDir + "3.Sketch.Shape.brp";
    const std::string sample3ActualPath = fcstdDocDir + "3.Sketch.solver.brp";

#if MCSOLVERENGINE_WITH_OCCT
    if (!verifySampleRegression(
            sample3XmlPath,
            "Sketch",
            sample3ExpectedPath,
            sample3ActualPath,
            "fcstdDoc/3.xml / Sketch",
            true,
            false,
            noParameters)) {
        return 1;
    }
#endif

    auto importedSample3 = McSolverEngine::DocumentXml::importSketchFromDocumentXmlFile(sample3XmlPath, "Sketch");
    if (!importedSample3.imported() || !McSolverEngine::Compat::solveSketch(importedSample3.model).solved()) {
        std::cerr << "Expected sample sketch fcstdDoc/3.xml / Sketch to import and solve successfully.\n";
        return 1;
    }

    {
        constexpr std::string_view ellipseAlignmentXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Document>
    <ObjectData Count="1">
        <Object name="Sketch">
            <Properties Count="2" TransientCount="0">
                <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                    <ConstraintList count="2">
                        <Constrain Name="" Type="17" Value="0.0" First="1" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="15" Value="0.0" First="0" FirstPos="1" Second="1" SecondPos="0" Third="-2000" ThirdPos="0" InternalAlignmentType="3" InternalAlignmentIndex="-1" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                    </ConstraintList>
                </Property>
                <Property name="Geometry" type="Part::PropertyGeometryList">
                    <GeometryList count="2">
                        <Geometry type="Part::GeomPoint" id="1" migrated="1">
                            <Point X="8.0" Y="5.0" Z="0.0"/>
                            <Construction value="1"/>
                        </Geometry>
                        <Geometry type="Part::GeomEllipse" id="2" migrated="1">
                            <Ellipse CenterX="0.0" CenterY="0.0" CenterZ="0.0" NormalX="0.0" NormalY="0.0" NormalZ="1.0" MajorRadius="2.0" MinorRadius="1.7320508075688772" AngleXU="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                    </GeometryList>
                </Property>
            </Properties>
        </Object>
    </ObjectData>
</Document>)";

        auto importedEllipse =
            McSolverEngine::DocumentXml::importSketchFromDocumentXml(ellipseAlignmentXml, "Sketch");
        if (!importedEllipse.imported()
            || importedEllipse.status != McSolverEngine::DocumentXml::ImportStatus::Success) {
            std::cerr << "Expected ellipse internal-alignment sample to import successfully.\n";
            return 1;
        }

        const auto importedEllipseSolve = McSolverEngine::Compat::solveSketch(importedEllipse.model);
        if (!importedEllipseSolve.solved()) {
            std::cerr << "Expected ellipse internal-alignment sample to solve successfully.\n";
            return 1;
        }

        const auto& alignedPoint =
            std::get<McSolverEngine::Compat::PointGeometry>(importedEllipse.model.geometries().front().data);
        if (std::abs(alignedPoint.point.x - 8.0) > 1e-7 || std::abs(alignedPoint.point.y - 5.0) > 1e-7) {
            std::cerr << "Blocked ellipse internal-alignment should be treated as unenforceable.\n";
            return 1;
        }

        const auto ellipseGeometry = McSolverEngine::Geometry::exportSketchGeometry(importedEllipse.model);
        if (!ellipseGeometry.exported() || ellipseGeometry.geometries.size() != 1
            || ellipseGeometry.geometries.front().geometry.kind != McSolverEngine::Compat::GeometryKind::Ellipse) {
            std::cerr << "Expected aligned ellipse sample to export as a single exact ellipse geometry record.\n";
            return 1;
        }

#if MCSOLVERENGINE_WITH_OCCT
        const auto ellipseBrep = McSolverEngine::BRep::exportSketchToBRep(importedEllipse.model);
        if (!ellipseBrep.exported() || !ellipseBrep.brep.has_value()) {
            std::cerr << "Expected aligned ellipse sample to export to BREP.\n";
            return 1;
        }
#endif
    }

    {
        McSolverEngine::Compat::SketchModel weightedCircle;
        const int circleId = weightedCircle.addCircle({0.0, 0.0}, 5.0, true);
        weightedCircle.addConstraint({
            .kind = McSolverEngine::Compat::ConstraintKind::Weight,
            .first = {.geometryIndex = circleId, .role = McSolverEngine::Compat::PointRole::None},
            .value = 2.25,
            .hasValue = true,
        });

        const auto weightedCircleSolve = McSolverEngine::Compat::solveSketch(weightedCircle);
        if (!weightedCircleSolve.solved()) {
            std::cerr << "Expected Weight constraint sample to solve successfully.\n";
            return 1;
        }

        const auto& weightedCircleGeometry =
            std::get<McSolverEngine::Compat::CircleGeometry>(weightedCircle.geometries().front().data);
        if (std::abs(weightedCircleGeometry.radius - 2.25) > 1e-8) {
            std::cerr << "Weight constraint did not behave like FreeCAD's helper-circle radius semantics.\n";
            return 1;
        }
    }

    {
        McSolverEngine::Compat::SketchModel snellsLawModel;
        const int incoming = snellsLawModel.addLineSegment({-1.0, 1.0}, {0.0, 0.0}, false, false, true);
        const int outgoing = snellsLawModel.addLineSegment({0.0, 0.0}, {1.0, 1.0}, false, false, true);
        const int boundary = snellsLawModel.addLineSegment({0.0, -2.0}, {0.0, 2.0}, true, false, true);
        snellsLawModel.addConstraint({
            .kind = McSolverEngine::Compat::ConstraintKind::SnellsLaw,
            .first = {.geometryIndex = incoming, .role = McSolverEngine::Compat::PointRole::End},
            .second = {.geometryIndex = outgoing, .role = McSolverEngine::Compat::PointRole::Start},
            .third = {.geometryIndex = boundary, .role = McSolverEngine::Compat::PointRole::None},
            .value = 1.0,
            .hasValue = true,
        });

        const auto snellsLawSolve = McSolverEngine::Compat::solveSketch(snellsLawModel);
        if (!snellsLawSolve.solved()) {
            std::cerr << "Expected SnellsLaw sample to solve successfully.\n";
            return 1;
        }
    }

    {
        McSolverEngine::Compat::SketchModel parabolaModel;
        parabolaModel.addArcOfParabola(
            {0.0, 0.0},  // vertex
            {0.0, 2.0},  // focus1 → focal=2
            -4.0,        // startAngle (u-parameter, not geometric angle)
            4.0          // endAngle (u-parameter)
        );

#if MCSOLVERENGINE_WITH_OCCT
        const auto parabolaBrep = McSolverEngine::BRep::exportSketchToBRep(parabolaModel);
        if (!parabolaBrep.exported() || !parabolaBrep.brep.has_value()) {
            std::cerr << "Expected ArcOfParabola sketch to export to BREP.\n";
            return 1;
        }
#endif
    }

    return 0;
}
