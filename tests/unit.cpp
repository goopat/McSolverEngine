// Unit tests covering areas not (or only indirectly) exercised by smoke.cpp / capi_smoke.cpp:
//   - ParameterValueUtils internal helpers (parseStrictNumeric, normalizeUnitSuffix,
//     parseDocumentParameterValue, convertDocumentParameterToInternal,
//     convertApiParameterToInternal, isLengthConstraintKind, isAngleConstraintKind)
//   - CompatModel: empty(), setPlacement, addArcOfEllipse, addArcOfHyperbola,
//     toString overloads, isPhase1ConstraintSupported for all kinds
//   - CompatSolver: Diameter / Equal / Parallel constraints, degreesOfFreedom,
//     conflicting / redundant detection
//   - ZipExtract: graceful failure on a nonexistent file path
//   - GeometryExport: construction and external flags are preserved in exported records
//   - DocumentXml: empty sketch import, import with an empty sketchName

#include <cmath>
#include <iostream>
#include <numbers>
#include <string>

#include "McSolverEngine/CompatModel.h"
#include "McSolverEngine/CompatSolver.h"
#include "McSolverEngine/DocumentXml.h"
#include "McSolverEngine/GeometryExport.h"
#include "McSolverEngine/ZipExtract.h"
#include "ParameterValueUtils.h"

namespace
{

int failCount = 0;

bool check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        ++failCount;
        return false;
    }
    return true;
}

// ── ParameterValueUtils ────────────────────────────────────────────────────

void testParseStrictNumeric()
{
    using McSolverEngine::Detail::parseStrictNumeric;

    check(parseStrictNumeric("3.14").has_value(), "parseStrictNumeric: plain decimal");
    check(std::abs(*parseStrictNumeric("3.14") - 3.14) < 1e-14, "parseStrictNumeric: value 3.14");

    check(parseStrictNumeric("  42  ").has_value(), "parseStrictNumeric: surrounded by spaces");
    check(*parseStrictNumeric("  42  ") == 42.0, "parseStrictNumeric: value 42 with spaces");

    check(parseStrictNumeric("0").has_value(), "parseStrictNumeric: zero");
    check(parseStrictNumeric("-5.0").has_value(), "parseStrictNumeric: negative");
    check(*parseStrictNumeric("-5.0") == -5.0, "parseStrictNumeric: negative value");

    check(parseStrictNumeric("1e3").has_value(), "parseStrictNumeric: scientific notation");
    check(*parseStrictNumeric("1e3") == 1000.0, "parseStrictNumeric: scientific value");

    check(parseStrictNumeric("1.5e-2").has_value(), "parseStrictNumeric: scientific negative exponent");
    check(std::abs(*parseStrictNumeric("1.5e-2") - 0.015) < 1e-14, "parseStrictNumeric: scientific neg-exp value");

    check(!parseStrictNumeric("").has_value(), "parseStrictNumeric: empty string");
    check(!parseStrictNumeric("   ").has_value(), "parseStrictNumeric: only whitespace");
    check(!parseStrictNumeric("abc").has_value(), "parseStrictNumeric: non-numeric");
    check(!parseStrictNumeric("3.14 mm").has_value(), "parseStrictNumeric: number with unit suffix");
    check(!parseStrictNumeric("3.14abc").has_value(), "parseStrictNumeric: trailing non-space chars");
}

void testNormalizeUnitSuffix()
{
    using McSolverEngine::Detail::normalizeUnitSuffix;

    check(normalizeUnitSuffix("mm") == "mm", "normalizeUnitSuffix: plain mm");
    check(normalizeUnitSuffix("MM") == "mm", "normalizeUnitSuffix: uppercase MM");
    check(normalizeUnitSuffix("  mm  ") == "mm", "normalizeUnitSuffix: mm with spaces");
    check(normalizeUnitSuffix("m m") == "mm", "normalizeUnitSuffix: internal space removed");
    check(normalizeUnitSuffix("m_m") == "mm", "normalizeUnitSuffix: underscore removed");
    check(normalizeUnitSuffix("Deg") == "deg", "normalizeUnitSuffix: mixed-case Deg");
    check(normalizeUnitSuffix("") == "", "normalizeUnitSuffix: empty");
}

void testIsKindHelpers()
{
    using McSolverEngine::Detail::isLengthConstraintKind;
    using McSolverEngine::Detail::isAngleConstraintKind;
    using McSolverEngine::Detail::expectedParameterUnit;
    using McSolverEngine::Compat::ConstraintKind;

    check(isLengthConstraintKind(ConstraintKind::Distance), "isLength: Distance");
    check(isLengthConstraintKind(ConstraintKind::DistanceX), "isLength: DistanceX");
    check(isLengthConstraintKind(ConstraintKind::DistanceY), "isLength: DistanceY");
    check(isLengthConstraintKind(ConstraintKind::Radius), "isLength: Radius");
    check(isLengthConstraintKind(ConstraintKind::Diameter), "isLength: Diameter");
    check(!isLengthConstraintKind(ConstraintKind::Angle), "isLength: Angle is not length");
    check(!isLengthConstraintKind(ConstraintKind::Horizontal), "isLength: Horizontal is not length");

    check(isAngleConstraintKind(ConstraintKind::Angle), "isAngle: Angle");
    check(!isAngleConstraintKind(ConstraintKind::Distance), "isAngle: Distance is not angle");

    check(expectedParameterUnit(ConstraintKind::Angle) == "degree", "expectedUnit: Angle → degree");
    check(expectedParameterUnit(ConstraintKind::Distance) == "mm", "expectedUnit: Distance → mm");
    check(expectedParameterUnit(ConstraintKind::Horizontal) == "numeric", "expectedUnit: Horizontal → numeric");
}

void testConvertApiParameterToInternal()
{
    using McSolverEngine::Detail::convertApiParameterToInternal;
    using McSolverEngine::Compat::ConstraintKind;

    // Angle → degrees to radians
    const double pi90 = std::numbers::pi / 2.0;
    check(
        std::abs(convertApiParameterToInternal(90.0, ConstraintKind::Angle) - pi90) < 1e-12,
        "convertApi: 90 deg → pi/2 rad"
    );
    check(
        std::abs(convertApiParameterToInternal(0.0, ConstraintKind::Angle)) < 1e-14,
        "convertApi: 0 deg → 0 rad"
    );

    // Non-angle → pass-through
    check(
        convertApiParameterToInternal(5.0, ConstraintKind::Distance) == 5.0,
        "convertApi: Distance pass-through"
    );
    check(
        convertApiParameterToInternal(3.5, ConstraintKind::Radius) == 3.5,
        "convertApi: Radius pass-through"
    );
}

void testParseDocumentParameterValue()
{
    using McSolverEngine::Detail::parseDocumentParameterValue;
    using McSolverEngine::Compat::ConstraintKind;

    // Length units (result in mm)
    check(
        std::abs(*parseDocumentParameterValue("10", ConstraintKind::Distance) - 10.0) < 1e-12,
        "parseDoc: dimensionless mm (Distance)"
    );
    check(
        std::abs(*parseDocumentParameterValue("10 mm", ConstraintKind::Distance) - 10.0) < 1e-12,
        "parseDoc: 10 mm"
    );
    check(
        std::abs(*parseDocumentParameterValue("1 cm", ConstraintKind::Distance) - 10.0) < 1e-12,
        "parseDoc: 1 cm → 10 mm"
    );
    check(
        std::abs(*parseDocumentParameterValue("1 m", ConstraintKind::Distance) - 1000.0) < 1e-10,
        "parseDoc: 1 m → 1000 mm"
    );
    check(
        std::abs(*parseDocumentParameterValue("1 km", ConstraintKind::Distance) - 1000000.0) < 1e-6,
        "parseDoc: 1 km → 1e6 mm"
    );
    check(
        std::abs(*parseDocumentParameterValue("1000 um", ConstraintKind::Distance) - 1.0) < 1e-9,
        "parseDoc: 1000 um → 1 mm"
    );
    check(
        std::abs(*parseDocumentParameterValue("1000000 nm", ConstraintKind::Distance) - 1.0) < 1e-6,
        "parseDoc: 1e6 nm → 1 mm"
    );
    check(
        std::abs(*parseDocumentParameterValue("1 in", ConstraintKind::Distance) - 25.4) < 1e-10,
        "parseDoc: 1 in → 25.4 mm"
    );
    check(
        std::abs(*parseDocumentParameterValue("1 ft", ConstraintKind::Distance) - 304.8) < 1e-10,
        "parseDoc: 1 ft → 304.8 mm"
    );

    // Invalid unit for length
    check(!parseDocumentParameterValue("5 kg", ConstraintKind::Distance).has_value(), "parseDoc: kg invalid for Distance");
    check(!parseDocumentParameterValue("5 deg", ConstraintKind::Distance).has_value(), "parseDoc: deg invalid for Distance");

    // Angle units (result in radians)
    check(
        std::abs(*parseDocumentParameterValue("90 deg", ConstraintKind::Angle) - std::numbers::pi / 2.0) < 1e-12,
        "parseDoc: 90 deg → pi/2"
    );
    check(
        std::abs(*parseDocumentParameterValue("90", ConstraintKind::Angle) - std::numbers::pi / 2.0) < 1e-12,
        "parseDoc: 90 (no unit) → pi/2 for angle"
    );
    check(
        std::abs(*parseDocumentParameterValue("1 rad", ConstraintKind::Angle) - 1.0) < 1e-12,
        "parseDoc: 1 rad → 1 radian"
    );
    check(
        std::abs(*parseDocumentParameterValue("1 radian", ConstraintKind::Angle) - 1.0) < 1e-12,
        "parseDoc: 1 radian → 1"
    );
    check(
        !parseDocumentParameterValue("1 mm", ConstraintKind::Angle).has_value(),
        "parseDoc: mm invalid for Angle"
    );

    // Dimensionless (no unit expected)
    check(
        std::abs(*parseDocumentParameterValue("7", ConstraintKind::Horizontal) - 7.0) < 1e-12,
        "parseDoc: 7 for Horizontal (dimensionless)"
    );
    check(
        !parseDocumentParameterValue("7 mm", ConstraintKind::Horizontal).has_value(),
        "parseDoc: mm invalid for Horizontal (dimensionless)"
    );

    // Empty / non-numeric
    check(!parseDocumentParameterValue("", ConstraintKind::Distance).has_value(), "parseDoc: empty string");
    check(!parseDocumentParameterValue("abc", ConstraintKind::Distance).has_value(), "parseDoc: non-numeric");
}

void testConvertDocumentParameterToInternal()
{
    using McSolverEngine::Detail::convertDocumentParameterToInternal;
    using McSolverEngine::Compat::ConstraintKind;

    // Angle: deg/degree/degrees → radians
    check(
        std::abs(*convertDocumentParameterToInternal(180.0, "deg", ConstraintKind::Angle) - std::numbers::pi) < 1e-12,
        "convertDoc: 180 deg → pi"
    );
    check(
        std::abs(*convertDocumentParameterToInternal(180.0, "degree", ConstraintKind::Angle) - std::numbers::pi) < 1e-12,
        "convertDoc: 180 degree → pi"
    );
    check(
        std::abs(*convertDocumentParameterToInternal(180.0, "degrees", ConstraintKind::Angle) - std::numbers::pi) < 1e-12,
        "convertDoc: 180 degrees → pi"
    );
    check(
        std::abs(*convertDocumentParameterToInternal(180.0, "", ConstraintKind::Angle) - std::numbers::pi) < 1e-12,
        "convertDoc: 180 empty-unit → pi (angle default deg)"
    );

    // Angle: rad/radian/radians → keep value as-is (already in radians)
    check(
        std::abs(*convertDocumentParameterToInternal(1.5, "rad", ConstraintKind::Angle) - 1.5) < 1e-12,
        "convertDoc: 1.5 rad → 1.5"
    );
    check(
        std::abs(*convertDocumentParameterToInternal(1.5, "radians", ConstraintKind::Angle) - 1.5) < 1e-12,
        "convertDoc: 1.5 radians → 1.5"
    );

    // Angle: invalid unit
    check(!convertDocumentParameterToInternal(1.0, "mm", ConstraintKind::Angle).has_value(), "convertDoc: mm invalid for Angle");

    // Length: all supported units
    check(std::abs(*convertDocumentParameterToInternal(1.0, "cm", ConstraintKind::Distance) - 10.0) < 1e-12, "convertDoc: 1 cm → 10");
    check(std::abs(*convertDocumentParameterToInternal(1.0, "m", ConstraintKind::Distance) - 1000.0) < 1e-10, "convertDoc: 1 m → 1000");
    check(std::abs(*convertDocumentParameterToInternal(1.0, "km", ConstraintKind::Distance) - 1000000.0) < 1.0, "convertDoc: 1 km → 1e6");
    check(std::abs(*convertDocumentParameterToInternal(1000.0, "um", ConstraintKind::Distance) - 1.0) < 1e-9, "convertDoc: 1000 um → 1");
    check(std::abs(*convertDocumentParameterToInternal(1000000.0, "nm", ConstraintKind::Distance) - 1.0) < 1e-6, "convertDoc: 1e6 nm → 1");
    check(std::abs(*convertDocumentParameterToInternal(1.0, "in", ConstraintKind::Distance) - 25.4) < 1e-10, "convertDoc: 1 in → 25.4");
    check(std::abs(*convertDocumentParameterToInternal(1.0, "ft", ConstraintKind::Distance) - 304.8) < 1e-10, "convertDoc: 1 ft → 304.8");

    // Length: alternative spellings (centimeter, metre…)
    check(std::abs(*convertDocumentParameterToInternal(1.0, "centimeter", ConstraintKind::Distance) - 10.0) < 1e-12, "convertDoc: centimeter");
    check(std::abs(*convertDocumentParameterToInternal(1.0, "metre", ConstraintKind::Distance) - 1000.0) < 1e-10, "convertDoc: metre");
    check(std::abs(*convertDocumentParameterToInternal(1.0, "inch", ConstraintKind::Distance) - 25.4) < 1e-10, "convertDoc: inch");
    check(std::abs(*convertDocumentParameterToInternal(1.0, "feet", ConstraintKind::Distance) - 304.8) < 1e-10, "convertDoc: feet");

    // Length: normalised spellings (uppercase/underscore)
    check(std::abs(*convertDocumentParameterToInternal(1.0, "CM", ConstraintKind::Distance) - 10.0) < 1e-12, "convertDoc: CM normalised");

    // Length: invalid unit
    check(!convertDocumentParameterToInternal(1.0, "kg", ConstraintKind::Distance).has_value(), "convertDoc: kg invalid for Distance");

    // Dimensionless: no unit → pass-through
    check(std::abs(*convertDocumentParameterToInternal(7.0, "", ConstraintKind::Horizontal) - 7.0) < 1e-12, "convertDoc: dimensionless no unit");
    // Dimensionless: any unit → fail
    check(!convertDocumentParameterToInternal(7.0, "mm", ConstraintKind::Horizontal).has_value(), "convertDoc: mm invalid for Horizontal");
}

// ── CompatModel ────────────────────────────────────────────────────────────

void testCompatModelEmpty()
{
    McSolverEngine::Compat::SketchModel model;
    check(model.empty(), "SketchModel: fresh model is empty");
    check(model.geometryCount() == 0, "SketchModel: fresh model geometryCount == 0");
    check(model.constraintCount() == 0, "SketchModel: fresh model constraintCount == 0");

    model.addLineSegment({0.0, 0.0}, {1.0, 0.0});
    check(!model.empty(), "SketchModel: non-empty after addLineSegment");

    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Horizontal,
        .first = {.geometryIndex = 0, .role = McSolverEngine::Compat::PointRole::None},
    });
    check(model.constraintCount() == 1, "SketchModel: constraintCount == 1 after addConstraint");
}

void testCompatModelSetPlacement()
{
    McSolverEngine::Compat::SketchModel model;
    // Default placement has qw=1 and everything else 0
    const auto& defPlacement = model.placement();
    check(defPlacement.px == 0.0 && defPlacement.py == 0.0 && defPlacement.pz == 0.0, "default placement translation is zero");
    check(defPlacement.qx == 0.0 && defPlacement.qy == 0.0 && defPlacement.qz == 0.0 && defPlacement.qw == 1.0, "default placement quaternion is identity");

    McSolverEngine::Compat::Placement placement {
        .px = 1.0, .py = 2.0, .pz = 3.0,
        .qx = 0.1, .qy = 0.2, .qz = 0.3, .qw = 0.9,
    };
    model.setPlacement(placement);
    const auto& p = model.placement();
    check(p.px == 1.0 && p.py == 2.0 && p.pz == 3.0, "setPlacement: translation roundtrip");
    check(p.qx == 0.1 && p.qy == 0.2 && p.qz == 0.3 && p.qw == 0.9, "setPlacement: quaternion roundtrip");
}

void testCompatModelAddGeometryReturnIndex()
{
    McSolverEngine::Compat::SketchModel model;
    const int p0 = model.addPoint({0.0, 0.0});
    const int l1 = model.addLineSegment({0.0, 0.0}, {1.0, 0.0});
    const int c2 = model.addCircle({0.0, 0.0}, 1.0);
    const int a3 = model.addArc({0.0, 0.0}, 1.0, 0.0, std::numbers::pi);
    const int e4 = model.addEllipse({0.0, 0.0}, {1.0, 0.0}, 0.5);
    const int ae5 = model.addArcOfEllipse({0.0, 0.0}, {1.0, 0.0}, 0.5, 0.0, std::numbers::pi);
    const int ah6 = model.addArcOfHyperbola({0.0, 0.0}, {2.0, 0.0}, 1.0, 0.0, std::numbers::pi / 4.0);
    const int ap7 = model.addArcOfParabola({0.0, 0.0}, {0.0, 1.0}, -2.0, 2.0);
    const int bs8 = model.addBSpline(
        {{{0.0, 0.0}, 1.0}, {{1.0, 1.0}, 1.0}, {{2.0, 0.0}, 1.0}},
        {{0.0, 3}, {1.0, 3}},
        2
    );

    check(p0 == 0, "addPoint returns 0");
    check(l1 == 1, "addLineSegment returns 1");
    check(c2 == 2, "addCircle returns 2");
    check(a3 == 3, "addArc returns 3");
    check(e4 == 4, "addEllipse returns 4");
    check(ae5 == 5, "addArcOfEllipse returns 5");
    check(ah6 == 6, "addArcOfHyperbola returns 6");
    check(ap7 == 7, "addArcOfParabola returns 7");
    check(bs8 == 8, "addBSpline returns 8");
    check(model.geometryCount() == 9, "model has 9 geometries");

    // Verify kinds
    check(model.geometries()[5].kind == McSolverEngine::Compat::GeometryKind::ArcOfEllipse, "ArcOfEllipse kind");
    check(model.geometries()[6].kind == McSolverEngine::Compat::GeometryKind::ArcOfHyperbola, "ArcOfHyperbola kind");

    // addConstraint return value
    const int c0 = model.addConstraint({.kind = McSolverEngine::Compat::ConstraintKind::Horizontal});
    check(c0 == 0, "addConstraint returns 0 for first constraint");
    const int c1 = model.addConstraint({.kind = McSolverEngine::Compat::ConstraintKind::Vertical});
    check(c1 == 1, "addConstraint returns 1 for second constraint");
}

void testCompatModelToStringFunctions()
{
    using McSolverEngine::Compat::GeometryKind;
    using McSolverEngine::Compat::ConstraintKind;
    using McSolverEngine::Compat::InternalAlignmentType;

    // GeometryKind
    check(McSolverEngine::Compat::toString(GeometryKind::Point) == "Point", "toString: Point");
    check(McSolverEngine::Compat::toString(GeometryKind::LineSegment) == "LineSegment", "toString: LineSegment");
    check(McSolverEngine::Compat::toString(GeometryKind::Circle) == "Circle", "toString: Circle");
    check(McSolverEngine::Compat::toString(GeometryKind::Arc) == "Arc", "toString: Arc");
    check(McSolverEngine::Compat::toString(GeometryKind::Ellipse) == "Ellipse", "toString: Ellipse");
    check(McSolverEngine::Compat::toString(GeometryKind::ArcOfEllipse) == "ArcOfEllipse", "toString: ArcOfEllipse");
    check(McSolverEngine::Compat::toString(GeometryKind::ArcOfHyperbola) == "ArcOfHyperbola", "toString: ArcOfHyperbola");
    check(McSolverEngine::Compat::toString(GeometryKind::ArcOfParabola) == "ArcOfParabola", "toString: ArcOfParabola");
    check(McSolverEngine::Compat::toString(GeometryKind::BSpline) == "BSpline", "toString: BSpline");

    // ConstraintKind
    check(McSolverEngine::Compat::toString(ConstraintKind::Coincident) == "Coincident", "toString: Coincident");
    check(McSolverEngine::Compat::toString(ConstraintKind::Horizontal) == "Horizontal", "toString: Horizontal");
    check(McSolverEngine::Compat::toString(ConstraintKind::Vertical) == "Vertical", "toString: Vertical");
    check(McSolverEngine::Compat::toString(ConstraintKind::DistanceX) == "DistanceX", "toString: DistanceX");
    check(McSolverEngine::Compat::toString(ConstraintKind::DistanceY) == "DistanceY", "toString: DistanceY");
    check(McSolverEngine::Compat::toString(ConstraintKind::Distance) == "Distance", "toString: Distance");
    check(McSolverEngine::Compat::toString(ConstraintKind::Parallel) == "Parallel", "toString: Parallel");
    check(McSolverEngine::Compat::toString(ConstraintKind::Tangent) == "Tangent", "toString: Tangent");
    check(McSolverEngine::Compat::toString(ConstraintKind::Perpendicular) == "Perpendicular", "toString: Perpendicular");
    check(McSolverEngine::Compat::toString(ConstraintKind::Angle) == "Angle", "toString: Angle");
    check(McSolverEngine::Compat::toString(ConstraintKind::Radius) == "Radius", "toString: Radius");
    check(McSolverEngine::Compat::toString(ConstraintKind::Diameter) == "Diameter", "toString: Diameter");
    check(McSolverEngine::Compat::toString(ConstraintKind::Equal) == "Equal", "toString: Equal");
    check(McSolverEngine::Compat::toString(ConstraintKind::Symmetric) == "Symmetric", "toString: Symmetric");
    check(McSolverEngine::Compat::toString(ConstraintKind::PointOnObject) == "PointOnObject", "toString: PointOnObject");
    check(McSolverEngine::Compat::toString(ConstraintKind::InternalAlignment) == "InternalAlignment", "toString: InternalAlignment");
    check(McSolverEngine::Compat::toString(ConstraintKind::SnellsLaw) == "SnellsLaw", "toString: SnellsLaw");
    check(McSolverEngine::Compat::toString(ConstraintKind::Block) == "Block", "toString: Block");
    check(McSolverEngine::Compat::toString(ConstraintKind::Weight) == "Weight", "toString: Weight");

    // InternalAlignmentType
    check(McSolverEngine::Compat::toString(InternalAlignmentType::Undef) == "Undef", "toString: Undef");
    check(McSolverEngine::Compat::toString(InternalAlignmentType::EllipseMajorDiameter) == "EllipseMajorDiameter", "toString: EllipseMajorDiameter");
    check(McSolverEngine::Compat::toString(InternalAlignmentType::EllipseMinorDiameter) == "EllipseMinorDiameter", "toString: EllipseMinorDiameter");
    check(McSolverEngine::Compat::toString(InternalAlignmentType::EllipseFocus1) == "EllipseFocus1", "toString: EllipseFocus1");
    check(McSolverEngine::Compat::toString(InternalAlignmentType::EllipseFocus2) == "EllipseFocus2", "toString: EllipseFocus2");
    check(McSolverEngine::Compat::toString(InternalAlignmentType::HyperbolaMajor) == "HyperbolaMajor", "toString: HyperbolaMajor");
    check(McSolverEngine::Compat::toString(InternalAlignmentType::HyperbolaMinor) == "HyperbolaMinor", "toString: HyperbolaMinor");
    check(McSolverEngine::Compat::toString(InternalAlignmentType::HyperbolaFocus) == "HyperbolaFocus", "toString: HyperbolaFocus");
    check(McSolverEngine::Compat::toString(InternalAlignmentType::ParabolaFocus) == "ParabolaFocus", "toString: ParabolaFocus");
    check(McSolverEngine::Compat::toString(InternalAlignmentType::BSplineControlPoint) == "BSplineControlPoint", "toString: BSplineControlPoint");
    check(McSolverEngine::Compat::toString(InternalAlignmentType::BSplineKnotPoint) == "BSplineKnotPoint", "toString: BSplineKnotPoint");
    check(McSolverEngine::Compat::toString(InternalAlignmentType::ParabolaFocalAxis) == "ParabolaFocalAxis", "toString: ParabolaFocalAxis");
}

void testIsPhase1ConstraintSupported()
{
    using McSolverEngine::Compat::ConstraintKind;
    using McSolverEngine::Compat::isPhase1ConstraintSupported;

    // All defined constraint kinds should be supported
    check(isPhase1ConstraintSupported(ConstraintKind::Coincident), "phase1: Coincident");
    check(isPhase1ConstraintSupported(ConstraintKind::Horizontal), "phase1: Horizontal");
    check(isPhase1ConstraintSupported(ConstraintKind::Vertical), "phase1: Vertical");
    check(isPhase1ConstraintSupported(ConstraintKind::DistanceX), "phase1: DistanceX");
    check(isPhase1ConstraintSupported(ConstraintKind::DistanceY), "phase1: DistanceY");
    check(isPhase1ConstraintSupported(ConstraintKind::Distance), "phase1: Distance");
    check(isPhase1ConstraintSupported(ConstraintKind::Parallel), "phase1: Parallel");
    check(isPhase1ConstraintSupported(ConstraintKind::Tangent), "phase1: Tangent");
    check(isPhase1ConstraintSupported(ConstraintKind::Perpendicular), "phase1: Perpendicular");
    check(isPhase1ConstraintSupported(ConstraintKind::Angle), "phase1: Angle");
    check(isPhase1ConstraintSupported(ConstraintKind::Radius), "phase1: Radius");
    check(isPhase1ConstraintSupported(ConstraintKind::Diameter), "phase1: Diameter");
    check(isPhase1ConstraintSupported(ConstraintKind::Equal), "phase1: Equal");
    check(isPhase1ConstraintSupported(ConstraintKind::Symmetric), "phase1: Symmetric");
    check(isPhase1ConstraintSupported(ConstraintKind::PointOnObject), "phase1: PointOnObject");
    check(isPhase1ConstraintSupported(ConstraintKind::InternalAlignment), "phase1: InternalAlignment");
    check(isPhase1ConstraintSupported(ConstraintKind::SnellsLaw), "phase1: SnellsLaw");
    check(isPhase1ConstraintSupported(ConstraintKind::Block), "phase1: Block");
    check(isPhase1ConstraintSupported(ConstraintKind::Weight), "phase1: Weight");
}

// ── CompatSolver ───────────────────────────────────────────────────────────

void testDiameterConstraint()
{
    McSolverEngine::Compat::SketchModel model;
    const int circleId = model.addCircle({0.0, 0.0}, 5.0);
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Diameter,
        .first = {.geometryIndex = circleId, .role = McSolverEngine::Compat::PointRole::None},
        .value = 6.0,
        .hasValue = true,
    });

    const auto result = McSolverEngine::Compat::solveSketch(model);
    check(result.solved(), "Diameter: sketch solves");
    const auto& circle = std::get<McSolverEngine::Compat::CircleGeometry>(model.geometries().front().data);
    check(std::abs(circle.radius - 3.0) < 1e-8, "Diameter=6 → radius=3");
    check(result.degreesOfFreedom >= 0, "Diameter: degreesOfFreedom is non-negative");
}

void testEqualConstraint()
{
    // Two lines, Equal forces them to the same length.
    McSolverEngine::Compat::SketchModel model;
    const int lineA = model.addLineSegment({0.0, 0.0}, {4.0, 0.0});
    const int lineB = model.addLineSegment({0.0, 2.0}, {7.0, 2.0});
    // Pin lineA start to origin and fix its direction
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceX,
        .first = {.geometryIndex = lineA, .role = McSolverEngine::Compat::PointRole::Start},
        .value = 0.0,
        .hasValue = true,
    });
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceY,
        .first = {.geometryIndex = lineA, .role = McSolverEngine::Compat::PointRole::Start},
        .value = 0.0,
        .hasValue = true,
    });
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Horizontal,
        .first = {.geometryIndex = lineA, .role = McSolverEngine::Compat::PointRole::None},
    });
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Distance,
        .first = {.geometryIndex = lineA, .role = McSolverEngine::Compat::PointRole::None},
        .value = 5.0,
        .hasValue = true,
    });
    // Pin lineB start in X and fix its direction
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceX,
        .first = {.geometryIndex = lineB, .role = McSolverEngine::Compat::PointRole::Start},
        .value = 0.0,
        .hasValue = true,
    });
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceY,
        .first = {.geometryIndex = lineB, .role = McSolverEngine::Compat::PointRole::Start},
        .value = 2.0,
        .hasValue = true,
    });
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Horizontal,
        .first = {.geometryIndex = lineB, .role = McSolverEngine::Compat::PointRole::None},
    });
    // Equal: lineA length == lineB length
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Equal,
        .first = {.geometryIndex = lineA, .role = McSolverEngine::Compat::PointRole::None},
        .second = {.geometryIndex = lineB, .role = McSolverEngine::Compat::PointRole::None},
    });

    const auto result = McSolverEngine::Compat::solveSketch(model);
    check(result.solved(), "Equal: sketch solves");

    const auto& segA = std::get<McSolverEngine::Compat::LineSegmentGeometry>(model.geometries()[0].data);
    const auto& segB = std::get<McSolverEngine::Compat::LineSegmentGeometry>(model.geometries()[1].data);
    const double lenA = std::abs(segA.end.x - segA.start.x);
    const double lenB = std::abs(segB.end.x - segB.start.x);
    check(std::abs(lenA - 5.0) < 1e-8, "Equal: lineA length == 5");
    check(std::abs(lenA - lenB) < 1e-8, "Equal: both lines have the same length");
}

void testParallelConstraint()
{
    // Two lines, Parallel forces them to have the same slope.
    McSolverEngine::Compat::SketchModel model;
    const int lineA = model.addLineSegment({0.0, 0.0}, {3.0, 0.0});
    const int lineB = model.addLineSegment({0.0, 2.0}, {4.0, 3.0});

    // Fix lineA: horizontal at length 4
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceX,
        .first = {.geometryIndex = lineA, .role = McSolverEngine::Compat::PointRole::Start},
        .value = 0.0, .hasValue = true,
    });
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceY,
        .first = {.geometryIndex = lineA, .role = McSolverEngine::Compat::PointRole::Start},
        .value = 0.0, .hasValue = true,
    });
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Horizontal,
        .first = {.geometryIndex = lineA, .role = McSolverEngine::Compat::PointRole::None},
    });
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Distance,
        .first = {.geometryIndex = lineA, .role = McSolverEngine::Compat::PointRole::None},
        .value = 4.0, .hasValue = true,
    });

    // Fix lineB start position
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceX,
        .first = {.geometryIndex = lineB, .role = McSolverEngine::Compat::PointRole::Start},
        .value = 0.0, .hasValue = true,
    });
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceY,
        .first = {.geometryIndex = lineB, .role = McSolverEngine::Compat::PointRole::Start},
        .value = 2.0, .hasValue = true,
    });
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Distance,
        .first = {.geometryIndex = lineB, .role = McSolverEngine::Compat::PointRole::None},
        .value = 3.0, .hasValue = true,
    });

    // Parallel
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Parallel,
        .first = {.geometryIndex = lineA, .role = McSolverEngine::Compat::PointRole::None},
        .second = {.geometryIndex = lineB, .role = McSolverEngine::Compat::PointRole::None},
    });

    const auto result = McSolverEngine::Compat::solveSketch(model);
    check(result.solved(), "Parallel: sketch solves");

    const auto& segA = std::get<McSolverEngine::Compat::LineSegmentGeometry>(model.geometries()[0].data);
    const auto& segB = std::get<McSolverEngine::Compat::LineSegmentGeometry>(model.geometries()[1].data);
    const double dxA = segA.end.x - segA.start.x;
    const double dyA = segA.end.y - segA.start.y;
    const double dxB = segB.end.x - segB.start.x;
    const double dyB = segB.end.y - segB.start.y;
    // cross-product of direction vectors should be ~0 for parallel lines
    const double cross = std::abs(dxA * dyB - dyA * dxB);
    check(cross < 1e-6, "Parallel: lines are parallel (cross-product ≈ 0)");
}

void testSolveConflictingRedundant()
{
    // Over-constrained sketch: line pinned at both ends with conflicting Distance.
    McSolverEngine::Compat::SketchModel model;
    const int lineId = model.addLineSegment({0.0, 0.0}, {5.0, 0.0});
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceX,
        .first = {.geometryIndex = lineId, .role = McSolverEngine::Compat::PointRole::Start},
        .value = 0.0, .hasValue = true,
    });
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceY,
        .first = {.geometryIndex = lineId, .role = McSolverEngine::Compat::PointRole::Start},
        .value = 0.0, .hasValue = true,
    });
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Horizontal,
        .first = {.geometryIndex = lineId, .role = McSolverEngine::Compat::PointRole::None},
    });
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Distance,
        .first = {.geometryIndex = lineId, .role = McSolverEngine::Compat::PointRole::None},
        .value = 4.0, .hasValue = true,
    });
    // A second conflicting length – completely fixes the sketch but contradicts the first
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Distance,
        .first = {.geometryIndex = lineId, .role = McSolverEngine::Compat::PointRole::None},
        .value = 6.0, .hasValue = true,
    });

    const auto result = McSolverEngine::Compat::solveSketch(model);
    // Solver should report over-constrained / conflicting
    check(!result.solved() || !result.conflicting.empty() || !result.redundant.empty(),
          "conflicting: over-constrained sketch has non-empty conflicting or redundant list, or fails to solve");
}

void testDegreesOfFreedom()
{
    // A fully-constrained sketch should have 0 DOF on success.
    McSolverEngine::Compat::SketchModel model;
    const int lineId = model.addLineSegment({0.0, 0.0}, {5.0, 0.0});
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceX,
        .first = {.geometryIndex = lineId, .role = McSolverEngine::Compat::PointRole::Start},
        .value = 0.0, .hasValue = true,
    });
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::DistanceY,
        .first = {.geometryIndex = lineId, .role = McSolverEngine::Compat::PointRole::Start},
        .value = 0.0, .hasValue = true,
    });
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Horizontal,
        .first = {.geometryIndex = lineId, .role = McSolverEngine::Compat::PointRole::None},
    });
    model.addConstraint({
        .kind = McSolverEngine::Compat::ConstraintKind::Distance,
        .first = {.geometryIndex = lineId, .role = McSolverEngine::Compat::PointRole::None},
        .value = 5.0, .hasValue = true,
    });

    const auto result = McSolverEngine::Compat::solveSketch(model);
    check(result.solved(), "DOF test: sketch solves");
    check(result.degreesOfFreedom == 0, "DOF test: fully constrained sketch has 0 DOF");
}

// ── ZipExtract ─────────────────────────────────────────────────────────────

void testZipExtractMissingFile()
{
    const auto result = McSolverEngine::ZipExtract::extractDocumentXml(
        "/nonexistent/path/that/does/not/exist.FCStd"
    );
    check(!result.success, "ZipExtract: nonexistent file reports failure");
    check(!result.errorMessage.empty(), "ZipExtract: nonexistent file has error message");
}

// ── GeometryExport ─────────────────────────────────────────────────────────

void testGeometryExportFlags()
{
    McSolverEngine::Compat::SketchModel model;
    // construction line
    model.addLineSegment({0.0, 0.0}, {1.0, 0.0}, /*construction*/ true);
    // external line
    model.addLineSegment({2.0, 0.0}, {3.0, 0.0}, /*construction*/ false, /*external*/ true);
    // blocked line
    model.addLineSegment({4.0, 0.0}, {5.0, 0.0}, /*construction*/ false, /*external*/ false, /*blocked*/ true);
    // regular line
    model.addLineSegment({6.0, 0.0}, {7.0, 0.0});

    const auto exported = McSolverEngine::Geometry::exportSketchGeometry(model);
    check(exported.exported(), "GeometryExport flags: export succeeds");
    check(exported.geometries.size() == 4, "GeometryExport flags: 4 geometries exported");

    check(exported.geometries[0].geometry.construction, "GeometryExport flags: record[0] is construction");
    check(!exported.geometries[0].geometry.external, "GeometryExport flags: record[0] is not external");
    check(!exported.geometries[0].geometry.blocked, "GeometryExport flags: record[0] is not blocked");

    check(!exported.geometries[1].geometry.construction, "GeometryExport flags: record[1] is not construction");
    check(exported.geometries[1].geometry.external, "GeometryExport flags: record[1] is external");
    check(!exported.geometries[1].geometry.blocked, "GeometryExport flags: record[1] is not blocked");

    check(!exported.geometries[2].geometry.construction, "GeometryExport flags: record[2] is not construction");
    check(!exported.geometries[2].geometry.external, "GeometryExport flags: record[2] is not external");
    check(exported.geometries[2].geometry.blocked, "GeometryExport flags: record[2] is blocked");

    check(!exported.geometries[3].geometry.construction, "GeometryExport flags: record[3] is not construction");
    check(!exported.geometries[3].geometry.external, "GeometryExport flags: record[3] is not external");
    check(!exported.geometries[3].geometry.blocked, "GeometryExport flags: record[3] is not blocked");
}

void testGeometryExportGeometryIndex()
{
    McSolverEngine::Compat::SketchModel model;
    model.addPoint({0.0, 0.0});
    model.addLineSegment({0.0, 0.0}, {1.0, 0.0});
    model.addCircle({0.5, 0.5}, 1.0);

    const auto exported = McSolverEngine::Geometry::exportSketchGeometry(model);
    check(exported.exported(), "GeometryExport index: export succeeds");
    check(exported.geometries.size() == 3, "GeometryExport index: 3 records");
    check(exported.geometries[0].geometryIndex == 0, "GeometryExport index: record[0].geometryIndex == 0");
    check(exported.geometries[1].geometryIndex == 1, "GeometryExport index: record[1].geometryIndex == 1");
    check(exported.geometries[2].geometryIndex == 2, "GeometryExport index: record[2].geometryIndex == 2");
}

// ── DocumentXml ────────────────────────────────────────────────────────────

void testDocumentXmlEmptySketch()
{
    constexpr std::string_view xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Document>
    <ObjectData Count="1">
        <Object name="EmptySketch">
            <Properties Count="2" TransientCount="0">
                <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                    <ConstraintList count="0">
                    </ConstraintList>
                </Property>
                <Property name="Geometry" type="Part::PropertyGeometryList">
                    <GeometryList count="0">
                    </GeometryList>
                </Property>
            </Properties>
        </Object>
    </ObjectData>
</Document>)";

    const auto imported = McSolverEngine::DocumentXml::importSketchFromDocumentXml(xml, "EmptySketch");
    check(imported.imported(), "empty sketch: imported successfully");
    check(imported.status == McSolverEngine::DocumentXml::ImportStatus::Success, "empty sketch: status is Success");
    check(imported.model.geometryCount() == 0, "empty sketch: 0 geometries");
    check(imported.model.constraintCount() == 0, "empty sketch: 0 constraints");

    // Solving an empty sketch is valid (trivially underconstrained)
    auto emptyModel = imported.model;
    const auto solveResult = McSolverEngine::Compat::solveSketch(emptyModel);
    check(solveResult.solved(), "empty sketch: solves successfully");
}

void testDocumentXmlEmptySketchName()
{
    // When sketchName is empty the importer should return the first sketch found.
    constexpr std::string_view xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Document>
    <ObjectData Count="1">
        <Object name="OnlySketch">
            <Properties Count="2" TransientCount="0">
                <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                    <ConstraintList count="1">
                        <Constrain Name="" Type="2" Value="0.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                    </ConstraintList>
                </Property>
                <Property name="Geometry" type="Part::PropertyGeometryList">
                    <GeometryList count="1">
                        <Geometry type="Part::GeomLineSegment" id="1" migrated="1">
                            <LineSegment StartX="0.0" StartY="0.0" StartZ="0.0" EndX="3.0" EndY="0.0" EndZ="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                    </GeometryList>
                </Property>
            </Properties>
        </Object>
    </ObjectData>
</Document>)";

    // Empty sketchName → first sketch object in document
    const auto imported = McSolverEngine::DocumentXml::importSketchFromDocumentXml(xml, "");
    check(imported.imported(), "empty sketchName: imported successfully");
    check(imported.model.geometryCount() == 1, "empty sketchName: 1 geometry");
    check(imported.sketchName == "OnlySketch", "empty sketchName: resolved sketch name is OnlySketch");
}

}  // namespace

int main()
{
    testParseStrictNumeric();
    testNormalizeUnitSuffix();
    testIsKindHelpers();
    testConvertApiParameterToInternal();
    testParseDocumentParameterValue();
    testConvertDocumentParameterToInternal();

    testCompatModelEmpty();
    testCompatModelSetPlacement();
    testCompatModelAddGeometryReturnIndex();
    testCompatModelToStringFunctions();
    testIsPhase1ConstraintSupported();

    testDiameterConstraint();
    testEqualConstraint();
    testParallelConstraint();
    testSolveConflictingRedundant();
    testDegreesOfFreedom();

    testZipExtractMissingFile();

    testGeometryExportFlags();
    testGeometryExportGeometryIndex();

    testDocumentXmlEmptySketch();
    testDocumentXmlEmptySketchName();

    if (failCount > 0) {
        std::cerr << "\n" << failCount << " test(s) failed.\n";
        return 1;
    }
    std::cout << "All unit tests passed.\n";
    return 0;
}
