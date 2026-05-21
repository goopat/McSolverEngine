#pragma once

#include <string>
#include <vector>

#include "McSolverEngine/Export.h"

namespace McSolverEngine::ZipExtract
{

struct ExtractResult
{
    bool success {false};
    std::string documentXml;
    std::string errorMessage;
};

ExtractResult extractDocumentXml(const std::string& fcstdPath);

}  // namespace McSolverEngine::ZipExtract
