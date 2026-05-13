#include "McSolverEngine/BRepExport.h"

#if MCSOLVERENGINE_WITH_OCCT

#include <fstream>
#include <limits>
#include <locale>
#include <sstream>

#include <BRepTools_ShapeSet.hxx>
#include <TopoDS_Shape.hxx>

#include "SketchShapeBuilder.h"

namespace McSolverEngine::BRep
{

namespace
{

void writeFreeCadStyleBRep(const TopoDS_Shape& shape, std::ostream& out)
{
    enum
    {
        VERSION_1 = 1,
        VERSION_2 = 2,
        VERSION_3 = 3
    };

    (void)VERSION_2;
    (void)VERSION_3;

    out.imbue(std::locale::classic());
    out.setf(std::ios::fixed, std::ios::floatfield);
    out.setf(std::ios::showpoint);
    out.precision(std::numeric_limits<double>::digits10 + 2);

    BRepTools_ShapeSet shapeSet(Standard_False);
    shapeSet.SetFormatNb(VERSION_1);
    shapeSet.Add(shape);
    shapeSet.Write(out);
    shapeSet.Write(shape, out);
}

[[nodiscard]] TopoDS_Shape withExplicitRootLocation(const TopoDS_Shape& shape)
{
    if (shape.IsNull()) {
        return shape;
    }

    if (!shape.Location().IsIdentity()) {
        return shape;
    }

    gp_Trsf transform;
    return shape.Located(TopLoc_Location(transform));
}

}  // namespace

ExportResult exportSketchToBRep(const Compat::SketchModel& model)
{
    ExportResult result;
    std::string error;
    const TopoDS_Shape shape = ShapeBuilder::buildSketchShape(model, error);
    if (shape.IsNull()) {
        result.messages.push_back(error.empty() ? "BREP export produced a null shape." : error);
        return result;
    }

    std::ostringstream stream;
    writeFreeCadStyleBRep(withExplicitRootLocation(ShapeBuilder::applySketchPlacement(shape, model)), stream);
    result.status = ExportStatus::Success;
    result.brep = stream.str();
    return result;
}

ExportStatus exportSketchToBRepFile(const Compat::SketchModel& model, std::string_view path)
{
    std::string error;
    const TopoDS_Shape shape = ShapeBuilder::buildSketchShape(model, error);
    if (shape.IsNull()) {
        return ExportStatus::Failed;
    }

    std::ofstream file(std::string(path), std::ios::binary);
    if (!file) {
        return ExportStatus::Failed;
    }

    writeFreeCadStyleBRep(withExplicitRootLocation(ShapeBuilder::applySketchPlacement(shape, model)), file);
    return file.good() ? ExportStatus::Success : ExportStatus::Failed;
}

}  // namespace McSolverEngine::BRep

#else

namespace McSolverEngine::BRep
{

namespace
{

constexpr std::string_view kOpenCascadeUnavailableMessage =
    "BREP export requires OCCT, but this McSolverEngine build was configured without it.";

}  // namespace

ExportResult exportSketchToBRep(const Compat::SketchModel& model)
{
    (void)model;

    ExportResult result;
    result.status = ExportStatus::OpenCascadeUnavailable;
    result.messages.emplace_back(kOpenCascadeUnavailableMessage);
    return result;
}

ExportStatus exportSketchToBRepFile(const Compat::SketchModel& model, std::string_view path)
{
    (void)model;
    (void)path;
    return ExportStatus::OpenCascadeUnavailable;
}

}  // namespace McSolverEngine::BRep

#endif
