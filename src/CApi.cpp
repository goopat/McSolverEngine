#include "McSolverEngine/CApi.h"

#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <string_view>
#include <vector>

#include "McSolverEngine/BRepExport.h"
#include "McSolverEngine/CompatModel.h"
#include "McSolverEngine/CompatSolver.h"
#include "McSolverEngine/DocumentXml.h"
#include "McSolverEngine/Engine.h"
#include "McSolverEngine/GeometryExport.h"
#include "WindowsAssertMode.h"

namespace
{

using McSolverEngine::Compat::ArcGeometry;
using McSolverEngine::Compat::ArcOfEllipseGeometry;
using McSolverEngine::Compat::ArcOfHyperbolaGeometry;
using McSolverEngine::Compat::ArcOfParabolaGeometry;
using McSolverEngine::Compat::BSplineGeometry;
using McSolverEngine::Compat::CircleGeometry;
using McSolverEngine::Compat::EllipseGeometry;
using McSolverEngine::Compat::GeometryKind;
using McSolverEngine::Compat::LineSegmentGeometry;
using McSolverEngine::Compat::Placement;
using McSolverEngine::Compat::PointGeometry;
using McSolverEngine::Compat::Point2;

[[nodiscard]] std::string_view safeStringView(const char* value)
{
    return value ? std::string_view(value) : std::string_view {};
}

[[nodiscard]] bool buildParameterMap(
    const char* const* parameterKeysUtf8,
    const char* const* parameterValuesUtf8,
    int parameterCount,
    McSolverEngine::ParameterMap& parameters
)
{
    parameters.clear();

    if (parameterCount < 0) {
        return false;
    }
    if (parameterCount == 0) {
        return true;
    }
    if (!parameterKeysUtf8 || !parameterValuesUtf8) {
        return false;
    }

    for (int i = 0; i < parameterCount; ++i) {
        if (!parameterKeysUtf8[i] || !parameterValuesUtf8[i]) {
            return false;
        }
        parameters.insert_or_assign(std::string(parameterKeysUtf8[i]), std::string(parameterValuesUtf8[i]));
    }

    return true;
}

[[nodiscard]] std::string_view toString(McSolverEngine::DocumentXml::ImportStatus status)
{
    switch (status) {
        case McSolverEngine::DocumentXml::ImportStatus::Success:
            return "Success";
        case McSolverEngine::DocumentXml::ImportStatus::Partial:
            return "Partial";
        case McSolverEngine::DocumentXml::ImportStatus::Failed:
            return "Failed";
    }
    return "Failed";
}

[[nodiscard]] McSolverEngineResultCode toCApiImportFailureCode(
    McSolverEngine::DocumentXml::ImportErrorCode errorCode
)
{
    switch (errorCode) {
        case McSolverEngine::DocumentXml::ImportErrorCode::None:
            return MCSOLVERENGINE_RESULT_IMPORT_FAILED;
        case McSolverEngine::DocumentXml::ImportErrorCode::VarSetExpressionUnsupportedSubset:
            return MCSOLVERENGINE_RESULT_VARSET_EXPRESSION_UNSUPPORTED_SUBSET;
    }
    return MCSOLVERENGINE_RESULT_IMPORT_FAILED;
}

[[nodiscard]] std::string_view toString(McSolverEngine::Compat::SolveStatus status)
{
    switch (status) {
        case McSolverEngine::Compat::SolveStatus::Success:
            return "Success";
        case McSolverEngine::Compat::SolveStatus::Converged:
            return "Converged";
        case McSolverEngine::Compat::SolveStatus::Failed:
            return "Failed";
        case McSolverEngine::Compat::SolveStatus::Invalid:
            return "Invalid";
        case McSolverEngine::Compat::SolveStatus::Unsupported:
            return "Unsupported";
    }
    return "Failed";
}

[[nodiscard]] std::string_view toString(McSolverEngine::Geometry::ExportStatus status)
{
    switch (status) {
        case McSolverEngine::Geometry::ExportStatus::Success:
            return "Success";
        case McSolverEngine::Geometry::ExportStatus::Failed:
            return "Failed";
    }
    return "Failed";
}

[[nodiscard]] std::string_view toString(McSolverEngine::BRep::ExportStatus status)
{
    switch (status) {
        case McSolverEngine::BRep::ExportStatus::Success:
            return "Success";
        case McSolverEngine::BRep::ExportStatus::Failed:
            return "Failed";
        case McSolverEngine::BRep::ExportStatus::OpenCascadeUnavailable:
            return "OpenCascadeUnavailable";
    }
    return "Failed";
}

[[nodiscard]] bool assignOutputString(std::string_view value, char** out)
{
    if (!out) {
        return false;
    }

    auto* buffer = new (std::nothrow) char[value.size() + 1];
    if (!buffer) {
        return false;
    }
    std::memcpy(buffer, value.data(), value.size());
    buffer[value.size()] = '\0';
    *out = buffer;
    return true;
}

[[nodiscard]] bool assignOwnedString(std::string_view value, const char** out)
{
    if (!out) {
        return false;
    }

    char* buffer = nullptr;
    if (!assignOutputString(value, &buffer)) {
        return false;
    }
    *out = buffer;
    return true;
}

void resetOutput(char** out)
{
    if (out) {
        *out = nullptr;
    }
}

void resetOutput(McSolverEngineGeometryResult** out)
{
    if (out) {
        *out = nullptr;
    }
}

void resetOutput(McSolverEngineBRepResult** out)
{
    if (out) {
        *out = nullptr;
    }
}

[[nodiscard]] McSolverEnginePoint2 toCApiPoint2(const Point2& point) noexcept
{
    return McSolverEnginePoint2 {.x = point.x, .y = point.y};
}

[[nodiscard]] McSolverEnginePlacement toCApiPlacement(const Placement& placement) noexcept
{
    return McSolverEnginePlacement {
        .px = placement.px,
        .py = placement.py,
        .pz = placement.pz,
        .qx = placement.qx,
        .qy = placement.qy,
        .qz = placement.qz,
        .qw = placement.qw,
    };
}

[[nodiscard]] McSolverEngineGeometryKind toCApiGeometryKind(GeometryKind kind) noexcept
{
    switch (kind) {
        case GeometryKind::Point:
            return MCSOLVERENGINE_GEOMETRY_POINT;
        case GeometryKind::LineSegment:
            return MCSOLVERENGINE_GEOMETRY_LINE_SEGMENT;
        case GeometryKind::Circle:
            return MCSOLVERENGINE_GEOMETRY_CIRCLE;
        case GeometryKind::Arc:
            return MCSOLVERENGINE_GEOMETRY_ARC;
        case GeometryKind::Ellipse:
            return MCSOLVERENGINE_GEOMETRY_ELLIPSE;
        case GeometryKind::ArcOfEllipse:
            return MCSOLVERENGINE_GEOMETRY_ARC_OF_ELLIPSE;
        case GeometryKind::ArcOfHyperbola:
            return MCSOLVERENGINE_GEOMETRY_ARC_OF_HYPERBOLA;
        case GeometryKind::ArcOfParabola:
            return MCSOLVERENGINE_GEOMETRY_ARC_OF_PARABOLA;
        case GeometryKind::BSpline:
            return MCSOLVERENGINE_GEOMETRY_BSPLINE;
    }
    return MCSOLVERENGINE_GEOMETRY_POINT;
}

void freeGeometryResult(McSolverEngineGeometryResult* value) noexcept
{
    if (!value) {
        return;
    }

    delete[] value->sketchName;
    delete[] value->importStatus;
    auto* messages = value->messages;
    if (messages) {
        for (int i = 0; i < value->messageCount; ++i) {
            delete[] messages[i];
        }
        delete[] messages;
    }
    delete[] value->solveStatus;
    delete[] value->conflicting;
    delete[] value->redundant;
    delete[] value->partiallyRedundant;
    delete[] value->exportKind;
    delete[] value->exportStatus;

    auto* geometries = const_cast<McSolverEngineGeometryRecord*>(value->geometries);
    if (geometries) {
        for (int i = 0; i < value->geometryCount; ++i) {
            delete[] geometries[i].poles;
            delete[] geometries[i].knots;
            if (auto* refs = geometries[i].constraints) {
                for (int j = 0; j < geometries[i].constraintCount; ++j) {
                    delete[] refs[j].expression;
                }
                delete[] refs;
            }
        }
        delete[] geometries;
    }
    delete value;
}

void freeBRepResult(McSolverEngineBRepResult* value) noexcept
{
    if (!value) {
        return;
    }

    delete[] value->sketchName;
    delete[] value->importStatus;
    auto* messages = value->messages;
    if (messages) {
        for (int i = 0; i < value->messageCount; ++i) {
            delete[] messages[i];
        }
        delete[] messages;
    }
    delete[] value->solveStatus;
    delete[] value->conflicting;
    delete[] value->redundant;
    delete[] value->partiallyRedundant;
    delete[] value->exportKind;
    delete[] value->exportStatus;
    delete[] value->brepUtf8;
    delete value;
}

[[nodiscard]] bool assignStringArray(
    const std::vector<std::string>& values,
    int* count,
    char*** out
)
{
    if (!count || !out) {
        return false;
    }

    *count = 0;
    *out = nullptr;
    if (values.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    if (values.empty()) {
        return true;
    }

    auto* array = new (std::nothrow) char*[values.size()] {};
    if (!array) {
        return false;
    }

    for (std::size_t i = 0; i < values.size(); ++i) {
        if (!assignOutputString(values[i], &array[i])) {
            for (std::size_t j = 0; j < i; ++j) {
                delete[] array[j];
            }
            delete[] array;
            return false;
        }
    }

    *count = static_cast<int>(values.size());
    *out = array;
    return true;
}

[[nodiscard]] bool assignIntArray(const std::vector<int>& values, int* count, const int** out)
{
    if (!count || !out) {
        return false;
    }

    *count = 0;
    *out = nullptr;
    if (values.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    if (values.empty()) {
        return true;
    }

    auto* array = new (std::nothrow) int[values.size()];
    if (!array) {
        return false;
    }
    std::memcpy(array, values.data(), values.size() * sizeof(int));
    *count = static_cast<int>(values.size());
    *out = array;
    return true;
}

template<typename ResultT, typename ImportResultT, typename SolveResultT>
[[nodiscard]] bool fillSolveMetadata(
    ResultT& target,
    const ImportResultT& imported,
    const SolveResultT* solveResult,
    const std::vector<std::string>& messages,
    std::string_view exportKind,
    std::string_view exportStatus
)
{
    if (!assignOwnedString(imported.sketchName, &target.sketchName)) {
        return false;
    }
    if (!assignOwnedString(toString(imported.status), &target.importStatus)) {
        return false;
    }
    target.skippedConstraints = imported.skippedConstraints;
    if (!assignStringArray(messages, &target.messageCount, &target.messages)) {
        return false;
    }

    if (solveResult) {
        if (!assignOwnedString(toString(solveResult->status), &target.solveStatus)) {
            return false;
        }
        target.degreesOfFreedom = solveResult->degreesOfFreedom;
        if (!assignIntArray(solveResult->conflicting, &target.conflictingCount, &target.conflicting)) {
            return false;
        }
        if (!assignIntArray(solveResult->redundant, &target.redundantCount, &target.redundant)) {
            return false;
        }
        if (!assignIntArray(
                solveResult->partiallyRedundant,
                &target.partiallyRedundantCount,
                &target.partiallyRedundant
            )) {
            return false;
        }
    }

    if (!assignOwnedString(exportKind, &target.exportKind)) {
        return false;
    }
    if (!assignOwnedString(exportStatus, &target.exportStatus)) {
        return false;
    }
    return true;
}

[[nodiscard]] bool copyBSplinePoles(
    const std::vector<McSolverEngine::Compat::BSplinePole>& source,
    McSolverEngineGeometryRecord& target
)
{
    if (source.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    target.poleCount = static_cast<int>(source.size());
    if (source.empty()) {
        target.poles = nullptr;
        return true;
    }

    auto* poles = new (std::nothrow) McSolverEngineBSplinePole[source.size()];
    if (!poles) {
        return false;
    }

    for (std::size_t i = 0; i < source.size(); ++i) {
        poles[i] = McSolverEngineBSplinePole {
            .point = toCApiPoint2(source[i].point),
            .weight = source[i].weight,
        };
    }
    target.poles = poles;
    return true;
}

[[nodiscard]] bool copyBSplineKnots(
    const std::vector<McSolverEngine::Compat::BSplineKnot>& source,
    McSolverEngineGeometryRecord& target
)
{
    if (source.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    target.knotCount = static_cast<int>(source.size());
    if (source.empty()) {
        target.knots = nullptr;
        return true;
    }

    auto* knots = new (std::nothrow) McSolverEngineBSplineKnot[source.size()];
    if (!knots) {
        return false;
    }

    for (std::size_t i = 0; i < source.size(); ++i) {
        knots[i] = McSolverEngineBSplineKnot {
            .value = source[i].value,
            .multiplicity = source[i].multiplicity,
        };
    }
    target.knots = knots;
    return true;
}

[[nodiscard]] McSolverEngineConstraintKind toCApiConstraintKind(McSolverEngine::Compat::ConstraintKind kind)
{
    return static_cast<McSolverEngineConstraintKind>(kind);
}

[[nodiscard]] bool copyConstraintRefs(
    const std::vector<McSolverEngine::Geometry::ConstraintRef>& source,
    McSolverEngineGeometryRecord& target
)
{
    target.constraintCount = static_cast<int>(source.size());
    if (source.empty()) {
        target.constraints = nullptr;
        return true;
    }
    auto* refs = new (std::nothrow) McSolverEngineConstraintRef[source.size()];
    if (!refs) {
        return false;
    }
    for (std::size_t i = 0; i < source.size(); ++i) {
        refs[i].kind = toCApiConstraintKind(source[i].kind);
        refs[i].originalIndex = source[i].originalIndex;
        if (source[i].expression.empty()) {
            refs[i].expression = nullptr;
        }
        else {
            char* buf = nullptr;
            if (!assignOutputString(source[i].expression, &buf)) {
                for (std::size_t j = 0; j < i; ++j) {
                    delete[] refs[j].expression;
                }
                delete[] refs;
                return false;
            }
            refs[i].expression = buf;
        }
    }
    target.constraints = refs;
    return true;
}

[[nodiscard]] bool fillGeometryRecord(
    const McSolverEngine::Geometry::GeometryRecord& source,
    McSolverEngineGeometryRecord& target
)
{
    target.geometryIndex = source.geometryIndex;
    target.originalId = source.originalId;
    target.kind = toCApiGeometryKind(source.geometry.kind);
    target.construction = source.geometry.construction ? 1 : 0;
    target.external = source.geometry.external ? 1 : 0;
    target.blocked = source.geometry.blocked ? 1 : 0;

    bool ok = false;
    switch (source.geometry.kind) {
        case GeometryKind::Point: {
            const auto& point = std::get<PointGeometry>(source.geometry.data);
            target.point = toCApiPoint2(point.point);
            ok = true;
            break;
        }
        case GeometryKind::LineSegment: {
            const auto& segment = std::get<LineSegmentGeometry>(source.geometry.data);
            target.start = toCApiPoint2(segment.start);
            target.end = toCApiPoint2(segment.end);
            ok = true;
            break;
        }
        case GeometryKind::Circle: {
            const auto& circle = std::get<CircleGeometry>(source.geometry.data);
            target.center = toCApiPoint2(circle.center);
            target.radius = circle.radius;
            ok = true;
            break;
        }
        case GeometryKind::Arc: {
            const auto& arc = std::get<ArcGeometry>(source.geometry.data);
            target.center = toCApiPoint2(arc.center);
            target.radius = arc.radius;
            target.startAngle = arc.startAngle;
            target.endAngle = arc.endAngle;
            ok = true;
            break;
        }
        case GeometryKind::Ellipse: {
            const auto& ellipse = std::get<EllipseGeometry>(source.geometry.data);
            target.center = toCApiPoint2(ellipse.center);
            target.focus1 = toCApiPoint2(ellipse.focus1);
            target.minorRadius = ellipse.minorRadius;
            ok = true;
            break;
        }
        case GeometryKind::ArcOfEllipse: {
            const auto& arc = std::get<ArcOfEllipseGeometry>(source.geometry.data);
            target.center = toCApiPoint2(arc.center);
            target.focus1 = toCApiPoint2(arc.focus1);
            target.minorRadius = arc.minorRadius;
            target.startAngle = arc.startAngle;
            target.endAngle = arc.endAngle;
            ok = true;
            break;
        }
        case GeometryKind::ArcOfHyperbola: {
            const auto& arc = std::get<ArcOfHyperbolaGeometry>(source.geometry.data);
            target.center = toCApiPoint2(arc.center);
            target.focus1 = toCApiPoint2(arc.focus1);
            target.minorRadius = arc.minorRadius;
            target.startAngle = arc.startAngle;
            target.endAngle = arc.endAngle;
            ok = true;
            break;
        }
        case GeometryKind::ArcOfParabola: {
            const auto& arc = std::get<ArcOfParabolaGeometry>(source.geometry.data);
            target.vertex = toCApiPoint2(arc.vertex);
            target.focus1 = toCApiPoint2(arc.focus1);
            target.startAngle = arc.startAngle;
            target.endAngle = arc.endAngle;
            ok = true;
            break;
        }
        case GeometryKind::BSpline: {
            const auto& spline = std::get<BSplineGeometry>(source.geometry.data);
            target.degree = spline.degree;
            target.periodic = spline.periodic ? 1 : 0;
            ok = copyBSplinePoles(spline.poles, target) && copyBSplineKnots(spline.knots, target);
            break;
        }
    }
    if (!ok) {
        return false;
    }
    return copyConstraintRefs(source.constraints, target);
}

template<typename ImportFn>
McSolverEngineResultCode runStructuredGeometryPipeline(ImportFn&& importFn, McSolverEngineGeometryResult** result)
{
    resetOutput(result);
    auto imported = importFn();

    auto* nativeResult = new (std::nothrow) McSolverEngineGeometryResult {};
    if (!nativeResult) {
        return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
    }

    nativeResult->placement = toCApiPlacement(imported.model.placement());
    *result = nativeResult;

    if (!imported.imported()) {
        if (!fillSolveMetadata<McSolverEngineGeometryResult, decltype(imported), McSolverEngine::Compat::SolveResult>(
                *nativeResult,
                imported,
                nullptr,
                imported.messages,
                "Geometry",
                "Skipped"
            )) {
            freeGeometryResult(nativeResult);
            *result = nullptr;
            return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
        }
        return toCApiImportFailureCode(imported.errorCode);
    }

    const auto solveResult = McSolverEngine::Compat::solveSketch(imported.model);
    nativeResult->placement = toCApiPlacement(imported.model.placement());
    if (!solveResult.solved()) {
        if (!fillSolveMetadata(*nativeResult, imported, &solveResult, imported.messages, "Geometry", "Skipped")) {
            freeGeometryResult(nativeResult);
            *result = nullptr;
            return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
        }
        return solveResult.status == McSolverEngine::Compat::SolveStatus::Unsupported
            ? MCSOLVERENGINE_RESULT_UNSUPPORTED
            : MCSOLVERENGINE_RESULT_SOLVE_FAILED;
    }

    const auto geometryResult = McSolverEngine::Geometry::exportSketchGeometry(imported.model);
    if (!geometryResult.exported()) {
        std::vector<std::string> messages = imported.messages;
        messages.insert(messages.end(), geometryResult.messages.begin(), geometryResult.messages.end());
        if (!fillSolveMetadata(*nativeResult, imported, &solveResult, messages, "Geometry", toString(geometryResult.status))) {
            freeGeometryResult(nativeResult);
            *result = nullptr;
            return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
        }
        return MCSOLVERENGINE_RESULT_GEOMETRY_EXPORT_FAILED;
    }
    if (geometryResult.geometries.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        freeGeometryResult(nativeResult);
        *result = nullptr;
        return MCSOLVERENGINE_RESULT_GEOMETRY_EXPORT_FAILED;
    }

    std::vector<std::string> messages = imported.messages;
    messages.insert(messages.end(), geometryResult.messages.begin(), geometryResult.messages.end());
    if (!fillSolveMetadata(*nativeResult, imported, &solveResult, messages, "Geometry", toString(geometryResult.status))) {
        freeGeometryResult(nativeResult);
        *result = nullptr;
        return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
    }

    nativeResult->placement = toCApiPlacement(geometryResult.placement);
    nativeResult->geometryCount = static_cast<int>(geometryResult.geometries.size());

    if (!geometryResult.geometries.empty()) {
        auto* records = new (std::nothrow) McSolverEngineGeometryRecord[geometryResult.geometries.size()] {};
        if (!records) {
            freeGeometryResult(nativeResult);
            *result = nullptr;
            return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
        }

        nativeResult->geometries = records;
        for (std::size_t i = 0; i < geometryResult.geometries.size(); ++i) {
            if (!fillGeometryRecord(geometryResult.geometries[i], records[i])) {
                freeGeometryResult(nativeResult);
                *result = nullptr;
                return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
            }
        }
    }

    return MCSOLVERENGINE_RESULT_SUCCESS;
}

template<typename ImportFn>
McSolverEngineResultCode runBRepPipeline(ImportFn&& importFn, McSolverEngineBRepResult** result)
{
    resetOutput(result);
    auto imported = importFn();

    auto* nativeResult = new (std::nothrow) McSolverEngineBRepResult {};
    if (!nativeResult) {
        return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
    }

    nativeResult->placement = toCApiPlacement(imported.model.placement());
    *result = nativeResult;

    if (!imported.imported()) {
        if (!fillSolveMetadata<McSolverEngineBRepResult, decltype(imported), McSolverEngine::Compat::SolveResult>(
                *nativeResult, imported, nullptr, imported.messages, "BRep", "Skipped")) {
            freeBRepResult(nativeResult);
            *result = nullptr;
            return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
        }
        return toCApiImportFailureCode(imported.errorCode);
    }

    const auto solveResult = McSolverEngine::Compat::solveSketch(imported.model);
    nativeResult->placement = toCApiPlacement(imported.model.placement());
    if (!solveResult.solved()) {
        if (!fillSolveMetadata(*nativeResult, imported, &solveResult, imported.messages, "BRep", "Skipped")) {
            freeBRepResult(nativeResult);
            *result = nullptr;
            return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
        }
        return solveResult.status == McSolverEngine::Compat::SolveStatus::Unsupported
            ? MCSOLVERENGINE_RESULT_UNSUPPORTED
            : MCSOLVERENGINE_RESULT_SOLVE_FAILED;
    }

    const auto brepResult = McSolverEngine::BRep::exportSketchToBRep(imported.model);

    std::vector<std::string> messages = imported.messages;
    messages.insert(messages.end(), brepResult.messages.begin(), brepResult.messages.end());

    if (brepResult.status == McSolverEngine::BRep::ExportStatus::OpenCascadeUnavailable) {
        if (!fillSolveMetadata(*nativeResult, imported, &solveResult, messages, "BRep", toString(brepResult.status))) {
            freeBRepResult(nativeResult);
            *result = nullptr;
            return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
        }
        return MCSOLVERENGINE_RESULT_OPEN_CASCADE_UNAVAILABLE;
    }
    if (!brepResult.exported()) {
        if (!fillSolveMetadata(*nativeResult, imported, &solveResult, messages, "BRep", toString(brepResult.status))) {
            freeBRepResult(nativeResult);
            *result = nullptr;
            return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
        }
        return MCSOLVERENGINE_RESULT_BREP_EXPORT_FAILED;
    }
    if (!brepResult.brep.has_value()) {
        if (!fillSolveMetadata(*nativeResult, imported, &solveResult, messages, "BRep", toString(brepResult.status))) {
            freeBRepResult(nativeResult);
            *result = nullptr;
            return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
        }
        return MCSOLVERENGINE_RESULT_BREP_EXPORT_FAILED;
    }

    if (!fillSolveMetadata(*nativeResult, imported, &solveResult, messages, "BRep", toString(brepResult.status))) {
        freeBRepResult(nativeResult);
        *result = nullptr;
        return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
    }

    if (!assignOwnedString(*brepResult.brep, &nativeResult->brepUtf8)) {
        freeBRepResult(nativeResult);
        *result = nullptr;
        return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
    }
    return MCSOLVERENGINE_RESULT_SUCCESS;
}

}  // namespace

extern "C"
{

const char* McSolverEngine_GetVersion(void)
{
    McSolverEngine::Detail::configureWindowsAssertMode();
    return McSolverEngine::Engine::version();
}

McSolverEngineResultCode McSolverEngine_SolveToGeometry(
    const char* documentXmlUtf8,
    const char* sketchNameUtf8,
    McSolverEngineGeometryResult** result
)
{
    McSolverEngine::Detail::configureWindowsAssertMode();
    if (!documentXmlUtf8 || !result) {
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_INVALID_ARGUMENT;
    }

    return runStructuredGeometryPipeline(
        [&] {
            return McSolverEngine::DocumentXml::importSketchFromDocumentXml(
                safeStringView(documentXmlUtf8),
                safeStringView(sketchNameUtf8)
            );
        },
        result
    );
}

McSolverEngineResultCode McSolverEngine_SolveToGeometryWithParameters(
    const char* documentXmlUtf8,
    const char* sketchNameUtf8,
    const char* const* parameterKeysUtf8,
    const char* const* parameterValuesUtf8,
    int parameterCount,
    McSolverEngineGeometryResult** result
)
{
    McSolverEngine::Detail::configureWindowsAssertMode();
    if (!documentXmlUtf8 || !result) {
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_INVALID_ARGUMENT;
    }

    McSolverEngine::ParameterMap parameters;
    if (!buildParameterMap(parameterKeysUtf8, parameterValuesUtf8, parameterCount, parameters)) {
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_INVALID_ARGUMENT;
    }

    return runStructuredGeometryPipeline(
        [&] {
            return McSolverEngine::DocumentXml::importSketchFromDocumentXml(
                safeStringView(documentXmlUtf8),
                parameters,
                safeStringView(sketchNameUtf8)
            );
        },
        result
    );
}

McSolverEngineResultCode McSolverEngine_SolveToBRep(
    const char* documentXmlUtf8,
    const char* sketchNameUtf8,
    McSolverEngineBRepResult** result
)
{
    McSolverEngine::Detail::configureWindowsAssertMode();
    if (!documentXmlUtf8 || !result) {
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_INVALID_ARGUMENT;
    }

    return runBRepPipeline(
        [&] {
            return McSolverEngine::DocumentXml::importSketchFromDocumentXml(
                safeStringView(documentXmlUtf8),
                safeStringView(sketchNameUtf8)
            );
        },
        result
    );
}

McSolverEngineResultCode McSolverEngine_SolveToBRepWithParameters(
    const char* documentXmlUtf8,
    const char* sketchNameUtf8,
    const char* const* parameterKeysUtf8,
    const char* const* parameterValuesUtf8,
    int parameterCount,
    McSolverEngineBRepResult** result
)
{
    McSolverEngine::Detail::configureWindowsAssertMode();
    if (!documentXmlUtf8 || !result) {
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_INVALID_ARGUMENT;
    }

    McSolverEngine::ParameterMap parameters;
    if (!buildParameterMap(parameterKeysUtf8, parameterValuesUtf8, parameterCount, parameters)) {
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_INVALID_ARGUMENT;
    }

    return runBRepPipeline(
        [&] {
            return McSolverEngine::DocumentXml::importSketchFromDocumentXml(
                safeStringView(documentXmlUtf8),
                parameters,
                safeStringView(sketchNameUtf8)
            );
        },
        result
    );
}

void McSolverEngine_FreeGeometryResult(McSolverEngineGeometryResult* value)
{
    freeGeometryResult(value);
}

void McSolverEngine_FreeBRepResult(McSolverEngineBRepResult* value)
{
    freeBRepResult(value);
}

}  // extern "C"
