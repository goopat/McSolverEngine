#pragma once

#include <memory>
#include <string>
#include <vector>

#include "McSolverEngine/Export.h"

namespace McSolverEngine::ZipExtract
{

struct ExtractResult
{
    bool success {false};
    std::unique_ptr<char[]> documentXml;
    std::size_t documentXmlSize {0};
    std::string errorMessage;
};

ExtractResult extractDocumentXml(const std::string& fcstdPath);

}  // namespace McSolverEngine::ZipExtract
