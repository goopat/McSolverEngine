# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```
cmake -B build -DMCSOLVERENGINE_WITH_OCCT=ON
cmake --build build --config Release
```

- CMake 3.22+, C++20 (`CXX_EXTENSIONS NO`; MSVC adds `/permissive-`)
- **Core library**: `McSolverEngineCore` (static) — all solver/model/import/export logic
- **Shared library**: `McSolverEngineNative` → `mcsolverengine_native` (DLL, C ABI)
- **CLI**: `McSolverEngineCli` → `mcsolverengine` (executable)

### Dependencies

| Dependency | Kind | Required |
|---|---|---|
| Eigen3 | header-only | always |
| Boost (Graph, Regex, Math) | header-only | always |
| OpenCASCADE (OCCT) | linked (.lib/.dll) | optional (`MCSOLVERENGINE_WITH_OCCT=ON`) |

Without OCCT: core solving, Document.xml import, and geometry export still build and work. BREP export returns `OpenCascadeUnavailable`.

## Testing

```
cmake --build build --target McSolverEngineSmokeTest && ctest --test-dir build
```

Or run individual test executables directly (they need OCCT DLLs on `PATH`):

```
build\Release\McSolverEngineSmokeTest.exe
build\Release\McSolverEngineCApiSmokeTest.exe
```

C# wrapper tests (`.NET Framework 4.8`):

```
dotnet test wrapper\csharp\tests\McSolverEngine.Wrapper.Tests.csproj
```

Test data lives in `fcstdDoc/` (FreeCAD Document.xml + reference BREP files). Smoke tests validate round-trip: import xml → solve → export BREP/geometry → compare against reference.

## Architecture

The engine is a stand-alone extraction of FreeCAD Sketcher's GCS constraint solver, rebuilt to be independent of FreeCAD's App/Gui/Document runtime. Layers (top to bottom):

1. **C ABI** (`src/CApi.cpp`, `include/McSolverEngine/CApi.h`) — stable C interface for cross-language interop. Allocates geometry/BREP result structs that callers must free (`McSolverEngine_FreeGeometryResult`, `McSolverEngine_FreeBRepResult`).

2. **Compat Model** (`include/McSolverEngine/CompatModel.h`, `src/CompatModel.cpp`) — replaces FreeCAD's `Sketch.cpp` assembly layer. Defines `SketchModel` (geometry + constraints + placement storage) and all geometry/constraint types as plain `std::variant`-based structs. No FreeCAD runtime types.

3. **Compat Solver** (`include/McSolverEngine/CompatSolver.h`, `src/CompatSolver.cpp`) — translates `SketchModel` geometries/constraints into GCS subsystem parameters, calls the solver, and writes results back.

4. **Document.xml Import** (`include/McSolverEngine/DocumentXml.h`, `src/DocumentXml.cpp`) — lightweight XML parser for FreeCAD's `Document.xml` format. Extracts sketch objects, geometries, constraints, `Placement`, `ExpressionEngine`, and `App::VarSet` parameter bindings. No external XML library.

5. **GCS Core** (`src/planegcs/`) — constraint solver ported from FreeCAD's `planegcs`. Uses DogLeg by default (`convergence=1e-10`, `maxIter=100`, `FullPivLU`). Key files: `GCS.h/.cpp` (system), `Constraints.h/.cpp`, `Geo.h/.cpp`, `SubSystem.h/.cpp`, `qp_eq.h/.cpp`.

6. **BREP Export** (`include/McSolverEngine/BRepExport.h`, `src/BRepExport.cpp`, `src/SketchShapeBuilder.cpp`) — OCCT-based BREP output. Converts solved geometry into `TopoDS_Edge`/`TopoDS_Wire`, writes via `BRepTools_ShapeSet` VERSION_1 text format.

7. **Geometry Export** (`include/McSolverEngine/GeometryExport.h`, `src/GeometryExport.cpp`) — structured geometry export with no OCCT dependency. Returns placement + geometry records using the same `McSolverEngine::Compat` geometry types.

### Compatibility shim

`SketcherGlobal.h` maps `SketcherExport` to `MCSOLVERENGINE_EXPORT` so the extracted `planegcs` sources can keep their original `#include "../../SketcherGlobal.h"` directives without pulling in FreeCAD.

### Namespace structure

- `McSolverEngine::Compat` — geometry/constraint model types and solver
- `McSolverEngine::DocumentXml` — FreeCAD Document.xml import
- `McSolverEngine::BRep` — OCCT-backed BREP export
- `McSolverEngine::Geometry` — precise geometry export
- `McSolverEngine` (root) — `Engine` class and `ParameterMap` typedef

### Parameter flow

`Document.xml` → import extracts `VarSet` defaults and `ExpressionEngine` constraint bindings → internal `Compat::Constraint` stores `parameterName`/`parameterKey`/`parameterDefaultValue` → caller can override via `ParameterMap` (keys are `VarSetName.PropertyName` or just `PropertyName`) → solver applies values to dimension constraints.

API parameter values must be pure numeric strings (no unit suffixes). Lengths are interpreted as mm, angles as degrees. Internally, angles are converted to radians before solving.
