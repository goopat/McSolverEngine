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

[[nodiscard]] QuantityValue makeQuantity(double value, QuantityDimension dimension = QuantityDimension::Dimensionless)
{
    return QuantityValue {.value = value, .dimension = dimension};
}

[[nodiscard]] const char* dimensionName(QuantityDimension dimension) noexcept
{
    switch (dimension) {
        case QuantityDimension::Dimensionless:
            return "dimensionless";
        case QuantityDimension::Length:
            return "length";
        case QuantityDimension::Angle:
            return "angle";
        case QuantityDimension::Area:
            return "area";
    }
    return "unknown";
}

[[nodiscard]] std::optional<QuantityValue> makeSupportedUnitQuantity(double value, std::string_view unit)
{
    unit = trim(unit);
    if (unit.empty()) {
        return makeQuantity(value);
    }

    if (unit == "mm" || unit == "millimeter" || unit == "millimeters"
        || unit == "millimetre" || unit == "millimetres") {
        return makeQuantity(value, QuantityDimension::Length);
    }
    if (unit == "cm" || unit == "centimeter" || unit == "centimeters"
        || unit == "centimetre" || unit == "centimetres") {
        return makeQuantity(value * 10.0, QuantityDimension::Length);
    }
    if (unit == "m" || unit == "meter" || unit == "meters" || unit == "metre"
        || unit == "metres") {
        return makeQuantity(value * 1000.0, QuantityDimension::Length);
    }
    if (unit == "km" || unit == "kilometer" || unit == "kilometers"
        || unit == "kilometre" || unit == "kilometres") {
        return makeQuantity(value * 1000000.0, QuantityDimension::Length);
    }
    if (unit == "um" || unit == "micrometer" || unit == "micrometers"
        || unit == "micrometre" || unit == "micrometres") {
        return makeQuantity(value * 0.001, QuantityDimension::Length);
    }
    if (unit == "nm" || unit == "nanometer" || unit == "nanometers"
        || unit == "nanometre" || unit == "nanometres") {
        return makeQuantity(value * 0.000001, QuantityDimension::Length);
    }
    if (unit == "in" || unit == "inch" || unit == "inches") {
        return makeQuantity(value * 25.4, QuantityDimension::Length);
    }
    if (unit == "ft" || unit == "foot" || unit == "feet") {
        return makeQuantity(value * 304.8, QuantityDimension::Length);
    }

    if (unit == "deg" || unit == "degree" || unit == "degrees") {
        return makeQuantity(value, QuantityDimension::Angle);
    }
    if (unit == "rad" || unit == "radian" || unit == "radians") {
        return makeQuantity(value * 180.0 / std::numbers::pi, QuantityDimension::Angle);
    }

    return std::nullopt;
}

[[nodiscard]] std::string formatQuantityForBinding(QuantityValue value)
{
    switch (value.dimension) {
        case QuantityDimension::Dimensionless:
            return formatDouble(value.value);
        case QuantityDimension::Length:
            return formatDouble(value.value) + " mm";
        case QuantityDimension::Angle:
            return formatDouble(value.value) + " deg";
        case QuantityDimension::Area:
            return formatDouble(value.value) + " mm^2";
    }
    return formatDouble(value.value);
}

[[nodiscard]] std::optional<QuantityValue> parseRawQuantityLiteral(
    std::string_view value,
    std::string& error,
    std::string_view key
)
{
    value = trim(value);
    if (value.empty()) {
        error = "VarSet parameter '" + makeString(key) + "' has an empty value.";
        return std::nullopt;
    }

    std::istringstream stream {std::string(value)};
    double parsed = 0.0;
    stream >> parsed;
    if (stream.fail() || !std::isfinite(parsed)) {
        error = "VarSet parameter '" + makeString(key)
            + "' is not a numeric or supported length/angle quantity value.";
        return std::nullopt;
    }

    std::string suffix;
    std::getline(stream, suffix);
    suffix = makeString(trim(suffix));
    if (const auto quantity = makeSupportedUnitQuantity(parsed, suffix)) {
        return quantity;
    }

    error = makeVarSetExpressionUnsupportedSubsetMessage(
        "VarSet parameter '" + makeString(key)
        + "' uses unsupported unit '" + suffix
        + "'; only length and angle units are supported."
    );
    return std::nullopt;
}

class VarSetExpressionParser
{
public:
    using Resolver = std::function<std::optional<QuantityValue>(const std::string&, std::string&)>;

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

    [[nodiscard]] std::optional<QuantityValue> parse(std::string& error)
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
        if (!std::isfinite(value->value)) {
            error = "VarSet expression evaluated to a non-finite value.";
            return std::nullopt;
        }

        return value;
    }

private:
    [[nodiscard]] std::optional<QuantityValue> fail(std::string message)
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

    [[nodiscard]] std::optional<QuantityValue> parseAdditive()
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
                value = add(*value, *rhs);
                if (!value) {
                    return std::nullopt;
                }
                continue;
            }
            if (consume('-')) {
                const auto rhs = parseMultiplicative();
                if (!rhs) {
                    return std::nullopt;
                }
                value = subtract(*value, *rhs);
                if (!value) {
                    return std::nullopt;
                }
                continue;
            }
            break;
        }

        return checked(*value);
    }

    [[nodiscard]] std::optional<QuantityValue> parseMultiplicative()
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
                value = multiply(*value, *rhs);
                if (!value) {
                    return std::nullopt;
                }
                continue;
            }
            if (consume('/')) {
                const auto rhs = parsePower();
                if (!rhs) {
                    return std::nullopt;
                }
                if (rhs->value == 0.0) {
                    return fail("Division by zero in VarSet expression.");
                }
                value = divide(*value, *rhs);
                if (!value) {
                    return std::nullopt;
                }
                continue;
            }
            if (consume('%')) {
                const auto rhs = parsePower();
                if (!rhs) {
                    return std::nullopt;
                }
                if (rhs->value == 0.0) {
                    return fail("Modulo by zero in VarSet expression.");
                }
                value = modulo(*value, *rhs);
                if (!value) {
                    return std::nullopt;
                }
                continue;
            }
            break;
        }

        return checked(*value);
    }

    [[nodiscard]] std::optional<QuantityValue> parsePower()
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
            value = power(*value, *exponent);
            if (!value) {
                return std::nullopt;
            }
        }

        return checked(*value);
    }

    [[nodiscard]] std::optional<QuantityValue> parseUnary()
    {
        if (consume('+')) {
            return parseUnary();
        }
        if (consume('-')) {
            auto value = parseUnary();
            if (!value) {
                return std::nullopt;
            }
            value->value = -value->value;
            return checked(*value);
        }
        return parsePrimary();
    }

    [[nodiscard]] std::optional<QuantityValue> parsePrimary()
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

    [[nodiscard]] std::optional<QuantityValue> parseNumber()
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
        return parseOptionalUnitSuffix(*parsed);
    }

    [[nodiscard]] std::optional<QuantityValue> parseLabelReference()
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

    [[nodiscard]] std::optional<QuantityValue> parseIdentifierOrFunction()
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
            return parseOptionalUnitSuffix(std::numbers::pi);
        }
        if (*identifier == "e") {
            return parseOptionalUnitSuffix(std::numbers::e);
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

    [[nodiscard]] std::optional<QuantityValue> parseOptionalUnitSuffix(double value)
    {
        skipWhitespace();
        if (position_ < expression_.size() && isIdentifierStart(expression_[position_])) {
            const auto unitStart = position_;
            const auto unit = parseIdentifierToken();
            if (unit) {
                if (const auto quantity = makeSupportedUnitQuantity(value, *unit)) {
                    return *quantity;
                }
                return fail(makeVarSetExpressionUnsupportedSubsetMessage(
                    "unit '" + *unit
                    + "' is outside the supported VarSet expression length/angle unit subset."
                ));
            }
            position_ = unitStart;
        }

        return makeQuantity(value);
    }

    [[nodiscard]] std::optional<QuantityValue> parseFunctionCall(const std::string& functionName)
    {
        std::vector<QuantityValue> arguments;
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

    [[nodiscard]] std::optional<QuantityValue> evaluateFunction(
        const std::string& functionName,
        const std::vector<QuantityValue>& arguments
    )
    {
        const auto requireCount = [&](std::size_t expected) -> bool {
            return arguments.size() == expected;
        };
        const auto requireDimensionless = [&](const QuantityValue& value) -> bool {
            return value.dimension == QuantityDimension::Dimensionless;
        };
        const auto toRadians = [](const QuantityValue& value) {
            return value.dimension == QuantityDimension::Angle ? value.value * std::numbers::pi / 180.0 : value.value;
        };

        if (functionName == "abs" && requireCount(1)) {
            return checked(makeQuantity(std::abs(arguments[0].value), arguments[0].dimension));
        }
        else if (functionName == "sin" && requireCount(1)) {
            if (arguments[0].dimension != QuantityDimension::Dimensionless
                && arguments[0].dimension != QuantityDimension::Angle) {
                return fail("sin() expects a dimensionless or angle argument.");
            }
            return checked(makeQuantity(std::sin(toRadians(arguments[0]))));
        }
        else if (functionName == "cos" && requireCount(1)) {
            if (arguments[0].dimension != QuantityDimension::Dimensionless
                && arguments[0].dimension != QuantityDimension::Angle) {
                return fail("cos() expects a dimensionless or angle argument.");
            }
            return checked(makeQuantity(std::cos(toRadians(arguments[0]))));
        }
        else if (functionName == "tan" && requireCount(1)) {
            if (arguments[0].dimension != QuantityDimension::Dimensionless
                && arguments[0].dimension != QuantityDimension::Angle) {
                return fail("tan() expects a dimensionless or angle argument.");
            }
            return checked(makeQuantity(std::tan(toRadians(arguments[0]))));
        }
        else if (functionName == "asin" && requireCount(1)) {
            if (!requireDimensionless(arguments[0])) {
                return fail("asin() expects a dimensionless argument.");
            }
            return checked(makeQuantity(std::asin(arguments[0].value) * 180.0 / std::numbers::pi, QuantityDimension::Angle));
        }
        else if (functionName == "acos" && requireCount(1)) {
            if (!requireDimensionless(arguments[0])) {
                return fail("acos() expects a dimensionless argument.");
            }
            return checked(makeQuantity(std::acos(arguments[0].value) * 180.0 / std::numbers::pi, QuantityDimension::Angle));
        }
        else if (functionName == "atan" && requireCount(1)) {
            if (!requireDimensionless(arguments[0])) {
                return fail("atan() expects a dimensionless argument.");
            }
            return checked(makeQuantity(std::atan(arguments[0].value) * 180.0 / std::numbers::pi, QuantityDimension::Angle));
        }
        else if (functionName == "atan2" && requireCount(2)) {
            if (arguments[0].dimension != arguments[1].dimension) {
                return fail("atan2() arguments have incompatible units.");
            }
            return checked(makeQuantity(
                std::atan2(arguments[0].value, arguments[1].value) * 180.0 / std::numbers::pi,
                QuantityDimension::Angle
            ));
        }
        else if (functionName == "sqrt" && requireCount(1)) {
            if (!requireDimensionless(arguments[0])) {
                return fail(makeVarSetExpressionUnsupportedSubsetMessage(
                    "sqrt() of a unit quantity would produce a derived unit, which is outside the supported subset."
                ));
            }
            return checked(makeQuantity(std::sqrt(arguments[0].value)));
        }
        else if (functionName == "pow" && requireCount(2)) {
            return power(arguments[0], arguments[1]);
        }
        else if (functionName == "cbrt" && requireCount(1)) {
            if (!requireDimensionless(arguments[0])) {
                return fail(makeVarSetExpressionUnsupportedSubsetMessage(
                    "cbrt() of a unit quantity would produce a derived unit, which is outside the supported subset."
                ));
            }
            return checked(makeQuantity(std::cbrt(arguments[0].value)));
        }
        else if (functionName == "hypot" && requireCount(2)) {
            if (arguments[0].dimension != arguments[1].dimension) {
                return fail("hypot() arguments have incompatible units.");
            }
            const auto dimension = arguments[0].dimension;
            return checked(makeQuantity(std::hypot(arguments[0].value, arguments[1].value), dimension));
        }
        else if (functionName == "cosh" && requireCount(1)) {
            if (!requireDimensionless(arguments[0])) {
                return fail("cosh() expects a dimensionless argument.");
            }
            return checked(makeQuantity(std::cosh(arguments[0].value)));
        }
        else if (functionName == "sinh" && requireCount(1)) {
            if (!requireDimensionless(arguments[0])) {
                return fail("sinh() expects a dimensionless argument.");
            }
            return checked(makeQuantity(std::sinh(arguments[0].value)));
        }
        else if (functionName == "tanh" && requireCount(1)) {
            if (!requireDimensionless(arguments[0])) {
                return fail("tanh() expects a dimensionless argument.");
            }
            return checked(makeQuantity(std::tanh(arguments[0].value)));
        }
        else if (functionName == "floor" && requireCount(1)) {
            return checked(makeQuantity(std::floor(arguments[0].value), arguments[0].dimension));
        }
        else if (functionName == "ceil" && requireCount(1)) {
            return checked(makeQuantity(std::ceil(arguments[0].value), arguments[0].dimension));
        }
        else if (functionName == "round" && requireCount(1)) {
            return checked(makeQuantity(std::round(arguments[0].value), arguments[0].dimension));
        }
        else if (functionName == "trunc" && requireCount(1)) {
            return checked(makeQuantity(std::trunc(arguments[0].value), arguments[0].dimension));
        }
        else if (functionName == "exp" && requireCount(1)) {
            if (!requireDimensionless(arguments[0])) {
                return fail("exp() expects a dimensionless argument.");
            }
            return checked(makeQuantity(std::exp(arguments[0].value)));
        }
        else if (functionName == "log" && requireCount(1)) {
            if (!requireDimensionless(arguments[0])) {
                return fail("log() expects a dimensionless argument.");
            }
            return checked(makeQuantity(std::log(arguments[0].value)));
        }
        else if (functionName == "log10" && requireCount(1)) {
            if (!requireDimensionless(arguments[0])) {
                return fail("log10() expects a dimensionless argument.");
            }
            return checked(makeQuantity(std::log10(arguments[0].value)));
        }
        else if (functionName == "mod" && requireCount(2)) {
            if (arguments[1].value == 0.0) {
                return fail("Modulo by zero in VarSet expression.");
            }
            return modulo(arguments[0], arguments[1]);
        }
        else if (functionName == "min" && !arguments.empty()) {
            return aggregate(arguments, [](double lhs, double rhs) { return std::min(lhs, rhs); });
        }
        else if (functionName == "max" && !arguments.empty()) {
            return aggregate(arguments, [](double lhs, double rhs) { return std::max(lhs, rhs); });
        }
        else if (functionName == "sum" && !arguments.empty()) {
            auto result = arguments.front();
            for (std::size_t i = 1; i < arguments.size(); ++i) {
                auto sum = add(result, arguments[i]);
                if (!sum) {
                    return std::nullopt;
                }
                result = *sum;
            }
            return checked(result);
        }
        else if (functionName == "average" && !arguments.empty()) {
            auto result = arguments.front();
            for (std::size_t i = 1; i < arguments.size(); ++i) {
                auto sum = add(result, arguments[i]);
                if (!sum) {
                    return std::nullopt;
                }
                result = *sum;
            }
            result.value /= static_cast<double>(arguments.size());
            return checked(result);
        }
        else if (functionName == "count" && !arguments.empty()) {
            return makeQuantity(static_cast<double>(arguments.size()));
        }
        else {
            return fail(makeVarSetExpressionUnsupportedSubsetMessage(
                "unsupported VarSet math function '" + functionName + "' or wrong argument count."
            ));
        }
    }

    [[nodiscard]] QuantityDimension compatibleDimension(const QuantityValue& lhs, const QuantityValue& rhs) const
    {
        if (lhs.dimension == rhs.dimension) {
            return lhs.dimension;
        }
        if (lhs.dimension == QuantityDimension::Dimensionless) {
            return rhs.dimension;
        }
        if (rhs.dimension == QuantityDimension::Dimensionless) {
            return lhs.dimension;
        }
        return QuantityDimension::Dimensionless;
    }

    [[nodiscard]] std::optional<QuantityValue> add(const QuantityValue& lhs, const QuantityValue& rhs)
    {
        const auto dimension = compatibleDimension(lhs, rhs);
        if (dimension == QuantityDimension::Dimensionless
            && lhs.dimension != QuantityDimension::Dimensionless
            && rhs.dimension != QuantityDimension::Dimensionless) {
            return fail(
                "Cannot add " + std::string(dimensionName(lhs.dimension)) + " and "
                + std::string(dimensionName(rhs.dimension)) + " quantities in VarSet expression."
            );
        }
        return checked(makeQuantity(lhs.value + rhs.value, dimension));
    }

    [[nodiscard]] std::optional<QuantityValue> subtract(const QuantityValue& lhs, const QuantityValue& rhs)
    {
        const auto dimension = compatibleDimension(lhs, rhs);
        if (dimension == QuantityDimension::Dimensionless
            && lhs.dimension != QuantityDimension::Dimensionless
            && rhs.dimension != QuantityDimension::Dimensionless) {
            return fail(
                "Cannot subtract " + std::string(dimensionName(rhs.dimension)) + " from "
                + std::string(dimensionName(lhs.dimension)) + " quantity in VarSet expression."
            );
        }
        return checked(makeQuantity(lhs.value - rhs.value, dimension));
    }

    [[nodiscard]] std::optional<QuantityValue> multiply(const QuantityValue& lhs, const QuantityValue& rhs)
    {
        if (lhs.dimension == QuantityDimension::Length && rhs.dimension == QuantityDimension::Length) {
            return checked(makeQuantity(lhs.value * rhs.value, QuantityDimension::Area));
        }
        if (lhs.dimension != QuantityDimension::Dimensionless
            && rhs.dimension != QuantityDimension::Dimensionless) {
            return fail(makeVarSetExpressionUnsupportedSubsetMessage(
                "multiplication of two unit quantities would produce a compound unit, which is outside the supported subset."
            ));
        }
        return checked(makeQuantity(lhs.value * rhs.value, compatibleDimension(lhs, rhs)));
    }

    [[nodiscard]] std::optional<QuantityValue> divide(const QuantityValue& lhs, const QuantityValue& rhs)
    {
        if (rhs.dimension == QuantityDimension::Dimensionless) {
            return checked(makeQuantity(lhs.value / rhs.value, lhs.dimension));
        }
        if (lhs.dimension == QuantityDimension::Area && rhs.dimension == QuantityDimension::Length) {
            return checked(makeQuantity(lhs.value / rhs.value, QuantityDimension::Length));
        }
        if (lhs.dimension == QuantityDimension::Length && rhs.dimension == QuantityDimension::Length) {
            return checked(makeQuantity(lhs.value / rhs.value));
        }
        if (rhs.dimension != QuantityDimension::Dimensionless) {
            return fail(makeVarSetExpressionUnsupportedSubsetMessage(
                "division by a unit quantity would produce a derived unit, which is outside the supported subset."
            ));
        }
        return checked(makeQuantity(lhs.value / rhs.value, lhs.dimension));
    }

    [[nodiscard]] std::optional<QuantityValue> modulo(const QuantityValue& lhs, const QuantityValue& rhs)
    {
        const auto dimension = compatibleDimension(lhs, rhs);
        if (dimension == QuantityDimension::Dimensionless
            && lhs.dimension != QuantityDimension::Dimensionless
            && rhs.dimension != QuantityDimension::Dimensionless) {
            return fail("Modulo arguments have incompatible units in VarSet expression.");
        }
        return checked(makeQuantity(std::fmod(lhs.value, rhs.value), lhs.dimension));
    }

    [[nodiscard]] std::optional<QuantityValue> power(const QuantityValue& lhs, const QuantityValue& rhs)
    {
        if (rhs.dimension != QuantityDimension::Dimensionless) {
            return fail("Exponent must be dimensionless in VarSet expression.");
        }
        if (lhs.dimension != QuantityDimension::Dimensionless && std::abs(rhs.value - 1.0) > 1e-12) {
            return fail(makeVarSetExpressionUnsupportedSubsetMessage(
                "powers of unit quantities other than exponent 1 are outside the supported subset."
            ));
        }
        return checked(makeQuantity(std::pow(lhs.value, rhs.value), lhs.dimension));
    }

    template<typename Reducer>
    [[nodiscard]] std::optional<QuantityValue> aggregate(
        const std::vector<QuantityValue>& arguments,
        Reducer reducer
    )
    {
        auto result = arguments.front();
        for (std::size_t i = 1; i < arguments.size(); ++i) {
            const auto dimension = compatibleDimension(result, arguments[i]);
            if (dimension == QuantityDimension::Dimensionless
                && result.dimension != QuantityDimension::Dimensionless
                && arguments[i].dimension != QuantityDimension::Dimensionless) {
                return fail("Aggregate function arguments have incompatible units in VarSet expression.");
            }
            result.value = reducer(result.value, arguments[i].value);
            result.dimension = dimension;
        }
        return checked(result);
    }

    [[nodiscard]] std::optional<QuantityValue> resolveQualifiedReference(
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

    [[nodiscard]] std::optional<QuantityValue> resolveProperty(const std::string& key)
    {
        std::string resolverError;
        const auto value = resolver_(key, resolverError);
        if (!value) {
            return fail(resolverError.empty() ? "Failed to resolve VarSet parameter '" + key + "'." : resolverError);
        }
        return *value;
    }

    [[nodiscard]] std::optional<QuantityValue> checked(QuantityValue value)
    {
        if (!std::isfinite(value.value)) {
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

[[nodiscard]] std::optional<QuantityValue> evaluateVarSetProperty(
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
    std::optional<QuantityValue> value;
    if (property.expression) {
        VarSetExpressionParser parser(
            *property.expression,
            catalog,
            property.objectName,
            [&](const std::string& dependencyKey, std::string& dependencyError) -> std::optional<QuantityValue> {
                return evaluateVarSetProperty(catalog, dependencyKey, states, dependencyError);
            }
        );
        value = parser.parse(error);
    }
    else if (property.hasRawValue) {
        value = parseRawQuantityLiteral(property.rawValue, error, key);
    }
    else {
        error = "VarSet parameter '" + key + "' has no value.";
    }

    if (!value) {
        return std::nullopt;
    }
    if (!std::isfinite(value->value)) {
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
        return formatQuantityForBinding(*propertyIt->second.evaluatedValue);
    }
    if (propertyIt->second.hasRawValue) {
        return propertyIt->second.rawValue;
    }
    return std::nullopt;
}

std::optional<std::string> evaluateExpressionValueForBinding(
    std::string_view expression,
    const VarSetCatalog& catalog,
    std::string& error
)
{
    VarSetExpressionParser parser(
        expression,
        catalog,
        {},
        [&](const std::string& dependencyKey, std::string& dependencyError) -> std::optional<QuantityValue> {
            const auto propertyIt = catalog.properties.find(dependencyKey);
            if (propertyIt == catalog.properties.end()) {
                dependencyError = "Expression references unknown VarSet parameter '" + dependencyKey + "'.";
                return std::nullopt;
            }
            if (propertyIt->second.evaluatedValue) {
                return *propertyIt->second.evaluatedValue;
            }
            if (propertyIt->second.hasRawValue) {
                return parseRawQuantityLiteral(propertyIt->second.rawValue, dependencyError, dependencyKey);
            }
            dependencyError = "VarSet parameter '" + dependencyKey + "' has no value.";
            return std::nullopt;
        }
    );

    const auto value = parser.parse(error);
    if (!value) {
        return std::nullopt;
    }
    return formatQuantityForBinding(*value);
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
    if (!isSimplePropertyPath(parameterName)) {
        return {};
    }

    ParsedParameterBinding binding;
    binding.parameterName = parameterName;
    binding.parameterKey = alias->second + "." + parameterName;
    return {.binding = binding};
}

}  // namespace McSolverEngine::DocumentXml::VarSetExpressions
