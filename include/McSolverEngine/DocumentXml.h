#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "McSolverEngine/CompatModel.h"
#include "McSolverEngine/Export.h"

namespace McSolverEngine::DocumentXml
{

enum class ImportStatus
{
    Success = 0,
    Partial = 1,
    Failed = 2,
};

enum class ImportErrorCode
{
    None = 0,
    VarSetExpressionUnsupportedSubset = 1001,
    VarSetParameterValidationFailed = 1002,
    SketchNotFound = 1003,
};

enum class InspectStatus
{
    Success = 0,
    Failed = 1,
};

struct ScalarPropertyInfo
{
    std::string name;
    std::string type;
    std::string scalarValue;
    std::string propertyXml;
};

struct InspectGeometryElement
{
    int index {-1};
    int originalId {-1};
    std::string type;
    bool construction {false};
    bool external {false};
    std::vector<int> constraintIndices;
};

struct InspectConstraintInfo
{
    int originalIndex {-1};
    int type {0};
    std::string kind;
    bool driving {true};
    double value {0.0};
    std::vector<int> referencedGeoIds;
};

struct SketchInfo
{
    std::string label;
    std::string objectName;
    std::vector<ScalarPropertyInfo> properties;
    std::vector<InspectGeometryElement> geometries;
    std::vector<InspectConstraintInfo> constraints;
};

struct VarSetParameterInfo
{
    std::string name;
    std::string type;
    std::string rawValue;
    std::string expression;
    std::string propertyXml;
};

struct VarSetInfo
{
    std::string label;
    std::string objectName;
    std::vector<VarSetParameterInfo> parameters;
};

struct InspectResult
{
    InspectStatus status {InspectStatus::Failed};
    std::vector<std::string> messages;
    std::vector<SketchInfo> sketches;
    std::vector<VarSetInfo> varSets;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return status == InspectStatus::Success;
    }
};

struct VarSetPropertyValue
{
    std::string key;
    std::string value;
    std::string unit;
};

struct ImportResult
{
    ImportStatus status {ImportStatus::Failed};
    ImportErrorCode errorCode {ImportErrorCode::None};
    Compat::SketchModel model;
    std::string sketchName;
    std::vector<std::string> messages;
    std::vector<VarSetPropertyValue> varSetProperties;
    std::size_t skippedConstraints {0};

    [[nodiscard]] bool imported() const noexcept
    {
        return status != ImportStatus::Failed;
    }
};

[[nodiscard]] MCSOLVERENGINE_EXPORT ImportResult importSketchFromDocumentXml(
    std::string_view xml,
    std::string_view sketchName = {}
);
[[nodiscard]] MCSOLVERENGINE_EXPORT ImportResult importSketchFromDocumentXml(
    std::string_view xml,
    const McSolverEngine::ParameterMap& parameters,
    std::string_view sketchName = {}
);
[[nodiscard]] MCSOLVERENGINE_EXPORT ImportResult importSketchFromDocumentXmlFile(
    std::string_view path,
    std::string_view sketchName = {}
);
[[nodiscard]] MCSOLVERENGINE_EXPORT ImportResult importSketchFromDocumentXmlFile(
    std::string_view path,
    const McSolverEngine::ParameterMap& parameters,
    std::string_view sketchName = {}
);

[[nodiscard]] MCSOLVERENGINE_EXPORT InspectResult inspectDocumentXml(std::string_view xml);

}  // namespace McSolverEngine::DocumentXml
