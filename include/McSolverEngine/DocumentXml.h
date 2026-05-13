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

struct ImportResult
{
    ImportStatus status {ImportStatus::Failed};
    Compat::SketchModel model;
    std::string sketchName;
    std::vector<std::string> messages;
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

}  // namespace McSolverEngine::DocumentXml
