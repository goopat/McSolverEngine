# CLAUDE.md

## Rules

- **Do not execute git write operations** (commit, push, etc.) unless the user explicitly asks. Ask for confirmation first.

## Build

```
cmake -B build -DMCSOLVERENGINE_WITH_OCCT=ON
cmake --build build --config Release
```

CMake 3.22+, C++20. pixi provides Eigen3/libboost-headers/OCCT from `.pixi/envs/default/Library/`; CMake requires Eigen3 and libboost-headers from that pixi environment, and when `MCSOLVERENGINE_WITH_OCCT=ON` it requires OCCT there too.

**Targets**: `McSolverEngineCore` (static lib), `McSolverEngineZip` (static lib, zlib inflate + ZipExtract), `McSolverEngineNative` (DLL, C ABI), `McSolverEngineCli` (executable).

**Dependencies**: Eigen3 3.4.0 (header-only) | libboost-headers 1.84.0 (Graph, Math) | zlib 1.3.2 inflate subset (bundled, symbols prefixed `McSolverEngine_`) | OpenCASCADE 7.8.1 (optional, `MCSOLVERENGINE_WITH_OCCT=ON`, BREP export only).

NuGet packages: `scripts/package_nuget.ps1`, output to `artifacts/nuget/`.

## Testing

```
ctest --test-dir build -C Release --output-on-failure
ctest --test-dir build -C Release -E csharp --output-on-failure   # C++ only
ctest --test-dir build -C Release -L csharp --output-on-failure   # C# only
ctest --test-dir build -C Release -R "^SmokeTest$" --output-on-failure
dotnet test wrapper\csharp\tests\McSolverEngine.Wrapper.Tests.csproj -c Release -f net8.0
python -m unittest wrapper/python/tests/test_wrapper.py -v
```

Regression data in `fcstdDoc/` (`.FCStd`, `.xml`, `.brp`, `.svg`). Covers samples 1–3, V102.1–V102.7 with parameterized, expression-driven, and multi-threaded scenarios. All three layers (C++ smoke, C API smoke, C# WrapperRegression, Python test_wrapper) also cover `InspectDocumentXml` with geometry/constraint/cross-reference assertions.

## Architecture

Standalone extraction of FreeCAD Sketcher's GCS constraint solver. 12 layers, top to bottom:

1. **C# Wrapper** — P/Invoke, `net48;net8.0`, no NuGet deps. `McSolverEngineClient.cs`
2. **Python Wrapper** — pure ctypes. `_bindings.py` (DLL discovery + struct defs), `_engine.py` (`Engine.solve_to_geometry/brep/extract_fcstd_doc`), `_svg.py` (geometry → SVG polyline, optionally uses `rhino3dm` for B-Splines)
3. **C ABI** — `CApi.h/cpp`, cross-language interop. Allocates result structs callers must free. Exposes `GetLastError()`, `GetVersion()`, parameter override interface
4. **Engine** — `Engine::version()` / `Engine::describe()`
5. **Compat Model** — `CompatModel.h/cpp`. `SketchModel` is the central in-memory model. All geometry/constraint types are `std::variant` structs — no FreeCAD types
6. **Compat Solver** — `CompatSolver.h/cpp`. Translates `SketchModel` → GCS parameters → solves → writes results back. Handles `ParameterMap` overrides
7. **Document.xml Import + Inspect** — `DocumentXml.h/cpp`. Custom XML parser (no external library). `importSketchFromDocumentXml` extracts geometries, constraints, Placement, ExpressionEngine, VarSet for solving. `inspectDocumentXml` is a lightweight browse API that returns sketch/varSet metadata including geometry element types, construction flags, constraint kinds, and cross-references without running the solver
8. **VarSet Expression Engine** — `VarSetExpressionEngine.cpp/h`. Lightweight FreeCAD expression subset: arithmetic, units, VarSet refs, math functions, pi/e, cycle detection
9. **FCStd ZIP Extraction** — `ZipExtract.cpp` + `src/third_party/zlib/`. Custom ZIP parser + raw-DEFLATE via bundled zlib inflate
10. **GCS Core** — `src/core/planegcs/`. DogLeg solver (`convergence=1e-10`, `maxIter=100`, `FullPivLU`). LGPL-2.1-or-later (derived from FreeCAD)
11. **BREP Export** — OCCT `TopoDS_Edge/Wire` → `BRepTools_ShapeSet` VERSION_1 text
12. **Geometry Export** — structured export, no OCCT dependency

**Shim**: `SketcherGlobal.h` maps `SketcherExport` → `MCSOLVERENGINE_EXPORT` so planegcs compiles unchanged.

**Namespaces**: `McSolverEngine::Compat` | `McSolverEngine::DocumentXml` | `McSolverEngine::BRep` | `McSolverEngine::Geometry` | `McSolverEngine::ZipExtract` | `McSolverEngine` (root: `Engine` + `ParameterMap`).

**Parameter flow**: Document.xml → VarSet defaults + ExpressionEngine bindings → `Compat::Constraint` stores `parameterName/key/defaultValue` → caller overrides via `ParameterMap` (keys: `VarSetName.PropertyName` only; bare property names are rejected) → solver applies values. API values must be pure numeric strings (no unit suffixes). Lengths = mm, angles = degrees (converted to radians internally).

## Key conventions

- **Compat boundary**: extend `Compat::SketchModel`, never reintroduce FreeCAD dependencies
- **Parameter lookup order**: full key (`VarSet.Property`) → imported default (ExpressionEngine / VarSet)
- **Import tolerance**: `DocumentXml` can return `ImportStatus::Partial` with `messages` + `skippedConstraints`; fatal VarSet issues use the dedicated unsupported-subset error path
- **Flag preservation**: `originalId`, `construction`, `external`, `blocked` must survive round-trips through import → solve → export → C API
- **Fixed geometry**: External and blocked geometry are solver-fixed references. Construction and external are excluded from BREP output
- **Inspect ↔ Export alignment**: `InspectConstraintInfo::originalIndex` counts every `<Constrain/>` element in XML order (including inactive/virtual-space) and matches `ConstraintRef::originalIndex` from the solve→export path. Geometry elements from `inspectDocumentXml` and `exportSketchGeometry` share the same order and `originalId` — indices are 1:1
- **Constraint cross-references**: `InspectGeometryElement::constraintIndices` lists all active non-virtual constraints that reference the geometry. `InspectConstraintInfo::referencedGeoIds` lists all geometry indices the constraint refers to (may contain duplicates for constraints like Coincident where first=second)
- **Regression corpus**: `fcstdDoc/` is authoritative. Add new fixtures when fixing bugs or adding features
