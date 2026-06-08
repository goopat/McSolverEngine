#include "McSolverEngine/CApi.h"

#include <cstring>
#include <exception>
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
#include "McSolverEngine/ZipExtract.h"
#include "WindowsAssertMode.h"

namespace
{

constexpr std::size_t kLastErrorBufferSize = 512;
thread_local char lastErrorBuffer[kLastErrorBufferSize] = {};

void setLastError(const char* message) noexcept
{
    std::strncpy(lastErrorBuffer, message, kLastErrorBufferSize - 1);
    lastErrorBuffer[kLastErrorBufferSize - 1] = '\0';
}

void clearLastError() noexcept
{
    lastErrorBuffer[0] = '\0';
}

void captureMessages(const std::vector<std::string>& messages) noexcept
{
    std::size_t offset = 0;
    for (std::size_t i = 0; i < messages.size() && offset < kLastErrorBufferSize - 1; ++i) {
        if (i > 0 && offset < kLastErrorBufferSize - 2) {
            lastErrorBuffer[offset++] = ';';
            lastErrorBuffer[offset++] = ' ';
        }
        const auto len = std::min(messages[i].size(), kLastErrorBufferSize - 1 - offset);
        std::memcpy(lastErrorBuffer + offset, messages[i].data(), len);
        offset += len;
    }
    lastErrorBuffer[offset] = '\0';
}

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
        case McSolverEngine::DocumentXml::ImportErrorCode::VarSetParameterValidationFailed:
            return MCSOLVERENGINE_RESULT_VARSET_PARAMETER_VALIDATION_FAILED;
        case McSolverEngine::DocumentXml::ImportErrorCode::SketchNotFound:
            return MCSOLVERENGINE_RESULT_SKETCH_NOT_FOUND;
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

void resetOutput(McSolverEngineDocumentInfo** out)
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

void freeVarSetProperties(int count, const McSolverEngineVarSetProperty* values) noexcept
{
    auto* properties = const_cast<McSolverEngineVarSetProperty*>(values);
    if (!properties) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        delete[] properties[i].keyUtf8;
        delete[] properties[i].valueUtf8;
        delete[] properties[i].unitUtf8;
    }
    delete[] properties;
}

void freeScalarPropertyInfos(int count, const McSolverEngineScalarPropertyInfo* values) noexcept
{
    auto* properties = const_cast<McSolverEngineScalarPropertyInfo*>(values);
    if (!properties) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        delete[] properties[i].nameUtf8;
        delete[] properties[i].typeUtf8;
        delete[] properties[i].scalarValueUtf8;
        delete[] properties[i].propertyXmlUtf8;
    }
    delete[] properties;
}

void freeVarSetParameterInfos(int count, const McSolverEngineVarSetParameterInfo* values) noexcept
{
    auto* parameters = const_cast<McSolverEngineVarSetParameterInfo*>(values);
    if (!parameters) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        delete[] parameters[i].nameUtf8;
        delete[] parameters[i].typeUtf8;
        delete[] parameters[i].rawValueUtf8;
        delete[] parameters[i].expressionUtf8;
        delete[] parameters[i].propertyXmlUtf8;
    }
    delete[] parameters;
}

void freeInspectConstraints(int count, const McSolverEngineInspectConstraint* values) noexcept
{
    auto* constraints = const_cast<McSolverEngineInspectConstraint*>(values);
    if (!constraints) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        delete[] constraints[i].kindUtf8;
        delete[] constraints[i].referencedGeoIds;
    }
    delete[] constraints;
}

void freeInspectGeometryElements(int count, const McSolverEngineInspectGeometryElement* values) noexcept
{
    auto* elements = const_cast<McSolverEngineInspectGeometryElement*>(values);
    if (!elements) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        delete[] elements[i].typeUtf8;
        delete[] elements[i].constraintIndices;
    }
    delete[] elements;
}

void freeSketchInfos(int count, const McSolverEngineSketchInfo* values) noexcept
{
    auto* sketches = const_cast<McSolverEngineSketchInfo*>(values);
    if (!sketches) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        delete[] sketches[i].labelUtf8;
        delete[] sketches[i].objectNameUtf8;
        freeScalarPropertyInfos(sketches[i].propertyCount, sketches[i].properties);
        freeInspectGeometryElements(sketches[i].geometryCount, sketches[i].geometries);
        freeInspectConstraints(sketches[i].constraintCount, sketches[i].constraints);
    }
    delete[] sketches;
}

void freeVarSetInfos(int count, const McSolverEngineVarSetInfo* values) noexcept
{
    auto* varSets = const_cast<McSolverEngineVarSetInfo*>(values);
    if (!varSets) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        delete[] varSets[i].labelUtf8;
        delete[] varSets[i].objectNameUtf8;
        freeVarSetParameterInfos(varSets[i].parameterCount, varSets[i].parameters);
    }
    delete[] varSets;
}

void freeDocumentInfo(McSolverEngineDocumentInfo* value) noexcept
{
    if (!value) {
        return;
    }

    auto* messages = value->messages;
    if (messages) {
        for (int i = 0; i < value->messageCount; ++i) {
            delete[] messages[i];
        }
        delete[] messages;
    }
    freeSketchInfos(value->sketchCount, value->sketches);
    freeVarSetInfos(value->varSetCount, value->varSets);
    delete value;
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
    freeVarSetProperties(value->varSetPropertyCount, value->varSetProperties);

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
    freeVarSetProperties(value->varSetPropertyCount, value->varSetProperties);
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

[[nodiscard]] bool assignVarSetProperties(
    const std::vector<McSolverEngine::DocumentXml::VarSetPropertyValue>& values,
    int* count,
    const McSolverEngineVarSetProperty** out
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

    auto* array = new (std::nothrow) McSolverEngineVarSetProperty[values.size()] {};
    if (!array) {
        return false;
    }

    for (std::size_t i = 0; i < values.size(); ++i) {
        if (!assignOwnedString(values[i].key, &array[i].keyUtf8)
            || !assignOwnedString(values[i].value, &array[i].valueUtf8)
            || !assignOwnedString(values[i].unit, &array[i].unitUtf8)) {
            freeVarSetProperties(static_cast<int>(values.size()), array);
            return false;
        }
    }

    *count = static_cast<int>(values.size());
    *out = array;
    return true;
}

[[nodiscard]] bool assignScalarPropertyInfos(
    const std::vector<McSolverEngine::DocumentXml::ScalarPropertyInfo>& values,
    int* count,
    const McSolverEngineScalarPropertyInfo** out
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

    auto* array = new (std::nothrow) McSolverEngineScalarPropertyInfo[values.size()] {};
    if (!array) {
        return false;
    }

    for (std::size_t i = 0; i < values.size(); ++i) {
        if (!assignOwnedString(values[i].name, &array[i].nameUtf8)
            || !assignOwnedString(values[i].type, &array[i].typeUtf8)
            || !assignOwnedString(values[i].scalarValue, &array[i].scalarValueUtf8)
            || !assignOwnedString(values[i].propertyXml, &array[i].propertyXmlUtf8)) {
            freeScalarPropertyInfos(static_cast<int>(values.size()), array);
            return false;
        }
    }

    *count = static_cast<int>(values.size());
    *out = array;
    return true;
}

[[nodiscard]] bool assignVarSetParameterInfos(
    const std::vector<McSolverEngine::DocumentXml::VarSetParameterInfo>& values,
    int* count,
    const McSolverEngineVarSetParameterInfo** out
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

    auto* array = new (std::nothrow) McSolverEngineVarSetParameterInfo[values.size()] {};
    if (!array) {
        return false;
    }

    for (std::size_t i = 0; i < values.size(); ++i) {
        if (!assignOwnedString(values[i].name, &array[i].nameUtf8)
            || !assignOwnedString(values[i].type, &array[i].typeUtf8)
            || !assignOwnedString(values[i].rawValue, &array[i].rawValueUtf8)
            || !assignOwnedString(values[i].expression, &array[i].expressionUtf8)
            || !assignOwnedString(values[i].propertyXml, &array[i].propertyXmlUtf8)) {
            freeVarSetParameterInfos(static_cast<int>(values.size()), array);
            return false;
        }
    }

    *count = static_cast<int>(values.size());
    *out = array;
    return true;
}

[[nodiscard]] bool assignInspectConstraints(
    const std::vector<McSolverEngine::DocumentXml::InspectConstraintInfo>& values,
    int* count,
    const McSolverEngineInspectConstraint** out
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

    auto* array = new (std::nothrow) McSolverEngineInspectConstraint[values.size()] {};
    if (!array) {
        return false;
    }

    for (std::size_t i = 0; i < values.size(); ++i) {
        array[i].originalIndex = values[i].originalIndex;
        array[i].type = values[i].type;
        array[i].driving = values[i].driving ? 1 : 0;
        array[i].value = values[i].value;
        if (!assignOwnedString(values[i].kind, &array[i].kindUtf8)
            || !assignIntArray(values[i].referencedGeoIds, &array[i].referencedGeoIdCount, &array[i].referencedGeoIds)) {
            freeInspectConstraints(static_cast<int>(values.size()), array);
            return false;
        }
    }

    *count = static_cast<int>(values.size());
    *out = array;
    return true;
}

[[nodiscard]] bool assignInspectGeometryElements(
    const std::vector<McSolverEngine::DocumentXml::InspectGeometryElement>& values,
    int* count,
    const McSolverEngineInspectGeometryElement** out
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

    auto* array = new (std::nothrow) McSolverEngineInspectGeometryElement[values.size()] {};
    if (!array) {
        return false;
    }

    for (std::size_t i = 0; i < values.size(); ++i) {
        array[i].index = values[i].index;
        array[i].originalId = values[i].originalId;
        array[i].construction = values[i].construction ? 1 : 0;
        array[i].external = values[i].external ? 1 : 0;
        if (!assignOwnedString(values[i].type, &array[i].typeUtf8)
            || !assignIntArray(values[i].constraintIndices, &array[i].constraintCount, &array[i].constraintIndices)) {
            freeInspectGeometryElements(static_cast<int>(values.size()), array);
            return false;
        }
    }

    *count = static_cast<int>(values.size());
    *out = array;
    return true;
}

[[nodiscard]] bool assignSketchInfos(
    const std::vector<McSolverEngine::DocumentXml::SketchInfo>& values,
    int* count,
    const McSolverEngineSketchInfo** out
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

    auto* array = new (std::nothrow) McSolverEngineSketchInfo[values.size()] {};
    if (!array) {
        return false;
    }

    for (std::size_t i = 0; i < values.size(); ++i) {
        if (!assignOwnedString(values[i].label, &array[i].labelUtf8)
            || !assignOwnedString(values[i].objectName, &array[i].objectNameUtf8)
            || !assignScalarPropertyInfos(values[i].properties, &array[i].propertyCount, &array[i].properties)
            || !assignInspectGeometryElements(values[i].geometries, &array[i].geometryCount, &array[i].geometries)
            || !assignInspectConstraints(values[i].constraints, &array[i].constraintCount, &array[i].constraints)) {
            freeSketchInfos(static_cast<int>(values.size()), array);
            return false;
        }
    }

    *count = static_cast<int>(values.size());
    *out = array;
    return true;
}

[[nodiscard]] bool assignVarSetInfos(
    const std::vector<McSolverEngine::DocumentXml::VarSetInfo>& values,
    int* count,
    const McSolverEngineVarSetInfo** out
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

    auto* array = new (std::nothrow) McSolverEngineVarSetInfo[values.size()] {};
    if (!array) {
        return false;
    }

    for (std::size_t i = 0; i < values.size(); ++i) {
        if (!assignOwnedString(values[i].label, &array[i].labelUtf8)
            || !assignOwnedString(values[i].objectName, &array[i].objectNameUtf8)
            || !assignVarSetParameterInfos(values[i].parameters, &array[i].parameterCount, &array[i].parameters)) {
            freeVarSetInfos(static_cast<int>(values.size()), array);
            return false;
        }
    }

    *count = static_cast<int>(values.size());
    *out = array;
    return true;
}

[[nodiscard]] bool assignDocumentInfo(
    const McSolverEngine::DocumentXml::InspectResult& inspected,
    McSolverEngineDocumentInfo** out
)
{
    if (!out) {
        return false;
    }

    *out = new (std::nothrow) McSolverEngineDocumentInfo {};
    if (!*out) {
        return false;
    }

    if (!assignSketchInfos(inspected.sketches, &(*out)->sketchCount, &(*out)->sketches)
        || !assignVarSetInfos(inspected.varSets, &(*out)->varSetCount, &(*out)->varSets)
        || !assignStringArray(inspected.messages, &(*out)->messageCount, &(*out)->messages)) {
        freeDocumentInfo(*out);
        *out = nullptr;
        return false;
    }

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
    if (!assignVarSetProperties(
            imported.varSetProperties,
            &target.varSetPropertyCount,
            &target.varSetProperties
        )) {
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

template<typename ImportFn, typename SolveFn>
McSolverEngineResultCode runStructuredGeometryPipeline(
    ImportFn&& importFn,
    SolveFn&& solveFn,
    McSolverEngineGeometryResult** result
)
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
        captureMessages(imported.messages);
        return toCApiImportFailureCode(imported.errorCode);
    }

    const auto solveResult = solveFn(imported.model);
    nativeResult->placement = toCApiPlacement(imported.model.placement());
    if (!solveResult.solved()) {
        if (!fillSolveMetadata(*nativeResult, imported, &solveResult, imported.messages, "Geometry", "Skipped")) {
            freeGeometryResult(nativeResult);
            *result = nullptr;
            return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
        }
        captureMessages(imported.messages);
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
        captureMessages(messages);
        return MCSOLVERENGINE_RESULT_GEOMETRY_EXPORT_FAILED;
    }
    if (geometryResult.geometries.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        freeGeometryResult(nativeResult);
        *result = nullptr;
        setLastError("geometry count exceeds int max");
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

template<typename ImportFn, typename SolveFn>
McSolverEngineResultCode runBRepPipeline(
    ImportFn&& importFn,
    SolveFn&& solveFn,
    McSolverEngineBRepResult** result
)
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
        captureMessages(imported.messages);
        return toCApiImportFailureCode(imported.errorCode);
    }

    const auto solveResult = solveFn(imported.model);
    nativeResult->placement = toCApiPlacement(imported.model.placement());
    if (!solveResult.solved()) {
        if (!fillSolveMetadata(*nativeResult, imported, &solveResult, imported.messages, "BRep", "Skipped")) {
            freeBRepResult(nativeResult);
            *result = nullptr;
            return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
        }
        captureMessages(imported.messages);
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
        captureMessages(messages);
        return MCSOLVERENGINE_RESULT_OPEN_CASCADE_UNAVAILABLE;
    }
    if (!brepResult.exported()) {
        if (!fillSolveMetadata(*nativeResult, imported, &solveResult, messages, "BRep", toString(brepResult.status))) {
            freeBRepResult(nativeResult);
            *result = nullptr;
            return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
        }
        captureMessages(messages);
        return MCSOLVERENGINE_RESULT_BREP_EXPORT_FAILED;
    }
    if (!brepResult.brep.has_value()) {
        if (!fillSolveMetadata(*nativeResult, imported, &solveResult, messages, "BRep", toString(brepResult.status))) {
            freeBRepResult(nativeResult);
            *result = nullptr;
            return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
        }
        captureMessages(messages);
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
    clearLastError();
    try {
        McSolverEngine::Detail::configureWindowsAssertMode();
        return McSolverEngine::Engine::version();
    } catch (const std::exception& e) {
        setLastError(e.what());
        return "";
    } catch (...) {
        setLastError("unknown exception");
        return "";
    }
}

McSolverEngineResultCode McSolverEngine_SolveToGeometry(
    const char* documentXmlUtf8,
    const char* sketchNameUtf8,
    McSolverEngineGeometryResult** result
)
{
    clearLastError();
    try {
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
            [](McSolverEngine::Compat::SketchModel& model) {
                return McSolverEngine::Compat::solveSketch(model);
            },
            result
        );
    } catch (const std::bad_alloc& e) {
        setLastError(e.what());
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
    } catch (const std::exception& e) {
        setLastError(e.what());
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_SOLVE_FAILED;
    } catch (...) {
        setLastError("unknown exception");
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_SOLVE_FAILED;
    }
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
    clearLastError();
    try {
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
            [&](McSolverEngine::Compat::SketchModel& model) {
                return McSolverEngine::Compat::solveSketch(model, parameters);
            },
            result
        );
    } catch (const std::bad_alloc& e) {
        setLastError(e.what());
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
    } catch (const std::exception& e) {
        setLastError(e.what());
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_SOLVE_FAILED;
    } catch (...) {
        setLastError("unknown exception");
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_SOLVE_FAILED;
    }
}

McSolverEngineResultCode McSolverEngine_SolveToBRep(
    const char* documentXmlUtf8,
    const char* sketchNameUtf8,
    McSolverEngineBRepResult** result
)
{
    clearLastError();
    try {
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
            [](McSolverEngine::Compat::SketchModel& model) {
                return McSolverEngine::Compat::solveSketch(model);
            },
            result
        );
    } catch (const std::bad_alloc& e) {
        setLastError(e.what());
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
    } catch (const std::exception& e) {
        setLastError(e.what());
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_SOLVE_FAILED;
    } catch (...) {
        setLastError("unknown exception");
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_SOLVE_FAILED;
    }
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
    clearLastError();
    try {
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
            [&](McSolverEngine::Compat::SketchModel& model) {
                return McSolverEngine::Compat::solveSketch(model, parameters);
            },
            result
        );
    } catch (const std::bad_alloc& e) {
        setLastError(e.what());
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
    } catch (const std::exception& e) {
        setLastError(e.what());
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_SOLVE_FAILED;
    } catch (...) {
        setLastError("unknown exception");
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_SOLVE_FAILED;
    }
}

void McSolverEngine_FreeGeometryResult(McSolverEngineGeometryResult* value)
{
    freeGeometryResult(value);
}

void McSolverEngine_FreeBRepResult(McSolverEngineBRepResult* value)
{
    freeBRepResult(value);
}

McSolverEngineResultCode McSolverEngine_InspectDocumentXml(
    const char* documentXmlUtf8,
    McSolverEngineDocumentInfo** result
)
{
    clearLastError();
    try {
        McSolverEngine::Detail::configureWindowsAssertMode();
        if (!documentXmlUtf8 || !result) {
            resetOutput(result);
            return MCSOLVERENGINE_RESULT_INVALID_ARGUMENT;
        }

        const auto inspected = McSolverEngine::DocumentXml::inspectDocumentXml(
            safeStringView(documentXmlUtf8)
        );
        if (!assignDocumentInfo(inspected, result)) {
            resetOutput(result);
            return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
        }

        if (!inspected.succeeded()) {
            if (inspected.messages.empty()) {
                setLastError("Document.xml inspection failed.");
            } else {
                captureMessages(inspected.messages);
            }
            return MCSOLVERENGINE_RESULT_IMPORT_FAILED;
        }

        return MCSOLVERENGINE_RESULT_SUCCESS;
    } catch (const std::bad_alloc& e) {
        setLastError(e.what());
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_OUT_OF_MEMORY;
    } catch (const std::exception& e) {
        setLastError(e.what());
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_IMPORT_FAILED;
    } catch (...) {
        setLastError("unknown exception");
        resetOutput(result);
        return MCSOLVERENGINE_RESULT_IMPORT_FAILED;
    }
}

void McSolverEngine_FreeDocumentInfo(McSolverEngineDocumentInfo* value)
{
    freeDocumentInfo(value);
}

McSolverEngineFCStdResultCode McSolverEngine_ExtractFCStdDoc(
    const char* fcstdPathUtf8,
    char** documentXmlOut
)
{
    clearLastError();
    if (!fcstdPathUtf8 || !documentXmlOut) {
        if (documentXmlOut) {
            *documentXmlOut = nullptr;
        }
        return MCSOLVERENGINE_FCSTD_OPEN_FAILED;
    }
    *documentXmlOut = nullptr;

    try {
        auto result = McSolverEngine::ZipExtract::extractDocumentXml(fcstdPathUtf8);
        if (!result.success) {
            setLastError(result.errorMessage.c_str());
            if (result.errorMessage.find("Failed to open") != std::string::npos) {
                return MCSOLVERENGINE_FCSTD_OPEN_FAILED;
            }
            if (result.errorMessage.find("Not a valid ZIP") != std::string::npos) {
                return MCSOLVERENGINE_FCSTD_NOT_ZIP;
            }
            if (result.errorMessage.find("Document.xml not found") != std::string::npos) {
                return MCSOLVERENGINE_FCSTD_XML_NOT_FOUND;
            }
            if (result.errorMessage.find("DEFLATE") != std::string::npos) {
                return MCSOLVERENGINE_FCSTD_DECOMPRESS_FAILED;
            }
            return MCSOLVERENGINE_FCSTD_OPEN_FAILED;
        }

        *documentXmlOut = result.documentXml.release();

        return MCSOLVERENGINE_FCSTD_SUCCESS;
    } catch (const std::bad_alloc& e) {
        setLastError(e.what());
        return MCSOLVERENGINE_FCSTD_OUT_OF_MEMORY;
    } catch (const std::exception& e) {
        setLastError(e.what());
        return MCSOLVERENGINE_FCSTD_DECOMPRESS_FAILED;
    } catch (...) {
        setLastError("unknown exception");
        return MCSOLVERENGINE_FCSTD_DECOMPRESS_FAILED;
    }
}

void McSolverEngine_FreeFCStdDoc(char* documentXml)
{
    delete[] documentXml;
}

const char* McSolverEngine_GetLastError(void)
{
    return lastErrorBuffer;
}

}  // extern "C"
