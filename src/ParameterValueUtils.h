#pragma once

#include "McSolverEngine/CompatModel.h"

#include <cctype>
#include <cmath>
#include <numbers>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace McSolverEngine::Detail
{

using ParsedApiParameterMap = std::unordered_map<std::string, double>;

[[nodiscard]] inline std::string_view trim(std::string_view value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    return value;
}

[[nodiscard]] inline bool isLengthConstraintKind(Compat::ConstraintKind kind) noexcept
{
    switch (kind) {
        case Compat::ConstraintKind::DistanceX:
        case Compat::ConstraintKind::DistanceY:
        case Compat::ConstraintKind::Distance:
        case Compat::ConstraintKind::Radius:
        case Compat::ConstraintKind::Diameter:
            return true;
        default:
            return false;
    }
}

[[nodiscard]] inline bool isAngleConstraintKind(Compat::ConstraintKind kind) noexcept
{
    return kind == Compat::ConstraintKind::Angle;
}

[[nodiscard]] inline std::string_view expectedParameterUnit(Compat::ConstraintKind kind) noexcept
{
    if (isAngleConstraintKind(kind)) {
        return "degree";
    }
    if (isLengthConstraintKind(kind)) {
        return "mm";
    }
    return "numeric";
}

[[nodiscard]] inline double convertApiParameterToInternal(double value, Compat::ConstraintKind kind) noexcept
{
    if (isAngleConstraintKind(kind)) {
        return value * std::numbers::pi / 180.0;
    }
    return value;
}

[[nodiscard]] inline std::optional<double> parseStrictNumeric(std::string_view value)
{
    const auto trimmedValue = trim(value);
    if (trimmedValue.empty()) {
        return std::nullopt;
    }

    std::istringstream stream {std::string(trimmedValue)};
    double parsed = 0.0;
    stream >> parsed;
    if (stream.fail() || !std::isfinite(parsed)) {
        return std::nullopt;
    }

    stream >> std::ws;
    if (!stream.eof()) {
        return std::nullopt;
    }

    return parsed;
}

[[nodiscard]] inline bool tryParseApiParameters(
    const McSolverEngine::ParameterMap& parameters,
    ParsedApiParameterMap& parsedParameters,
    std::string* invalidKey = nullptr
)
{
    parsedParameters.clear();
    parsedParameters.reserve(parameters.size());

    for (const auto& [key, value] : parameters) {
        const auto parsed = parseStrictNumeric(value);
        if (!parsed) {
            if (invalidKey) {
                *invalidKey = key;
            }
            return false;
        }
        parsedParameters.insert_or_assign(key, *parsed);
    }

    return true;
}

[[nodiscard]] inline std::string normalizeUnitSuffix(std::string_view suffix)
{
    suffix = trim(suffix);

    std::string normalized;
    normalized.reserve(suffix.size());
    for (const char ch : suffix) {
        const auto uch = static_cast<unsigned char>(ch);
        if (std::isspace(uch) != 0 || ch == '_') {
            continue;
        }
        normalized.push_back(static_cast<char>(std::tolower(uch)));
    }

    return normalized;
}

[[nodiscard]] inline std::optional<double> convertDocumentParameterToInternal(
    double value,
    std::string_view unitSuffix,
    Compat::ConstraintKind kind
)
{
    const auto normalizedUnit = normalizeUnitSuffix(unitSuffix);

    if (isAngleConstraintKind(kind)) {
        if (normalizedUnit.empty() || normalizedUnit == "deg" || normalizedUnit == "degree"
            || normalizedUnit == "degrees") {
            return convertApiParameterToInternal(value, kind);
        }
        if (normalizedUnit == "rad" || normalizedUnit == "radian" || normalizedUnit == "radians") {
            return value;
        }
        return std::nullopt;
    }

    if (isLengthConstraintKind(kind)) {
        if (normalizedUnit.empty() || normalizedUnit == "mm" || normalizedUnit == "millimeter"
            || normalizedUnit == "millimeters" || normalizedUnit == "millimetre"
            || normalizedUnit == "millimetres") {
            return value;
        }
        if (normalizedUnit == "cm" || normalizedUnit == "centimeter"
            || normalizedUnit == "centimeters" || normalizedUnit == "centimetre"
            || normalizedUnit == "centimetres") {
            return value * 10.0;
        }
        if (normalizedUnit == "m" || normalizedUnit == "meter" || normalizedUnit == "meters"
            || normalizedUnit == "metre" || normalizedUnit == "metres") {
            return value * 1000.0;
        }
        if (normalizedUnit == "km" || normalizedUnit == "kilometer"
            || normalizedUnit == "kilometers" || normalizedUnit == "kilometre"
            || normalizedUnit == "kilometres") {
            return value * 1000000.0;
        }
        if (normalizedUnit == "um" || normalizedUnit == "micrometer"
            || normalizedUnit == "micrometers" || normalizedUnit == "micrometre"
            || normalizedUnit == "micrometres") {
            return value * 0.001;
        }
        if (normalizedUnit == "nm" || normalizedUnit == "nanometer"
            || normalizedUnit == "nanometers" || normalizedUnit == "nanometre"
            || normalizedUnit == "nanometres") {
            return value * 0.000001;
        }
        if (normalizedUnit == "in" || normalizedUnit == "inch" || normalizedUnit == "inches") {
            return value * 25.4;
        }
        if (normalizedUnit == "ft" || normalizedUnit == "foot" || normalizedUnit == "feet") {
            return value * 304.8;
        }
        return std::nullopt;
    }

    if (!normalizedUnit.empty()) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] inline std::optional<double> parseDocumentParameterValue(
    std::string_view value,
    Compat::ConstraintKind kind
)
{
    const auto trimmedValue = trim(value);
    if (trimmedValue.empty()) {
        return std::nullopt;
    }

    std::istringstream stream {std::string(trimmedValue)};
    double parsed = 0.0;
    stream >> parsed;
    if (stream.fail() || !std::isfinite(parsed)) {
        return std::nullopt;
    }

    std::string suffix;
    std::getline(stream, suffix);
    return convertDocumentParameterToInternal(parsed, suffix, kind);
}

}  // namespace McSolverEngine::Detail
