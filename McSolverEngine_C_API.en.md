# McSolverEngine C API Reference

## 1. Overview

McSolverEngine exposes a stable C ABI via `mcsolverengine_native.dll` (Windows) / `libmcsolverengine_native.so` (Linux) / `libmcsolverengine_native.dylib` (macOS). The API is declared in `include/McSolverEngine/CApi.h`.

**Design principles**:
- All `char*` parameters and return values use **UTF-8 encoding**
- All result structs are allocated by the native layer; callers must free via the corresponding `McSolverEngine_Free*` function
- All C++ exceptions are caught internally and converted to error codes + `GetLastError()` diagnostic messages — no exceptions cross the C boundary
- `GetLastError()` returns a thread-local buffer (512 bytes), valid until the next API call on the same thread

---

## 2. Types

### 2.1 Result Codes

#### `McSolverEngineResultCode` — solve/export pipeline return codes

| Value | Enum | Meaning |
|-------|------|---------|
| 0 | `MCSOLVERENGINE_RESULT_SUCCESS` | Pipeline succeeded |
| 1 | `MCSOLVERENGINE_RESULT_INVALID_ARGUMENT` | Null/invalid parameter |
| 2 | `MCSOLVERENGINE_RESULT_IMPORT_FAILED` | Document.xml parsing failed |
| 3 | `MCSOLVERENGINE_RESULT_SOLVE_FAILED` | GCS solver failed or diverged |
| 4 | `MCSOLVERENGINE_RESULT_UNSUPPORTED` | Sketch contains unsupported constraint/geometry |
| 5 | `MCSOLVERENGINE_RESULT_GEOMETRY_EXPORT_FAILED` | Geometry export failed |
| 6 | `MCSOLVERENGINE_RESULT_BREP_EXPORT_FAILED` | BREP export failed |
| 7 | `MCSOLVERENGINE_RESULT_OPEN_CASCADE_UNAVAILABLE` | OCCT not linked (BREP unavailable) |
| 8 | `MCSOLVERENGINE_RESULT_OUT_OF_MEMORY` | Allocation failure |
| 9 | `MCSOLVERENGINE_RESULT_VARSET_EXPRESSION_UNSUPPORTED_SUBSET` | VarSet expression uses unsupported FreeCAD expression features |
| 10 | `MCSOLVERENGINE_RESULT_VARSET_PARAMETER_VALIDATION_FAILED` | VarSet parameter validation failed (invalid key format or value contains unit suffix) |
| 11 | `MCSOLVERENGINE_RESULT_SKETCH_NOT_FOUND` | Specified sketch not found in Document.xml |

#### `McSolverEngineFCStdResultCode` — FCStd extraction return codes

| Value | Enum | Meaning |
|-------|------|---------|
| 0 | `MCSOLVERENGINE_FCSTD_SUCCESS` | Extraction succeeded |
| 1 | `MCSOLVERENGINE_FCSTD_OPEN_FAILED` | File not found or read error |
| 2 | `MCSOLVERENGINE_FCSTD_NOT_ZIP` | Not a valid ZIP archive |
| 3 | `MCSOLVERENGINE_FCSTD_XML_NOT_FOUND` | Document.xml entry not in archive |
| 4 | `MCSOLVERENGINE_FCSTD_DECOMPRESS_FAILED` | DEFLATE decompression error |
| 5 | `MCSOLVERENGINE_FCSTD_OUT_OF_MEMORY` | Allocation failure |

### 2.2 Geometry Kinds

```c
typedef enum McSolverEngineGeometryKind {
    MCSOLVERENGINE_GEOMETRY_POINT            = 0,
    MCSOLVERENGINE_GEOMETRY_LINE_SEGMENT     = 1,
    MCSOLVERENGINE_GEOMETRY_CIRCLE           = 2,
    MCSOLVERENGINE_GEOMETRY_ARC              = 3,
    MCSOLVERENGINE_GEOMETRY_ELLIPSE          = 4,
    MCSOLVERENGINE_GEOMETRY_ARC_OF_ELLIPSE   = 5,
    MCSOLVERENGINE_GEOMETRY_ARC_OF_HYPERBOLA = 6,
    MCSOLVERENGINE_GEOMETRY_ARC_OF_PARABOLA  = 7,
    MCSOLVERENGINE_GEOMETRY_BSPLINE          = 8
} McSolverEngineGeometryKind;
```

### 2.3 Constraint Kinds

```c
typedef enum McSolverEngineConstraintKind {
    MCSOLVERENGINE_CONSTRAINT_COINCIDENT        = 0,
    MCSOLVERENGINE_CONSTRAINT_HORIZONTAL        = 1,
    MCSOLVERENGINE_CONSTRAINT_VERTICAL          = 2,
    MCSOLVERENGINE_CONSTRAINT_DISTANCE_X        = 3,
    MCSOLVERENGINE_CONSTRAINT_DISTANCE_Y        = 4,
    MCSOLVERENGINE_CONSTRAINT_DISTANCE          = 5,
    MCSOLVERENGINE_CONSTRAINT_PARALLEL          = 6,
    MCSOLVERENGINE_CONSTRAINT_TANGENT           = 7,
    MCSOLVERENGINE_CONSTRAINT_PERPENDICULAR     = 8,
    MCSOLVERENGINE_CONSTRAINT_ANGLE             = 9,
    MCSOLVERENGINE_CONSTRAINT_RADIUS            = 10,
    MCSOLVERENGINE_CONSTRAINT_DIAMETER          = 11,
    MCSOLVERENGINE_CONSTRAINT_EQUAL             = 12,
    MCSOLVERENGINE_CONSTRAINT_SYMMETRIC         = 13,
    MCSOLVERENGINE_CONSTRAINT_POINT_ON_OBJECT   = 14,
    MCSOLVERENGINE_CONSTRAINT_INTERNAL_ALIGNMENT = 15,
    MCSOLVERENGINE_CONSTRAINT_SNELLS_LAW        = 16,
    MCSOLVERENGINE_CONSTRAINT_BLOCK             = 17,
    MCSOLVERENGINE_CONSTRAINT_WEIGHT            = 18
} McSolverEngineConstraintKind;
```

### 2.4 Primitive Structs

```c
typedef struct McSolverEnginePoint2 {
    double x;
    double y;
} McSolverEnginePoint2;

typedef struct McSolverEnginePlacement {
    double px;   // base point X
    double py;   // base point Y
    double pz;   // base point Z
    double qx;   // rotation quaternion X
    double qy;   // rotation quaternion Y
    double qz;   // rotation quaternion Z
    double qw;   // rotation quaternion W
} McSolverEnginePlacement;

typedef struct McSolverEngineBSplinePole {
    McSolverEnginePoint2 point;
    double weight;
} McSolverEngineBSplinePole;

typedef struct McSolverEngineBSplineKnot {
    double value;
    int multiplicity;
} McSolverEngineBSplineKnot;
```

### 2.4.1 VarSet property record

```c
typedef struct McSolverEngineVarSetProperty {
    const char* keyUtf8;   // UTF-8, always ObjectName.PropertyName
    const char* valueUtf8; // UTF-8 value string
    const char* unitUtf8;  // UTF-8 unit, or "" when not applicable
} McSolverEngineVarSetProperty;
```

`varSetProperties` returns **all scalar `App::VarSet` properties** after API overrides and reduced Sketch / VarSet expression evaluation. Numerically evaluable values are normalized to canonical units (length=`mm`, angle=`deg`, area=`mm^2`, dimensionless=`""`). String/bool-style properties are returned as raw text with `unitUtf8=""`. Keys always use the real object name, never the `Label` alias.

### 2.4.2 Document.xml inspection records

```c
typedef struct McSolverEngineScalarPropertyInfo {
    const char* nameUtf8;
    const char* typeUtf8;
    const char* scalarValueUtf8;
    const char* propertyXmlUtf8;
} McSolverEngineScalarPropertyInfo;

typedef struct McSolverEngineInspectConstraint {
    int originalIndex;               // XML position counting every <Constrain/> element
    int type;                        // raw constraint type number (1=Coincident, 2=Horizontal, …)
    const char* kindUtf8;            // human-readable kind name ("Coincident", "Distance", …)
    int driving;                     // 1 if driving, 0 if non-driving (reference)
    double value;                    // constraint value
    int referencedGeoIdCount;
    const int* referencedGeoIds;     // geometry indices this constraint references
} McSolverEngineInspectConstraint;

typedef struct McSolverEngineInspectGeometryElement {
    int index;                       // position in sketch geometry list (internal first, then external)
    int originalId;                  // XML geometry id attribute
    const char* typeUtf8;            // geometry type string ("Part::GeomLineSegment", …)
    int construction;                // 1 if construction geometry
    int external;                    // 1 if external reference geometry
    int constraintCount;
    const int* constraintIndices;    // indices (originalIndex) of constraints referencing this geometry
} McSolverEngineInspectGeometryElement;

typedef struct McSolverEngineSketchInfo {
    const char* labelUtf8;
    const char* objectNameUtf8;
    int propertyCount;
    const McSolverEngineScalarPropertyInfo* properties;
    int geometryCount;
    const McSolverEngineInspectGeometryElement* geometries;
    int constraintCount;
    const McSolverEngineInspectConstraint* constraints;
} McSolverEngineSketchInfo;

typedef struct McSolverEngineVarSetParameterInfo {
    const char* nameUtf8;
    const char* typeUtf8;
    const char* rawValueUtf8;
    const char* expressionUtf8;
    const char* propertyXmlUtf8;
} McSolverEngineVarSetParameterInfo;

typedef struct McSolverEngineVarSetInfo {
    const char* labelUtf8;
    const char* objectNameUtf8;
    int parameterCount;
    const McSolverEngineVarSetParameterInfo* parameters;
} McSolverEngineVarSetInfo;

typedef struct McSolverEngineDocumentInfo {
    int sketchCount;
    const McSolverEngineSketchInfo* sketches;
    int varSetCount;
    const McSolverEngineVarSetInfo* varSets;
    int messageCount;
    char** messages;
} McSolverEngineDocumentInfo;
```

Rules:

- `InspectDocumentXml` only parses `documentXmlUtf8`; it does **not** apply parameter overrides, evaluate expressions, or solve constraints
- `SketchInfo.properties` only includes scalar properties: `Float / Integer / Quantity / String / Bool`
- Non-scalar properties such as `Geometry`, `Constraints`, `ExpressionEngine`, `Placement`, and `Shape` are **not** listed in `properties` — geometry elements and constraints are instead exposed via `geometries` and `constraints`
- `geometries` lists every `<Geometry>` element in XML order: internal geometry first (from `Geometry` property), then external geometry (from `ExternalGeo` property). Element order and `originalId` match the solve→export `GeometryRecord` list 1:1
- `constraints` lists every active, non-virtual-space `<Constrain/>` element; `originalIndex` counts all `<Constrain/>` elements in XML order (including inactive/virtual-space) and matches `ConstraintRef::originalIndex` from the solve→export path
- `constraintIndices` / `referencedGeoIds` provide bidirectional cross-references between geometries and constraints
- `VarSetInfo.parameters` includes all scalar parameters, including `Label`, `Label2`, and `Visibility`
- `expressionUtf8` carries the raw VarSet `ExpressionEngine` expression for the same path; it is an empty string when absent
- `propertyXmlUtf8` preserves the original `<Property>...</Property>` snippet for forward compatibility

### 2.5 Geometry Record

```c
typedef struct McSolverEngineGeometryRecord {
    int geometryIndex;          // index in SketchModel geometry list
    int originalId;             // original Document.xml geometry ID
    McSolverEngineGeometryKind kind;
    int construction;           // 1 if construction geometry
    int external;              // 1 if external reference geometry
    int blocked;               // 1 if solver-blocked (fixed)

    // Fields populated based on geometry kind (unused fields are zero/null)
    McSolverEnginePoint2 point;       // Point
    McSolverEnginePoint2 start;       // LineSegment start
    McSolverEnginePoint2 end;         // LineSegment end
    McSolverEnginePoint2 center;      // Circle / Arc / Ellipse / ArcOfEllipse / ArcOfHyperbola
    McSolverEnginePoint2 focus1;      // Ellipse / ArcOfEllipse / ArcOfHyperbola / ArcOfParabola
    McSolverEnginePoint2 vertex;       // ArcOfParabola vertex
    double radius;                    // Circle / Arc
    double minorRadius;               // Ellipse / ArcOfEllipse / ArcOfHyperbola
    double startAngle;                // Arc / ArcOfEllipse / ArcOfHyperbola / ArcOfParabola
    double endAngle;                  // Arc / ArcOfEllipse / ArcOfHyperbola / ArcOfParabola
    int degree;                       // BSpline degree
    int periodic;                     // BSpline periodic flag
    int poleCount;                    // BSpline pole count
    const McSolverEngineBSplinePole* poles;  // BSpline poles array
    int knotCount;                    // BSpline knot count
    const McSolverEngineBSplineKnot* knots;  // BSpline knots array

    // Expression-driven constraint refs (may be 0/null)
    int constraintCount;
    const McSolverEngineConstraintRef* constraints;
} McSolverEngineGeometryRecord;
```

### 2.6 Constraint Reference

```c
typedef struct McSolverEngineConstraintRef {
    McSolverEngineConstraintKind kind;
    int originalIndex;           // index in Document.xml ConstraintList
    const char* expression;      // UTF-8 expression string, or NULL
} McSolverEngineConstraintRef;
```

### 2.7 Result Structs

#### Geometry Result

```c
typedef struct McSolverEngineGeometryResult {
    // --- Input metadata ---
    const char* sketchName;          // UTF-8
    const char* importStatus;        // "Success" | "Partial" | "Failed"
    int skippedConstraints;
    int messageCount;
    char** messages;                 // UTF-8 array of import/process messages

    // --- Solve metadata ---
    const char* solveStatus;         // "Success" | "Converged" | "Failed" | "Invalid" | "Unsupported"
    int degreesOfFreedom;
    int conflictingCount;
    const int* conflicting;           // array of conflicting constraint indices
    int redundantCount;
    const int* redundant;             // array of redundant constraint indices
    int partiallyRedundantCount;
    const int* partiallyRedundant;    // array of partially redundant constraint indices

    // --- Export metadata ---
    const char* exportKind;          // "Geometry"
    const char* exportStatus;        // "Success" | "Failed"

    // --- Results ---
    int varSetPropertyCount;
    const McSolverEngineVarSetProperty* varSetProperties;
    McSolverEnginePlacement placement;
    int geometryCount;
    const McSolverEngineGeometryRecord* geometries;
} McSolverEngineGeometryResult;
```

#### BREP Result

```c
typedef struct McSolverEngineBRepResult {
    // --- Input metadata (same fields as GeometryResult) ---
    const char* sketchName;
    const char* importStatus;
    int skippedConstraints;
    int messageCount;
    char** messages;

    // --- Solve metadata (same fields as GeometryResult) ---
    const char* solveStatus;
    int degreesOfFreedom;
    int conflictingCount;
    const int* conflicting;
    int redundantCount;
    const int* redundant;
    int partiallyRedundantCount;
    const int* partiallyRedundant;

    // --- Export metadata ---
    const char* exportKind;          // "BRep"
    const char* exportStatus;        // "Success" | "Failed" | "OpenCascadeUnavailable"

    // --- Results ---
    int varSetPropertyCount;
    const McSolverEngineVarSetProperty* varSetProperties;
    McSolverEnginePlacement placement;
    const char* brepUtf8;            // full BREP text (UTF-8), or NULL
} McSolverEngineBRepResult;
```

---

## 3. Functions

### 3.1 `McSolverEngine_GetVersion`

```c
const char* McSolverEngine_GetVersion(void);
```

Returns the engine version string (UTF-8). The pointer is statically owned — do not free.

---

### 3.2 `McSolverEngine_SolveToGeometry`

```c
McSolverEngineResultCode McSolverEngine_SolveToGeometry(
    const char* documentXmlUtf8,
    const char* sketchNameUtf8,
    McSolverEngineGeometryResult** result
);
```

Full pipeline: import `Document.xml` → solve with GCS → export structured geometry.

**Parameters**:
- `documentXmlUtf8` — FreeCAD `Document.xml` content as UTF-8 string (not a file path)
- `sketchNameUtf8` — name of the `Sketcher::SketchObject` to solve (e.g. `"Sketch"`, `"Sketch001"`)
- `result` — output, allocated by native layer; caller frees via `McSolverEngine_FreeGeometryResult`

**Pipeline states reflected in `result`**:
- **Import failure**: `importStatus != "Success"`, `exportKind` set to `"Geometry"`, `exportStatus` set to `"Skipped"`, `solveStatus` is `NULL`, `geometryCount = 0`
- **Solve failure**: `importStatus == "Success"`, `solveStatus != "Success"`/"Converged", `exportKind` set to `"Geometry"`, `exportStatus` set to `"Skipped"`, `geometryCount = 0`
- **Export failure**: import and solve succeeded, `exportStatus == "Failed"`, `geometryCount = 0`
- **Success**: import, solve, and export all succeeded; `geometries` populated

Even on failure, the result struct carries metadata (`messages`, `conflicting`, `redundant`, `partiallyRedundant`) for diagnostics.

---

### 3.3 `McSolverEngine_SolveToGeometryWithParameters`

```c
McSolverEngineResultCode McSolverEngine_SolveToGeometryWithParameters(
    const char* documentXmlUtf8,
    const char* sketchNameUtf8,
    const char* const* parameterKeysUtf8,
    const char* const* parameterValuesUtf8,
    int parameterCount,
    McSolverEngineGeometryResult** result
);
```

Same as `SolveToGeometry` but with VarSet parameter overrides.

**Parameter rules**:
- `parameterKeysUtf8` / `parameterValuesUtf8` — parallel arrays of `parameterCount` entries
- Key format: `"VarSetObjectNameOrLabel.PropertyName"` only; bare property names are rejected and reserved for future Sketch-property overrides
- Values must be **pure numeric strings** (e.g. `"8.5"`, `"45"`) — no unit suffixes (`"8.5 mm"` is rejected)
- Length constraints (DistanceX/Y/Distance/Radius/Diameter): values interpreted as **mm**
- Angle constraints (Angle): values interpreted as **degrees** (converted to radians internally)
- Overrides are applied to VarSet data before expression evaluation

**Null handling**: `parameterCount = 0` is equivalent to calling `SolveToGeometry` without parameters, regardless of whether `parameterKeysUtf8` is NULL.

---

### 3.4 `McSolverEngine_SolveToBRep`

```c
McSolverEngineResultCode McSolverEngine_SolveToBRep(
    const char* documentXmlUtf8,
    const char* sketchNameUtf8,
    McSolverEngineBRepResult** result
);
```

Full pipeline: import → solve → export BREP text.

Same semantics as `SolveToGeometry`, except:
- Export produces BREP text (`brepUtf8`) instead of geometry records
- If built without OCCT (`MCSOLVERENGINE_WITH_OCCT=OFF`): `exportStatus = "OpenCascadeUnavailable"`, `brepUtf8 = NULL`, return code `MCSOLVERENGINE_RESULT_OPEN_CASCADE_UNAVAILABLE`

---

### 3.5 `McSolverEngine_SolveToBRepWithParameters`

```c
McSolverEngineResultCode McSolverEngine_SolveToBRepWithParameters(
    const char* documentXmlUtf8,
    const char* sketchNameUtf8,
    const char* const* parameterKeysUtf8,
    const char* const* parameterValuesUtf8,
    int parameterCount,
    McSolverEngineBRepResult** result
);
```

Same as `SolveToBRep` with VarSet parameter overrides (same parameter rules as `SolveToGeometryWithParameters`).

---

### 3.6 `McSolverEngine_InspectDocumentXml`

```c
McSolverEngineResultCode McSolverEngine_InspectDocumentXml(
    const char* documentXmlUtf8,
    McSolverEngineDocumentInfo** result
);
```

Parses `Document.xml` and returns all Sketch/VarSet metadata including geometry element types, construction flags, constraint kinds, and bidirectional cross-references — without running the solver.

- Input is `documentXmlUtf8` only
- No parameter overrides
- No Sketch / VarSet expression evaluation
- No constraint solving
- Each sketch exposes: scalar properties, geometry elements (type, originalId, construction/external flags, constraint cross-references), and constraints (originalIndex, raw type, kind name, driving flag, value, referenced geometry indices)
- Geometry element order matches the solve→export `GeometryRecord` list 1:1
- `InspectConstraint::originalIndex` matches `ConstraintRef::originalIndex` from solve→export (counts every `<Constrain/>` in XML order)
- Returns `MCSOLVERENGINE_RESULT_SUCCESS` on success
- On parse failure returns `MCSOLVERENGINE_RESULT_IMPORT_FAILED`; `result` may still carry partial metadata with `messages` populated
- Returns `MCSOLVERENGINE_RESULT_OUT_OF_MEMORY` on allocation failure

---

### 3.7 `McSolverEngine_FreeGeometryResult`

```c
void McSolverEngine_FreeGeometryResult(McSolverEngineGeometryResult* value);
```

Frees a geometry result and all owned sub-objects (strings, arrays, geometry records, constraint refs, BSpline poles/knots). Passing `NULL` is safe (no-op).

---

### 3.8 `McSolverEngine_FreeBRepResult`

```c
void McSolverEngine_FreeBRepResult(McSolverEngineBRepResult* value);
```

Frees a BREP result and all owned sub-objects. Passing `NULL` is safe (no-op).

---

### 3.9 `McSolverEngine_FreeDocumentInfo`

```c
void McSolverEngine_FreeDocumentInfo(McSolverEngineDocumentInfo* value);
```

Frees a document inspection result and all owned sub-objects. Passing `NULL` is safe (no-op).

---

### 3.10 `McSolverEngine_ExtractFCStdDoc`

```c
McSolverEngineFCStdResultCode McSolverEngine_ExtractFCStdDoc(
    const char* fcstdPathUtf8,
    char** documentXmlOut
);
```

Extracts `Document.xml` from a `.FCStd` file (ZIP archive). Supports STORE (method 0) and DEFLATE (method 8) compression.

**Parameters**:
- `fcstdPathUtf8` — filesystem path to the `.FCStd` file (UTF-8)
- `documentXmlOut` — output, newly allocated UTF-8 string; caller frees via `McSolverEngine_FreeFCStdDoc`

**Implementation note**: The internal ZIP parser and zlib inflate are statically linked with symbol prefix `McSolverEngine_` (no zlib symbols exported from the DLL, zero conflict with other zlib instances in the same process).

---

### 3.11 `McSolverEngine_FreeFCStdDoc`

```c
void McSolverEngine_FreeFCStdDoc(char* documentXml);
```

Frees the string returned by `McSolverEngine_ExtractFCStdDoc`. Passing `NULL` is safe (no-op).

---

### 3.12 `McSolverEngine_GetLastError`

```c
const char* McSolverEngine_GetLastError(void);
```

Returns the last error message for the calling thread (UTF-8, thread-local, 512-byte buffer). Valid until the next API call on the same thread. Returns an empty string if no error occurred.

---

## 4. Processing Pipeline

The `Solve*` functions follow a four-phase pipeline. Phase 0 is optional and only needed when the input is a `.FCStd` file rather than a standalone `Document.xml` string.

```
Phase 0 (optional)        Phase 1         Phase 2        Phase 3
Extract Document.xml  →  Import  →  Solve (GCS)  →  Export (Geometry or BREP)
from FCStd file
```

### Phase 0 (optional): Extract Document.xml from FCStd

When the input is a `.FCStd` file (ZIP archive), first extract the embedded `Document.xml`:

```c
McSolverEngineFCStdResultCode rc = McSolverEngine_ExtractFCStdDoc("sketch.FCStd", &xml);
if (rc != MCSOLVERENGINE_FCSTD_SUCCESS) {
    fprintf(stderr, "Extract error: %s\n", McSolverEngine_GetLastError());
    return;
}
// xml now contains the Document.xml content — feed it to Phase 1
// ... after use, call McSolverEngine_FreeFCStdDoc(xml);
```

Supported compression methods: STORE (method 0) and DEFLATE (method 8). The internal ZIP parser and zlib inflate are statically linked with the symbol prefix `McSolverEngine_` — no zlib symbols are exported from the DLL, guaranteeing zero conflict with other zlib instances in the same process.

If the caller already has `Document.xml` content (e.g. from an external extraction tool, or a standalone `.xml` file), skip this phase and proceed directly to Phase 1.

### Phase 1: Import

Parses `Document.xml` content for the named sketch:
- Recognizes `App::VarSet` objects, their properties, and their `ExpressionEngine`
- Collects numerically evaluable Sketch scalar properties and the Sketch `ExpressionEngine` entries that target those properties
- Parses sketch geometry (`Geometry`, `ExternalGeo`) and constraints (`Constraints`)
- Maps `Placement` for non-XY-plane sketches
- Applies caller-provided VarSet overrides, then evaluates the reduced Sketch/VarSet expression subset in dependency order
- Binds dimensional constraints either to direct VarSet parameters or to reduced-subset expressions resolved from VarSet + Sketch scalar properties

**Import states**:
- `"Success"` — all data imported cleanly
- `"Partial"` — import succeeded with warnings (some constraints skipped)
- `"Failed"` — unrecoverable error

### Phase 2: Solve

Runs GCS constraint solver (DogLeg, with fallback chain: DogLeg → LevenbergMarquardt → BFGS → SQP augmented). Key parameters:
- `convergence = 1e-10`, `maxIter = 100`, `dogLegGaussStep = FullPivLU`

**Solve states**:
- `"Success"` — fully constrained, solved
- `"Converged"` — solver converged (under-constrained but valid)
- `"Failed"` — diverged or numerically failed
- `"Invalid"` — invalid solver input
- `"Unsupported"` — unsupported constraint combination

Redundant/conflicting constraint indices are populated even on partial failure.

### Phase 3: Export

- **Geometry export**: Always available, no OCCT dependency. Returns structured geometry records with `originalId`, `construction`/`external`/`blocked` flags, and expression-driven constraint references.
- **BREP export**: Requires OCCT (`MCSOLVERENGINE_WITH_OCCT=ON`). Produces OCCT `BRepTools_ShapeSet` VERSION_1 text. Construction and external geometry are excluded from BREP output. Non-XY-plane sketches have their `Placement` mapped to root shape location.

---

## 5. OCCT Build Switch (`MCSOLVERENGINE_WITH_OCCT`)

McSolverEngine supports two build configurations controlled by the CMake option `-DMCSOLVERENGINE_WITH_OCCT=ON|OFF`.

### 5.1 Build-time impact

| Build | CMake | OCCT dependency | BREP export |
|-------|-------|-----------------|-------------|
| **With OCCT** | `-DMCSOLVERENGINE_WITH_OCCT=ON` | Links `TKBRep`, `TKTopAlgo`, `TKShHealing` `.lib` files | Available |
| **Without OCCT** | `-DMCSOLVERENGINE_WITH_OCCT=OFF` (default) | No OCCT linkage | Unavailable |

In both configurations the DLL is named `mcsolverengine_native.dll`. Geometry export is **always available** regardless of the OCCT switch.

OCCT is a **runtime dependency** in OCCT-enabled builds: in addition to link-time `.lib` files, the corresponding OCCT `.dll` files must be present in the DLL search path at runtime.

### 5.2 API behavior per configuration

| Function | With OCCT | Without OCCT |
|----------|-----------|--------------|
| `McSolverEngine_GetVersion` | Normal | Normal |
| `McSolverEngine_SolveToGeometry` | Normal | Normal |
| `McSolverEngine_SolveToGeometryWithParameters` | Normal | Normal |
| `McSolverEngine_SolveToBRep` | Returns BREP text in `brepUtf8` | Returns `MCSOLVERENGINE_RESULT_OPEN_CASCADE_UNAVAILABLE`, `exportStatus = "OpenCascadeUnavailable"`, `brepUtf8 = NULL` |
| `McSolverEngine_SolveToBRepWithParameters` | Same as above | Same as above |
| `McSolverEngine_ExtractFCStdDoc` | Normal | Normal |
| `McSolverEngine_GetLastError` | Normal | Normal |
| Free functions | Normal | Normal |

Key points:

- **Geometry functions always work** — they have no OCCT dependency
- **BREP functions fail gracefully** without OCCT — they return a structured result with metadata, an explicit status, and a NULL `brepUtf8`; the caller can inspect `exportStatus` and `messages` as with any other failure
- **Return value is not a crash** — calling BREP functions without OCCT does not crash or throw; the error is surfaced deterministically via the return code

### 5.3 NuGet packages

Two pre-built NuGet variants map directly to the OCCT switch:

| Package | OCCT | BREP |
|---------|------|------|
| `McSolverEngine_UseOcct` | Required at runtime | Available |
| `McSolverEngine_NoOcct` | Not required | Returns `OpenCascadeUnavailable` |

Both packages contain the same C API surface; the difference is only whether OCCT-backed BREP export works.

---

## 6. Parameter Passing Conventions

### Value format

Parameter values passed via `parameterValuesUtf8` must be pure numeric strings:

| Constraint type | Unit | Example |
|-----------------|------|---------|
| DistanceX, DistanceY, Distance | mm | `"8.5"` |
| Radius, Diameter | mm | `"12.0"` |
| Angle | degrees | `"45"` |

Rejected: `"8.5 mm"`, `"45 deg"`, `"1 cm"`.

### Key format

- **Only supported form**: `"VarSetObjectNameOrLabel.PropertyName"` — e.g. `"Config.Width"` or `"Parameters.Width"`
- Bare short keys such as `"Width"` are rejected

### Lookup order

1. Explicit object-name or label key match (`VarSetObjectNameOrLabel.PropertyName`)
2. Fall back to imported default (from VarSet / Sketch property defaults or Document.xml `ExpressionEngine`)

### Reduced Sketch / VarSet expressions

Document.xml `ExpressionEngine` values are evaluated during import for:
- `App::VarSet` properties
- Sketch scalar properties targeted by the Sketch object's own `ExpressionEngine`

The reduced expression subset includes:
- Arithmetic: `+`, `-`, `*`, `/`, `%`, `^` (left-associative)
- Math functions: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `sinh`, `cosh`, `tanh`, `sqrt`, `cbrt`, `pow`, `hypot`, `exp`, `log`, `log10`, `floor`, `ceil`, `round`, `trunc`, `abs`, `min`, `max`, `sum`, `average`, `count`, `mod`
- Constants: `pi`, `e` (case-sensitive)
- References:
  - `Param` within the same VarSet or the same Sketch object
  - `VarSetName.Param` / `<<Label>>.Param`
  - `SketchObjectName.Prop` / `<<Sketch Label>>.Prop`
- Limited units: length (`mm`, `cm`, `m`, `km`, `um`, `nm`, `in`, `ft`) and angle (`deg`, `degree`, `degrees`, `rad`, `radian`, `radians`)
- Cycle detection for the combined Sketch/VarSet property graph

If an expression uses FreeCAD features beyond this subset (spreadsheet refs, geometry properties, complex Quantity/Unit, conditional/logic, etc.), import returns `MCSOLVERENGINE_RESULT_VARSET_EXPRESSION_UNSUPPORTED_SUBSET` with diagnostic message tag `MCSOLVERENGINE_IMPORT_VARSET_EXPRESSION_UNSUPPORTED_SUBSET`.

---

## 7. DLL Discovery & Loading

### File names

| Platform | DLL name |
|----------|----------|
| Windows x64 | `mcsolverengine_native.dll` |
| Linux | `libmcsolverengine_native.so` |
| macOS | `libmcsolverengine_native.dylib` |

### C# Wrapper binding

```csharp
[DllImport("mcsolverengine_native", CallingConvention = CallingConvention.Cdecl)]
```

The C# wrapper (`McSolverEngineClient.cs`) provides:
- `ConfigureNativeLibrary(string path)` / `ConfigureNativeLibraryDirectory(string dir)` — explicit DLL location
- .NET Framework: loads via `kernel32.dll` `LoadLibrary`
- .NET 6+: loads via `NativeLibrary.Load` with OS-aware filename
- Caches the loaded handle; switching to a different native build requires a new process

### Python binding

```python
# _bindings.py — ctypes
_native = ctypes.CDLL(_discover_library_path("mcsolverengine_native"))
```

Auto-discovers the DLL from the package directory or `PATH`.

---

## 8. Memory Management

| Allocation | Free function |
|------------|---------------|
| `McSolverEngineGeometryResult*` | `McSolverEngine_FreeGeometryResult` |
| `McSolverEngineBRepResult*` | `McSolverEngine_FreeBRepResult` |
| `char*` (from ExtractFCStdDoc) | `McSolverEngine_FreeFCStdDoc` |
| `McSolverEngine_GetVersion()` return | Statically owned — do not free |
| `McSolverEngine_GetLastError()` return | Thread-local buffer — do not free |

All result structs and their nested sub-objects (geometry record arrays, BSpline pole/knot arrays, constraint ref arrays, string arrays) are freed by the top-level free function. Callers must not free individual sub-objects.

**After calling a Free function**: the pointer becomes invalid; the caller should set it to `NULL`.

---

## 9. Thread Safety

- `McSolverEngine_GetLastError()` uses a thread-local error buffer — one message per thread
- Each API call clears the thread-local error buffer at entry
- All solve/export pipelines are reentrant (no shared mutable state)
- Result structs are independently allocated per call

---

## 10. Error Handling Pattern

```c
McSolverEngineGeometryResult* result = NULL;
McSolverEngineResultCode rc = McSolverEngine_SolveToGeometry(xml, "Sketch", &result);

if (rc != MCSOLVERENGINE_RESULT_SUCCESS) {
    fprintf(stderr, "Error: %s\n", McSolverEngine_GetLastError());
    // result may still carry partial metadata — inspect if desired
}

if (result) {
    McSolverEngine_FreeGeometryResult(result);
}
```

**Key points**:
- Always check the return code
- On failure, `result` may be non-NULL with partial metadata (messages, conflicting/redundant indices)
- `GetLastError()` is valid only until the next API call
- Always free `result` when non-NULL, regardless of return code
