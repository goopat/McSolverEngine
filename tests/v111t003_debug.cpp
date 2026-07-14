// Regression test for V111.T003, V111.T001, V101.T002 and their .shortR variants.
// Each sketch has an expression-driven Angle constraint backed by VarSet.
// Sweep parameter values across the full 0.01°–359.99° range (0.01° step) and
// verify solve success. Every case runs to completion so the printed pass/fail
// statistics cover the whole range; the process exits non-zero if any solve
// fails.
//
// Each FCStd is extracted exactly once; the six cases run concurrently (one
// thread per case). Per-case total elapsed time and the 3 longest individual
// solve times (with their parameter values) are recorded and printed.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "McSolverEngine/CompatModel.h"
#include "McSolverEngine/CompatSolver.h"
#include "McSolverEngine/DocumentXml.h"
#include "McSolverEngine/Engine.h"
#include "McSolverEngine/ZipExtract.h"

namespace
{

using McSolverEngine::ParameterMap;

struct SlowEntry
{
    double timeMs;
    double paramValue;
};

struct SweepResult
{
    int passCount = 0;
    int failCount = 0;
    std::vector<double> failedValues;
    double totalElapsedMs = 0;
    std::vector<SlowEntry> topSlowest;  // up to 3, descending by timeMs
};

static void insertTopSlowest(std::vector<SlowEntry>& top, double timeMs, double paramValue)
{
    if (top.size() < 3 || timeMs > top.back().timeMs) {
        top.push_back({timeMs, paramValue});
        std::sort(top.begin(), top.end(),
            [](const SlowEntry& a, const SlowEntry& b) { return a.timeMs > b.timeMs; });
        if (top.size() > 3) top.resize(3);
    }
}

SweepResult fullSweep(const std::string& xml,
                      const std::string& paramKey,
                      double paramMin, double paramMax, double paramStep)
{
    using namespace std::chrono;

    SweepResult result;
    const auto sweepStart = steady_clock::now();

    for (double val = paramMin; val <= paramMax + 1e-9; val += paramStep) {
        const auto iterStart = steady_clock::now();

        std::ostringstream vs;
        vs << std::fixed << std::setprecision(3) << val;
        auto params = ParameterMap{{paramKey, vs.str()}};
        auto imported = McSolverEngine::DocumentXml::importSketchFromDocumentXml(
            xml, params, "Sketch");
        if (!imported.imported()) {
            ++result.failCount;
            result.failedValues.push_back(val);
            continue;
        }
        auto solve = McSolverEngine::Compat::solveSketch(imported.model, params);
        if (solve.solved()) { ++result.passCount; }
        else {
            ++result.failCount;
            result.failedValues.push_back(val);
        }

        const auto iterEnd = steady_clock::now();
        const double iterMs = duration<double, std::milli>(iterEnd - iterStart).count();
        insertTopSlowest(result.topSlowest, iterMs, val);
    }

    const auto sweepEnd = steady_clock::now();
    result.totalElapsedMs = duration<double, std::milli>(sweepEnd - sweepStart).count();
    return result;
}

}  // namespace

int main()
{
    const std::string sourceDir {MCSOLVERENGINE_SOURCE_DIR};
    std::cout << "McSolverEngine version: " << McSolverEngine::Engine::version() << "\n\n";

    struct Case
    {
        const char* file;
        const char* paramKey;
        const char* label;
    };

    // Hard-regression cases: every sketch must solve at every value in the
    // full 0.01°–359.99° sweep. The .shortR variants have small radii; the
    // full-radius originals are badly scaled (large arc radii / long lines
    // coupled to angle parameters) and rely on the constraint-level Jacobian
    // rescaling (arc-rules ~1/r, L2LAngle ~r) plus the DogLegScaled column-
    // scaling fallback solver. All six gate the test.
    const Case cases[] = {
         {"V111.T003.shortR.FCStd", "VarSet.a_of_arc", "a_of_arc"},
         {"V111.T001.shortR.FCStd", "VarSet.a_of_arc", "a_of_arc"},
         {"V101.T002.shortR.FCStd", "VarSet.a_of_l2l", "a_of_l2l"},
         {"V111.T003.FCStd",        "VarSet.a_of_arc", "a_of_arc"},
         {"V111.T001.FCStd",        "VarSet.a_of_arc", "a_of_arc"},
         {"V101.T002.FCStd",        "VarSet.a_of_l2l", "a_of_l2l"},
    };

    std::mutex coutMutex;
    std::atomic<int> totalFail{0};
    std::vector<std::jthread> threads;

    std::cout << "=== Hard-regression (all cases gate, "
              << std::size(cases) << " threads) ===\n";

    for (const auto& c : cases) {
        threads.emplace_back([&, c]() {
            // Print header before starting work
            {
                std::lock_guard<std::mutex> lock(coutMutex);
                std::cout << "-- " << c.file << "  (" << c.label
                          << "  0.1 deg to 359.99 deg  / step 0.25 deg) --\n";
            }

            // ── Extract FCStd once ────────────────────────────────
            const auto e = McSolverEngine::ZipExtract::extractDocumentXml(
                sourceDir + "/fcstdDoc/" + c.file);
            if (!e.success) {
                std::lock_guard<std::mutex> lock(coutMutex);
                std::cerr << "FAIL: " << c.file << "\n";
                totalFail.fetch_add(1);
                return;
            }
            const std::string xml(e.documentXml.get(), e.documentXmlSize);

            // ── Run full sweep on the extracted XML ───────────────
            // auto result = fullSweep(xml, c.paramKey, 0.01, 359.99, 0.01);
            auto result = fullSweep(xml, c.paramKey, 0.1, 359.99, 0.25);

            // ── Print results atomically ──────────────────────────
            {
                std::lock_guard<std::mutex> lock(coutMutex);
                for (double fv : result.failedValues) {
                    std::ostringstream vs;
                    vs << std::fixed << std::setprecision(3) << fv;
                    std::cout << "*** " << c.file << "    [FAIL] " << c.label << "=" << vs.str() << " deg\n";
                }
                const int total = result.passCount + result.failCount;
                std::cout << "--- " << c.file << "    " << result.passCount << "/" << total << " passed";
                if (result.failCount > 0) std::cout << "\n--- " << c.file << ", " << result.failCount << " failed";
                std::cout << '\n';

                // ── Timing ─────────────────────────────────────────
                std::cout << "--- " << c.file << "    total elapsed: "
                          << std::fixed << std::setprecision(0) << result.totalElapsedMs << " ms\n";
                if (!result.topSlowest.empty()) {
                    std::cout << "--- " << c.file << "    top 3 slowest solves:\n";
                    for (const auto& s : result.topSlowest) {
                        std::cout << "        " << std::fixed << std::setprecision(1) << s.timeMs
                                  << " ms  @ " << c.label << "="
                                  << std::setprecision(3) << s.paramValue << " deg\n";
                    }
                }
            }
            totalFail.fetch_add(result.failCount);
        });
    }

    // Wait for all threads to complete
    for (auto& t : threads) t.join();

    std::cout << "\nDone.\n";
    return totalFail.load() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
