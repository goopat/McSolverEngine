// Debug test for V111.T003.FCStd with VarSet.a_of_arc parameter override.
//
// Reproduce: default solve (a_of_arc=50°) succeeds, VarSet.a_of_arc=60° fails with
// SOLVE_FAILED. This test exercises a range of values and prints diagnostics to
// isolate the failure.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "McSolverEngine/CompatModel.h"
#include "McSolverEngine/CompatSolver.h"
#include "McSolverEngine/DocumentXml.h"
#include "McSolverEngine/Engine.h"
#include "McSolverEngine/GeometryExport.h"
#include "McSolverEngine/ZipExtract.h"

namespace
{

using McSolverEngine::ParameterMap;

bool expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

struct TestResult
{
    std::string label;
    bool importOk {false};
    bool solveOk {false};
    int geometryCount {0};
    double startAngle {0.0};
    double endAngle {0.0};
    double radius {0.0};
    std::vector<std::string> messages;
};

TestResult runSolve(
    const std::string& xml,
    const std::string& label,
    const ParameterMap& parameters)
{
    TestResult result;
    result.label = label;

    auto imported = McSolverEngine::DocumentXml::importSketchFromDocumentXml(
        xml, parameters, "Sketch");

    result.messages = imported.messages;
    result.importOk = imported.imported()
        && imported.status != McSolverEngine::DocumentXml::ImportStatus::Failed;

    if (!result.importOk) {
        return result;
    }

    auto solveResult = McSolverEngine::Compat::solveSketch(imported.model, parameters);
    result.solveOk = solveResult.solved();

    if (result.solveOk) {
        auto geoResult = McSolverEngine::Geometry::exportSketchGeometry(imported.model);
        result.geometryCount = static_cast<int>(geoResult.geometries.size());

        for (const auto& g : geoResult.geometries) {
            if (auto* arc = std::get_if<McSolverEngine::Compat::ArcGeometry>(&g.geometry.data)) {
                result.startAngle = arc->startAngle;
                result.endAngle = arc->endAngle;
                result.radius = arc->radius;
            }
        }
    }

    return result;
}

void printResult(const TestResult& r)
{
    std::cout << "  " << r.label << ":\n"
              << "    import: " << (r.importOk ? "OK" : "FAILED") << '\n'
              << "    solve:  " << (r.solveOk ? "OK" : "FAILED") << '\n';

    if (!r.messages.empty()) {
        for (const auto& m : r.messages) {
            std::cout << "    message: " << m << '\n';
        }
    }

    if (r.solveOk) {
        std::cout << "    geometryCount: " << r.geometryCount << '\n'
                  << "    arc radius:    " << r.radius << '\n'
                  << "    arc startAngle: " << r.startAngle
                  << " (" << r.startAngle * 180.0 / std::numbers::pi << "°)\n"
                  << "    arc endAngle:   " << r.endAngle
                  << " (" << r.endAngle * 180.0 / std::numbers::pi << "°)\n"
                  << "    arc span:       " << (r.endAngle - r.startAngle)
                  << " (" << (r.endAngle - r.startAngle) * 180.0 / std::numbers::pi << "°)\n";
    }
    std::cout << std::endl;
}

}  // namespace

int main()
{
    const std::string sourceDir {MCSOLVERENGINE_SOURCE_DIR};
    const std::string fcstdPath = sourceDir + "/fcstdDoc/V111.T003.FCStd";

    std::cout << "McSolverEngine version: " << McSolverEngine::Engine::version() << '\n';
    std::cout << "FCStd path: " << fcstdPath << "\n\n";

    // ── Extract Document.xml from FCStd ──────────────────────────
    auto extractResult = McSolverEngine::ZipExtract::extractDocumentXml(fcstdPath);
    if (!expect(extractResult.success, "Extract Document.xml from V111.T003.FCStd")) {
        std::cerr << "  Error: " << extractResult.errorMessage << '\n';
        return EXIT_FAILURE;
    }

    const std::string xml(
        extractResult.documentXml.get(),
        extractResult.documentXmlSize);
    std::cout << "Extracted Document.xml: " << xml.size() << " bytes\n\n";

    // ── Inspect the document (read-only, no solver) ──────────────
    {
        auto inspect = McSolverEngine::DocumentXml::inspectDocumentXml(xml);
        if (inspect.succeeded()) {
            std::cout << "=== Inspect Document ===\n"
                      << "  Sketches: " << inspect.sketches.size() << '\n'
                      << "  VarSets:  " << inspect.varSets.size() << '\n';

            for (const auto& vs : inspect.varSets) {
                std::cout << "  VarSet \"" << vs.objectName
                          << "\" (label=\"" << vs.label << "\"):\n";
                for (const auto& p : vs.parameters) {
                    std::cout << "    - " << p.name
                              << "  type=" << p.type
                              << "  rawValue=\"" << p.rawValue << "\""
                              << "  expr=\"" << p.expression << "\"\n";
                }
            }

            for (const auto& sk : inspect.sketches) {
                std::cout << "  Sketch \"" << sk.objectName
                          << "\" (label=\"" << sk.label << "\"):\n"
                          << "    geometries: " << sk.geometries.size() << '\n'
                          << "    constraints: " << sk.constraints.size() << '\n';

                for (const auto& g : sk.geometries) {
                    std::cout << "    geo[" << g.index << "] id=" << g.originalId
                              << " type=" << g.type
                              << " construction=" << g.construction
                              << " external=" << g.external
                              << " constraintIndices=[";
                    for (std::size_t ci = 0; ci < g.constraintIndices.size(); ++ci) {
                        if (ci > 0) std::cout << ", ";
                        std::cout << g.constraintIndices[ci];
                    }
                    std::cout << "]\n";
                }

                for (const auto& c : sk.constraints) {
                    std::cout << "    constraint[" << c.originalIndex
                              << "] type=" << c.type
                              << " kind=" << c.kind
                              << " driving=" << c.driving
                              << " value=" << c.value
                              << " refGeoIds=[";
                    for (std::size_t ri = 0; ri < c.referencedGeoIds.size(); ++ri) {
                        if (ri > 0) std::cout << ", ";
                        std::cout << c.referencedGeoIds[ri];
                    }
                    std::cout << "]\n";
                }
            }
            std::cout << '\n';
        } else {
            std::cout << "Inspect failed:\n";
            for (const auto& m : inspect.messages) {
                std::cout << "  " << m << '\n';
            }
        }
    }

    // ── Default solve (no parameter override) ────────────────────
    std::cout << "=== Default solve (no parameters) ===\n";
    {
        auto r = runSolve(xml, "default", ParameterMap{});
        printResult(r);

        if (!r.solveOk) {
            std::cerr << "ERROR: default solve must succeed before testing overrides.\n";
            return EXIT_FAILURE;
        }
    }

    // ── Constraint value diagnostics ────────────────────────────
    std::cout << "=== Constraint value diagnostics ===\n";

    for (int testVal : {50, 60}) {
        ParameterMap params;
        if (testVal != 50) {
            params = ParameterMap{{"VarSet.a_of_arc", std::to_string(testVal)}};
        }

        auto imported = McSolverEngine::DocumentXml::importSketchFromDocumentXml(
            xml, params, "Sketch");

        std::cout << "  a_of_arc=" << testVal << "°:\n";
        std::cout << "    import: " << (imported.imported() ? "OK" : "FAILED") << '\n';

        const auto& constraints = imported.model.constraints();
        for (std::size_t ci = 0; ci < constraints.size(); ++ci) {
            const auto& c = constraints[ci];
            std::cout << "    constraint[" << ci << "]: kind="
                      << static_cast<int>(c.kind)
                      << " driving=" << c.driving
                      << " value=" << c.value;
            if (c.hasParameterDefaultValue) {
                std::cout << " hasParamDefault=true"
                          << " paramDefault=" << c.parameterDefaultValue
                          << " (" << c.parameterDefaultValue * 180.0 / std::numbers::pi << " deg)";
            }
            if (!c.parameterKey.empty()) {
                std::cout << " paramKey=\"" << c.parameterKey << "\"";
            }
            std::cout << '\n';
        }

        // Also check geometry initial values
        std::cout << "    geometries:\n";
        for (std::size_t gi = 0; gi < imported.model.geometries().size(); ++gi) {
            const auto& g = imported.model.geometries()[gi];
            std::cout << "      geo[" << gi << "]: kind=" << static_cast<int>(g.kind)
                      << " construction=" << g.construction
                      << " external=" << g.external;
            if (auto* arc = std::get_if<McSolverEngine::Compat::ArcGeometry>(&g.data)) {
                std::cout << " center=(" << arc->center.x << "," << arc->center.y << ")"
                          << " r=" << arc->radius
                          << " startAngle=" << arc->startAngle
                          << " (" << arc->startAngle * 180.0 / std::numbers::pi << " deg)"
                          << " endAngle=" << arc->endAngle
                          << " (" << arc->endAngle * 180.0 / std::numbers::pi << " deg)";
            }
            std::cout << '\n';
        }
        std::cout << '\n';
    }

    // ── Parameter override sweep ─────────────────────────────────
    std::cout << "=== Parameter override sweep (VarSet.a_of_arc = N°) ===\n";

    const int testValues[] = {30, 35, 40, 45, 50, 55, 60, 65, 70, 80, 90, 10, 20, 100, 120};

    int passCount = 0;
    int failCount = 0;

    for (int val : testValues) {
        std::ostringstream label;
        label << "a_of_arc=" << val << "°";

        auto r = runSolve(xml, label.str(),
                           ParameterMap{{"VarSet.a_of_arc", std::to_string(val)}});

        if (r.solveOk) {
            ++passCount;
            std::cout << "  [PASS] " << label.str()
                      << "  span=" << (r.endAngle - r.startAngle) * 180.0 / std::numbers::pi << "°\n";
        } else {
            ++failCount;
            std::cout << "  [FAIL] " << label.str();
            if (!r.importOk) {
                std::cout << " (import failed)";
                for (const auto& m : r.messages) {
                    std::cout << "  msg=" << m;
                }
            }
            std::cout << '\n';
        }
    }

    std::cout << "\n  Summary: " << passCount << " passed, " << failCount << " failed out of "
              << (passCount + failCount) << " values\n\n";

    // ── Try bare property name format ────────────────────────────
    std::cout << "=== Alternative key formats ===\n";
    {
        for (const auto& [key, label] : {
                 std::pair{"VarSet.a_of_arc", "VarSet.a_of_arc=60"},
                 std::pair{"a_of_arc", "a_of_arc=60 (bare)"},
             }) {
            auto r = runSolve(xml, label, ParameterMap{{std::string(key), "60"}});
            std::cout << "  key=\"" << key << "\": "
                      << (r.solveOk ? "solved" : "FAILED");
            if (!r.importOk) {
                std::cout << " (import failed)";
            }
            std::cout << '\n';
        }
        std::cout << '\n';
    }

    // ── Verify VarSet properties in result ───────────────────────
    std::cout << "=== VarSet property export check ===\n";
    {
        auto imported = McSolverEngine::DocumentXml::importSketchFromDocumentXml(
            xml, ParameterMap{{"VarSet.a_of_arc", "45"}}, "Sketch");

        std::cout << "  Import status: "
                  << (imported.imported() ? "imported" : "failed") << '\n';

        for (const auto& p : imported.varSetProperties) {
            std::cout << "    " << p.key << " = \"" << p.value << "\""
                      << (p.unit.empty() ? "" : " " + p.unit) << '\n';
        }

        // Verify the override took effect
        auto it = std::find_if(
            imported.varSetProperties.begin(),
            imported.varSetProperties.end(),
            [](const auto& p) { return p.key == "VarSet.a_of_arc"; });

        if (it != imported.varSetProperties.end()) {
            const bool valueCorrect = it->value == "45";
            std::cout << "\n  VarSet.a_of_arc override: "
                      << (valueCorrect ? "CORRECT (45)" : "WRONG (got " + it->value + ")") << '\n';
        } else {
            std::cout << "\n  VarSet.a_of_arc NOT FOUND in varSetProperties!\n";
        }
    }

    // ── V111.T001.FCStd ─────────────────────────────────────────
    std::cout << "=== V111.T001.FCStd ===\n";

    const std::string fcstdPathT001 = sourceDir + "/fcstdDoc/V111.T001.FCStd";
    auto extrT001 = McSolverEngine::ZipExtract::extractDocumentXml(fcstdPathT001);
    if (!expect(extrT001.success, "Extract Document.xml from V111.T001.FCStd")) {
        std::cerr << "  Error: " << extrT001.errorMessage << '\n';
        return EXIT_FAILURE;
    }
    const std::string xmlT001(extrT001.documentXml.get(), extrT001.documentXmlSize);
    std::cout << "  Extracted: " << xmlT001.size() << " bytes\n\n";

    // Quick inspect
    auto inspect001 = McSolverEngine::DocumentXml::inspectDocumentXml(xmlT001);
    if (inspect001.succeeded()) {
        for (const auto& sk : inspect001.sketches) {
            if (sk.objectName == "Sketch") {
                std::cout << "  Sketch geometries: " << sk.geometries.size()
                          << "  constraints: " << sk.constraints.size() << '\n';
                std::cout << "  Angle constraint: ["
                          << sk.constraints.back().originalIndex << "] "
                          << sk.constraints.back().kind
                          << " value=" << sk.constraints.back().value << '\n';
            }
        }
    }
    std::cout << '\n';

    // Default solve with full diagnostics
    std::cout << "  Default solve (no params):\n";
    {
        auto imported = McSolverEngine::DocumentXml::importSketchFromDocumentXml(
            xmlT001, ParameterMap{}, "Sketch");
        std::cout << "    import: " << (imported.imported() ? "OK" : "FAILED") << '\n';
        if (!imported.messages.empty()) {
            for (const auto& m : imported.messages) {
                std::cout << "    msg: " << m << '\n';
            }
        }

        auto solveResult = McSolverEngine::Compat::solveSketch(imported.model, ParameterMap{});
        std::cout << "    solve: "
                  << (solveResult.solved() ? "OK" : "FAILED")
                  << "  dof=" << solveResult.degreesOfFreedom
                  << "  conflicting=" << solveResult.conflicting.size()
                  << "  redundant=" << solveResult.redundant.size()
                  << "  partialRedundant=" << solveResult.partiallyRedundant.size()
                  << '\n';

        if (!solveResult.conflicting.empty()) {
            std::cout << "    conflicting constraints:";
            for (int c : solveResult.conflicting) std::cout << " " << c;
            std::cout << '\n';
        }
        if (!solveResult.redundant.empty()) {
            std::cout << "    redundant constraints:";
            for (int c : solveResult.redundant) std::cout << " " << c;
            std::cout << '\n';
        }

        // Print initial geometry state (arc start/end points, point coords)
        std::cout << "    initial arc params:\n";
        for (std::size_t gi = 0; gi < imported.model.geometries().size(); ++gi) {
            const auto& g = imported.model.geometries()[gi];
            if (auto* arc = std::get_if<McSolverEngine::Compat::ArcGeometry>(&g.data)) {
                std::cout << "      arc[" << gi << "]: c=(" << arc->center.x << "," << arc->center.y
                          << ") r=" << arc->radius
                          << " sa=" << arc->startAngle
                          << " ea=" << arc->endAngle << '\n';
            } else if (auto* pt = std::get_if<McSolverEngine::Compat::PointGeometry>(&g.data)) {
                std::cout << "      point[" << gi << "]: (" << pt->point.x << "," << pt->point.y << ")\n";
            } else if (auto* ln = std::get_if<McSolverEngine::Compat::LineSegmentGeometry>(&g.data)) {
                std::cout << "      line[" << gi << "]: (" << ln->start.x << "," << ln->start.y
                          << ")->(" << ln->end.x << "," << ln->end.y << ")"
                          << " ext=" << g.external << " constr=" << g.construction << '\n';
            }
        }
    }

    // Parameter sweep
    std::cout << "  Parameter sweep (VarSet.a_of_arc):\n";
    {
        int pass001 = 0, fail001 = 0;
        for (int val : {30, 40, 50, 60, 70, 90}) {
            auto r = runSolve(xmlT001,
                              "V111.T001/a_of_arc=" + std::to_string(val),
                              ParameterMap{{"VarSet.a_of_arc", std::to_string(val)}});
            if (r.solveOk) {
                ++pass001;
                std::cout << "    [PASS] a_of_arc=" << val << "°";
            } else {
                ++fail001;
                std::cout << "    [FAIL] a_of_arc=" << val << "°";
            }
            std::cout << "  geometryCount=" << r.geometryCount
                      << "  importOk=" << r.importOk << '\n';
        }
        std::cout << "    Summary: " << pass001 << " passed, " << fail001 << " failed\n\n";
        if (!expect(pass001 == 6, "All V111.T001 parameter values should solve")) {
            return EXIT_FAILURE;
        }
    }

    std::cout << "\nDone.\n";
    return EXIT_SUCCESS;
}
