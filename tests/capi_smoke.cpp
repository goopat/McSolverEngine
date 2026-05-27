#include <atomic>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#ifndef MCSOLVERENGINE_WITH_OCCT
#    define MCSOLVERENGINE_WITH_OCCT 0
#endif

#include "McSolverEngine/CApi.h"
#include "src/WindowsAssertMode.h"

namespace
{

bool contains(std::string_view text, std::string_view needle)
{
    return text.find(needle) != std::string_view::npos;
}

bool expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

const McSolverEngineVarSetProperty* findVarSetProperty(
    const McSolverEngineVarSetProperty* properties,
    int count,
    std::string_view key
)
{
    if (!properties || count <= 0) {
        return nullptr;
    }
    for (int i = 0; i < count; ++i) {
        if (properties[i].keyUtf8 != nullptr && key == properties[i].keyUtf8) {
            return &properties[i];
        }
    }
    return nullptr;
}

bool expectVarSetProperty(
    const McSolverEngineVarSetProperty* properties,
    int count,
    std::string_view key,
    std::string_view expectedValue,
    std::string_view expectedUnit
)
{
    const auto* property = findVarSetProperty(properties, count, key);
    return property != nullptr && property->valueUtf8 != nullptr && expectedValue == property->valueUtf8
        && property->unitUtf8 != nullptr && expectedUnit == property->unitUtf8;
}

const McSolverEngineSketchInfo* findSketchInfo(
    const McSolverEngineSketchInfo* sketches,
    int count,
    std::string_view objectName
)
{
    if (!sketches || count <= 0) {
        return nullptr;
    }
    for (int i = 0; i < count; ++i) {
        if (sketches[i].objectNameUtf8 != nullptr && objectName == sketches[i].objectNameUtf8) {
            return &sketches[i];
        }
    }
    return nullptr;
}

const McSolverEngineScalarPropertyInfo* findScalarPropertyInfo(
    const McSolverEngineScalarPropertyInfo* properties,
    int count,
    std::string_view name
)
{
    if (!properties || count <= 0) {
        return nullptr;
    }
    for (int i = 0; i < count; ++i) {
        if (properties[i].nameUtf8 != nullptr && name == properties[i].nameUtf8) {
            return &properties[i];
        }
    }
    return nullptr;
}

const McSolverEngineVarSetInfo* findVarSetInfo(
    const McSolverEngineVarSetInfo* varSets,
    int count,
    std::string_view objectName
)
{
    if (!varSets || count <= 0) {
        return nullptr;
    }
    for (int i = 0; i < count; ++i) {
        if (varSets[i].objectNameUtf8 != nullptr && objectName == varSets[i].objectNameUtf8) {
            return &varSets[i];
        }
    }
    return nullptr;
}

const McSolverEngineVarSetParameterInfo* findVarSetParameterInfo(
    const McSolverEngineVarSetParameterInfo* parameters,
    int count,
    std::string_view name
)
{
    if (!parameters || count <= 0) {
        return nullptr;
    }
    for (int i = 0; i < count; ++i) {
        if (parameters[i].nameUtf8 != nullptr && name == parameters[i].nameUtf8) {
            return &parameters[i];
        }
    }
    return nullptr;
}

double lineLength(const McSolverEngineGeometryRecord& record)
{
    const double dx = record.end.x - record.start.x;
    const double dy = record.end.y - record.start.y;
    return std::sqrt(dx * dx + dy * dy);
}

double lineAngleDegrees(const McSolverEngineGeometryRecord& record)
{
    return std::atan2(record.end.y - record.start.y, record.end.x - record.start.x) * 180.0 / 3.14159265358979323846;
}

std::string samplePath()
{
    return std::string(MCSOLVERENGINE_SOURCE_DIR) + "/fcstdDoc/1.xml";
}

std::string readTextFile(const std::string& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {};
    }

    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

class ScopedTestTimer
{
public:
    explicit ScopedTestTimer(const char* testName)
        : name(testName)
        , startedAt(std::chrono::steady_clock::now())
    {
    }

    ~ScopedTestTimer()
    {
        const auto elapsed =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startedAt).count();
        std::cout << "[capi_smoke] " << name << ": " << elapsed << " ms\n";
    }

private:
    const char* name;
    std::chrono::steady_clock::time_point startedAt;
};

}  // namespace

int main()
{
    McSolverEngine::Detail::configureWindowsAssertMode();

    {
        ScopedTestTimer timer("GetVersion");
        if (!expect(McSolverEngine_GetVersion() != nullptr, "Expected version pointer to be valid.")) {
            return EXIT_FAILURE;
        }
        if (!expect(std::string(McSolverEngine_GetVersion()).empty() == false, "Expected non-empty version string.")) {
            return EXIT_FAILURE;
        }
        if (!expect(
                McSolverEngine_GetLastError() != nullptr && std::string(McSolverEngine_GetLastError()).empty(),
                "Expected GetLastError to return empty string after successful GetVersion."
            )) {
            return EXIT_FAILURE;
        }
    }

    std::string documentXml;
    {
        ScopedTestTimer timer("Load sample Document.xml");
        documentXml = readTextFile(samplePath());
        if (!expect(!documentXml.empty(), "Expected sample Document.xml content to load.")) {
            return EXIT_FAILURE;
        }
    }

    constexpr std::string_view inspectDocumentXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Document>
    <Objects Count="2">
        <Object type="App::VarSet" name="VarSet001" id="1"/>
        <Object type="Sketcher::SketchObject" name="SketchA" id="2"/>
    </Objects>
    <ObjectData Count="2">
        <Object name="VarSet001">
            <Properties Count="6" TransientCount="0">
                <Property name="Label" type="App::PropertyString">
                    <String value="Parameters"/>
                </Property>
                <Property name="Label2" type="App::PropertyString">
                    <String value=""/>
                </Property>
                <Property name="Visibility" type="App::PropertyBool">
                    <Bool value="true"/>
                </Property>
                <Property name="Base" type="App::PropertyFloat">
                    <Float value="5.0"/>
                </Property>
                <Property name="Width" type="App::PropertyFloat">
                    <Float value="0.0"/>
                </Property>
                <Property name="ExpressionEngine" type="App::PropertyExpressionEngine">
                    <ExpressionEngine count="1">
                        <Expression path="Width" expression="Base * 2"/>
                    </ExpressionEngine>
                </Property>
            </Properties>
        </Object>
        <Object name="SketchA">
            <Properties Count="6" TransientCount="0">
                <Property name="Label" type="App::PropertyString">
                    <String value="Sketch Alpha"/>
                </Property>
                <Property name="Visibility" type="App::PropertyBool">
                    <Bool value="true"/>
                </Property>
                <Property name="Comment" type="App::PropertyString">
                    <String value="demo"/>
                </Property>
                <Property name="Width" type="App::PropertyFloat">
                    <Float value="12.5"/>
                </Property>
                <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                    <ConstraintList count="0">
                    </ConstraintList>
                </Property>
                <Property name="Geometry" type="Part::PropertyGeometryList">
                    <GeometryList count="1">
                        <Geometry type="Part::GeomLineSegment" id="1" migrated="1">
                            <LineSegment StartX="0.0" StartY="0.0" StartZ="0.0" EndX="1.0" EndY="0.0" EndZ="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                    </GeometryList>
                </Property>
            </Properties>
        </Object>
    </ObjectData>
</Document>)";

    {
        ScopedTestTimer timer("InspectDocumentXml");
        McSolverEngineDocumentInfo* inspected = nullptr;
        const auto inspectCode = McSolverEngine_InspectDocumentXml(inspectDocumentXml.data(), &inspected);
        if (!expect(inspectCode == MCSOLVERENGINE_RESULT_SUCCESS, "Expected Document inspection to succeed.")
            || !expect(inspected != nullptr, "Expected Document inspection result pointer.")
            || !expect(inspected->sketchCount == 1, "Expected one sketch in inspection result.")
            || !expect(inspected->varSetCount == 1, "Expected one varset in inspection result.")
            || !expect(inspected->messageCount == 0, "Expected no inspection messages for valid input.")) {
            McSolverEngine_FreeDocumentInfo(inspected);
            return EXIT_FAILURE;
        }

        const auto* sketch = findSketchInfo(inspected->sketches, inspected->sketchCount, "SketchA");
        if (!expect(sketch != nullptr, "Expected SketchA in inspection result.")
            || !expect(sketch->labelUtf8 != nullptr && std::string(sketch->labelUtf8) == "Sketch Alpha", "Expected SketchA label.")
            || !expect(sketch->propertyCount == 4, "Expected SketchA to expose only scalar properties.")) {
            McSolverEngine_FreeDocumentInfo(inspected);
            return EXIT_FAILURE;
        }

        const auto* visibilityProperty = findScalarPropertyInfo(sketch->properties, sketch->propertyCount, "Visibility");
        const auto* widthProperty = findScalarPropertyInfo(sketch->properties, sketch->propertyCount, "Width");
        if (!expect(
                visibilityProperty != nullptr
                    && visibilityProperty->typeUtf8 != nullptr
                    && std::string(visibilityProperty->typeUtf8) == "App::PropertyBool"
                    && visibilityProperty->scalarValueUtf8 != nullptr
                    && std::string(visibilityProperty->scalarValueUtf8) == "true"
                    && visibilityProperty->propertyXmlUtf8 != nullptr
                    && contains(visibilityProperty->propertyXmlUtf8, "<Bool value=\"true\"/>"),
                "Expected sketch bool property to expose type, scalar value, and property XML."
            )
            || !expect(
                widthProperty != nullptr
                    && widthProperty->scalarValueUtf8 != nullptr
                    && std::string(widthProperty->scalarValueUtf8) == "12.5",
                "Expected sketch float property to expose scalar value."
            )
            || !expect(
                findScalarPropertyInfo(sketch->properties, sketch->propertyCount, "Geometry") == nullptr,
                "Expected sketch inspection to ignore non-scalar Geometry property."
            )
            || !expect(
                findScalarPropertyInfo(sketch->properties, sketch->propertyCount, "Constraints") == nullptr,
                "Expected sketch inspection to ignore non-scalar Constraints property."
            )) {
            McSolverEngine_FreeDocumentInfo(inspected);
            return EXIT_FAILURE;
        }

        const auto* varSet = findVarSetInfo(inspected->varSets, inspected->varSetCount, "VarSet001");
        if (!expect(varSet != nullptr, "Expected VarSet001 in inspection result.")
            || !expect(varSet->labelUtf8 != nullptr && std::string(varSet->labelUtf8) == "Parameters", "Expected VarSet label.")
            || !expect(varSet->parameterCount == 5, "Expected all scalar VarSet parameters including Label/Label2/Visibility.")) {
            McSolverEngine_FreeDocumentInfo(inspected);
            return EXIT_FAILURE;
        }

        const auto* label2Parameter = findVarSetParameterInfo(varSet->parameters, varSet->parameterCount, "Label2");
        const auto* widthParameter = findVarSetParameterInfo(varSet->parameters, varSet->parameterCount, "Width");
        if (!expect(
                label2Parameter != nullptr
                    && label2Parameter->rawValueUtf8 != nullptr
                    && std::string(label2Parameter->rawValueUtf8).empty()
                    && label2Parameter->propertyXmlUtf8 != nullptr
                    && contains(label2Parameter->propertyXmlUtf8, "Property name=\"Label2\""),
                "Expected VarSet empty string parameters to be preserved."
            )
            || !expect(
                widthParameter != nullptr
                    && widthParameter->rawValueUtf8 != nullptr
                    && std::string(widthParameter->rawValueUtf8) == "0.0"
                    && widthParameter->expressionUtf8 != nullptr
                    && std::string(widthParameter->expressionUtf8) == "Base * 2"
                    && widthParameter->propertyXmlUtf8 != nullptr
                    && contains(widthParameter->propertyXmlUtf8, "Property name=\"Width\""),
                "Expected VarSet parameters to expose raw value, expression, and property XML."
            )) {
            McSolverEngine_FreeDocumentInfo(inspected);
            return EXIT_FAILURE;
        }

        McSolverEngine_FreeDocumentInfo(inspected);
    }

    {
        ScopedTestTimer timer("InspectDocumentXml invalid XML");
        McSolverEngineDocumentInfo* inspected = nullptr;
        const auto inspectCode = McSolverEngine_InspectDocumentXml("not valid xml", &inspected);
        if (!expect(
                inspectCode == MCSOLVERENGINE_RESULT_IMPORT_FAILED,
                "Expected invalid XML inspection to fail with IMPORT_FAILED."
            )
            || !expect(inspected != nullptr, "Expected inspection failure to still return result metadata.")
            || !expect(
                inspected->messageCount == 1
                    && inspected->messages != nullptr
                    && inspected->messages[0] != nullptr
                    && contains(inspected->messages[0], "ObjectData"),
                "Expected invalid XML inspection to expose parse failure message."
            )
            || !expect(
                McSolverEngine_GetLastError() != nullptr && !std::string(McSolverEngine_GetLastError()).empty(),
                "Expected GetLastError non-empty after inspection import failure."
            )) {
            McSolverEngine_FreeDocumentInfo(inspected);
            return EXIT_FAILURE;
        }
        McSolverEngine_FreeDocumentInfo(inspected);
    }

    {
        ScopedTestTimer timer("SolveToGeometry");
        McSolverEngineGeometryResult* structuredGeometry = nullptr;
        const auto structuredGeometryCode =
            McSolverEngine_SolveToGeometry(documentXml.c_str(), "Sketch", &structuredGeometry);
        if (!expect(
                structuredGeometryCode == MCSOLVERENGINE_RESULT_SUCCESS,
                "Expected structured geometry C API call to succeed."
            )) {
            return EXIT_FAILURE;
        }
        if (!expect(structuredGeometry != nullptr, "Expected structured geometry result pointer.")) {
            return EXIT_FAILURE;
        }
        if (!expect(
                structuredGeometry->sketchName != nullptr && std::string(structuredGeometry->sketchName) == "Sketch",
                "Expected structured geometry result to expose sketch name."
            )) {
            McSolverEngine_FreeGeometryResult(structuredGeometry);
            return EXIT_FAILURE;
        }
        if (!expect(
                structuredGeometry->importStatus != nullptr && std::string(structuredGeometry->importStatus) == "Success",
                "Expected structured geometry result to expose import status."
            )) {
            McSolverEngine_FreeGeometryResult(structuredGeometry);
            return EXIT_FAILURE;
        }
        if (!expect(
                structuredGeometry->solveStatus != nullptr && std::string(structuredGeometry->solveStatus) == "Success",
                "Expected structured geometry result to expose solve status."
            )) {
            McSolverEngine_FreeGeometryResult(structuredGeometry);
            return EXIT_FAILURE;
        }
        if (!expect(
                structuredGeometry->exportKind != nullptr && std::string(structuredGeometry->exportKind) == "Geometry",
                "Expected structured geometry result to expose export kind."
            )) {
            McSolverEngine_FreeGeometryResult(structuredGeometry);
            return EXIT_FAILURE;
        }
        if (!expect(
                structuredGeometry->exportStatus != nullptr && std::string(structuredGeometry->exportStatus) == "Success",
                "Expected structured geometry result to expose export status."
            )) {
            McSolverEngine_FreeGeometryResult(structuredGeometry);
            return EXIT_FAILURE;
        }
        if (!expect(
                structuredGeometry->geometryCount == 11,
                "Expected structured geometry result to contain 11 geometries (including construction and external)."
            )) {
            McSolverEngine_FreeGeometryResult(structuredGeometry);
            return EXIT_FAILURE;
        }

        bool hasLine = false;
        bool hasArc = false;
        bool allHaveOriginalId = true;
        for (int i = 0; i < structuredGeometry->geometryCount; ++i) {
            const auto& record = structuredGeometry->geometries[i];
            hasLine = hasLine || record.kind == MCSOLVERENGINE_GEOMETRY_LINE_SEGMENT;
            hasArc = hasArc || record.kind == MCSOLVERENGINE_GEOMETRY_ARC;
            allHaveOriginalId = allHaveOriginalId && record.originalId > -99999999;
        }
        if (!expect(allHaveOriginalId, "Expected all structured geometry records to have originalId > -99999999.")) {
            McSolverEngine_FreeGeometryResult(structuredGeometry);
            return EXIT_FAILURE;
        }
        if (!expect(hasLine, "Expected structured geometry result to contain a line segment.")) {
            McSolverEngine_FreeGeometryResult(structuredGeometry);
            return EXIT_FAILURE;
        }
        if (!expect(hasArc, "Expected structured geometry result to contain an arc.")) {
            McSolverEngine_FreeGeometryResult(structuredGeometry);
            return EXIT_FAILURE;
        }
        if (!expect(
                McSolverEngine_GetLastError() != nullptr && std::string(McSolverEngine_GetLastError()).empty(),
                "Expected GetLastError to return empty string after successful SolveToGeometry."
            )) {
            McSolverEngine_FreeGeometryResult(structuredGeometry);
            return EXIT_FAILURE;
        }
        McSolverEngine_FreeGeometryResult(structuredGeometry);
    }

    {
        ScopedTestTimer timer("SolveToBRep");
        McSolverEngineBRepResult* brepResult = nullptr;
        const auto brepCode = McSolverEngine_SolveToBRep(documentXml.c_str(), "Sketch", &brepResult);
        const std::string brepPayload = brepResult && brepResult->brepUtf8 ? brepResult->brepUtf8 : "";
        McSolverEngine_FreeBRepResult(brepResult);

#if MCSOLVERENGINE_WITH_OCCT
        if (!expect(brepCode == MCSOLVERENGINE_RESULT_SUCCESS, "Expected BRep C API call to succeed.")) {
            return EXIT_FAILURE;
        }
        if (!expect(
                contains(brepPayload, "CASCADE Topology V1"),
                "Expected BRep payload to contain serialized BREP text."
            )) {
            return EXIT_FAILURE;
        }
#else
        if (!expect(
                brepCode == MCSOLVERENGINE_RESULT_OPEN_CASCADE_UNAVAILABLE,
                "Expected BRep C API call to report missing OpenCascade."
            )) {
            return EXIT_FAILURE;
        }
        if (!expect(brepPayload.empty(), "Expected no-OCCT BRep payload to be empty.")) {
            return EXIT_FAILURE;
        }
#endif
    }

    constexpr std::string_view parameterizedDocumentXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Document>
    <Objects Count="2">
        <Object type="App::VarSet" name="VarSet" id="1"/>
        <Object type="Sketcher::SketchObject" name="Sketch" id="2"/>
    </Objects>
    <ObjectData Count="2">
        <Object name="VarSet">
            <Properties Count="2" TransientCount="0">
                <Property name="Label" type="App::PropertyString">
                    <String value="Parameters"/>
                </Property>
                <Property name="Width" type="App::PropertyFloat">
                    <Float value="6.0"/>
                </Property>
            </Properties>
        </Object>
        <Object name="Sketch">
            <Properties Count="3" TransientCount="0">
                <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                    <ConstraintList count="2">
                        <Constrain Name="" Type="2" Value="0.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="6" Value="0.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                    </ConstraintList>
                </Property>
                <Property name="ExpressionEngine" type="App::PropertyExpressionEngine">
                    <ExpressionEngine count="1">
                        <Expression path="Constraints[1]" expression="&lt;&lt;Parameters&gt;&gt;.Width"/>
                    </ExpressionEngine>
                </Property>
                <Property name="Geometry" type="Part::PropertyGeometryList">
                    <GeometryList count="1">
                        <Geometry type="Part::GeomLineSegment" id="1" migrated="1">
                            <LineSegment StartX="0.0" StartY="0.0" StartZ="0.0" EndX="3.0" EndY="1.0" EndZ="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                    </GeometryList>
                </Property>
            </Properties>
        </Object>
    </ObjectData>
</Document>)";

    const char* parameterKeys[] = {"VarSet.Width"};
    const char* parameterValues[] = {"8.5"};
    {
        ScopedTestTimer timer("SolveToGeometryWithParameters");
        McSolverEngineGeometryResult* parameterizedGeometry = nullptr;
        const auto parameterizedGeometryCode = McSolverEngine_SolveToGeometryWithParameters(
            parameterizedDocumentXml.data(),
            "Sketch",
            parameterKeys,
            parameterValues,
            1,
            &parameterizedGeometry
        );
        if (!expect(
                parameterizedGeometryCode == MCSOLVERENGINE_RESULT_SUCCESS,
                "Expected parameterized Geometry C API call to succeed."
            )) {
            return EXIT_FAILURE;
        }
        if (!expect(
                parameterizedGeometry != nullptr && parameterizedGeometry->geometryCount == 1,
                "Expected parameterized Geometry result to contain one line."
            )) {
            McSolverEngine_FreeGeometryResult(parameterizedGeometry);
            return EXIT_FAILURE;
        }
        if (!expect(
                parameterizedGeometry->geometries[0].kind == MCSOLVERENGINE_GEOMETRY_LINE_SEGMENT,
                "Expected parameterized Geometry result to expose a line segment."
            )) {
            McSolverEngine_FreeGeometryResult(parameterizedGeometry);
            return EXIT_FAILURE;
        }
        if (!expect(
                parameterizedGeometry->geometries[0].originalId > -99999999,
                "Expected parameterized Geometry result to have originalId > -99999999."
            )) {
            McSolverEngine_FreeGeometryResult(parameterizedGeometry);
            return EXIT_FAILURE;
        }
        if (!expect(
                std::abs(lineLength(parameterizedGeometry->geometries[0]) - 8.5) <= 1e-9,
                "Expected parameterized Geometry result to apply the overridden length."
            )) {
            McSolverEngine_FreeGeometryResult(parameterizedGeometry);
            return EXIT_FAILURE;
        }
        McSolverEngine_FreeGeometryResult(parameterizedGeometry);
    }

    {
        ScopedTestTimer timer("SolveToBRepWithParameters");
        McSolverEngineBRepResult* parameterizedBrepResult = nullptr;
        const auto parameterizedBrepCode = McSolverEngine_SolveToBRepWithParameters(
            parameterizedDocumentXml.data(),
            "Sketch",
            parameterKeys,
            parameterValues,
            1,
            &parameterizedBrepResult
        );
        const std::string parameterizedBrepPayload =
            parameterizedBrepResult && parameterizedBrepResult->brepUtf8 ? parameterizedBrepResult->brepUtf8 : "";
        McSolverEngine_FreeBRepResult(parameterizedBrepResult);

#if MCSOLVERENGINE_WITH_OCCT
        if (!expect(
                parameterizedBrepCode == MCSOLVERENGINE_RESULT_SUCCESS,
                "Expected parameterized BRep C API call to succeed."
            )) {
            return EXIT_FAILURE;
        }
        if (!expect(
                contains(parameterizedBrepPayload, "CASCADE Topology V1"),
                "Expected parameterized BRep payload to contain serialized BREP text."
            )) {
            return EXIT_FAILURE;
        }
#else
        if (!expect(
                parameterizedBrepCode == MCSOLVERENGINE_RESULT_OPEN_CASCADE_UNAVAILABLE,
                "Expected parameterized BRep C API call to report missing OpenCascade."
            )) {
            return EXIT_FAILURE;
        }
        if (!expect(parameterizedBrepPayload.empty(), "Expected no-OCCT parameterized BRep payload to be empty.")) {
            return EXIT_FAILURE;
        }
#endif
    }

    constexpr std::string_view parameterizedAngleDocumentXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Document>
    <Objects Count="2">
        <Object type="App::VarSet" name="VarSet" id="1"/>
        <Object type="Sketcher::SketchObject" name="Sketch" id="2"/>
    </Objects>
    <ObjectData Count="2">
        <Object name="VarSet">
            <Properties Count="2" TransientCount="0">
                <Property name="Label" type="App::PropertyString">
                    <String value="Parameters"/>
                </Property>
                <Property name="Angle" type="App::PropertyQuantity">
                    <Quantity value="90 deg"/>
                </Property>
            </Properties>
        </Object>
        <Object name="Sketch">
            <Properties Count="3" TransientCount="0">
                <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                    <ConstraintList count="4">
                        <Constrain Name="" Type="7" Value="0.0" First="0" FirstPos="1" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="8" Value="0.0" First="0" FirstPos="1" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="6" Value="10.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="9" Value="0.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                    </ConstraintList>
                </Property>
                <Property name="ExpressionEngine" type="App::PropertyExpressionEngine">
                    <ExpressionEngine count="1">
                        <Expression path="Constraints[3]" expression="&lt;&lt;Parameters&gt;&gt;.Angle"/>
                    </ExpressionEngine>
                </Property>
                <Property name="Geometry" type="Part::PropertyGeometryList">
                    <GeometryList count="1">
                        <Geometry type="Part::GeomLineSegment" id="1" migrated="1">
                            <LineSegment StartX="1.0" StartY="2.0" StartZ="0.0" EndX="4.0" EndY="6.0" EndZ="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                    </GeometryList>
                </Property>
            </Properties>
        </Object>
    </ObjectData>
</Document>)";

    {
        ScopedTestTimer timer("SolveToGeometry angle default");
        McSolverEngineGeometryResult* defaultAngleGeometry = nullptr;
        const auto defaultAngleGeometryCode =
            McSolverEngine_SolveToGeometry(parameterizedAngleDocumentXml.data(), "Sketch", &defaultAngleGeometry);
        if (!expect(
                defaultAngleGeometryCode == MCSOLVERENGINE_RESULT_SUCCESS,
                "Expected default angle Geometry C API call to succeed."
            )) {
            return EXIT_FAILURE;
        }
        if (!expect(
                defaultAngleGeometry != nullptr && defaultAngleGeometry->geometryCount == 1,
                "Expected default angle Geometry result to contain one line."
            )) {
            McSolverEngine_FreeGeometryResult(defaultAngleGeometry);
            return EXIT_FAILURE;
        }
        if (!expect(
                defaultAngleGeometry->geometries[0].originalId > -99999999,
                "Expected default angle Geometry result to have originalId > -99999999."
            )) {
            McSolverEngine_FreeGeometryResult(defaultAngleGeometry);
            return EXIT_FAILURE;
        }
        if (!expect(
                std::abs(lineLength(defaultAngleGeometry->geometries[0]) - 10.0) <= 1e-9
                    && std::abs(lineAngleDegrees(defaultAngleGeometry->geometries[0]) - 90.0) <= 1e-9,
                "Expected default angle Geometry result to convert degree parameters before solving."
            )) {
            McSolverEngine_FreeGeometryResult(defaultAngleGeometry);
            return EXIT_FAILURE;
        }
        McSolverEngine_FreeGeometryResult(defaultAngleGeometry);
    }

    const char* angleParameterKeys[] = {"Angle"};
    const char* angleParameterValues[] = {"45"};
    {
        ScopedTestTimer timer("SolveToGeometryWithParameters angle override");
        McSolverEngineGeometryResult* overriddenAngleGeometry = nullptr;
        const auto overriddenAngleGeometryCode = McSolverEngine_SolveToGeometryWithParameters(
            parameterizedAngleDocumentXml.data(),
            "Sketch",
            angleParameterKeys,
            angleParameterValues,
            1,
            &overriddenAngleGeometry
        );
        if (!expect(
                overriddenAngleGeometryCode == MCSOLVERENGINE_RESULT_SUCCESS,
                "Expected overridden angle Geometry C API call to succeed."
            )) {
            return EXIT_FAILURE;
        }
        if (!expect(
                overriddenAngleGeometry != nullptr && overriddenAngleGeometry->geometryCount == 1
                    && overriddenAngleGeometry->geometries[0].originalId > -99999999,
                "Expected overridden angle Geometry result to have valid originalId."
            )) {
            McSolverEngine_FreeGeometryResult(overriddenAngleGeometry);
            return EXIT_FAILURE;
        }
        if (!expect(
                std::abs(lineLength(overriddenAngleGeometry->geometries[0]) - 10.0) <= 1e-9
                    && std::abs(lineAngleDegrees(overriddenAngleGeometry->geometries[0]) - 45.0) <= 1e-9,
                "Expected overridden angle Geometry result to interpret API values as degrees."
            )) {
            McSolverEngine_FreeGeometryResult(overriddenAngleGeometry);
            return EXIT_FAILURE;
        }
        McSolverEngine_FreeGeometryResult(overriddenAngleGeometry);
    }

    const char* invalidAngleParameterValues[] = {"45 deg"};
    {
        ScopedTestTimer timer("SolveToGeometryWithParameters invalid angle");
        McSolverEngineGeometryResult* invalidAngleGeometry = nullptr;
        const auto invalidAngleGeometryCode = McSolverEngine_SolveToGeometryWithParameters(
            parameterizedAngleDocumentXml.data(),
            "Sketch",
            angleParameterKeys,
            invalidAngleParameterValues,
            1,
            &invalidAngleGeometry
        );
        if (!expect(
                invalidAngleGeometryCode == MCSOLVERENGINE_RESULT_IMPORT_FAILED,
                "Expected non-numeric API parameter values to fail C API import."
            )) {
            return EXIT_FAILURE;
        }
        if (!expect(
                invalidAngleGeometry != nullptr
                    && invalidAngleGeometry->importStatus != nullptr
                    && std::string(invalidAngleGeometry->importStatus) == "Failed",
                "Expected invalid parameter C API result metadata to expose failed import status."
            )) {
            McSolverEngine_FreeGeometryResult(invalidAngleGeometry);
            return EXIT_FAILURE;
        }
        if (!expect(
                McSolverEngine_GetLastError() != nullptr && !std::string(McSolverEngine_GetLastError()).empty(),
                "Expected GetLastError non-empty after invalid angle parameter import failure."
            )) {
            McSolverEngine_FreeGeometryResult(invalidAngleGeometry);
            return EXIT_FAILURE;
        }
        std::cout << "    got expected GetLastError: " << McSolverEngine_GetLastError() << '\n';
        McSolverEngine_FreeGeometryResult(invalidAngleGeometry);
    }

    constexpr std::string_view evaluatedVarSetPropertiesDocumentXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Document>
    <Objects Count="2">
        <Object type="App::VarSet" name="VarSet001" id="1"/>
        <Object type="Sketcher::SketchObject" name="Sketch" id="2"/>
    </Objects>
    <ObjectData Count="2">
        <Object name="VarSet001">
            <Properties Count="10" TransientCount="0">
                <Property name="Label" type="App::PropertyString">
                    <String value="Parameters"/>
                </Property>
                <Property name="Base" type="App::PropertyFloat">
                    <Float value="4.0"/>
                </Property>
                <Property name="Length" type="App::PropertyQuantity">
                    <Quantity value="1 m"/>
                </Property>
                <Property name="Angle" type="App::PropertyQuantity">
                    <Quantity value="1.5707963267948966 rad"/>
                </Property>
                <Property name="Name" type="App::PropertyString">
                    <String value="widget"/>
                </Property>
                <Property name="Visible" type="App::PropertyBool">
                    <Bool value="true"/>
                </Property>
                <Property name="DoubleBase" type="App::PropertyFloat">
                    <Float value="0.0"/>
                </Property>
                <Property name="Width" type="App::PropertyFloat">
                    <Float value="0.0"/>
                </Property>
                <Property name="Area" type="App::PropertyFloat">
                    <Float value="0.0"/>
                </Property>
                <Property name="ExpressionEngine" type="App::PropertyExpressionEngine">
                    <ExpressionEngine count="3">
                        <Expression path="DoubleBase" expression="Base * 2"/>
                        <Expression path="Width" expression="DoubleBase + 1"/>
                        <Expression path="Area" expression="Length * Length"/>
                    </ExpressionEngine>
                </Property>
            </Properties>
        </Object>
        <Object name="Sketch">
            <Properties Count="3" TransientCount="0">
                <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                    <ConstraintList count="2">
                        <Constrain Name="" Type="2" Value="0.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                        <Constrain Name="" Type="6" Value="0.0" First="0" FirstPos="0" Second="-2000" SecondPos="0" Third="-2000" ThirdPos="0" LabelDistance="10.0" LabelPosition="0.0" IsDriving="1" IsInVirtualSpace="0" IsActive="1" />
                    </ConstraintList>
                </Property>
                <Property name="ExpressionEngine" type="App::PropertyExpressionEngine">
                    <ExpressionEngine count="1">
                        <Expression path="Constraints[1]" expression="&lt;&lt;Parameters&gt;&gt;.Width"/>
                    </ExpressionEngine>
                </Property>
                <Property name="Geometry" type="Part::PropertyGeometryList">
                    <GeometryList count="1">
                        <Geometry type="Part::GeomLineSegment" id="1" migrated="1">
                            <LineSegment StartX="0.0" StartY="0.0" StartZ="0.0" EndX="3.0" EndY="1.0" EndZ="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                    </GeometryList>
                </Property>
            </Properties>
        </Object>
    </ObjectData>
</Document>)";

    const char* evaluatedVarSetParameterKeys[] = {"Parameters.Base"};
    const char* evaluatedVarSetParameterValues[] = {"6"};

    {
        ScopedTestTimer timer("SolveToGeometry evaluated VarSet properties");
        McSolverEngineGeometryResult* geometryResult = nullptr;
        const auto geometryCode = McSolverEngine_SolveToGeometry(
            evaluatedVarSetPropertiesDocumentXml.data(),
            "Sketch",
            &geometryResult
        );
        if (!expect(geometryCode == MCSOLVERENGINE_RESULT_SUCCESS, "Expected Geometry call with evaluated VarSet properties to succeed.")
            || !expect(geometryResult != nullptr, "Expected Geometry result pointer for evaluated VarSet properties.")
            || !expect(geometryResult->varSetPropertyCount == 9, "Expected Geometry result to expose all scalar VarSet properties.")
            || !expectVarSetProperty(geometryResult->varSetProperties, geometryResult->varSetPropertyCount, "VarSet001.Label", "Parameters", "")
            || !expectVarSetProperty(geometryResult->varSetProperties, geometryResult->varSetPropertyCount, "VarSet001.Base", "4", "")
            || !expectVarSetProperty(geometryResult->varSetProperties, geometryResult->varSetPropertyCount, "VarSet001.Length", "1000", "mm")
            || !expectVarSetProperty(geometryResult->varSetProperties, geometryResult->varSetPropertyCount, "VarSet001.Angle", "90", "deg")
            || !expectVarSetProperty(geometryResult->varSetProperties, geometryResult->varSetPropertyCount, "VarSet001.Name", "widget", "")
            || !expectVarSetProperty(geometryResult->varSetProperties, geometryResult->varSetPropertyCount, "VarSet001.Visible", "true", "")
            || !expectVarSetProperty(geometryResult->varSetProperties, geometryResult->varSetPropertyCount, "VarSet001.Area", "1000000", "mm^2")) {
            McSolverEngine_FreeGeometryResult(geometryResult);
            return EXIT_FAILURE;
        }
        McSolverEngine_FreeGeometryResult(geometryResult);
    }

    {
        ScopedTestTimer timer("SolveToGeometryWithParameters evaluated VarSet properties");
        McSolverEngineGeometryResult* geometryResult = nullptr;
        const auto geometryCode = McSolverEngine_SolveToGeometryWithParameters(
            evaluatedVarSetPropertiesDocumentXml.data(),
            "Sketch",
            evaluatedVarSetParameterKeys,
            evaluatedVarSetParameterValues,
            1,
            &geometryResult
        );
        if (!expect(geometryCode == MCSOLVERENGINE_RESULT_SUCCESS, "Expected parameterized Geometry call with evaluated VarSet properties to succeed.")
            || !expect(geometryResult != nullptr, "Expected parameterized Geometry result pointer for evaluated VarSet properties.")
            || !expectVarSetProperty(geometryResult->varSetProperties, geometryResult->varSetPropertyCount, "VarSet001.Base", "6", "")
            || !expectVarSetProperty(geometryResult->varSetProperties, geometryResult->varSetPropertyCount, "VarSet001.DoubleBase", "12", "")
            || !expectVarSetProperty(geometryResult->varSetProperties, geometryResult->varSetPropertyCount, "VarSet001.Width", "13", "")) {
            McSolverEngine_FreeGeometryResult(geometryResult);
            return EXIT_FAILURE;
        }
        McSolverEngine_FreeGeometryResult(geometryResult);
    }

    {
        ScopedTestTimer timer("SolveToBRep evaluated VarSet properties");
        McSolverEngineBRepResult* brepResult = nullptr;
        const auto brepCode = McSolverEngine_SolveToBRep(
            evaluatedVarSetPropertiesDocumentXml.data(),
            "Sketch",
            &brepResult
        );
        if (!expect(
                brepCode == MCSOLVERENGINE_RESULT_SUCCESS
                    || brepCode == MCSOLVERENGINE_RESULT_OPEN_CASCADE_UNAVAILABLE,
                "Expected BRep call with evaluated VarSet properties to succeed or report missing OCCT."
            )
            || !expect(brepResult != nullptr, "Expected BRep result pointer for evaluated VarSet properties.")
            || !expectVarSetProperty(brepResult->varSetProperties, brepResult->varSetPropertyCount, "VarSet001.Width", "9", "")
            || !expectVarSetProperty(brepResult->varSetProperties, brepResult->varSetPropertyCount, "VarSet001.Area", "1000000", "mm^2")) {
            McSolverEngine_FreeBRepResult(brepResult);
            return EXIT_FAILURE;
        }
        McSolverEngine_FreeBRepResult(brepResult);
    }

    {
        ScopedTestTimer timer("SolveToBRepWithParameters evaluated VarSet properties");
        McSolverEngineBRepResult* brepResult = nullptr;
        const auto brepCode = McSolverEngine_SolveToBRepWithParameters(
            evaluatedVarSetPropertiesDocumentXml.data(),
            "Sketch",
            evaluatedVarSetParameterKeys,
            evaluatedVarSetParameterValues,
            1,
            &brepResult
        );
        if (!expect(
                brepCode == MCSOLVERENGINE_RESULT_SUCCESS
                    || brepCode == MCSOLVERENGINE_RESULT_OPEN_CASCADE_UNAVAILABLE,
                "Expected parameterized BRep call with evaluated VarSet properties to succeed or report missing OCCT."
            )
            || !expect(brepResult != nullptr, "Expected parameterized BRep result pointer for evaluated VarSet properties.")
            || !expectVarSetProperty(brepResult->varSetProperties, brepResult->varSetPropertyCount, "VarSet001.Base", "6", "")
            || !expectVarSetProperty(brepResult->varSetProperties, brepResult->varSetPropertyCount, "VarSet001.Width", "13", "")) {
            McSolverEngine_FreeBRepResult(brepResult);
            return EXIT_FAILURE;
        }
        McSolverEngine_FreeBRepResult(brepResult);
    }

    constexpr std::string_view unsupportedVarSetExpressionDocumentXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Document>
    <Objects Count="2">
        <Object type="App::VarSet" name="VarSet" id="1"/>
        <Object type="Sketcher::SketchObject" name="Sketch" id="2"/>
    </Objects>
    <ObjectData Count="2">
        <Object name="VarSet">
            <Properties Count="2" TransientCount="0">
                <Property name="Width" type="App::PropertyFloat">
                    <Float value="0.0"/>
                </Property>
                <Property name="ExpressionEngine" type="App::PropertyExpressionEngine">
                    <ExpressionEngine count="1">
                        <Expression path="Width" expression="Spreadsheet.Width + 1"/>
                    </ExpressionEngine>
                </Property>
            </Properties>
        </Object>
        <Object name="Sketch">
            <Properties Count="2" TransientCount="0">
                <Property name="Constraints" type="Sketcher::PropertyConstraintList">
                    <ConstraintList count="0">
                    </ConstraintList>
                </Property>
                <Property name="Geometry" type="Part::PropertyGeometryList">
                    <GeometryList count="1">
                        <Geometry type="Part::GeomLineSegment" id="1" migrated="1">
                            <LineSegment StartX="0.0" StartY="0.0" StartZ="0.0" EndX="1.0" EndY="0.0" EndZ="0.0"/>
                            <Construction value="0"/>
                        </Geometry>
                    </GeometryList>
                </Property>
            </Properties>
        </Object>
    </ObjectData>
</Document>)";
    {
        ScopedTestTimer timer("SolveToGeometry unsupported VarSet expression subset");
        McSolverEngineGeometryResult* unsupportedVarSetExpressionGeometry = nullptr;
        const auto unsupportedVarSetExpressionCode = McSolverEngine_SolveToGeometry(
            unsupportedVarSetExpressionDocumentXml.data(),
            "Sketch",
            &unsupportedVarSetExpressionGeometry
        );
        if (!expect(
                unsupportedVarSetExpressionCode
                    == MCSOLVERENGINE_RESULT_VARSET_EXPRESSION_UNSUPPORTED_SUBSET,
                "Expected non-VarSet expression references to return the dedicated C API error code."
            )) {
            return EXIT_FAILURE;
        }
        if (!expect(
                unsupportedVarSetExpressionGeometry != nullptr
                    && unsupportedVarSetExpressionGeometry->messageCount == 1
                    && unsupportedVarSetExpressionGeometry->messages != nullptr
                    && unsupportedVarSetExpressionGeometry->messages[0] != nullptr
                    && contains(
                        unsupportedVarSetExpressionGeometry->messages[0],
                        "MCSOLVERENGINE_IMPORT_VARSET_EXPRESSION_UNSUPPORTED_SUBSET"
                    ),
                "Expected unsupported VarSet expression result metadata to include the import error code."
            )) {
            McSolverEngine_FreeGeometryResult(unsupportedVarSetExpressionGeometry);
            return EXIT_FAILURE;
        }
        McSolverEngine_FreeGeometryResult(unsupportedVarSetExpressionGeometry);
    }

    {
        ScopedTestTimer timer("SolveToGeometry constraint expression V102.6");
        const auto v1026Xml = readTextFile(std::string(MCSOLVERENGINE_SOURCE_DIR) + "/fcstdDoc/V102.6.xml");
        if (!expect(!v1026Xml.empty(), "Expected V102.6.xml to load.")) {
            return EXIT_FAILURE;
        }

        McSolverEngineGeometryResult* v1026Result = nullptr;
        const auto v1026Code = McSolverEngine_SolveToGeometry(v1026Xml.c_str(), "Sketch", &v1026Result);
        if (!expect(v1026Code == MCSOLVERENGINE_RESULT_SUCCESS, "Expected V102.6 to succeed.")) {
            return EXIT_FAILURE;
        }
        if (!expect(v1026Result != nullptr, "Expected V102.6 result pointer.")) {
            return EXIT_FAILURE;
        }

        // V102.6 has 4 expression-driven constraint refs:
        //   originalId=2:  Constraint[2] Angle(kind=9) with "VarSet.R1"
        //   originalId=2:  Constraint[11] Distance(kind=5) with "VarSet001.V2L2"
        //   originalId=-1: Constraint[2] Angle(kind=9) with "VarSet.R1" (external geometry)
        //   originalId=6:  Constraint[15] Radius(kind=10) with "VarSet.L1 * 3"
        int exprCount = 0;
        bool foundAngleR1_geo2 = false;
        bool foundDistV2L2 = false;
        bool foundAngleR1_ext = false;
        bool foundRadiusL1 = false;
        for (int i = 0; i < v1026Result->geometryCount; ++i) {
            const auto& record = v1026Result->geometries[i];
            exprCount += record.constraintCount;
            for (int j = 0; j < record.constraintCount; ++j) {
                const auto& ref = record.constraints[j];
                if (record.originalId == 2 && ref.kind == MCSOLVERENGINE_CONSTRAINT_ANGLE
                    && ref.originalIndex == 2 && ref.expression != nullptr && std::string(ref.expression) == "VarSet.R1") {
                    foundAngleR1_geo2 = true;
                }
                if (record.originalId == 2 && ref.kind == MCSOLVERENGINE_CONSTRAINT_DISTANCE
                    && ref.originalIndex == 11 && ref.expression != nullptr && std::string(ref.expression) == "VarSet001.V2L2") {
                    foundDistV2L2 = true;
                }
                if (record.originalId == -1 && ref.kind == MCSOLVERENGINE_CONSTRAINT_ANGLE
                    && ref.originalIndex == 2 && ref.expression != nullptr && std::string(ref.expression) == "VarSet.R1") {
                    foundAngleR1_ext = true;
                }
                if (record.originalId == 6 && ref.kind == MCSOLVERENGINE_CONSTRAINT_RADIUS
                    && ref.originalIndex == 15 && ref.expression != nullptr && std::string(ref.expression) == "VarSet.L1 * 3") {
                    foundRadiusL1 = true;
                }
            }
        }

        if (!expect(exprCount == 4,
                    (std::string("V102.6: expected 4 expression refs, got ") + std::to_string(exprCount)).c_str())) {
            McSolverEngine_FreeGeometryResult(v1026Result);
            return EXIT_FAILURE;
        }
        if (!expect(foundAngleR1_geo2, "V102.6: expected originalId=2 Constraint[2] Angle with VarSet.R1.")) {
            McSolverEngine_FreeGeometryResult(v1026Result);
            return EXIT_FAILURE;
        }
        if (!expect(foundDistV2L2, "V102.6: expected originalId=2 Constraint[11] Distance with VarSet001.V2L2.")) {
            McSolverEngine_FreeGeometryResult(v1026Result);
            return EXIT_FAILURE;
        }
        if (!expect(foundAngleR1_ext, "V102.6: expected originalId=-1 Constraint[2] Angle with VarSet.R1.")) {
            McSolverEngine_FreeGeometryResult(v1026Result);
            return EXIT_FAILURE;
        }
        if (!expect(foundRadiusL1, "V102.6: expected originalId=6 Constraint[15] Radius with VarSet.L1 * 3.")) {
            McSolverEngine_FreeGeometryResult(v1026Result);
            return EXIT_FAILURE;
        }

        McSolverEngine_FreeGeometryResult(v1026Result);
    }

    {
        ScopedTestTimer timer("SolveToGeometry constraint expression V102.5");
        const auto v1025Xml = readTextFile(std::string(MCSOLVERENGINE_SOURCE_DIR) + "/fcstdDoc/V102.5.xml");
        if (!expect(!v1025Xml.empty(), "Expected V102.5.xml to load.")) {
            return EXIT_FAILURE;
        }

        McSolverEngineGeometryResult* v1025Result = nullptr;
        const auto v1025Code = McSolverEngine_SolveToGeometry(v1025Xml.c_str(), "Sketch", &v1025Result);
        if (!expect(v1025Code == MCSOLVERENGINE_RESULT_SUCCESS, "Expected V102.5 to succeed.")) {
            return EXIT_FAILURE;
        }
        if (!expect(v1025Result != nullptr, "Expected V102.5 result pointer.")) {
            return EXIT_FAILURE;
        }

        // Only expression-driven constraints are exported.
        // V102.5 has one expression: <<VarSet>>.R1 on the Diameter constraint at originalId=17.
        int exprCount = 0;
        bool foundExpected = false;
        for (int i = 0; i < v1025Result->geometryCount; ++i) {
            const auto& record = v1025Result->geometries[i];
            exprCount += record.constraintCount;
            for (int j = 0; j < record.constraintCount; ++j) {
                if (record.originalId == 17
                    && record.constraints[j].kind == MCSOLVERENGINE_CONSTRAINT_DIAMETER
                    && record.constraints[j].expression != nullptr
                    && std::string(record.constraints[j].expression) == "<<VarSet>>.R1") {
                    foundExpected = true;
                }
            }
        }

        if (!expect(exprCount == 1,
                    (std::string("Expected 1 expression-driven constraint total, got ") + std::to_string(exprCount)).c_str())) {
            McSolverEngine_FreeGeometryResult(v1025Result);
            return EXIT_FAILURE;
        }
        if (!expect(foundExpected,
                    "Expected originalId=17 to have Diameter with expression <<VarSet>>.R1.")) {
            McSolverEngine_FreeGeometryResult(v1025Result);
            return EXIT_FAILURE;
        }

        McSolverEngine_FreeGeometryResult(v1025Result);
    }

    {
        ScopedTestTimer timer("SolveToGeometry invalid XML");
        McSolverEngineGeometryResult* result = nullptr;
        const auto code = McSolverEngine_SolveToGeometry("not valid xml", "Sketch", &result);
        if (!expect(
                code == MCSOLVERENGINE_RESULT_IMPORT_FAILED,
                "Expected invalid XML to fail with IMPORT_FAILED."
            )) {
            McSolverEngine_FreeGeometryResult(result);
            return EXIT_FAILURE;
        }
        if (!expect(
                result != nullptr && result->importStatus != nullptr
                    && std::string(result->importStatus) == "Failed",
                "Expected invalid XML result to expose failed import status."
            )) {
            McSolverEngine_FreeGeometryResult(result);
            return EXIT_FAILURE;
        }
        if (!expect(
                McSolverEngine_GetLastError() != nullptr && !std::string(McSolverEngine_GetLastError()).empty(),
                "Expected GetLastError non-empty after invalid XML import failure."
            )) {
            McSolverEngine_FreeGeometryResult(result);
            return EXIT_FAILURE;
        }
        std::cout << "    got expected GetLastError: " << McSolverEngine_GetLastError() << '\n';
        McSolverEngine_FreeGeometryResult(result);
    }

    {
        ScopedTestTimer timer("SolveToGeometryWithParameters non-existent sketch name");
        const auto v1024Xml =
            readTextFile(std::string(MCSOLVERENGINE_SOURCE_DIR) + "/fcstdDoc/V102.4.xml");
        if (!expect(!v1024Xml.empty(), "Expected V102.4.xml to load.")) {
            return EXIT_FAILURE;
        }

        McSolverEngineGeometryResult* result = nullptr;
        const auto code =
            McSolverEngine_SolveToGeometry(v1024Xml.c_str(), "NonExistentSketch", &result);
        if (!expect(
                code == MCSOLVERENGINE_RESULT_IMPORT_FAILED,
                "Expected non-existent sketch name to fail with IMPORT_FAILED."
            )) {
            McSolverEngine_FreeGeometryResult(result);
            return EXIT_FAILURE;
        }
        if (!expect(
                result != nullptr && result->importStatus != nullptr
                    && std::string(result->importStatus) == "Failed",
                "Expected non-existent sketch result to expose failed import status."
            )) {
            McSolverEngine_FreeGeometryResult(result);
            return EXIT_FAILURE;
        }
        if (!expect(
                McSolverEngine_GetLastError() != nullptr && !std::string(McSolverEngine_GetLastError()).empty(),
                "Expected GetLastError non-empty after missing sketch import failure."
            )) {
            McSolverEngine_FreeGeometryResult(result);
            return EXIT_FAILURE;
        }
        std::cout << "    got expected GetLastError: " << McSolverEngine_GetLastError() << '\n';
        McSolverEngine_FreeGeometryResult(result);
    }

    {
        ScopedTestTimer timer("SolveToGeometryWithParameters non-existent parameter key");
        const auto v1024Xml =
            readTextFile(std::string(MCSOLVERENGINE_SOURCE_DIR) + "/fcstdDoc/V102.4.xml");
        if (!expect(!v1024Xml.empty(), "Expected V102.4.xml to load.")) {
            return EXIT_FAILURE;
        }

        const char* keys[] = {"NonExistentParam"};
        const char* values[] = {"42.0"};
        McSolverEngineGeometryResult* result = nullptr;
        const auto code = McSolverEngine_SolveToGeometryWithParameters(
            v1024Xml.c_str(), "Sketch", keys, values, 1, &result);
        if (!expect(
                code == MCSOLVERENGINE_RESULT_IMPORT_FAILED,
                "Expected non-existent parameter key to fail with IMPORT_FAILED."
            )) {
            McSolverEngine_FreeGeometryResult(result);
            return EXIT_FAILURE;
        }
        if (!expect(
                result != nullptr && result->importStatus != nullptr
                    && std::string(result->importStatus) == "Failed",
                "Expected non-existent parameter result to expose failed import status."
            )) {
            McSolverEngine_FreeGeometryResult(result);
            return EXIT_FAILURE;
        }
        if (!expect(
                McSolverEngine_GetLastError() != nullptr && !std::string(McSolverEngine_GetLastError()).empty(),
                "Expected GetLastError non-empty after unknown parameter import failure."
            )) {
            McSolverEngine_FreeGeometryResult(result);
            return EXIT_FAILURE;
        }
        std::cout << "    got expected GetLastError: " << McSolverEngine_GetLastError() << '\n';
        McSolverEngine_FreeGeometryResult(result);
    }

    {
        ScopedTestTimer timer("MultiThreaded SolveToGeometryWithParameters V102.4 (8 threads)");
        const auto v1024Xml =
            readTextFile(std::string(MCSOLVERENGINE_SOURCE_DIR) + "/fcstdDoc/V102.4.xml");
        if (!expect(!v1024Xml.empty(), "Expected V102.4.xml to load.")) {
            return EXIT_FAILURE;
        }

        constexpr int threadCount = 8;
        std::vector<std::thread> threads;
        threads.reserve(threadCount);
        std::atomic<int> okCount{0};
        std::atomic<int> failCount{0};

        for (int i = 0; i < threadCount; ++i) {
            threads.emplace_back([&, i]() {
                const double l1 = 40.0 + i * 5.0;
                const auto l1Str = std::to_string(l1);
                const char* keys[] = {"VarSet.L1"};
                const char* values[] = {l1Str.c_str()};

                McSolverEngineGeometryResult* result = nullptr;
                const auto code = McSolverEngine_SolveToGeometryWithParameters(
                    v1024Xml.c_str(), "Sketch", keys, values, 1, &result);
                if (code == MCSOLVERENGINE_RESULT_SUCCESS && result != nullptr) {
                    okCount.fetch_add(1, std::memory_order_relaxed);
                }
                else {
                    failCount.fetch_add(1, std::memory_order_relaxed);
                }
                McSolverEngine_FreeGeometryResult(result);
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        if (!expect(
                okCount.load() == threadCount,
                (std::string("Expected all ") + std::to_string(threadCount)
                 + " threads to succeed, failures: " + std::to_string(failCount.load()))
                    .c_str()
            )) {
            return EXIT_FAILURE;
        }
    }

    {
        ScopedTestTimer timer("ExtractFCStdDoc round-trip (all .FCStd files)");
        const std::string docDir = std::string(MCSOLVERENGINE_SOURCE_DIR) + "/fcstdDoc";
        const char* names[] = {
            "1", "2", "3",
            "V102.1", "V102.2", "V102.4", "V102.4.plus1", "V102.5", "V102.6"
        };
        constexpr int count = sizeof(names) / sizeof(names[0]);

        for (int i = 0; i < count; ++i) {
            const auto fcstdPath = docDir + "/" + names[i] + ".FCStd";
            const auto xmlPath = docDir + "/" + names[i] + ".xml";
            const auto expectedXml = readTextFile(xmlPath);
            if (!expect(
                    !expectedXml.empty(),
                    (std::string("Expected ") + xmlPath + " to load.").c_str()
                )) {
                return EXIT_FAILURE;
            }

            char* extracted = nullptr;
            const auto code = McSolverEngine_ExtractFCStdDoc(fcstdPath.c_str(), &extracted);
            if (!expect(
                    code == MCSOLVERENGINE_FCSTD_SUCCESS,
                    (std::string("Expected ExtractFCStdDoc to succeed for ") + fcstdPath
                     + ". Error: " + McSolverEngine_GetLastError())
                        .c_str()
                )) {
                McSolverEngine_FreeFCStdDoc(extracted);
                return EXIT_FAILURE;
            }

            const std::string extractedStr(extracted ? extracted : "");
            McSolverEngine_FreeFCStdDoc(extracted);

            if (!expect(
                    extractedStr == expectedXml,
                    (std::string("Expected extracted Document.xml to match ") + xmlPath
                     + " (got " + std::to_string(extractedStr.size()) + " bytes, expected "
                     + std::to_string(expectedXml.size()) + " bytes)")
                        .c_str()
                )) {
                return EXIT_FAILURE;
            }
        }
    }

    // ── C API null-argument guards ─────────────────────────────────────────────

    {
        // Null document XML → INVALID_ARGUMENT (SolveToGeometry)
        McSolverEngineGeometryResult* geoResult = nullptr;
        const auto codeNullDoc = McSolverEngine_SolveToGeometry(nullptr, "Sketch", &geoResult);
        if (!expect(codeNullDoc == MCSOLVERENGINE_RESULT_INVALID_ARGUMENT, "C API: null document → INVALID_ARGUMENT"))
            return EXIT_FAILURE;
        if (!expect(geoResult == nullptr, "C API: null document → result pointer set to null"))
            return EXIT_FAILURE;

        // Null result pointer → INVALID_ARGUMENT (SolveToGeometry)
        const auto codeNullResult = McSolverEngine_SolveToGeometry("<doc/>", "Sketch", nullptr);
        if (!expect(codeNullResult == MCSOLVERENGINE_RESULT_INVALID_ARGUMENT, "C API: null result pointer → INVALID_ARGUMENT"))
            return EXIT_FAILURE;

        // Null document XML → INVALID_ARGUMENT (SolveToBRep)
        McSolverEngineBRepResult* brepResult = nullptr;
        const auto codeNullBrepDoc = McSolverEngine_SolveToBRep(nullptr, "Sketch", &brepResult);
        if (!expect(codeNullBrepDoc == MCSOLVERENGINE_RESULT_INVALID_ARGUMENT, "C API BRep: null document → INVALID_ARGUMENT"))
            return EXIT_FAILURE;
        if (!expect(brepResult == nullptr, "C API BRep: null document → result pointer set to null"))
            return EXIT_FAILURE;

        // Null result pointer → INVALID_ARGUMENT (SolveToBRep)
        const auto codeNullBrepResult = McSolverEngine_SolveToBRep("<doc/>", "Sketch", nullptr);
        if (!expect(codeNullBrepResult == MCSOLVERENGINE_RESULT_INVALID_ARGUMENT, "C API BRep: null result pointer → INVALID_ARGUMENT"))
            return EXIT_FAILURE;

        McSolverEngineDocumentInfo* inspectResult = nullptr;
        const auto codeNullInspectDoc = McSolverEngine_InspectDocumentXml(nullptr, &inspectResult);
        if (!expect(codeNullInspectDoc == MCSOLVERENGINE_RESULT_INVALID_ARGUMENT, "C API Inspect: null document → INVALID_ARGUMENT"))
            return EXIT_FAILURE;
        if (!expect(inspectResult == nullptr, "C API Inspect: null document → result pointer set to null"))
            return EXIT_FAILURE;

        const auto codeNullInspectResult = McSolverEngine_InspectDocumentXml("<doc/>", nullptr);
        if (!expect(codeNullInspectResult == MCSOLVERENGINE_RESULT_INVALID_ARGUMENT, "C API Inspect: null result pointer → INVALID_ARGUMENT"))
            return EXIT_FAILURE;

        // Negative parameter count → INVALID_ARGUMENT (SolveToGeometryWithParameters)
        const char* keys[] = {"K"};
        const char* vals[] = {"1"};
        McSolverEngineGeometryResult* paramResult = nullptr;
        const auto codeNegCount = McSolverEngine_SolveToGeometryWithParameters(
            "<doc/>", "Sketch", keys, vals, -1, &paramResult);
        if (!expect(codeNegCount == MCSOLVERENGINE_RESULT_INVALID_ARGUMENT, "C API: negative parameter count → INVALID_ARGUMENT"))
            return EXIT_FAILURE;
        if (!expect(paramResult == nullptr, "C API: negative parameter count → result pointer set to null"))
            return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
