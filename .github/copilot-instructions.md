# McSolverEngine Copilot Instructions

## Build and test commands

This repository is a CMake-based C++20 project. Standalone builds define these top-level targets:

- `McSolverEngineCore` (static library)
- `McSolverEngineZip` (static library, zlib inflate + ZIP extraction)
- `McSolverEngineNative` (shared library exposing the C ABI)
- `McSolverEngineCli` (CLI executable)
- `McSolverEngineSmokeTest`, `McSolverEngineCApiSmokeTest`, and `McSolverEngineUnitTest` when `MCSOLVERENGINE_BUILD_TESTS=ON`

Configure and build:

```sh
cmake -S . -B build -DMCSOLVERENGINE_BUILD_TESTS=ON
cmake --build build --config Debug
```

Run the full native test suite:

```sh
ctest --test-dir build -C Debug --output-on-failure
```

Run a single registered test executable:

```sh
ctest --test-dir build -C Debug -R McSolverEngineSmokeTest --output-on-failure
ctest --test-dir build -C Debug -R McSolverEngineCApiSmokeTest --output-on-failure
```

Useful configure switches:

```sh
cmake -S . -B build -DMCSOLVERENGINE_BUILD_TESTS=ON -DMCSOLVERENGINE_WITH_OCCT=OFF
```

Run .NET and Python wrapper tests:

```sh
dotnet test wrapper/csharp/tests/McSolverEngine.Wrapper.Tests.csproj -c Release -f net8.0
python -m unittest wrapper/python/tests/test_wrapper.py -v
```

Dependency notes:

- `Eigen3` is required and must come from `.pixi`.
- `libboost-headers` is required by the extracted `planegcs` solver code and must come from `.pixi`.
- `OpenCASCADE` is optional only when `MCSOLVERENGINE_WITH_OCCT=OFF`; when that option is `ON`, CMake requires OCCT from `.pixi`.
- `CMakeLists.txt` auto-probes `..\.pixi\envs\default\Library\...` for Eigen, Boost, and OCCT; these dependencies do not fall back outside `.pixi`.
- Current linkage is mixed, not fully dynamic:
  - `Eigen3` is consumed as a header-only dependency in this project.
  - `libboost-headers` is currently consumed via headers only (`Boost.Graph`, `Boost.Math constants`); the build does not explicitly link Boost binary libraries.
  - `OpenCASCADE` is a runtime dynamic dependency when enabled: the build links `.lib` import libraries and the resulting binaries require the matching OCCT `.dll` files on `PATH`.
  - On Windows, the generated projects use the dynamic MSVC runtime (`/MD` and `/MDd`), not static CRT linkage.

## High-level architecture

`McSolverEngine` is a standalone extraction of the FreeCAD Sketcher solver stack. The important flow is:

1. `src\DocumentXml.cpp` parses FreeCAD `Document.xml` sketch data into `Compat::SketchModel`.
2. `src\CompatSolver.cpp` translates that compat model into `planegcs` primitives and solves with a four-level fallback chain: `GCS::DogLeg` → LevenbergMarquardt → BFGS → SQP augmented.
3. Export happens through either:
   - `src\GeometryExport.cpp` for exact structured geometry plus `Placement`
   - `src\BRepExport.cpp` + `src\SketchShapeBuilder.cpp` for OCCT-backed BREP output
4. `src\CApi.cpp` wraps the full import -> solve -> export pipeline as a stable C ABI.

The core public model is in `include\McSolverEngine\CompatModel.h`:

- geometry is stored as `std::variant` payloads plus `construction`, `external`, and `blocked` flags
- constraints use `ElementRef { geometryIndex, PointRole }` instead of raw FreeCAD XML ids
- `Placement` is stored on the sketch model and carried through exports

`src\core\planegcs\` is vendored solver code from FreeCAD. `SketcherGlobal.h` is only a compatibility shim so the extracted solver sources keep compiling with their original include expectations.

The CLI in `apps\mcsolverengine_cli.cpp` is minimal; the real product surface is the native library and C API.

The library packaging split matters:

- `McSolverEngineCore` is a static library that contains almost all engine logic.
- `McSolverEngineNative` is the shared-library ABI surface.
- `McSolverEngineCli` and the native smoke tests link against `McSolverEngineCore`.

## Key conventions

- The `Document.xml` importer is intentionally lightweight and string-based; it does not use a general XML library. Extend it by following the existing `extractAttribute` / `findPropertyBlock` / `findFirstTag` helpers in `src\DocumentXml.cpp`.
- Import failures vs partial support are distinct. Malformed XML or broken geometry references should fail the import. Unsupported constraint forms should usually increment `skippedConstraints`, append a message, and return `ImportStatus::Partial` instead of hard-failing.
- `ExternalGeo` and construction geometry are imported into the same `SketchModel`, but both `GeometryExport` and `SketchShapeBuilder` intentionally skip `geometry.construction` and `geometry.external` when producing outputs.
- Placement is preserved separately from exact geometry. `Geometry::exportSketchGeometry(...)` returns 2D geometry plus `placement`; OCCT export applies that placement when building the final shape.
- The solver layer owns persistent `double` storage because `planegcs` works with raw parameter pointers. Follow the existing `SolveContext` pattern rather than introducing stack-backed parameter lifetimes.
- The solver uses a four-level fallback chain: `GCS::DogLeg` → LevenbergMarquardt → BFGS → SQP augmented. If solver behavior changes, inspect both conflict/redundancy reporting and solved-geometry writeback in `src\CompatSolver.cpp`.
- Native regression tests are plain executables in `tests\`, not a unit-test framework. `tests\smoke.cpp` and `tests\capi_smoke.cpp` use direct assertions, sample `fcstdDoc\*.xml` inputs, and checked-in `.brp` baselines.
- If you touch import/export behavior, check `fcstdDoc\` regression assets. The smoke test validates both exact geometry and, when OCCT is enabled, serialized BREP output against those checked-in samples.
- The C API allocates structured geometry/BREP results on the native side. Callers must release them with `McSolverEngine_FreeGeometryResult(...)` and `McSolverEngine_FreeBRepResult(...)`.
