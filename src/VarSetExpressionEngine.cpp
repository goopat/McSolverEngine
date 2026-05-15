#include "VarSetExpressionEngine.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <iomanip>
#include <numbers>
#include <numeric>
#include <sstream>
#include <utility>

namespace McSolverEngine::DocumentXml::VarSetExpressions
{

namespace
{

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

[[nodiscard]] std::string formatDouble(double value)
{
    std::ostringstream stream;
    stream << std::setprecision(17) << value;
    return stream.str();
}

[[nodiscard]] bool isIdentifierStart(char ch) noexcept
{
    const auto uch = static_cast<unsigned char>(ch);
    return std::isalpha(uch) != 0 || ch == '_';
}

[[nodiscard]] bool isIdentifierChar(char ch) noexcept
{
    const auto uch = static_cast<unsigned char>(ch);
    return std::isalnum(uch) != 0 || ch == '_';
}

[[nodiscard]] bool isSimplePropertyPath(std::string_view value) noexcept
{
    value = trim(value);
    if (value.empty() || !isIdentifierStart(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), isIdentifierChar);
}

class VarSetExpressionParser
{
public:
    using Resolver = std::function<std::optional<double>(const std::string&, std::string&)>;

    VarSetExpressionParser(
        std::string_view expression,
        const VarSetCatalog& catalog,
        std::string_view currentObjectName,
        Resolver resolver
    )
        : expression_(expression)
        , catalog_(catalog)
        , currentObjectName_(currentObjectName)
        , resolver_(std::move(resolver))
    {}

    [[nodiscard]] std::optional<double> parse(std::string& error)
    {
        skipWhitespace();
        if (position_ < expression_.size() && expression_[position_] == '=') {
            ++position_;
            skipWhitespace();
        }

        const auto value = parseAdditive();
        if (!value) {
            error = error_;
            return std::nullopt;
        }
        skipWhitespace();
        if (position_ != expression_.size()) {
            error = "Unexpected token in VarSet expression near '" + makeString(expression_.substr(position_)) + "'.";
            return std::nullopt;
        }
        if (!std::isfinite(*value)) {
            error = "VarSet expression evaluated to a non-finite value.";
            return std::nullopt;
        }

        return *value;
    }

private:
    [[nodiscard]] std::optional<double> fail(std::string message)
    {
        if (error_.empty()) {
            error_ = std::move(message);
        }
        return std::nullopt;
    }

    void skipWhitespace()
    {
        while (position_ < expression_.size()
               && std::isspace(static_cast<unsigned char>(expression_[position_])) != 0) {
            ++position_;
        }
    }

    [[nodiscard]] bool consume(char ch)
    {
        skipWhitespace();
        if (position_ >= expression_.size() || expression_[position_] != ch) {
            return false;
        }
        ++position_;
        return true;
    }

    [[nodiscard]] std::optional<double> parseAdditive()
    {
        auto value = parseMultiplicative();
        if (!value) {
            return std::nullopt;
        }

        while (true) {
            if (consume('+')) {
                const auto rhs = parseMultiplicative();
                if (!rhs) {
                    return std::nullopt;
                }
                *value += *rhs;
                continue;
            }
            if (consume('-')) {
                const auto rhs = parseMultiplicative();
                if (!rhs) {
                    return std::nullopt;
                }
                *value -= *rhs;
                continue;
            }
            break;
        }

        return checked(*value);
    }

    [[nodiscard]] std::optional<double> parseMultiplicative()
    {
        auto value = parsePower();
        if (!value) {
            return std::nullopt;
        }

        while (true) {
            if (consume('*')) {
                const auto rhs = parsePower();
                if (!rhs) {
                    return std::nullopt;
                }
                *value *= *rhs;
                continue;
            }
            if (consume('/')) {
                const auto rhs = parsePower();
                if (!rhs) {
                    return std::nullopt;
                }
                if (*rhs == 0.0) {
                    return fail("Division by zero in VarSet expression.");
                }
                *value /= *rhs;
                continue;
            }
            if (consume('%')) {
                const auto rhs = parsePower();
                if (!rhs) {
                    return std::nullopt;
                }
                if (*rhs == 0.0) {
                    return fail("Modulo by zero in VarSet expression.");
                }
                *value = std::fmod(*value, *rhs);
                continue;
            }
            break;
        }

        return checked(*value);
    }

    [[nodiscard]] std::optional<double> parsePower()
    {
        auto value = parseUnary();
        if (!value) {
            return std::nullopt;
        }

        while (consume('^')) {
            const auto exponent = parseUnary();
            if (!exponent) {
                return std::nullopt;
            }
            value = std::pow(*value, *exponent);
        }

        return checked(*value);
    }

    [[nodiscard]] std::optional<double> parseUnary()
    {
        if (consume('+')) {
            return parseUnary();
        }
        if (consume('-')) {
            const auto value = parseUnary();
            if (!value) {
                return std::nullopt;
            }
            return checked(-*value);
        }
        return parsePrimary();
    }

    [[nodiscard]] std::optional<double> parsePrimary()
    {
        skipWhitespace();
        if (position_ >= expression_.size()) {
            return fail("Unexpected end of VarSet expression.");
        }

        if (consume('(')) {
            const auto value = parseAdditive();
            if (!value) {
                return std::nullopt;
            }
            if (!consume(')')) {
                return fail("Missing ')' in VarSet expression.");
            }
            return value;
        }

        if (expression_.compare(position_, 2, "<<") == 0) {
            return parseLabelReference();
        }

        const char ch = expression_[position_];
        if (std::isdigit(static_cast<unsigned char>(ch)) != 0 || ch == '.') {
            return parseNumber();
        }
        if (isIdentifierStart(ch)) {
            return parseIdentifierOrFunction();
        }

        return fail("Unexpected token in VarSet expression near '" + makeString(expression_.substr(position_)) + "'.");
    }

    [[nodiscard]] std::optional<double> parseNumber()
    {
        const auto start = position_;
        bool sawDigit = false;
        while (position_ < expression_.size()
               && std::isdigit(static_cast<unsigned char>(expression_[position_])) != 0) {
            sawDigit = true;
            ++position_;
        }
        if (position_ < expression_.size() && expression_[position_] == '.') {
            ++position_;
            while (position_ < expression_.size()
                   && std::isdigit(static_cast<unsigned char>(expression_[position_])) != 0) {
                sawDigit = true;
                ++position_;
            }
        }
        if (!sawDigit) {
            return fail("Expected numeric literal in VarSet expression.");
        }

        if (position_ < expression_.size() && (expression_[position_] == 'e' || expression_[position_] == 'E')) {
            const auto exponentStart = position_;
            ++position_;
            if (position_ < expression_.size() && (expression_[position_] == '+' || expression_[position_] == '-')) {
                ++position_;
            }
            bool sawExponentDigit = false;
            while (position_ < expression_.size()
                   && std::isdigit(static_cast<unsigned char>(expression_[position_])) != 0) {
                sawExponentDigit = true;
                ++position_;
            }
            if (!sawExponentDigit) {
                position_ = exponentStart;
            }
        }

        const auto token = expression_.substr(start, position_ - start);
        const auto parsed = McSolverEngine::Detail::parseStrictNumeric(token);
        if (!parsed) {
            return fail("Invalid numeric literal '" + makeString(token) + "' in VarSet expression.");
        }
        return *parsed;
    }

    [[nodiscard]] std::optional<double> parseLabelReference()
    {
        const auto close = expression_.find(">>", position_ + 2);
        if (close == std::string_view::npos) {
            return fail("Malformed <<...>> VarSet reference in expression.");
        }

        const auto objectRef = trim(expression_.substr(position_ + 2, close - position_ - 2));
        position_ = close + 2;
        skipWhitespace();
        if (!consume('.')) {
            return fail("VarSet label reference must be followed by a property name.");
        }
        const auto propertyName = parseIdentifierToken();
        if (!propertyName) {
            return fail("VarSet label reference is missing a property name.");
        }
        if (objectRef.empty()) {
            return fail("VarSet label reference is missing an object name.");
        }

        return resolveQualifiedReference(makeString(objectRef), *propertyName);
    }

    [[nodiscard]] std::optional<std::string> parseIdentifierToken()
    {
        skipWhitespace();
        if (position_ >= expression_.size() || !isIdentifierStart(expression_[position_])) {
            return std::nullopt;
        }

        const auto start = position_;
        ++position_;
        while (position_ < expression_.size() && isIdentifierChar(expression_[position_])) {
            ++position_;
        }
        return makeString(expression_.substr(start, position_ - start));
    }

    [[nodiscard]] std::optional<double> parseIdentifierOrFunction()
    {
        const auto identifier = parseIdentifierToken();
        if (!identifier) {
            return fail("Expected identifier in VarSet expression.");
        }

        skipWhitespace();
        if (consume('(')) {
            return parseFunctionCall(*identifier);
        }

        skipWhitespace();
        if (consume('.')) {
            const auto propertyName = parseIdentifierToken();
            if (!propertyName) {
                return fail("Qualified VarSet reference is missing a property name.");
            }
            return resolveQualifiedReference(*identifier, *propertyName);
        }

        if (*identifier == "pi") {
            return std::numbers::pi;
        }
        if (*identifier == "e") {
            return std::numbers::e;
        }

        const std::string localKey = currentObjectName_ + "." + *identifier;
        if (catalog_.properties.find(localKey) != catalog_.properties.end()) {
            return resolveProperty(localKey);
        }

        if (catalog_.aliases.find(*identifier) != catalog_.aliases.end()) {
            return fail("VarSet object reference '" + *identifier + "' must include a property name.");
        }
        return fail("Unknown identifier '" + *identifier + "' in VarSet expression.");
    }

    [[nodiscard]] std::optional<double> parseFunctionCall(const std::string& functionName)
    {
        std::vector<double> arguments;
        skipWhitespace();
        if (!consume(')')) {
            while (true) {
                const auto argument = parseAdditive();
                if (!argument) {
                    return std::nullopt;
                }
                arguments.push_back(*argument);

                if (consume(')')) {
                    break;
                }
                if (!consume(',')) {
                    return fail("Expected ',' or ')' in VarSet function call.");
                }
            }
        }

        return evaluateFunction(functionName, arguments);
    }

    [[nodiscard]] std::optional<double> evaluateFunction(
        const std::string& functionName,
        const std::vector<double>& arguments
    )
    {
        const auto requireCount = [&](std::size_t expected) -> bool {
            return arguments.size() == expected;
        };

        double value = 0.0;
        if (functionName == "abs" && requireCount(1)) {
            value = std::abs(arguments[0]);
        }
        else if (functionName == "sin" && requireCount(1)) {
            value = std::sin(arguments[0]);
        }
        else if (functionName == "cos" && requireCount(1)) {
            value = std::cos(arguments[0]);
        }
        else if (functionName == "tan" && requireCount(1)) {
            value = std::tan(arguments[0]);
        }
        else if (functionName == "asin" && requireCount(1)) {
            value = std::asin(arguments[0]);
        }
        else if (functionName == "acos" && requireCount(1)) {
            value = std::acos(arguments[0]);
        }
        else if (functionName == "atan" && requireCount(1)) {
            value = std::atan(arguments[0]);
        }
        else if (functionName == "atan2" && requireCount(2)) {
            value = std::atan2(arguments[0], arguments[1]);
        }
        else if (functionName == "sqrt" && requireCount(1)) {
            value = std::sqrt(arguments[0]);
        }
        else if (functionName == "pow" && requireCount(2)) {
            value = std::pow(arguments[0], arguments[1]);
        }
        else if (functionName == "cbrt" && requireCount(1)) {
            value = std::cbrt(arguments[0]);
        }
        else if (functionName == "hypot" && requireCount(2)) {
            value = std::hypot(arguments[0], arguments[1]);
        }
        else if (functionName == "cosh" && requireCount(1)) {
            value = std::cosh(arguments[0]);
        }
        else if (functionName == "sinh" && requireCount(1)) {
            value = std::sinh(arguments[0]);
        }
        else if (functionName == "tanh" && requireCount(1)) {
            value = std::tanh(arguments[0]);
        }
        else if (functionName == "floor" && requireCount(1)) {
            value = std::floor(arguments[0]);
        }
        else if (functionName == "ceil" && requireCount(1)) {
            value = std::ceil(arguments[0]);
        }
        else if (functionName == "round" && requireCount(1)) {
            value = std::round(arguments[0]);
        }
        else if (functionName == "trunc" && requireCount(1)) {
            value = std::trunc(arguments[0]);
        }
        else if (functionName == "exp" && requireCount(1)) {
            value = std::exp(arguments[0]);
        }
        else if (functionName == "log" && requireCount(1)) {
            value = std::log(arguments[0]);
        }
        else if (functionName == "log10" && requireCount(1)) {
            value = std::log10(arguments[0]);
        }
        else if (functionName == "mod" && requireCount(2)) {
            if (arguments[1] == 0.0) {
                return fail("Modulo by zero in VarSet expression.");
            }
            value = std::fmod(arguments[0], arguments[1]);
        }
        else if (functionName == "min" && !arguments.empty()) {
            value = *std::min_element(arguments.begin(), arguments.end());
        }
        else if (functionName == "max" && !arguments.empty()) {
            value = *std::max_element(arguments.begin(), arguments.end());
        }
        else if (functionName == "sum" && !arguments.empty()) {
            value = std::accumulate(arguments.begin(), arguments.end(), 0.0);
        }
        else if (functionName == "average" && !arguments.empty()) {
            value = std::accumulate(arguments.begin(), arguments.end(), 0.0)
                / static_cast<double>(arguments.size());
        }
        else if (functionName == "count" && !arguments.empty()) {
            value = static_cast<double>(arguments.size());
        }
        else {
            return fail(makeVarSetExpressionUnsupportedSubsetMessage(
                "unsupported VarSet math function '" + functionName + "' or wrong argument count."
            ));
        }

        return checked(value);
    }

    [[nodiscard]] std::optional<double> resolveQualifiedReference(
        const std::string& objectRef,
        const std::string& propertyName
    )
    {
        const auto alias = catalog_.aliases.find(objectRef);
        if (alias == catalog_.aliases.end()) {
            return fail(makeVarSetExpressionUnsupportedSubsetMessage(
                "expression references non-VarSet object '" + objectRef + "'."
            ));
        }

        const std::string key = alias->second + "." + propertyName;
        if (catalog_.properties.find(key) == catalog_.properties.end()) {
            return fail("Expression references unknown VarSet parameter '" + key + "'.");
        }
        return resolveProperty(key);
    }

    [[nodiscard]] std::optional<double> resolveProperty(const std::string& key)
    {
        std::string resolverError;
        const auto value = resolver_(key, resolverError);
        if (!value) {
            return fail(resolverError.empty() ? "Failed to resolve VarSet parameter '" + key + "'." : resolverError);
        }
        return *value;
    }

    [[nodiscard]] std::optional<double> checked(double value)
    {
        if (!std::isfinite(value)) {
            return fail("VarSet expression evaluated to a non-finite value.");
        }
        return value;
    }

    std::string_view expression_;
    const VarSetCatalog& catalog_;
    std::string currentObjectName_;
    Resolver resolver_;
    std::size_t position_ {0};
    std::string error_;
};

enum class VarSetVisitState
{
    Visiting,
    Done,
};

[[nodiscard]] std::optional<double> evaluateVarSetProperty(
    VarSetCatalog& catalog,
    const std::string& key,
    std::map<std::string, VarSetVisitState>& states,
    std::string& error
)
{
    auto propertyIt = catalog.properties.find(key);
    if (propertyIt == catalog.properties.end()) {
        error = "Expression references unknown VarSet parameter '" + key + "'.";
        return std::nullopt;
    }

    auto& property = propertyIt->second;
    if (property.evaluatedValue) {
        return *property.evaluatedValue;
    }

    const auto stateIt = states.find(key);
    if (stateIt != states.end()) {
        if (stateIt->second == VarSetVisitState::Visiting) {
            error = "VarSet expression dependency cycle detected at '" + key + "'.";
            return std::nullopt;
        }
        if (stateIt->second == VarSetVisitState::Done && property.evaluatedValue) {
            return *property.evaluatedValue;
        }
    }

    states[key] = VarSetVisitState::Visiting;
    std::optional<double> value;
    if (property.expression) {
        VarSetExpressionParser parser(
            *property.expression,
            catalog,
            property.objectName,
            [&](const std::string& dependencyKey, std::string& dependencyError) -> std::optional<double> {
                return evaluateVarSetProperty(catalog, dependencyKey, states, dependencyError);
            }
        );
        value = parser.parse(error);
    }
    else if (property.hasRawValue) {
        value = McSolverEngine::Detail::parseStrictNumeric(property.rawValue);
        if (!value) {
            error = "VarSet parameter '" + key
                + "' is not a pure numeric value and cannot be used in a VarSet expression.";
        }
    }
    else {
        error = "VarSet parameter '" + key + "' has no value.";
    }

    if (!value) {
        return std::nullopt;
    }
    if (!std::isfinite(*value)) {
        error = "VarSet parameter '" + key + "' evaluated to a non-finite value.";
        return std::nullopt;
    }

    property.evaluatedValue = *value;
    states[key] = VarSetVisitState::Done;
    return *value;
}

[[nodiscard]] bool setVarSetRawValue(
    VarSetCatalog& catalog,
    const std::string& key,
    double value,
    bool createIfObjectExists
)
{
    auto propertyIt = catalog.properties.find(key);
    if (propertyIt == catalog.properties.end()) {
        if (!createIfObjectExists) {
            return false;
        }
        const auto separator = key.find('.');
        if (separator == std::string::npos || separator == 0 || separator == key.size() - 1) {
            return false;
        }
        const auto objectName = key.substr(0, separator);
        if (catalog.aliases.find(objectName) == catalog.aliases.end()) {
            return false;
        }
        auto& property = ensureVarSetProperty(catalog, objectName, key.substr(separator + 1));
        propertyIt = catalog.properties.find(property.objectName + "." + property.propertyName);
    }

    propertyIt->second.rawValue = formatDouble(value);
    propertyIt->second.hasRawValue = true;
    propertyIt->second.evaluatedValue.reset();
    return true;
}

}  // namespace

std::string makeVarSetExpressionUnsupportedSubsetMessage(std::string_view detail)
{
    return "[" + std::string(VarSetExpressionUnsupportedSubsetCode)
        + "] VarSet expression support is a reduced FreeCAD subset; " + std::string(detail);
}

void rebuildVarSetShortNameLookup(VarSetCatalog& catalog)
{
    catalog.keysByPropertyName.clear();
    for (const auto& [key, property] : catalog.properties) {
        catalog.keysByPropertyName[property.propertyName].push_back(key);
    }
}

VarSetProperty& ensureVarSetProperty(
    VarSetCatalog& catalog,
    std::string_view objectName,
    std::string_view propertyName
)
{
    const std::string key = makeString(objectName) + "." + makeString(propertyName);
    auto& property = catalog.properties[key];
    property.objectName = makeString(objectName);
    property.propertyName = makeString(propertyName);
    return property;
}

std::optional<std::string> parseVarSetExpressionPath(std::string_view path)
{
    path = trim(path);
    if (!isSimplePropertyPath(path)) {
        return std::nullopt;
    }
    return makeString(path);
}

bool applyApiParametersToVarSets(
    VarSetCatalog& catalog,
    const McSolverEngine::Detail::ParsedApiParameterMap& parameters,
    std::vector<std::string>& messages
)
{
    bool success = true;
    for (const auto& [rawKey, value] : parameters) {
        bool applied = false;
        const auto separator = rawKey.find('.');
        if (separator != std::string::npos) {
            const auto objectRef = rawKey.substr(0, separator);
            const auto parameterName = rawKey.substr(separator + 1);
            const auto alias = catalog.aliases.find(objectRef);
            if (alias != catalog.aliases.end() && !parameterName.empty()) {
                applied = setVarSetRawValue(catalog, alias->second + "." + parameterName, value, true);
            }
        }
        else {
            const auto shortNameIt = catalog.keysByPropertyName.find(rawKey);
            if (shortNameIt != catalog.keysByPropertyName.end()) {
                if (shortNameIt->second.size() > 1) {
                    messages.push_back(
                        "Parameter override '" + rawKey
                        + "' is ambiguous across multiple VarSet parameters; use an explicit VarSet.Property key."
                    );
                    success = false;
                    continue;
                }
                applied = setVarSetRawValue(catalog, shortNameIt->second.front(), value, false);
            }
        }

        (void)applied;
    }

    rebuildVarSetShortNameLookup(catalog);
    return success;
}

bool evaluateVarSetExpressions(VarSetCatalog& catalog, ImportResult& result)
{
    std::map<std::string, VarSetVisitState> states;
    for (const auto& [key, property] : catalog.properties) {
        if (!property.expression) {
            continue;
        }

        std::string error;
        if (!evaluateVarSetProperty(catalog, key, states, error)) {
            if (!error.empty()
                && error.find(VarSetExpressionUnsupportedSubsetCode) != std::string::npos) {
                result.errorCode = ImportErrorCode::VarSetExpressionUnsupportedSubset;
            }
            result.messages.push_back(
                error.empty() ? "Failed to evaluate VarSet expression '" + key + "'." : error
            );
            return false;
        }
    }
    return true;
}

std::optional<std::string> getVarSetValueForBinding(
    const VarSetCatalog& catalog,
    const std::string& key
)
{
    const auto propertyIt = catalog.properties.find(key);
    if (propertyIt == catalog.properties.end()) {
        return std::nullopt;
    }
    if (propertyIt->second.evaluatedValue) {
        return formatDouble(*propertyIt->second.evaluatedValue);
    }
    if (propertyIt->second.hasRawValue) {
        return propertyIt->second.rawValue;
    }
    return std::nullopt;
}

ParameterBindingParseResult parseParameterBindingExpression(
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
            return {};
        }
        objectRef = makeString(expression.substr(2, close - 2));
        parameterName = makeString(trim(expression.substr(close + 3)));
    }
    else {
        const auto separator = expression.find('.');
        if (separator == std::string_view::npos) {
            return {};
        }
        objectRef = makeString(trim(expression.substr(0, separator)));
        parameterName = makeString(trim(expression.substr(separator + 1)));
    }

    if (objectRef.empty() || parameterName.empty()) {
        return {};
    }

    const auto alias = catalog.aliases.find(objectRef);
    if (alias == catalog.aliases.end()) {
        return {
            .binding = std::nullopt,
            .externalReference = true,
            .error = makeVarSetExpressionUnsupportedSubsetMessage(
                "constraint expression references non-VarSet object '" + objectRef + "'."
            ),
        };
    }

    ParsedParameterBinding binding;
    binding.parameterName = parameterName;
    binding.parameterKey = alias->second + "." + parameterName;
    return {.binding = binding};
}

}  // namespace McSolverEngine::DocumentXml::VarSetExpressions
