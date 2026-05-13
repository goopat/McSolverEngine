#pragma once

#include <string>

#include <TopoDS_Shape.hxx>
#include <gp_Trsf.hxx>

#include "McSolverEngine/CompatModel.h"

namespace McSolverEngine::ShapeBuilder
{

[[nodiscard]] gp_Trsf makePlacementTransform(const Compat::Placement& placement);
[[nodiscard]] TopoDS_Shape buildSketchShape(const Compat::SketchModel& model, std::string& error);
[[nodiscard]] TopoDS_Shape applySketchPlacement(const TopoDS_Shape& shape, const Compat::SketchModel& model);

}  // namespace McSolverEngine::ShapeBuilder
