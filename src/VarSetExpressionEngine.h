#pragma once

#include "McSolverEngine/DocumentXml.h"
#include "ParameterValueUtils.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace McSolverEngine::DocumentXml::VarSetExpressions
{

struct VarSetProperty
{
    std::string objectName;
    std::string propertyName;
    std::string rawValue;
    bool hasRawValue {false};
    std::optional<std::string> expression;
    std::optional<double> evaluatedValue;
};

struct VarSetCatalog
{
    std::map<std::string, VarSetProperty> properties;
    std::unordered_map<std::string, std::string> aliases;
    std::unordered_map<std::string, std::vector<std::string>> keysByPropertyName;
};

struct ParsedParameterBinding
{
    std::string parameterName;
    std::string parameterKey;
};

struct ParameterBindingParseResult
{
    std::optional<ParsedParameterBinding> binding;
    bool externalReference {false};
    std::string error;
};

inline constexpr std::string_view VarSetExpressionUnsupportedSubsetCode =
    "MCSOLVERENGINE_IMPORT_VARSET_EXPRESSION_UNSUPPORTED_SUBSET";

[[nodiscard]] std::string makeVarSetExpressionUnsupportedSubsetMessage(std::string_view detail);

void rebuildVarSetShortNameLookup(VarSetCatalog& catalog);

[[nodiscard]] VarSetProperty& ensureVarSetProperty(
    VarSetCatalog& catalog,
    std::string_view objectName,
    std::string_view propertyName
);

[[nodiscard]] std::optional<std::string> parseVarSetExpressionPath(std::string_view path);

[[nodiscard]] bool applyApiParametersToVarSets(
    VarSetCatalog& catalog,
    const McSolverEngine::Detail::ParsedApiParameterMap& parameters,
    std::vector<std::string>& messages
);

[[nodiscard]] bool evaluateVarSetExpressions(VarSetCatalog& catalog, ImportResult& result);

[[nodiscard]] std::optional<std::string> getVarSetValueForBinding(
    const VarSetCatalog& catalog,
    const std::string& key
);

[[nodiscard]] ParameterBindingParseResult parseParameterBindingExpression(
    std::string_view expression,
    const VarSetCatalog& catalog
);

}  // namespace McSolverEngine::DocumentXml::VarSetExpressions
