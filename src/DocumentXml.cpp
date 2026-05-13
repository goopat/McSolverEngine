#include "McSolverEngine/DocumentXml.h"

#include "ParameterValueUtils.h"

#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace McSolverEngine::DocumentXml
{

namespace
{

using McSolverEngine::Detail::ParsedApiParameterMap;

constexpr int GeoUndef = -2000;

struct ObjectBlock
{
    std::string name;
    std::string type;
    std::string_view content;
};

struct PropertyBlock
{
    std::string name;
    std::string type;
    std::string_view content;
};

struct RawConstraint
{
    std::string name;
    int type {0};
    double value {0.0};
    int first {GeoUndef};
    int firstPos {0};
    int second {GeoUndef};
    int secondPos {0};
    int third {GeoUndef};
    int thirdPos {0};
    bool isDriving {true};
    bool isVirtualSpace {false};
    bool isActive {true};
    int internalAlignmentType {0};
    int internalAlignmentIndex {-1};
};

struct ImportedConstraintLookup
{
    std::unordered_map<int, int> rawIndexToModelIndex;
    std::unordered_map<std::string, int> nameToModelIndex;
};

struct ConstraintExpressionBinding
{
    std::optional<int> rawIndex;
    std::string constraintName;
    std::string path;
    std::string expression;
};

struct VarSetCatalog
{
    McSolverEngine::ParameterMap values;
    std::unordered_map<std::string, std::string> aliases;
};

struct ParsedParameterBinding
{
    std::string parameterName;
    std::string parameterKey;
};

[[nodiscard]] std::string makeString(std::string_view value)
{
    return std::string(value.begin(), value.end());
}

[[nodiscard]] std::string_view trim(std::string_view value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    return value;
}

[[nodiscard]] std::string unescapeXmlAttribute(std::string_view value)
{
    std::string result;
    result.reserve(value.size());

    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '&') {
            result.push_back(value[i]);
            continue;
        }

        if (value.compare(i, 4, "&lt;") == 0) {
            result.push_back('<');
            i += 3;
            continue;
        }
        if (value.compare(i, 4, "&gt;") == 0) {
            result.push_back('>');
            i += 3;
            continue;
        }
        if (value.compare(i, 5, "&amp;") == 0) {
            result.push_back('&');
            i += 4;
            continue;
        }
        if (value.compare(i, 6, "&quot;") == 0) {
            result.push_back('"');
            i += 5;
            continue;
        }
        if (value.compare(i, 6, "&apos;") == 0) {
            result.push_back('\'');
            i += 5;
            continue;
        }

        result.push_back(value[i]);
    }

    return result;
}

[[nodiscard]] std::optional<std::string_view> extractAttribute(std::string_view element, std::string_view name)
{
    const std::string needle = makeString(name) + "=\"";
    const auto attributeStart = element.find(needle);
    if (attributeStart == std::string_view::npos) {
        return std::nullopt;
    }

    const auto valueStart = attributeStart + needle.size();
    const auto valueEnd = element.find('"', valueStart);
    if (valueEnd == std::string_view::npos) {
        return std::nullopt;
    }

    return element.substr(valueStart, valueEnd - valueStart);
}

[[nodiscard]] std::optional<int> extractIntAttribute(std::string_view element, std::string_view name)
{
    const auto value = extractAttribute(element, name);
    if (!value) {
        return std::nullopt;
    }

    std::istringstream stream(makeString(*value));
    int parsed = 0;
    stream >> parsed;
    if (stream.fail() || !stream.eof()) {
        return std::nullopt;
    }

    return parsed;
}

[[nodiscard]] std::optional<double> extractDoubleAttribute(std::string_view element, std::string_view name)
{
    const auto value = extractAttribute(element, name);
    if (!value) {
        return std::nullopt;
    }

    std::istringstream stream(makeString(*value));
    double parsed = 0.0;
    stream >> parsed;
    if (stream.fail() || !stream.eof() || !std::isfinite(parsed)) {
        return std::nullopt;
    }

    return parsed;
}

[[nodiscard]] bool extractBoolAttribute(std::string_view element, std::string_view name, bool fallback)
{
    const auto value = extractAttribute(element, name);
    if (!value) {
        return fallback;
    }

    return *value != "0" && *value != "false" && *value != "False";
}

[[nodiscard]] std::optional<std::string_view> findPropertyBlock(
    std::string_view objectBlock,
    std::string_view propertyName
)
{
    const std::string needle = "<Property name=\"" + makeString(propertyName) + "\"";
    const auto propertyStart = objectBlock.find(needle);
    if (propertyStart == std::string_view::npos) {
        return std::nullopt;
    }

    const auto propertyEnd = objectBlock.find("</Property>", propertyStart);
    if (propertyEnd == std::string_view::npos) {
        return std::nullopt;
    }

    return objectBlock.substr(propertyStart, propertyEnd + std::string_view("</Property>").size() - propertyStart);
}

[[nodiscard]] std::vector<PropertyBlock> collectPropertyBlocks(std::string_view objectBlock)
{
    std::vector<PropertyBlock> properties;
    std::size_t cursor = 0;
    while (true) {
        const auto propertyStart = objectBlock.find("<Property ", cursor);
        if (propertyStart == std::string_view::npos) {
            break;
        }

        const auto propertyHeaderEnd = objectBlock.find('>', propertyStart);
        const auto propertyEnd = objectBlock.find("</Property>", propertyStart);
        if (propertyHeaderEnd == std::string_view::npos || propertyEnd == std::string_view::npos) {
            break;
        }

        const auto header = objectBlock.substr(propertyStart, propertyHeaderEnd - propertyStart + 1);
        const auto propertyName = extractAttribute(header, "name");
        if (propertyName) {
            properties.push_back({
                .name = makeString(*propertyName),
                .type = makeString(extractAttribute(header, "type").value_or(std::string_view {})),
                .content = objectBlock.substr(
                    propertyStart,
                    propertyEnd + std::string_view("</Property>").size() - propertyStart
                ),
            });
        }

        cursor = propertyEnd + std::string_view("</Property>").size();
    }
    return properties;
}

[[nodiscard]] std::optional<std::string_view> findFirstTag(std::string_view block, std::string_view tagName)
{
    const std::string needle = "<" + makeString(tagName) + " ";
    const auto tagStart = block.find(needle);
    if (tagStart == std::string_view::npos) {
        return std::nullopt;
    }

    const auto tagEnd = block.find("/>", tagStart);
    if (tagEnd == std::string_view::npos) {
        return std::nullopt;
    }

    return block.substr(tagStart, tagEnd + 2 - tagStart);
}

[[nodiscard]] std::unordered_map<std::string, std::string> collectObjectTypes(std::string_view xml)
{
    std::unordered_map<std::string, std::string> types;
    std::size_t cursor = 0;
    while (true) {
        const auto objectStart = xml.find("<Object ", cursor);
        if (objectStart == std::string_view::npos) {
            break;
        }

        const auto objectHeaderEnd = xml.find('>', objectStart);
        if (objectHeaderEnd == std::string_view::npos) {
            break;
        }

        const auto header = xml.substr(objectStart, objectHeaderEnd - objectStart + 1);
        const auto objectName = extractAttribute(header, "name");
        const auto objectType = extractAttribute(header, "type");
        if (objectName && objectType) {
            types.emplace(makeString(*objectName), makeString(*objectType));
        }
        cursor = objectHeaderEnd + 1;
    }
    return types;
}

[[nodiscard]] std::vector<ObjectBlock> collectObjectDataBlocks(
    std::string_view xml,
    const std::unordered_map<std::string, std::string>& objectTypes
)
{
    std::vector<ObjectBlock> blocks;
    const auto objectDataStart = xml.find("<ObjectData");
    if (objectDataStart == std::string_view::npos) {
        return blocks;
    }

    const auto objectDataEnd = xml.find("</ObjectData>", objectDataStart);
    if (objectDataEnd == std::string_view::npos) {
        return blocks;
    }

    auto cursor = objectDataStart;
    while (true) {
        const auto objectStart = xml.find("<Object ", cursor);
        if (objectStart == std::string_view::npos || objectStart >= objectDataEnd) {
            break;
        }

        const auto objectHeaderEnd = xml.find('>', objectStart);
        const auto objectClose = xml.find("</Object>", objectHeaderEnd);
        if (objectHeaderEnd == std::string_view::npos || objectClose == std::string_view::npos) {
            break;
        }

        const auto header = xml.substr(objectStart, objectHeaderEnd - objectStart + 1);
        const auto objectName = extractAttribute(header, "name");
        if (objectName) {
            const auto typeIt = objectTypes.find(makeString(*objectName));
            blocks.push_back({
                .name = makeString(*objectName),
                .type = typeIt == objectTypes.end() ? std::string {} : typeIt->second,
                .content = xml.substr(objectStart, objectClose + std::string_view("</Object>").size() - objectStart),
            });
        }
        cursor = objectClose + std::string_view("</Object>").size();
    }

    return blocks;
}

[[nodiscard]] std::optional<ObjectBlock> findSketchObjectBlock(
    const std::vector<ObjectBlock>& objects,
    std::string_view requestedSketchName
)
{
    for (const auto& object : objects) {
        if (!requestedSketchName.empty()) {
            if (object.name == requestedSketchName) {
                return object;
            }
            continue;
        }

        if ((object.type == "Sketcher::SketchObject"
             || object.content.find("Sketcher::PropertyConstraintList") != std::string_view::npos)
            && object.content.find("Property name=\"Geometry\"") != std::string_view::npos) {
            return object;
        }
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> extractPropertyStringValue(std::string_view propertyBlock)
{
    const auto stringTag = findFirstTag(propertyBlock, "String");
    if (!stringTag) {
        return std::nullopt;
    }

    const auto value = extractAttribute(*stringTag, "value");
    if (!value) {
        return std::nullopt;
    }

    return unescapeXmlAttribute(*value);
}

[[nodiscard]] std::optional<std::string> extractPropertyScalarValue(std::string_view propertyBlock)
{
    for (const auto* tagName : {"Float", "Integer", "Quantity", "String"}) {
        if (const auto tag = findFirstTag(propertyBlock, tagName)) {
            if (const auto value = extractAttribute(*tag, "value")) {
                return unescapeXmlAttribute(*value);
            }
            if (const auto altValue = extractAttribute(*tag, "Value")) {
                return unescapeXmlAttribute(*altValue);
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] VarSetCatalog collectVarSetCatalog(const std::vector<ObjectBlock>& objects)
{
    VarSetCatalog catalog;
    for (const auto& object : objects) {
        if (object.type != "App::VarSet") {
            continue;
        }

        catalog.aliases.emplace(object.name, object.name);
        if (const auto labelProperty = findPropertyBlock(object.content, "Label")) {
            if (const auto label = extractPropertyStringValue(*labelProperty); label && !label->empty()) {
                catalog.aliases.emplace(*label, object.name);
            }
        }

        for (const auto& property : collectPropertyBlocks(object.content)) {
            if (const auto value = extractPropertyScalarValue(property.content)) {
                catalog.values.emplace(object.name + "." + property.name, *value);
            }
        }
    }

    return catalog;
}

[[nodiscard]] std::optional<ConstraintExpressionBinding> parseConstraintExpressionBinding(
    std::string_view expressionElement
)
{
    const auto path = extractAttribute(expressionElement, "path");
    const auto expression = extractAttribute(expressionElement, "expression");
    if (!path || !expression) {
        return std::nullopt;
    }

    ConstraintExpressionBinding binding;
    binding.path = unescapeXmlAttribute(*path);
    binding.expression = unescapeXmlAttribute(*expression);

    const std::string_view normalizedPath = trim(binding.path);
    if (!normalizedPath.starts_with("Constraints")) {
        return std::nullopt;
    }

    if (normalizedPath.size() > 12 && normalizedPath[11] == '[' && normalizedPath.back() == ']') {
        const auto indexView = normalizedPath.substr(12, normalizedPath.size() - 13);
        std::istringstream stream(makeString(indexView));
        int parsedIndex = -1;
        stream >> parsedIndex;
        if (!stream.fail() && stream.eof()) {
            binding.rawIndex = parsedIndex;
            return binding;
        }
    }

    if (normalizedPath.size() > 12 && normalizedPath[11] == '.') {
        binding.constraintName = makeString(normalizedPath.substr(12));
        return binding;
    }

    return std::nullopt;
}

[[nodiscard]] std::vector<ConstraintExpressionBinding> collectConstraintExpressionBindings(
    std::string_view objectBlock
)
{
    std::vector<ConstraintExpressionBinding> bindings;
    const auto expressionProperty = findPropertyBlock(objectBlock, "ExpressionEngine");
    if (!expressionProperty) {
        return bindings;
    }

    std::size_t cursor = 0;
    while (true) {
        const auto expressionStart = expressionProperty->find("<Expression ", cursor);
        if (expressionStart == std::string_view::npos) {
            break;
        }

        const auto expressionEnd = expressionProperty->find("/>", expressionStart);
        if (expressionEnd == std::string_view::npos) {
            break;
        }

        const auto element = expressionProperty->substr(expressionStart, expressionEnd + 2 - expressionStart);
        if (const auto binding = parseConstraintExpressionBinding(element)) {
            bindings.push_back(*binding);
        }
        cursor = expressionEnd + 2;
    }

    return bindings;
}

[[nodiscard]] std::optional<ParsedParameterBinding> parseParameterBindingExpression(
    std::string_view expression,
    const VarSetCatalog& catalog
)
{
    expression = trim(expression);
    if (!expression.empty() && expression.front() == '=') {
        expression.remove_prefix(1);
        expression = trim(expression);
    }

    std::string objectRef;
    std::string parameterName;
    if (expression.starts_with("<<")) {
        const auto close = expression.find(">>.");
        if (close == std::string_view::npos) {
            return std::nullopt;
        }
        objectRef = makeString(expression.substr(2, close - 2));
        parameterName = makeString(trim(expression.substr(close + 3)));
    }
    else {
        const auto separator = expression.find('.');
        if (separator == std::string_view::npos) {
            return std::nullopt;
        }
        objectRef = makeString(trim(expression.substr(0, separator)));
        parameterName = makeString(trim(expression.substr(separator + 1)));
    }

    if (objectRef.empty() || parameterName.empty()) {
        return std::nullopt;
    }

    const auto alias = catalog.aliases.find(objectRef);
    const std::string& objectName = alias == catalog.aliases.end() ? objectRef : alias->second;

    ParsedParameterBinding binding;
    binding.parameterName = parameterName;
    binding.parameterKey = objectName + "." + parameterName;
    return binding;
}

struct ParameterOverrideResolution
{
    std::optional<double> value;
};

[[nodiscard]] ParameterOverrideResolution resolveParameterOverrideValue(
    const Compat::Constraint& constraint,
    const ParsedApiParameterMap& parameters
)
{
    const auto tryResolve = [&](const std::string& key) -> ParameterOverrideResolution {
        if (key.empty()) {
            return {};
        }
        const auto it = parameters.find(key);
        if (it == parameters.end()) {
            return {};
        }
        return {.value = McSolverEngine::Detail::convertApiParameterToInternal(
            it->second,
            constraint.kind
        )};
    };

    if (const auto exact = tryResolve(constraint.parameterKey); exact.value) {
        return exact;
    }
    if (const auto shortName = tryResolve(constraint.parameterName); shortName.value) {
        return shortName;
    }
    return {};
}

void applyConstraintExpressionBindings(
    Compat::SketchModel& model,
    const ImportedConstraintLookup& lookup,
    const std::vector<ConstraintExpressionBinding>& bindings,
    const VarSetCatalog& catalog,
    const ParsedApiParameterMap& parameters,
    std::vector<std::string>& messages,
    bool& hadBindingGaps
)
{
    auto& constraints = model.constraints();
    for (const auto& binding : bindings) {
        int modelIndex = -1;
        if (binding.rawIndex) {
            const auto it = lookup.rawIndexToModelIndex.find(*binding.rawIndex);
            if (it == lookup.rawIndexToModelIndex.end()) {
                continue;
            }
            modelIndex = it->second;
        }
        else if (!binding.constraintName.empty()) {
            const auto it = lookup.nameToModelIndex.find(binding.constraintName);
            if (it == lookup.nameToModelIndex.end()) {
                continue;
            }
            modelIndex = it->second;
        }
        else {
            continue;
        }

        if (modelIndex < 0 || modelIndex >= static_cast<int>(constraints.size())) {
            continue;
        }

        auto& constraint = constraints[static_cast<std::size_t>(modelIndex)];
        if (!constraint.hasValue) {
            messages.push_back(
                "Ignored ExpressionEngine binding for non-dimensional constraint path: " + binding.path
            );
            hadBindingGaps = true;
            continue;
        }

        constraint.parameterExpression = binding.expression;
        const auto parameterBinding = parseParameterBindingExpression(binding.expression, catalog);
        if (!parameterBinding) {
            if (const auto constantValue =
                    McSolverEngine::Detail::parseDocumentParameterValue(binding.expression, constraint.kind)) {
                constraint.value = *constantValue;
                continue;
            }

            messages.push_back(
                "Ignored unsupported constraint expression binding '" + binding.expression
                + "' at path " + binding.path + "."
            );
            hadBindingGaps = true;
            continue;
        }

        constraint.parameterName = parameterBinding->parameterName;
        constraint.parameterKey = parameterBinding->parameterKey;

        const auto defaultIt = catalog.values.find(constraint.parameterKey);
        if (defaultIt != catalog.values.end()) {
            if (const auto defaultValue =
                    McSolverEngine::Detail::parseDocumentParameterValue(defaultIt->second, constraint.kind)) {
                constraint.parameterDefaultValue = *defaultValue;
                constraint.hasParameterDefaultValue = true;
                constraint.value = *defaultValue;
            }
            else {
                messages.push_back(
                    "VarSet parameter '" + constraint.parameterKey
                    + "' could not be converted to the expected "
                    + std::string(McSolverEngine::Detail::expectedParameterUnit(constraint.kind))
                    + " parameter value."
                );
                hadBindingGaps = true;
            }
        }

        const auto overrideValue = resolveParameterOverrideValue(constraint, parameters);
        if (overrideValue.value) {
            constraint.value = *overrideValue.value;
            constraint.parameterDefaultValue = *overrideValue.value;
            constraint.hasParameterDefaultValue = true;
            continue;
        }

        if (!constraint.hasParameterDefaultValue) {
            messages.push_back(
                "Constraint expression '" + binding.expression
                + "' did not resolve to a numeric VarSet value."
            );
            hadBindingGaps = true;
        }
    }
}

[[nodiscard]] Compat::PointRole mapPointRole(int rawPos)
{
    switch (rawPos) {
        case 1:
            return Compat::PointRole::Start;
        case 2:
            return Compat::PointRole::End;
        case 3:
            return Compat::PointRole::Mid;
        case 0:
        default:
            return Compat::PointRole::None;
    }
}

[[nodiscard]] Compat::ElementRef invalidRef()
{
    return Compat::ElementRef {.geometryIndex = -1, .role = Compat::PointRole::None};
}

[[nodiscard]] bool resolveRef(
    int rawGeoId,
    int rawPos,
    const std::vector<int>& internalGeometryMap,
    const std::unordered_map<int, int>& externalGeometryMap,
    Compat::ElementRef& outRef
)
{
    if (rawGeoId == GeoUndef) {
        outRef = invalidRef();
        return true;
    }

    outRef.role = mapPointRole(rawPos);

    if (rawGeoId >= 0) {
        if (rawGeoId >= static_cast<int>(internalGeometryMap.size())) {
            return false;
        }

        outRef.geometryIndex = internalGeometryMap[rawGeoId];
        return true;
    }

    const auto external = externalGeometryMap.find(rawGeoId);
    if (external == externalGeometryMap.end()) {
        return false;
    }

    outRef.geometryIndex = external->second;
    return true;
}

[[nodiscard]] bool parseConstruction(std::string_view geometryBlock)
{
    const auto construction = findFirstTag(geometryBlock, "Construction");
    return construction ? extractBoolAttribute(*construction, "value", false) : false;
}

[[nodiscard]] Compat::Point2 directionFromAngle(double angle)
{
    return Compat::Point2 {.x = std::cos(angle), .y = std::sin(angle)};
}

[[nodiscard]] bool importGeometryBlock(
    std::string_view geometryBlock,
    bool external,
    Compat::SketchModel& model,
    std::string& error
)
{
    const auto geometryHeaderEnd = geometryBlock.find('>');
    if (geometryHeaderEnd == std::string_view::npos) {
        error = "Malformed <Geometry> block.";
        return false;
    }

    const auto header = geometryBlock.substr(0, geometryHeaderEnd + 1);
    const auto geometryType = extractAttribute(header, "type");
    if (!geometryType) {
        error = "Geometry block missing type attribute.";
        return false;
    }

    const bool construction = parseConstruction(geometryBlock);

    if (*geometryType == "Part::GeomLineSegment") {
        const auto lineTag = findFirstTag(geometryBlock, "LineSegment");
        if (!lineTag) {
            error = "Line geometry missing <LineSegment/> payload.";
            return false;
        }

        const auto startX = extractDoubleAttribute(*lineTag, "StartX");
        const auto startY = extractDoubleAttribute(*lineTag, "StartY");
        const auto endX = extractDoubleAttribute(*lineTag, "EndX");
        const auto endY = extractDoubleAttribute(*lineTag, "EndY");
        if (!startX || !startY || !endX || !endY) {
            error = "Line geometry has incomplete coordinate data.";
            return false;
        }

        model.addLineSegment(
            {.x = *startX, .y = *startY},
            {.x = *endX, .y = *endY},
            construction,
            external
        );
        return true;
    }

    if (*geometryType == "Part::GeomPoint") {
        auto pointTag = findFirstTag(geometryBlock, "Point");
        if (!pointTag) {
            pointTag = findFirstTag(geometryBlock, "GeomPoint");
        }
        if (!pointTag) {
            error = "Point geometry missing <Point/> or <GeomPoint/> payload.";
            return false;
        }

        const auto x = extractDoubleAttribute(*pointTag, "X");
        const auto y = extractDoubleAttribute(*pointTag, "Y");
        if (!x || !y) {
            error = "Point geometry has incomplete coordinate data.";
            return false;
        }

        model.addPoint({.x = *x, .y = *y}, construction, external);
        return true;
    }

    if (*geometryType == "Part::GeomCircle") {
        const auto circleTag = findFirstTag(geometryBlock, "Circle");
        if (!circleTag) {
            error = "Circle geometry missing <Circle/> payload.";
            return false;
        }

        const auto centerX = extractDoubleAttribute(*circleTag, "CenterX");
        const auto centerY = extractDoubleAttribute(*circleTag, "CenterY");
        const auto radius = extractDoubleAttribute(*circleTag, "Radius");
        if (!centerX || !centerY || !radius) {
            error = "Circle geometry has incomplete coordinate data.";
            return false;
        }

        model.addCircle({.x = *centerX, .y = *centerY}, *radius, construction, external);
        return true;
    }

    if (*geometryType == "Part::GeomArcOfCircle") {
        const auto arcTag = findFirstTag(geometryBlock, "ArcOfCircle");
        if (!arcTag) {
            error = "Arc geometry missing <ArcOfCircle/> payload.";
            return false;
        }

        const auto centerX = extractDoubleAttribute(*arcTag, "CenterX");
        const auto centerY = extractDoubleAttribute(*arcTag, "CenterY");
        const auto radius = extractDoubleAttribute(*arcTag, "Radius");
        const auto startAngle = extractDoubleAttribute(*arcTag, "StartAngle");
        const auto endAngle = extractDoubleAttribute(*arcTag, "EndAngle");
        if (!centerX || !centerY || !radius || !startAngle || !endAngle) {
            error = "Arc geometry has incomplete coordinate data.";
            return false;
        }

        model.addArc(
            {.x = *centerX, .y = *centerY},
            *radius,
            *startAngle,
            *endAngle,
            construction,
            external
        );
        return true;
    }

    if (*geometryType == "Part::GeomEllipse") {
        const auto ellipseTag = findFirstTag(geometryBlock, "Ellipse");
        if (!ellipseTag) {
            error = "Ellipse geometry missing <Ellipse/> payload.";
            return false;
        }

        const auto centerX = extractDoubleAttribute(*ellipseTag, "CenterX");
        const auto centerY = extractDoubleAttribute(*ellipseTag, "CenterY");
        const auto majorRadius = extractDoubleAttribute(*ellipseTag, "MajorRadius");
        const auto minorRadius = extractDoubleAttribute(*ellipseTag, "MinorRadius");
        const auto angleXU = extractDoubleAttribute(*ellipseTag, "AngleXU");
        if (!centerX || !centerY || !majorRadius || !minorRadius || !angleXU) {
            error = "Ellipse geometry has incomplete coordinate data.";
            return false;
        }

        const double focalDistance = std::sqrt(
            (*majorRadius) * (*majorRadius) - (*minorRadius) * (*minorRadius)
        );
        const auto axis = directionFromAngle(*angleXU);
        model.addEllipse(
            {.x = *centerX, .y = *centerY},
            {.x = *centerX + axis.x * focalDistance, .y = *centerY + axis.y * focalDistance},
            *minorRadius,
            construction,
            external
        );
        return true;
    }

    if (*geometryType == "Part::GeomArcOfEllipse") {
        const auto arcTag = findFirstTag(geometryBlock, "ArcOfEllipse");
        if (!arcTag) {
            error = "Arc-of-ellipse geometry missing <ArcOfEllipse/> payload.";
            return false;
        }

        const auto centerX = extractDoubleAttribute(*arcTag, "CenterX");
        const auto centerY = extractDoubleAttribute(*arcTag, "CenterY");
        const auto majorRadius = extractDoubleAttribute(*arcTag, "MajorRadius");
        const auto minorRadius = extractDoubleAttribute(*arcTag, "MinorRadius");
        const auto angleXU = extractDoubleAttribute(*arcTag, "AngleXU");
        const auto startAngle = extractDoubleAttribute(*arcTag, "StartAngle");
        const auto endAngle = extractDoubleAttribute(*arcTag, "EndAngle");
        if (!centerX || !centerY || !majorRadius || !minorRadius || !angleXU || !startAngle || !endAngle) {
            error = "Arc-of-ellipse geometry has incomplete coordinate data.";
            return false;
        }

        const double focalDistance = std::sqrt(
            (*majorRadius) * (*majorRadius) - (*minorRadius) * (*minorRadius)
        );
        const auto axis = directionFromAngle(*angleXU);
        model.addArcOfEllipse(
            {.x = *centerX, .y = *centerY},
            {.x = *centerX + axis.x * focalDistance, .y = *centerY + axis.y * focalDistance},
            *minorRadius,
            *startAngle,
            *endAngle,
            construction,
            external
        );
        return true;
    }

    if (*geometryType == "Part::GeomArcOfHyperbola") {
        const auto arcTag = findFirstTag(geometryBlock, "ArcOfHyperbola");
        if (!arcTag) {
            error = "Arc-of-hyperbola geometry missing <ArcOfHyperbola/> payload.";
            return false;
        }

        const auto centerX = extractDoubleAttribute(*arcTag, "CenterX");
        const auto centerY = extractDoubleAttribute(*arcTag, "CenterY");
        const auto majorRadius = extractDoubleAttribute(*arcTag, "MajorRadius");
        const auto minorRadius = extractDoubleAttribute(*arcTag, "MinorRadius");
        const auto angleXU = extractDoubleAttribute(*arcTag, "AngleXU");
        const auto startAngle = extractDoubleAttribute(*arcTag, "StartAngle");
        const auto endAngle = extractDoubleAttribute(*arcTag, "EndAngle");
        if (!centerX || !centerY || !majorRadius || !minorRadius || !angleXU || !startAngle || !endAngle) {
            error = "Arc-of-hyperbola geometry has incomplete coordinate data.";
            return false;
        }

        const double focalDistance = std::sqrt(
            (*majorRadius) * (*majorRadius) + (*minorRadius) * (*minorRadius)
        );
        const auto axis = directionFromAngle(*angleXU);
        model.addArcOfHyperbola(
            {.x = *centerX, .y = *centerY},
            {.x = *centerX + axis.x * focalDistance, .y = *centerY + axis.y * focalDistance},
            *minorRadius,
            *startAngle,
            *endAngle,
            construction,
            external
        );
        return true;
    }

    if (*geometryType == "Part::GeomArcOfParabola") {
        const auto arcTag = findFirstTag(geometryBlock, "ArcOfParabola");
        if (!arcTag) {
            error = "Arc-of-parabola geometry missing <ArcOfParabola/> payload.";
            return false;
        }

        const auto centerX = extractDoubleAttribute(*arcTag, "CenterX");
        const auto centerY = extractDoubleAttribute(*arcTag, "CenterY");
        const auto focal = extractDoubleAttribute(*arcTag, "Focal");
        const auto angleXU = extractDoubleAttribute(*arcTag, "AngleXU");
        const auto startAngle = extractDoubleAttribute(*arcTag, "StartAngle");
        const auto endAngle = extractDoubleAttribute(*arcTag, "EndAngle");
        if (!centerX || !centerY || !focal || !angleXU || !startAngle || !endAngle) {
            error = "Arc-of-parabola geometry has incomplete coordinate data.";
            return false;
        }

        const auto axis = directionFromAngle(*angleXU);
        model.addArcOfParabola(
            {.x = *centerX, .y = *centerY},
            {.x = *centerX + axis.x * (*focal), .y = *centerY + axis.y * (*focal)},
            *startAngle,
            *endAngle,
            construction,
            external
        );
        return true;
    }

    if (*geometryType == "Part::GeomBSplineCurve") {
        const auto splineTag = geometryBlock.find("<BSplineCurve ");
        if (splineTag == std::string_view::npos) {
            error = "BSpline geometry missing <BSplineCurve> payload.";
            return false;
        }

        const auto splineTagEnd = geometryBlock.find('>', splineTag);
        const auto splineClose = geometryBlock.find("</BSplineCurve>", splineTagEnd);
        if (splineTagEnd == std::string_view::npos || splineClose == std::string_view::npos) {
            error = "Malformed <BSplineCurve> payload.";
            return false;
        }

        const auto splineHeader = geometryBlock.substr(splineTag, splineTagEnd - splineTag + 1);
        const auto degree = extractIntAttribute(splineHeader, "Degree");
        const auto isPeriodic = extractBoolAttribute(splineHeader, "IsPeriodic", false);
        if (!degree) {
            error = "BSpline geometry missing Degree attribute.";
            return false;
        }

        std::vector<Compat::BSplinePole> poles;
        auto cursor = splineTagEnd + 1;
        while (true) {
            const auto poleStart = geometryBlock.find("<Pole ", cursor);
            if (poleStart == std::string_view::npos || poleStart >= splineClose) {
                break;
            }
            const auto poleEnd = geometryBlock.find("/>", poleStart);
            if (poleEnd == std::string_view::npos || poleEnd > splineClose) {
                error = "Malformed <Pole/> entry in BSpline geometry.";
                return false;
            }
            const auto poleTag = geometryBlock.substr(poleStart, poleEnd + 2 - poleStart);
            const auto x = extractDoubleAttribute(poleTag, "X");
            const auto y = extractDoubleAttribute(poleTag, "Y");
            const auto weight = extractDoubleAttribute(poleTag, "Weight");
            if (!x || !y || !weight) {
                error = "BSpline pole has incomplete coordinate data.";
                return false;
            }
            poles.push_back({.point = {.x = *x, .y = *y}, .weight = *weight});
            cursor = poleEnd + 2;
        }

        std::vector<Compat::BSplineKnot> knots;
        cursor = splineTagEnd + 1;
        while (true) {
            const auto knotStart = geometryBlock.find("<Knot ", cursor);
            if (knotStart == std::string_view::npos || knotStart >= splineClose) {
                break;
            }
            const auto knotEnd = geometryBlock.find("/>", knotStart);
            if (knotEnd == std::string_view::npos || knotEnd > splineClose) {
                error = "Malformed <Knot/> entry in BSpline geometry.";
                return false;
            }
            const auto knotTag = geometryBlock.substr(knotStart, knotEnd + 2 - knotStart);
            const auto value = extractDoubleAttribute(knotTag, "Value");
            const auto multiplicity = extractIntAttribute(knotTag, "Mult");
            if (!value || !multiplicity) {
                error = "BSpline knot has incomplete data.";
                return false;
            }
            knots.push_back({.value = *value, .multiplicity = *multiplicity});
            cursor = knotEnd + 2;
        }

        if (poles.empty() || knots.empty()) {
            error = "BSpline geometry must contain poles and knots.";
            return false;
        }

        model.addBSpline(
            std::move(poles),
            std::move(knots),
            *degree,
            isPeriodic,
            construction,
            external
        );
        return true;
    }

    error = "Unsupported geometry type: " + makeString(*geometryType);
    return false;
}

[[nodiscard]] bool importGeometryProperty(
    std::string_view propertyBlock,
    bool external,
    Compat::SketchModel& model,
    std::vector<int>& internalGeometryMap,
    std::unordered_map<int, int>& externalGeometryMap,
    std::string& error
)
{
    auto cursor = std::size_t {0};
    while (true) {
        const auto geometryStart = propertyBlock.find("<Geometry ", cursor);
        if (geometryStart == std::string_view::npos) {
            return true;
        }

        const auto geometryHeaderEnd = propertyBlock.find('>', geometryStart);
        const auto geometryEnd = propertyBlock.find("</Geometry>", geometryHeaderEnd);
        if (geometryHeaderEnd == std::string_view::npos || geometryEnd == std::string_view::npos) {
            error = "Malformed geometry list.";
            return false;
        }

        const auto geometryBlock =
            propertyBlock.substr(geometryStart, geometryEnd + std::string_view("</Geometry>").size() - geometryStart);
        const auto beforeCount = model.geometryCount();
        if (!importGeometryBlock(geometryBlock, external, model, error)) {
            return false;
        }

        const auto importedIndex = static_cast<int>(beforeCount);
        if (external) {
            const auto geometryHeader = geometryBlock.substr(0, geometryHeaderEnd - geometryStart + 1);
            const auto rawId = extractIntAttribute(geometryHeader, "id");
            if (!rawId) {
                error = "External geometry missing id attribute.";
                return false;
            }

            externalGeometryMap[*rawId] = importedIndex;
        }
        else {
            internalGeometryMap.push_back(importedIndex);
        }

        cursor = geometryEnd + std::string_view("</Geometry>").size();
    }
}

[[nodiscard]] bool importPlacementProperty(
    std::string_view objectBlock,
    Compat::SketchModel& model,
    std::string& error
)
{
    const auto placementProperty = findPropertyBlock(objectBlock, "Placement");
    if (!placementProperty) {
        return true;
    }

    const auto placementTag = findFirstTag(*placementProperty, "PropertyPlacement");
    if (!placementTag) {
        error = "Placement property missing <PropertyPlacement/> payload.";
        return false;
    }

    const auto px = extractDoubleAttribute(*placementTag, "Px");
    const auto py = extractDoubleAttribute(*placementTag, "Py");
    const auto pz = extractDoubleAttribute(*placementTag, "Pz");
    const auto q0 = extractDoubleAttribute(*placementTag, "Q0");
    const auto q1 = extractDoubleAttribute(*placementTag, "Q1");
    const auto q2 = extractDoubleAttribute(*placementTag, "Q2");
    const auto q3 = extractDoubleAttribute(*placementTag, "Q3");
    if (!px || !py || !pz || !q0 || !q1 || !q2 || !q3) {
        error = "Placement property has incomplete transform data.";
        return false;
    }

    model.setPlacement(Compat::Placement {
        .px = *px,
        .py = *py,
        .pz = *pz,
        .qx = *q0,
        .qy = *q1,
        .qz = *q2,
        .qw = *q3,
    });
    return true;
}

[[nodiscard]] std::optional<RawConstraint> parseRawConstraint(std::string_view element)
{
    const auto name = extractAttribute(element, "Name");
    const auto type = extractIntAttribute(element, "Type");
    const auto value = extractDoubleAttribute(element, "Value");
    const auto first = extractIntAttribute(element, "First");
    const auto firstPos = extractIntAttribute(element, "FirstPos");
    const auto second = extractIntAttribute(element, "Second");
    const auto secondPos = extractIntAttribute(element, "SecondPos");
    const auto third = extractIntAttribute(element, "Third");
    const auto thirdPos = extractIntAttribute(element, "ThirdPos");

    if (!type || !value || !first || !firstPos || !second || !secondPos || !third || !thirdPos) {
        return std::nullopt;
    }

    return RawConstraint {
        .name = name ? unescapeXmlAttribute(*name) : std::string {},
        .type = *type,
        .value = *value,
        .first = *first,
        .firstPos = *firstPos,
        .second = *second,
        .secondPos = *secondPos,
        .third = *third,
        .thirdPos = *thirdPos,
        .isDriving = extractBoolAttribute(element, "IsDriving", true),
        .isVirtualSpace = extractBoolAttribute(element, "IsInVirtualSpace", false),
        .isActive = extractBoolAttribute(element, "IsActive", true),
        .internalAlignmentType = extractIntAttribute(element, "InternalAlignmentType").value_or(0),
        .internalAlignmentIndex = extractIntAttribute(element, "InternalAlignmentIndex").value_or(-1),
    };
}

[[nodiscard]] bool addConstraintFromRaw(
    const RawConstraint& raw,
    int rawIndex,
    const std::vector<int>& internalGeometryMap,
    const std::unordered_map<int, int>& externalGeometryMap,
    ImportedConstraintLookup& lookup,
    ImportResult& result
)
{
    Compat::ElementRef first;
    Compat::ElementRef second;
    Compat::ElementRef third;

    if (!resolveRef(raw.first, raw.firstPos, internalGeometryMap, externalGeometryMap, first)
        || !resolveRef(raw.second, raw.secondPos, internalGeometryMap, externalGeometryMap, second)
        || !resolveRef(raw.third, raw.thirdPos, internalGeometryMap, externalGeometryMap, third)) {
        result.status = ImportStatus::Failed;
        result.messages.push_back("Constraint references a geometry id that is not present in the imported sketch.");
        return false;
    }

    Compat::Constraint constraint;
    constraint.first = first;
    constraint.second = second;
    constraint.third = third;
    constraint.value = raw.value;
    constraint.driving = raw.isDriving;

    switch (raw.type) {
        case 1:
            constraint.kind = Compat::ConstraintKind::Coincident;
            break;
        case 2:
            constraint.kind = Compat::ConstraintKind::Horizontal;
            break;
        case 3:
            constraint.kind = Compat::ConstraintKind::Vertical;
            break;
        case 4:
            constraint.kind = Compat::ConstraintKind::Parallel;
            break;
        case 5:
            constraint.kind = Compat::ConstraintKind::Tangent;
            constraint.hasValue = true;
            break;
        case 6:
            constraint.kind = Compat::ConstraintKind::Distance;
            constraint.hasValue = true;
            break;
        case 7:
            constraint.kind = Compat::ConstraintKind::DistanceX;
            constraint.hasValue = true;
            break;
        case 8:
            constraint.kind = Compat::ConstraintKind::DistanceY;
            constraint.hasValue = true;
            break;
        case 9:
            constraint.kind = Compat::ConstraintKind::Angle;
            constraint.hasValue = true;
            break;
        case 10:
            constraint.kind = Compat::ConstraintKind::Perpendicular;
            constraint.hasValue = raw.firstPos != 0 || raw.secondPos != 0 || raw.third != GeoUndef;
            break;
        case 11:
            constraint.kind = Compat::ConstraintKind::Radius;
            constraint.hasValue = true;
            break;
        case 12:
            constraint.kind = Compat::ConstraintKind::Equal;
            break;
        case 14:
            constraint.kind = Compat::ConstraintKind::Symmetric;
            break;
        case 15:
            constraint.kind = Compat::ConstraintKind::InternalAlignment;
            switch (raw.internalAlignmentType) {
                case 1:
                    constraint.alignmentType = Compat::InternalAlignmentType::EllipseMajorDiameter;
                    break;
                case 2:
                    constraint.alignmentType = Compat::InternalAlignmentType::EllipseMinorDiameter;
                    break;
                case 3:
                    constraint.alignmentType = Compat::InternalAlignmentType::EllipseFocus1;
                    break;
                case 4:
                    constraint.alignmentType = Compat::InternalAlignmentType::EllipseFocus2;
                    break;
                case 5:
                    constraint.alignmentType = Compat::InternalAlignmentType::HyperbolaMajor;
                    break;
                case 6:
                    constraint.alignmentType = Compat::InternalAlignmentType::HyperbolaMinor;
                    break;
                case 7:
                    constraint.alignmentType = Compat::InternalAlignmentType::HyperbolaFocus;
                    break;
                case 8:
                    constraint.alignmentType = Compat::InternalAlignmentType::ParabolaFocus;
                    break;
                case 9:
                    constraint.alignmentType = Compat::InternalAlignmentType::BSplineControlPoint;
                    break;
                case 10:
                    constraint.alignmentType = Compat::InternalAlignmentType::BSplineKnotPoint;
                    break;
                case 11:
                    constraint.alignmentType = Compat::InternalAlignmentType::ParabolaFocalAxis;
                    break;
                default:
                    ++result.skippedConstraints;
                    result.messages.push_back(
                        "Skipped InternalAlignment constraint with unsupported subtype: "
                        + std::to_string(raw.internalAlignmentType)
                    );
                    return true;
            }
            constraint.internalAlignmentIndex = raw.internalAlignmentIndex;
            break;
        case 16:
            constraint.kind = Compat::ConstraintKind::SnellsLaw;
            constraint.hasValue = true;
            break;
        case 17:
            constraint.kind = Compat::ConstraintKind::Block;
            if (first.geometryIndex >= 0
                && static_cast<std::size_t>(first.geometryIndex) < result.model.geometries().size()) {
                result.model.geometries()[static_cast<std::size_t>(first.geometryIndex)].blocked = true;
            }
            break;
        case 18:
            constraint.kind = Compat::ConstraintKind::Diameter;
            constraint.hasValue = true;
            break;
        case 19:
            constraint.kind = Compat::ConstraintKind::Weight;
            constraint.hasValue = true;
            break;
        case 13:
            if (raw.firstPos == 0 || raw.secondPos != 0 || raw.third != GeoUndef) {
                ++result.skippedConstraints;
                result.messages.push_back("Skipped unsupported PointOnObject constraint form.");
                return true;
            }
            constraint.kind = Compat::ConstraintKind::PointOnObject;
            break;
        default:
            ++result.skippedConstraints;
            result.messages.push_back("Skipped unsupported constraint type: " + std::to_string(raw.type));
            return true;
    }

    const auto modelIndex = static_cast<int>(result.model.constraintCount());
    result.model.addConstraint(std::move(constraint));
    lookup.rawIndexToModelIndex.emplace(rawIndex, modelIndex);
    if (!raw.name.empty()) {
        lookup.nameToModelIndex.emplace(raw.name, modelIndex);
    }
    return true;
}

}  // namespace

ImportResult importSketchFromDocumentXml(std::string_view xml, std::string_view sketchName)
{
    return importSketchFromDocumentXml(xml, McSolverEngine::ParameterMap {}, sketchName);
}

ImportResult importSketchFromDocumentXml(
    std::string_view xml,
    const McSolverEngine::ParameterMap& parameters,
    std::string_view sketchName
)
{
    ImportResult result;
    ParsedApiParameterMap parsedParameters;
    std::string invalidParameterKey;
    if (!McSolverEngine::Detail::tryParseApiParameters(
            parameters,
            parsedParameters,
            &invalidParameterKey)) {
        result.status = ImportStatus::Failed;
        result.messages.push_back(
            "Parameter '" + invalidParameterKey
            + "' must be a numeric value. API parameter units are fixed to mm for length constraints and degree for angle constraints."
        );
        return result;
    }

    const auto objectTypes = collectObjectTypes(xml);
    const auto objectBlocks = collectObjectDataBlocks(xml, objectTypes);
    const auto object = findSketchObjectBlock(objectBlocks, sketchName);
    if (!object) {
        result.messages.push_back(
            sketchName.empty() ? "No sketch-like object was found in Document.xml."
                               : "Requested sketch object was not found in Document.xml."
        );
        return result;
    }

    result.sketchName = object->name;

    const auto geometryProperty = findPropertyBlock(object->content, "Geometry");
    if (!geometryProperty) {
        result.messages.push_back("Sketch object does not contain a Geometry property.");
        return result;
    }

    std::vector<int> internalGeometryMap;
    std::unordered_map<int, int> externalGeometryMap;
    ImportedConstraintLookup importedConstraints;
    const auto varSetCatalog = collectVarSetCatalog(objectBlocks);
    std::string error;

    if (!importPlacementProperty(object->content, result.model, error)) {
        result.messages.push_back(error);
        return result;
    }

    if (!importGeometryProperty(
            *geometryProperty,
            false,
            result.model,
            internalGeometryMap,
            externalGeometryMap,
            error)) {
        result.messages.push_back(error);
        return result;
    }

    if (const auto externalProperty = findPropertyBlock(object->content, "ExternalGeo")) {
        if (!importGeometryProperty(
                *externalProperty,
                true,
                result.model,
                internalGeometryMap,
                externalGeometryMap,
                error)) {
            result.messages.push_back(error);
            return result;
        }
    }

    bool hadBindingGaps = false;
    const auto constraintsProperty = findPropertyBlock(object->content, "Constraints");
    if (constraintsProperty) {
        auto cursor = std::size_t {0};
        int rawConstraintIndex = 0;
        while (true) {
            const auto constraintStart = constraintsProperty->find("<Constrain ", cursor);
            if (constraintStart == std::string_view::npos) {
                break;
            }

            const auto constraintEnd = constraintsProperty->find("/>", constraintStart);
            if (constraintEnd == std::string_view::npos) {
                result.messages.push_back("Malformed <Constrain/> entry.");
                result.status = ImportStatus::Failed;
                return result;
            }

            const auto constraintElement =
                constraintsProperty->substr(constraintStart, constraintEnd + 2 - constraintStart);
            const auto rawConstraint = parseRawConstraint(constraintElement);
            if (!rawConstraint) {
                result.messages.push_back("Constraint entry is missing required attributes.");
                result.status = ImportStatus::Failed;
                return result;
            }

            if (rawConstraint->isActive && !rawConstraint->isVirtualSpace) {
                if (!addConstraintFromRaw(
                        *rawConstraint,
                        rawConstraintIndex,
                        internalGeometryMap,
                        externalGeometryMap,
                        importedConstraints,
                        result)) {
                    return result;
                }
            }

            ++rawConstraintIndex;
            cursor = constraintEnd + 2;
        }

        applyConstraintExpressionBindings(
            result.model,
            importedConstraints,
            collectConstraintExpressionBindings(object->content),
            varSetCatalog,
            parsedParameters,
            result.messages,
            hadBindingGaps
        );
    }

    result.status = (result.skippedConstraints == 0 && !hadBindingGaps) ? ImportStatus::Success
                                                                         : ImportStatus::Partial;
    return result;
}

ImportResult importSketchFromDocumentXmlFile(std::string_view path, std::string_view sketchName)
{
    return importSketchFromDocumentXmlFile(path, McSolverEngine::ParameterMap {}, sketchName);
}

ImportResult importSketchFromDocumentXmlFile(
    std::string_view path,
    const McSolverEngine::ParameterMap& parameters,
    std::string_view sketchName
)
{
    std::ifstream input(makeString(path), std::ios::binary);
    ImportResult result;

    if (!input) {
        result.messages.push_back("Failed to open Document.xml file.");
        return result;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return importSketchFromDocumentXml(buffer.str(), parameters, sketchName);
}

}  // namespace McSolverEngine::DocumentXml
