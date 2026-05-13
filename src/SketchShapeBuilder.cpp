#include "SketchShapeBuilder.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <string>
#include <variant>

#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_WireError.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Ellipse.hxx>
#include <Geom_Hyperbola.hxx>
#include <Geom_Parabola.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Precision.hxx>
#include <ShapeFix_Wire.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Elips.hxx>
#include <gp_Hypr.hxx>
#include <gp_Parab.hxx>
#include <gp_Pnt.hxx>
#include <gp_Quaternion.hxx>
#include <gp_Vec.hxx>

namespace McSolverEngine::ShapeBuilder
{

namespace
{

[[nodiscard]] gp_Pnt makePoint(const Compat::Point2& point)
{
    return gp_Pnt(point.x, point.y, 0.0);
}

[[nodiscard]] gp_Dir makeAxisDirection(const Compat::Point2& from, const Compat::Point2& to)
{
    gp_Vec axis(to.x - from.x, to.y - from.y, 0.0);
    if (axis.SquareMagnitude() <= Precision::SquareConfusion()) {
        return gp_Dir(1.0, 0.0, 0.0);
    }
    return gp_Dir(axis);
}

[[nodiscard]] gp_Ax2 makeConicAxis(const Compat::Point2& origin, const Compat::Point2& axisPoint)
{
    return gp_Ax2(makePoint(origin), gp_Dir(0.0, 0.0, 1.0), makeAxisDirection(origin, axisPoint));
}

[[nodiscard]] double ellipseMajorRadius(const Compat::Point2& center, const Compat::Point2& focus1, double minorRadius)
{
    const double dx = focus1.x - center.x;
    const double dy = focus1.y - center.y;
    return std::sqrt(dx * dx + dy * dy + minorRadius * minorRadius);
}

[[nodiscard]] double hyperbolaMajorRadius(const Compat::Point2& center, const Compat::Point2& focus1, double minorRadius)
{
    const double dx = focus1.x - center.x;
    const double dy = focus1.y - center.y;
    return std::sqrt(std::max(0.0, dx * dx + dy * dy - minorRadius * minorRadius));
}

[[nodiscard]] bool appendGeometry(
    const Compat::Geometry& geometry,
    std::list<TopoDS_Edge>& edges,
    std::list<TopoDS_Vertex>& vertices,
    std::string& error
)
{
    if (geometry.construction || geometry.external) {
        return true;
    }

    switch (geometry.kind) {
        case Compat::GeometryKind::Point: {
            const auto& point = std::get<Compat::PointGeometry>(geometry.data);
            vertices.push_back(BRepBuilderAPI_MakeVertex(makePoint(point.point)).Vertex());
            return true;
        }
        case Compat::GeometryKind::LineSegment: {
            const auto& line = std::get<Compat::LineSegmentGeometry>(geometry.data);
            edges.push_back(BRepBuilderAPI_MakeEdge(makePoint(line.start), makePoint(line.end)).Edge());
            return true;
        }
        case Compat::GeometryKind::Circle: {
            const auto& circle = std::get<Compat::CircleGeometry>(geometry.data);
            const gp_Circ gpCircle(
                gp_Ax2(makePoint(circle.center), gp_Dir(0.0, 0.0, 1.0)),
                circle.radius
            );
            edges.push_back(BRepBuilderAPI_MakeEdge(gpCircle).Edge());
            return true;
        }
        case Compat::GeometryKind::Arc: {
            const auto& arc = std::get<Compat::ArcGeometry>(geometry.data);
            const gp_Circ gpCircle(
                gp_Ax2(makePoint(arc.center), gp_Dir(0.0, 0.0, 1.0)),
                arc.radius
            );
            edges.push_back(BRepBuilderAPI_MakeEdge(gpCircle, arc.startAngle, arc.endAngle).Edge());
            return true;
        }
        case Compat::GeometryKind::Ellipse: {
            const auto& ellipse = std::get<Compat::EllipseGeometry>(geometry.data);
            const gp_Elips gpEllipse(
                makeConicAxis(ellipse.center, ellipse.focus1),
                ellipseMajorRadius(ellipse.center, ellipse.focus1, ellipse.minorRadius),
                ellipse.minorRadius
            );
            edges.push_back(BRepBuilderAPI_MakeEdge(Handle(Geom_Ellipse)(new Geom_Ellipse(gpEllipse))).Edge());
            return true;
        }
        case Compat::GeometryKind::ArcOfEllipse: {
            const auto& arc = std::get<Compat::ArcOfEllipseGeometry>(geometry.data);
            const gp_Elips gpEllipse(
                makeConicAxis(arc.center, arc.focus1),
                ellipseMajorRadius(arc.center, arc.focus1, arc.minorRadius),
                arc.minorRadius
            );
            Handle(Geom_Ellipse) curve = new Geom_Ellipse(gpEllipse);
            Handle(Geom_TrimmedCurve) trimmed =
                new Geom_TrimmedCurve(curve, arc.startAngle, arc.endAngle, true, true);
            edges.push_back(BRepBuilderAPI_MakeEdge(trimmed).Edge());
            return true;
        }
        case Compat::GeometryKind::ArcOfHyperbola: {
            const auto& arc = std::get<Compat::ArcOfHyperbolaGeometry>(geometry.data);
            const gp_Hypr gpHyperbola(
                makeConicAxis(arc.center, arc.focus1),
                hyperbolaMajorRadius(arc.center, arc.focus1, arc.minorRadius),
                arc.minorRadius
            );
            Handle(Geom_Hyperbola) curve = new Geom_Hyperbola(gpHyperbola);
            Handle(Geom_TrimmedCurve) trimmed =
                new Geom_TrimmedCurve(curve, arc.startAngle, arc.endAngle, true, true);
            edges.push_back(BRepBuilderAPI_MakeEdge(trimmed).Edge());
            return true;
        }
        case Compat::GeometryKind::ArcOfParabola: {
            const auto& arc = std::get<Compat::ArcOfParabolaGeometry>(geometry.data);
            const double focal = std::hypot(arc.focus1.x - arc.vertex.x, arc.focus1.y - arc.vertex.y);
            const gp_Parab gpParabola(
                makeConicAxis(arc.vertex, arc.focus1),
                focal
            );
            Handle(Geom_Parabola) curve = new Geom_Parabola(gpParabola);
            const double scale = focal > Precision::Confusion() ? (2.0 * focal) : 1.0;
            Handle(Geom_TrimmedCurve) trimmed =
                new Geom_TrimmedCurve(curve, arc.startAngle / scale, arc.endAngle / scale, true, true);
            edges.push_back(BRepBuilderAPI_MakeEdge(trimmed).Edge());
            return true;
        }
        case Compat::GeometryKind::BSpline: {
            const auto& spline = std::get<Compat::BSplineGeometry>(geometry.data);
            if (spline.poles.empty() || spline.knots.empty()) {
                error = "Invalid BSpline geometry in sketch shape export.";
                return false;
            }

            TColgp_Array1OfPnt poles(1, static_cast<Standard_Integer>(spline.poles.size()));
            TColStd_Array1OfReal weights(1, static_cast<Standard_Integer>(spline.poles.size()));
            TColStd_Array1OfReal knots(1, static_cast<Standard_Integer>(spline.knots.size()));
            TColStd_Array1OfInteger multiplicities(1, static_cast<Standard_Integer>(spline.knots.size()));
            for (Standard_Integer i = 1; i <= poles.Length(); ++i) {
                const auto& pole = spline.poles[static_cast<std::size_t>(i - 1)];
                poles.SetValue(i, gp_Pnt(pole.point.x, pole.point.y, 0.0));
                weights.SetValue(i, pole.weight);
            }
            for (Standard_Integer i = 1; i <= knots.Length(); ++i) {
                const auto& knot = spline.knots[static_cast<std::size_t>(i - 1)];
                knots.SetValue(i, knot.value);
                multiplicities.SetValue(i, knot.multiplicity);
            }

            Handle(Geom_BSplineCurve) curve = new Geom_BSplineCurve(
                poles,
                weights,
                knots,
                multiplicities,
                spline.degree,
                spline.periodic,
                true
            );
            edges.push_back(BRepBuilderAPI_MakeEdge(curve).Edge());
            return true;
        }
    }

    error = "Unsupported geometry kind in sketch shape export.";
    return false;
}

}  // namespace

gp_Trsf makePlacementTransform(const Compat::Placement& placement)
{
    gp_Trsf transform;
    transform.SetTransformation(
        gp_Quaternion(placement.qx, placement.qy, placement.qz, placement.qw),
        gp_Vec(placement.px, placement.py, placement.pz)
    );
    return transform;
}

TopoDS_Shape buildSketchShape(const Compat::SketchModel& model, std::string& error)
{
    std::list<TopoDS_Edge> edgeList;
    std::list<TopoDS_Vertex> vertexList;
    std::list<TopoDS_Wire> wires;

    for (const auto& geometry : model.geometries()) {
        if (!appendGeometry(geometry, edgeList, vertexList, error)) {
            return {};
        }
    }

    while (!edgeList.empty()) {
        BRepBuilderAPI_MakeWire makeWire;
        makeWire.Add(edgeList.front());
        edgeList.pop_front();

        TopoDS_Wire newWire = makeWire.Wire();
        bool found = false;
        do {
            found = false;
            for (auto edgeIt = edgeList.begin(); edgeIt != edgeList.end(); ++edgeIt) {
                makeWire.Add(*edgeIt);
                if (makeWire.Error() != BRepBuilderAPI_DisconnectedWire) {
                    found = true;
                    edgeList.erase(edgeIt);
                    newWire = makeWire.Wire();
                    break;
                }
            }
        } while (found);

        ShapeFix_Wire fixWire;
        fixWire.SetPrecision(Precision::Confusion());
        fixWire.Load(newWire);
        fixWire.FixReorder();
        fixWire.FixConnected();
        fixWire.FixClosed();
        wires.push_back(fixWire.Wire());
    }

    if (wires.size() == 1 && vertexList.empty()) {
        return *wires.begin();
    }

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);

    for (auto& wire : wires) {
        builder.Add(compound, wire);
    }

    for (auto& vertex : vertexList) {
        builder.Add(compound, vertex);
    }

    return compound;
}

TopoDS_Shape applySketchPlacement(const TopoDS_Shape& shape, const Compat::SketchModel& model)
{
    if (shape.IsNull()) {
        return shape;
    }

    const TopLoc_Location placementLocation(makePlacementTransform(model.placement()));
    return shape.Located(placementLocation * shape.Location());
}

}  // namespace McSolverEngine::ShapeBuilder
