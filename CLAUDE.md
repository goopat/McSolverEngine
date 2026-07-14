# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Rules

- **Do not execute git write operations** (commit, push, etc.) unless the user explicitly asks. Ask for confirmation first.

## Build

```sh
# Full build with OCCT BREP export (OCCT must be in .pixi)
cmake -B build -DMCSOLVERENGINE_WITH_OCCT=ON
cmake --build build --config Release

# Minimal build without OCCT (BREP export returns OpenCascadeUnavailable)
cmake -B build
cmake --build build --config Release

# Debug build with tests (MCSOLVERENGINE_BUILD_TESTS defaults ON)
cmake -B build -DMCSOLVERENGINE_WITH_OCCT=ON
cmake --build build --config Debug
```

CMake 3.22+, C++20, MSVC with `/permissive-` and `/utf-8`. Dependencies come from `.pixi/envs/default/Library/` — Eigen3 3.4.0 (header-only), libboost-headers 1.84.0 (header-only, Boost.Graph + Boost.Math), zlib 1.3.2 inflate subset (bundled, symbols prefixed `McSolverEngine_`), and OCCT 7.8.1 (optional, `MCSOLVERENGINE_WITH_OCCT=ON`, BREP export only). Run `pixi install` to populate the pixi environment before building.

**Targets**: `McSolverEngineCore` (static lib), `McSolverEngineZip` (static lib, zlib inflate + ZIP extraction), `McSolverEngineNative` (DLL, C ABI, output name `mcsolverengine_native`), `McSolverEngineCli` (executable, output name `mcsolverengine`).

NuGet packages: `scripts/package_nuget.ps1 -Variant UseOcct|NoOcct -Version 0.1.0`, output to `artifacts/nuget/`.

## Testing

```sh
# All C++ tests
ctest --test-dir build -C Release --output-on-failure

# C++ only (exclude C#)
ctest --test-dir build -C Release -E csharp --output-on-failure

# C# only
ctest --test-dir build -C Release -L csharp --output-on-failure

# Single test by name
ctest --test-dir build -C Release -R "^SmokeTest$" --output-on-failure     # smoke.cpp
ctest --test-dir build -C Release -R "^CApiSmokeTest$" --output-on-failure  # capi_smoke.cpp
ctest --test-dir build -C Release -R "^UnitTest$" --output-on-failure       # unit.cpp

# .NET wrapper tests
dotnet test wrapper/csharp/tests/McSolverEngine.Wrapper.Tests.csproj -c Release -f net8.0

# Python wrapper tests
python -m unittest wrapper/python/tests/test_wrapper.py -v
```

When running test executables directly (e.g. for debugging), add the build output dir and OCCT bin dir (if applicable) to PATH:
```powershell
$env:PATH = "C:\work\McSolverEngine\.pixi\envs\default\Library\bin;C:\work\McSolverEngine\build\Release;$env:PATH"
.\build\Release\McSolverEngineSmokeTest.exe
```

**Test structure**:
- `tests/smoke.cpp` — 48+ regression cases: all geometry/constraint types, expression engine, parameterized solving, Block, BREP token comparison
- `tests/capi_smoke.cpp` — C ABI regression: structured Geometry/BRep export, OCCT/no-OCCT paths, expression-driven constraint references
- `tests/unit.cpp` — unit tests: ParameterValueUtils, CompatModel API, Diameter/Equal/Parallel constraints, conflict/redundancy detection, ZIP/GeometryExport/XML edge cases

The V111.9 regression cases allow BREP object order drift (non-deterministic due to floating-point) but still validate geometry content within tolerance. All other BREP regressions require strict token-order match.

Regression data in `fcstdDoc/` (`.FCStd`, `.xml`, `.brp`, `.solver.brp`) — the authoritative corpus. `.gitattributes` enforces LF line-endings for `fcstdDoc/*.xml` (FreeCAD stores them as LF inside `.FCStd`; git must not convert to CRLF on checkout).

## Architecture

Standalone extraction of FreeCAD Sketcher's GCS constraint solver. 12 layers, top to bottom:

1. **C# Wrapper** — P/Invoke, `net48;net8.0`, no NuGet deps. `McSolverEngineClient.cs`
2. **Python Wrapper** — pure ctypes. `_bindings.py` (DLL discovery + struct defs), `_engine.py` (`Engine.solve_to_geometry/brep/extract_fcstd_doc`), `_svg.py` (geometry → SVG polyline, optionally uses `rhino3dm` for B-Splines)
3. **C ABI** — `CApi.h/cpp`, cross-language interop. Allocates result structs callers must free. Exposes `GetLastError()`, `GetVersion()`, parameter override interface, and `InspectDocumentXml`
4. **Engine** — `Engine::version()` / `Engine::describe()`
5. **Compat Model** — `CompatModel.h/cpp`. `SketchModel` is the central in-memory model. All geometry/constraint types are `std::variant` structs — no FreeCAD types
6. **Compat Solver** — `CompatSolver.h/cpp`. Translates `SketchModel` → GCS parameters → solves → writes results back. Handles `ParameterMap` overrides. **Four-level solver fallback chain**: DogLeg → LevenbergMarquardt → BFGS → SQP augmented (aligns with FreeCAD's `internalSolve`). Key params: `convergence=1e-10`, `maxIter=100`, `FullPivLU`
7. **Document.xml Import + Inspect** — `DocumentXml.h/cpp`. Custom XML parser (no external library). `importSketchFromDocumentXml` extracts geometries, constraints, Placement, ExpressionEngine, VarSet for solving. `inspectDocumentXml` is a lightweight browse API returning sketch/varSet metadata including geometry element types, construction flags, constraint kinds, and cross-references without running the solver
8. **VarSet Expression Engine** — `VarSetExpressionEngine.cpp/h`. Lightweight FreeCAD expression subset: arithmetic, trig/hyperbolic functions, pi/e constants, pow/sqrt/log, min/max/sum/average/count, limited length/angle unit conversion. Supports VarSet/Sketch property reference chains with cycle detection. Non-VarSet object references (e.g. `Spreadsheet.Width`) return `VarSetExpressionUnsupportedSubset` error
9. **FCStd ZIP Extraction** — `ZipExtract.cpp` + `src/third_party/zlib/`. Custom ZIP parser + raw-DEFLATE via bundled zlib inflate
10. **GCS Core** — `src/core/planegcs/`. Vendored from FreeCAD, LGPL-2.1-or-later. Keep as close to upstream as possible — put semantic fixes and behavior alignments in upper layers (DocumentXml, CompatSolver), not in planegcs itself
11. **BREP Export** — OCCT `TopoDS_Edge/Wire` → `BRepTools_ShapeSet` VERSION_1 text
12. **Geometry Export** — structured export, no OCCT dependency

**Shim**: `SketcherGlobal.h` maps `SketcherExport` → `MCSOLVERENGINE_EXPORT` so planegcs compiles unchanged.

**Namespaces**: `McSolverEngine::Compat` | `McSolverEngine::DocumentXml` | `McSolverEngine::BRep` | `McSolverEngine::Geometry` | `McSolverEngine::ZipExtract` | `McSolverEngine` (root: `Engine` + `ParameterMap`).

**Parameter flow**: Document.xml → VarSet defaults + ExpressionEngine bindings → `Compat::Constraint` stores `parameterName`/`parameterKey`/`parameterDefaultValue` → caller overrides via `ParameterMap` (keys must be `VarSetName.PropertyName`; bare property names are rejected) → solver applies values. API values must be pure numeric strings (no unit suffixes). Lengths = mm, angles = degrees (converted to radians internally).

All 4 solver result APIs (`SolveToGeometry`, `SolveToGeometryWithParameters`, `SolveToBRep`, `SolveToBRepWithParameters`) return the full VarSet scalar property table (`ObjectName.PropertyName → value: string + unit: string`). Numerically evaluable results use canonical units: length=`mm`, angle=`deg`, area=`mm^2`, dimensionless=`""`.

## Key conventions

- **Planegcs maintenance boundary**: Keep `src/core/planegcs/` as close to FreeCAD upstream as possible. Put semantic corrections, behavior alignments, and input compatibility patches in `DocumentXml.cpp`, `CompatSolver.cpp`, `CApi.cpp`, or wrapper/tests layers — never in planegcs itself. This makes it easier to diff against upstream and trace divergence.
- **Compat boundary**: Extend `Compat::SketchModel`, never reintroduce FreeCAD dependencies
- **Parameter lookup order**: Full key (`VarSet.Property`) → imported default (ExpressionEngine / VarSet)
- **Import tolerance**: `DocumentXml` can return `ImportStatus::Partial` with `messages` + `skippedConstraints`; fatal VarSet issues use the dedicated unsupported-subset error path (`VarSetExpressionUnsupportedSubset`)
- **Flag preservation**: `originalId`, `construction`, `external`, `blocked` must survive round-trips through import → solve → export → C API
- **Fixed geometry**: External and blocked geometry are solver-fixed references. Construction and external are excluded from BREP output (but included in Geometry export)
- **Inspect ↔ Export alignment**: `InspectConstraintInfo::originalIndex` counts every `<Constrain/>` element in XML order (including inactive/virtual-space) and matches `ConstraintRef::originalIndex` from the solve→export path. Geometry elements from `inspectDocumentXml` and `exportSketchGeometry` share the same order and `originalId` — indices are 1:1
- **Constraint cross-references**: `InspectGeometryElement::constraintIndices` lists all active non-virtual constraints that reference the geometry. `InspectConstraintInfo::referencedGeoIds` lists all geometry indices the constraint refers to (may contain duplicates for constraints like Coincident where first=second)
- **Solver parameter lifetime**: The solver layer owns persistent `double` storage because planegcs works with raw parameter pointers. Follow the existing `SolveContext` pattern — never introduce stack-backed parameter lifetimes
- **Unit conventions for API parameters**: Length constraints (DistanceX/Y, Distance, Radius, Diameter) = mm. Angle constraints = degrees. Angles are converted to radians internally before entering the solver. API parameter values must be pure numeric strings — `"8.5"`, not `"8.5 mm"` or `"45 deg"`
- **VarSet expression subset**: Arithmetic, trig/hyperbolic, pi/e, pow/sqrt/log, min/max/sum/average/count, limited unit conversion (length: mm/cm/m/km/um/nm/in/ft; angle: deg/rad). Supports `<<Label>>.Param` and `VarSetName.Param` references. No Spreadsheet refs, no full Quantity/Unit, no conditional/logic expressions — these return `VarSetExpressionUnsupportedSubset`
- **Windows debug behavior**: `WindowsAssertMode.h` redirects CRT/STL assertion failures to stderr instead of blocking message boxes. All native entry points (CLI, smoke tests) enable this
- **Git line endings**: `fcstdDoc/*.xml` must stay LF. FreeCAD stores `Document.xml` as LF inside `.FCStd` ZIPs, and extraction preserves bytes as-is. `.gitattributes` enforces `eol=lf` for these files. If comparing extracted XML against disk references, a CRLF checkout will cause spurious mismatches
- **Regression corpus**: `fcstdDoc/` is authoritative. Add new fixtures when fixing bugs or adding features. Covers samples 1–3, V102.1–V102.8, V111.9 with parameterized, expression-driven, and multi-threaded scenarios
- **Constraint compatibility**: The `Document.xml` importer handles both new-format (`ElementIds`/`ElementPositions`) and old-format (`First`/`Second`/`Third`) constraint element fields, matching FreeCAD's `Constraint::Restore()` precedence (new fields first, old fields overwrite first three)
