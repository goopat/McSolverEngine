# Cross-Sketch Cascade Update Design

## 1. FreeCAD Architecture Reference

### 1.1 Key Data Structures (from FreeCAD source)

```
SketchObject:
  ExternalGeometry  App::PropertyLinkSubList     → [(DocumentObject*, subElement), ...]
  ExternalGeo       Part::PropertyGeometryList    → [Part::Geometry*, ...]  (projected copies)
  externalGeoRef    vector<string>                → ["Pad.Edge1", "Box.Face3", ...]  (parallel to ExternalGeometry)
  externalGeoRefMap map<string, vector<long>>    → key → [geometryId, ...]  (one ref → multiple geometries)
  externalGeoMap    map<long, int>                → geometryId → index in ExternalGeo
  geoLastId         long                          → monotonically increasing geometry ID counter
```

### 1.2 ExternalGeometry XML Format

```xml
<Property name="ExternalGeometry" type="App::PropertyLinkSubList">
    <LinkSubList count="2">
        <Link obj="SketchB" sub="Edge1"/>
        <Link obj="SketchB" sub="Edge2"/>
    </LinkSubList>
</Property>
```

Each `<Link>` has:
- `obj`: Source document object name (e.g., `"SketchB"`)
- `sub`: Sub-element name (e.g., `"Edge1"`, `"Edge2"`, `"Vertex3"`)

### 1.3 ExternalGeometryExtension (per-geometry metadata in XML)

```xml
<GeoExtensions count="2">
    <GeoExtension type="Sketcher::SketchGeometryExtension" id="-1" internalGeometryType="0" 
                  geometryModeFlags="00000000000000000000000000000000" geometryLayer="0"/>
    <GeoExtension type="Sketcher::ExternalGeometryExtension" name="" Ref="" Flags="0"/>
</GeoExtensions>
```

Flags (bitset): Defining(0)=allow building shape, Frozen(1)=freeze, Detached(2)=intend detach, Missing(3)=missing ref, Sync(4)=sync frozen.

### 1.4 GeoId Convention

```
GeoId >= 0      → index in Geometry (internal geometry)
GeoId = -1      → Horizontal Axis / Root Point
GeoId = -2      → Vertical Axis
GeoId <= -3     → external geometry: ExternalGeo[(-GeoId) - 1]
GeoId = -2000   → undefined
```

### 1.5 Reference Key Format (externalGeoRef)

```
For Part::Feature:    "ObjectName.ElementName"     e.g. "Pad.Edge1"
For Datum objects:    "ObjectName."                e.g. "DatumPlane."
```

### 1.6 Sketcher Element Naming — How "Edge1" Maps to Geometry

In FreeCAD's Sketcher, every geometry element (LineSegment, Circle, Arc, etc.) is an **edge**.
The Sketcher has its own element type registration (`SketchObject.cpp:11171`):
```cpp
{ "Vertex", "Edge", "ExternalEdge", "Constraint", "InternalEdge", ... }
```

**Edge numbering**: Follows the order in `getCompleteGeometry()`, which is:
1. All internal geometries (GeoId 0, 1, 2, ...)
2. External geometries in reverse order from `ExternalGeo`

"Edge1" → 1st non-construction internal geometry

**Vertex numbering** (from `rebuildVertexIndex()`, line 9244):
| Geometry Type | Vertices |
|---|---|
| GeomPoint | 1: start |
| GeomLineSegment | 2: start, end |
| GeomCircle | 1: mid (center) |
| GeomArcOfCircle | 3: start, end, mid |
| GeomEllipse | 1: mid |
| GeomArcOfEllipse | 3: start, end, mid |
| GeomArcOfHyperbola | 3: start, end, mid |
| GeomArcOfParabola | 3: start, end, mid |

The horizontal and vertical axes (ExternalGeo[0] and [1], GeoId=-1 and -2) are excluded from vertex numbering (iterated `geometry.end() - 2`).

**Resolution chain** for `"SketchB.Edge1"`:
1. `GeoFeature::resolveElement()` walks object hierarchy
2. SketchObject's `getElementName("Edge1")` → `ComplexGeoData::getElementName()`
3. Creates `IndexedName{type="Edge", index=1}`, looks up `MappedName` in ElementMap
4. Resolves to `GeoElementId{GeoId, PointPos}`
5. `_getGeometry(GeoId)` retrieves the actual `Part::Geometry*`

**Mapped element prefix** (`;`-prefixed names like `";:H1234:5,Edge5"`):
- `Data::newElementName()` strips `.Edge5` → `";:H1234:5"`
- `Data::oldElementName()` strips mapped prefix → `"Edge5"`

### 1.7 Cascade Update Flow (FreeCAD)

```
SketchObject::execute()
  → solve()                          // solve with current geometry
  → rebuildExternalGeometry()        // update external geo from sources
  → acceptGeometry()                 // commit

rebuildExternalGeometry(defining, addIntersection):
  1. For each (obj, sub, key) in ExternalGeometry:
     a. Check Frozen/Sync → skip frozen, rebuild synced
     b. Get source TopoDS_Shape from obj → getSubShape(sub)
     c. Project 3D shape → 2D geometry on sketch plane
     d. Set Ref=key on new geometry
     e. Set Defining flag on last geometry
  2. Allocate geometry IDs via externalGeoRefMap
  3. Update ExternalGeo (copyFlags preserves Frozen/Detached)
  4. Mark missing references
  5. Clean up ExternalGeometry if refs removed
  6. rebuildVertexIndex()
```

---

## 2. McSolverEngine Implementation Plan

### Guiding Principles

1. **Keep planegcs unchanged** — per CLAUDE.md convention
2. **Zero new public API** — no new functions, types, or headers exposed; all cascade logic is internal to existing `importSketchFromDocumentXml` + `solveSketch`
3. **Sketch-plane-aware projection** — use `Placement` to compute 2D affine transform between sketch planes (different-plane aware)
4. **Pure linear algebra (no OCCT dependency)** — the projection between two planar coordinate systems is a closed-form affine transform
5. **Backward compatible** — documents without `ExternalGeometry` links behave identically to today

### Phase 1: Parse ExternalGeometry Links → Store in Model

**File:** `src/DocumentXml.cpp`

Parse `<LinkSubList>` entries in `<Property name="ExternalGeometry">`:

```xml
<LinkSubList count="2">
    <Link obj="SketchB" sub="Edge1"/>
    <Link obj="SketchB" sub="Edge2"/>
</LinkSubList>
```

Each `<Link>` corresponds positionally to an `ExternalGeo` geometry element.

**New internal type** (in `include/McSolverEngine/CompatModel.h`):

```cpp
struct ExternalGeometrySource {
    std::string sourceObject;   // "SketchB"
    std::string sourceSub;      // "Edge1"
};

struct Geometry {
    // ... existing fields unchanged ...
    std::optional<ExternalGeometrySource> externalSource;  // NEW
};
```

When `externalSource` has a value, this external geometry is a projected copy of geometry from `sourceObject`.

**Integration point** — in `importSketchFromDocumentXml()`, after importing ExternalGeo (line 1710):

```cpp
auto externalLinks = parseExternalGeometryLinks(object->content);
if (!externalLinks.empty()) {
    size_t linkIdx = 0;
    for (auto& geo : result.model.geometries()) {
        if (geo.external && linkIdx < externalLinks.size()) {
            geo.externalSource = ExternalGeometrySource{
                .sourceObject = externalLinks[linkIdx].sourceObject,
                .sourceSub = externalLinks[linkIdx].sourceSub,
            };
            ++linkIdx;
        }
    }
}
```

Defensive: if no `ExternalGeometry` property, or `count="0"`, or malformed XML → empty vector → no change to behavior.

### Phase 2: SketchModel Stores Dependency Context

**File:** `include/McSolverEngine/CompatModel.h`

Add private members to `SketchModel`:

```cpp
class SketchModel {
public:
    // ... existing public API unchanged ...

private:
    std::vector<Geometry> geometries_;
    std::vector<Constraint> constraints_;
    Placement placement_;

    // ──── NEW: cascade context ────
    // Dependency sketches imported from the same Document.xml.
    // Key: source object name (e.g. "SketchB").
    // Populated by importSketchFromDocumentXml when ExternalGeometry
    // links reference other sketches in the same document.
    std::unordered_map<std::string, SketchModel> dependencyModels_;

    // Topologically sorted dependency names.
    // Empty when there are no cross-sketch references.
    std::vector<std::string> dependencySolveOrder_;

    friend SolveResult solveSketch(SketchModel& model);
    friend class DocumentImportContext;  // for populate during import
};
```

### Phase 3: Import All Dependencies Recursively

**File:** `src/DocumentXml.cpp` — modifies `importSketchFromDocumentXml()`

**New internal helpers** (anonymous namespace or static):

```cpp
// Look up an ObjectBlock by object name in the already-collected blocks
const ObjectBlock* findObjectBlockByName(
    const std::vector<ObjectBlock>& blocks,
    std::string_view name
);

// Import a single ObjectBlock into a SketchModel.
// Extracts the import logic currently inlined in importSketchFromDocumentXml.
// This is a refactor: the existing code that populates result.model from
// object->content is moved into this function, so it can be reused for
// dependency sketches.
ImportResult importSingleObjectBlock(
    const ObjectBlock& object,
    const std::vector<ObjectBlock>& allBlocks,
    const std::unordered_map<std::string, std::string>& objectTypes,
    const Detail::ParsedApiParameterMap& parsedParameters
);
```

**Integration** — after the requested sketch is imported and `externalSource` assigned:

```cpp
// ── Recursively import dependency sketches ──
// Use DFS to discover and import all transitive dependencies
std::function<void(const std::string&)> importDeps;
std::unordered_set<std::string> imported;

importDeps = [&](const std::string& srcName) {
    if (imported.count(srcName)) return;
    
    auto* srcBlock = findObjectBlockByName(objectBlocks, srcName);
    if (!srcBlock) return;  // reference to non-existent or non-sketch object
    
    auto srcResult = importSingleObjectBlock(*srcBlock, objectBlocks, objectTypes, parsedParameters);
    if (!srcResult.imported()) return;
    
    // First, recursively import this dependency's own dependencies
    for (const auto& geo : srcResult.model.geometries()) {
        if (geo.externalSource.has_value()) {
            importDeps(geo.externalSource->sourceObject);
        }
    }
    
    imported.insert(srcName);
    result.model.dependencyModels_.emplace(srcName, std::move(srcResult.model));
};

// Kick off DFS from each direct dependency
for (const auto& geo : result.model.geometries()) {
    if (geo.externalSource.has_value()) {
        importDeps(geo.externalSource->sourceObject);
    }
}

// Build topological solve order (DFS post-order = reverse topological)
// with cycle detection
std::unordered_set<std::string> visited, inStack;
std::vector<std::string> order;
bool hasCycle = false;

std::function<void(const std::string&)> topoDfs =
    [&](const std::string& name) {
        if (hasCycle) return;
        if (inStack.count(name)) { hasCycle = true; return; }
        if (visited.count(name)) return;
        
        inStack.insert(name);
        auto it = result.model.dependencyModels_.find(name);
        if (it != result.model.dependencyModels_.end()) {
            // Visit sub-dependencies first (post-order)
            for (const auto& geo : it->second.geometries()) {
                if (geo.externalSource.has_value()) {
                    topoDfs(geo.externalSource->sourceObject);
                }
            }
        }
        inStack.erase(name);
        visited.insert(name);
        order.push_back(name);
    };

for (const auto& [name, _] : result.model.dependencyModels_) {
    topoDfs(name);
}

if (!hasCycle) {
    result.model.dependencySolveOrder_ = std::move(order);
}
// If cycle detected, dependencySolveOrder_ stays empty; 
// solveSketch will fall through to existing logic (no cascade)
```

**Key design points:**
- `importSingleObjectBlock` is a refactor — it extracts existing code from `importSketchFromDocumentXml`, no new import logic
- DFS ensures transitive dependencies (B depends on C → all three imported)
- Topological sort with cycle detection at import time avoids repeated work at solve time
- Empty `dependencySolveOrder_` means either no deps or cycle detected — both degrade gracefully to non-cascade solve

### Phase 4: Internal Utilities (Not Exposed)

Three internal implementation files, all in `src/` only:

**`src/PlaneTransform.h/.cpp`** — 2D affine transform between sketch planes (same derivation as before).

**`src/SubElementResolver.h/.cpp`** — maps `"Edge1"` to geometry index in source model.

**`src/DocumentSolver.h/.cpp`** — contains two internal functions:

```cpp
// Internal: not in public headers
namespace McSolverEngine::Compat {

/// Topological sort of dependency models
std::vector<std::string> computeSolveOrder(
    const std::unordered_map<std::string, SketchModel>& deps
);

/// Update external geometry in `dependent` from solved `source` model.
/// Called automatically by solveSketch before solving the dependent.
void updateExternalGeometry(
    SketchModel& dependent,
    const std::string& sourceObjectName,
    const SketchModel& solvedSource
);

}  // namespace
```

### Phase 5: Cascade Logic in solveSketch

**File:** `src/CompatSolver.cpp` — modify `solveSketch(SketchModel& model)`:

```cpp
SolveResult solveSketch(SketchModel& model) {
    // ──── NEW: cascade preamble ────
    // dependencySolveOrder_ is non-empty only when:
    //   (a) external geometry links were parsed successfully, AND
    //   (b) dependency sketches were found and imported, AND
    //   (c) no cycles were detected in the dependency graph
    if (!model.dependencySolveOrder_.empty()) {
        // dependencySolveOrder_ is already in topological order (DFS post-order)
        for (const auto& depName : model.dependencySolveOrder_) {
            auto it = model.dependencyModels_.find(depName);
            if (it == model.dependencyModels_.end()) continue;
            
            // Recursive solve: handles transitive dependencies automatically
            // (it->second may have its own non-empty dependencySolveOrder_)
            auto depResult = solveSketch(it->second);
            if (!depResult.solved()) {
                return {SolveStatus::Failed, -1, {}, {}, {}};
            }
        }
        
        // All sources solved — now update external geometry in this model
        for (const auto& depName : model.dependencySolveOrder_) {
            auto it = model.dependencyModels_.find(depName);
            if (it == model.dependencyModels_.end()) continue;
            updateExternalGeometry(model, depName, it->second);
        }
    }
    // Note: if dependencySolveOrder_ is empty, this could mean:
    //   (a) No external references → correct, no cascade needed
    //   (b) External refs exist but dep import or topo sort failed →
    //       falls through to existing solve with stale external geo
    //       (caller can inspect model.geometries() for externalSource)
    
    // ──── Existing solve logic unchanged below ────
    ...
}
```

### Phase 6: Testing

Same test cases as before (basic cascade, multi-level, diamond, cycle, missing ref, backward compat, different-plane).

All tests call only existing public API:

```cpp
auto imported = importSketchFromDocumentXml(xml, "SketchA");
auto result = solveSketch(imported.model);
// Verify result and imported.model.geometries() after solve
```

---

## 3. Key Design Decisions

### 3.1 Zero New Public API

All cascade logic is internal:

| What | Where | Visibility |
|---|---|---|
| Parse `ExternalGeometry` links | `DocumentXml.cpp` | internal |
| `ExternalGeometrySource` | `CompatModel.h`, inside `Geometry` | public struct, but nested in existing type |
| `dependencyModels_`, `dependencySolveOrder_` | `CompatModel.h`, in `SketchModel` | **private** |
| PlaneTransform, SubElementResolver | `src/PlaneTransform.h`, etc. | **internal (not in include/)** |
| `updateExternalGeometry` | `src/DocumentSolver.h` | **internal (not in include/)** |
| Cascade preamble in `solveSketch` | `CompatSolver.cpp` | existing public function, extended |

External callers use the same two functions as always:
```cpp
auto r = importSketchFromDocumentXml(xml, "SketchA");  // unchanged signature
auto s = solveSketch(r.model);                          // unchanged signature
// Cascade happens automatically inside solveSketch
```

### 3.2 2D Affine Transform Between Sketch Planes

Same derivation as before. No OCCT dependency.

### 3.3 No New Save/Export

Existing BREP, Geometry, XML export unchanged. `solveSketch` mutates the model in-place (as it already does); dependency models live inside the same `SketchModel` and are transparent to callers.

---

## 4. File Summary

| File | Action |
|---|---|
| `include/McSolverEngine/CompatModel.h` | Edit: `ExternalGeometrySource` + `Geometry.externalSource` + private `dependencyModels_`/`dependencySolveOrder_` |
| `src/DocumentXml.cpp` | Edit: parse `ExternalGeometry` links; recursive dependency import |
| `src/PlaneTransform.h` | New (internal) |
| `src/PlaneTransform.cpp` | New (internal) |
| `src/SubElementResolver.h` | New (internal) |
| `src/SubElementResolver.cpp` | New (internal) |
| `src/DocumentSolver.h` | New (internal): `computeSolveOrder`, `updateExternalGeometry` |
| `src/DocumentSolver.cpp` | New (internal) |
| `src/CompatSolver.cpp` | Edit: cascade preamble in `solveSketch` |
| `tests/smoke.cpp` | Edit: cascade test cases |
| `tests/unit.cpp` | Edit: unit tests |
| `CMakeLists.txt` | Edit: add new source files |


---

## 4. Implementation Order & Dependencies

```
Phase 1 (parse links)  ──┐
Phase 2 (model)       ──┤
Phase 3 (plane math)  ──┼──> Phase 5 (cascade solve + updateExternalGeometry)
Phase 4 (sub-element) ──┘
                              Tests ── after all phases
```

Phase 1-4 are independent.
Phase 5 depends on 2+3+4.

---

## 5. File Summary

| File | Action |
|---|---|
| `include/McSolverEngine/CompatModel.h` | Edit: `ExternalGeometrySource` + `Geometry.externalSource` + private `dependencyModels_`/`dependencySolveOrder_` |
| `src/DocumentXml.cpp` | Edit: parse `ExternalGeometry` links; `findObjectBlockByName`; `importSingleObjectBlock`; recursive dependency import |
| `src/PlaneTransform.h` | New (internal): `PlaneTransform`, `quaternionToRotationMatrix` |
| `src/PlaneTransform.cpp` | New (internal) |
| `src/SubElementResolver.h` | New (internal): `resolveSubElementToGeoIndex`, `extractSimpleElementName` |
| `src/SubElementResolver.cpp` | New (internal) |
| `src/DocumentSolver.h` | New (internal): `updateExternalGeometry` |
| `src/DocumentSolver.cpp` | New (internal): external geometry update + `computeSolveOrder` (DFS with cycle detection) |
| `src/CompatSolver.cpp` | Edit: cascade preamble in `solveSketch` |
| `tests/smoke.cpp` | Edit: cascade test cases |
| `tests/unit.cpp` | Edit: unit tests |
| `CMakeLists.txt` | Edit: add new source files |

---

## 6. Addendum: Real FreeCAD 1.0.1 Documents (2026-07-21)

The original design was written against hand-simplified Document.xml.
Regression testing with real FreeCAD 1.0.1 documents (`fcstdDoc/V101.Cascade*.FCStd`)
required three corrections; the implementation now handles both formats.

### 6.1 External geometry constraint ids are positional

In-memory, constraints reference external geometry by **position** in the
`ExternalGeo` list, not by the XML `id` attribute:

```
ExternalGeo[0] → geo id -1 (H axis)     ExternalGeo[2] → geo id -3
ExternalGeo[1] → geo id -2 (V axis)     ExternalGeo[3] → geo id -4 ...
```

(FreeCAD: `_getGeometry`, `GeoEnum::RefExt`.) The XML `id` attribute carries
the persistent element-map id instead (e.g. `id="75"`). Import registers
both mappings in `externalGeometryMap`.

### 6.2 H/V axes are always ExternalGeo[0]/[1]

Real documents always store the sketch H/V axes as the first two
`ExternalGeo` entries (ids -1/-2, construction lines). They are **not**
part of the `ExternalGeometry` link list, so the ordered link pairing
skips them (detected by `originalId == -1 && -2` on the first two
external entries). Hand-written documents without axes are unaffected.

### 6.3 Topological-naming refs are authoritative

Real documents bind each external geometry to its source element with a
`ref` attribute on the `<Geometry>` element:

```xml
<Geometry type="Part::GeomPoint" id="1" ref="Sketch002.;g3v2;SKT" ...>
```

The g-id matches the source geometry's persistent id (saved as its XML
`id` attribute); `v` selects a vertex (`v1`=start, `v2`=end, `v3`=mid).
The `Edge`/`Vertex` names in the `<Link>` elements may be **stale** (they
are element-map display names from an older sketch state), so the ref
overrides the link pairing wherever present. `SubElementResolver`
resolves `gN` (edge by `originalId`) and `gNvM` (vertex + role)
directly; `isVertexSubElementName()` covers both naming styles.

### 6.4 Regression: V101.Cascade

`tests/smoke.cpp` imports `fcstdDoc/V101.Cascade.xml` with the four VarSet
parameters from `V101.Cascade.solver.FCStd` (the FreeCAD recompute) and
compares exported BREPs token-by-token for Sketch/Sketch001/Sketch002.
Sketch003's internal line chain admits multiple solution branches (the
downstream zigzag folds either way; the cascaded arc and external point
match FreeCAD exactly), so its BREP comparison is non-fatal — same
policy as V111.10 — with an explicit assertion on the cascaded external
point coordinates instead.
