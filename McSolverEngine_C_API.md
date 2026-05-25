# McSolverEngine C API 参考手册

## 1. 概述

McSolverEngine 通过 `mcsolverengine_native.dll`（Windows）/ `libmcsolverengine_native.so`（Linux）/ `libmcsolverengine_native.dylib`（macOS）对外提供稳定的 C ABI。接口声明位于 `include/McSolverEngine/CApi.h`。

**设计原则**：
- 所有 `char*` 参数和返回值均使用 **UTF-8 编码**
- 所有结果结构体由原生层分配内存；调用方必须通过对应的 `McSolverEngine_Free*` 函数释放
- 所有 C++ 异常在内部捕获并转换为错误码 + `GetLastError()` 诊断信息 — 无异常穿透 C 边界
- `GetLastError()` 返回线程局部缓冲区（512 字节），在同一线程的下一次 API 调用前有效

---

## 2. 类型定义

### 2.1 结果码

#### `McSolverEngineResultCode` — 求解/导出管线返回码

| 值 | 枚举 | 含义 |
|---|------|------|
| 0 | `MCSOLVERENGINE_RESULT_SUCCESS` | 管线执行成功 |
| 1 | `MCSOLVERENGINE_RESULT_INVALID_ARGUMENT` | 参数为空或无效 |
| 2 | `MCSOLVERENGINE_RESULT_IMPORT_FAILED` | Document.xml 解析失败 |
| 3 | `MCSOLVERENGINE_RESULT_SOLVE_FAILED` | GCS 求解器失败或发散 |
| 4 | `MCSOLVERENGINE_RESULT_UNSUPPORTED` | 草图包含不支持的约束/几何 |
| 5 | `MCSOLVERENGINE_RESULT_GEOMETRY_EXPORT_FAILED` | 几何导出失败 |
| 6 | `MCSOLVERENGINE_RESULT_BREP_EXPORT_FAILED` | BREP 导出失败 |
| 7 | `MCSOLVERENGINE_RESULT_OPEN_CASCADE_UNAVAILABLE` | 未链接 OCCT（BREP 不可用） |
| 8 | `MCSOLVERENGINE_RESULT_OUT_OF_MEMORY` | 内存分配失败 |
| 9 | `MCSOLVERENGINE_RESULT_VARSET_EXPRESSION_UNSUPPORTED_SUBSET` | VarSet 表达式使用了不支持的 FreeCAD 表达式特性 |

#### `McSolverEngineFCStdResultCode` — FCStd 提取返回码

| 值 | 枚举 | 含义 |
|---|------|------|
| 0 | `MCSOLVERENGINE_FCSTD_SUCCESS` | 提取成功 |
| 1 | `MCSOLVERENGINE_FCSTD_OPEN_FAILED` | 文件未找到或读取错误 |
| 2 | `MCSOLVERENGINE_FCSTD_NOT_ZIP` | 不是有效的 ZIP 归档 |
| 3 | `MCSOLVERENGINE_FCSTD_XML_NOT_FOUND` | 归档中未找到 Document.xml |
| 4 | `MCSOLVERENGINE_FCSTD_DECOMPRESS_FAILED` | DEFLATE 解压错误 |
| 5 | `MCSOLVERENGINE_FCSTD_OUT_OF_MEMORY` | 内存分配失败 |

### 2.2 几何类型

```c
typedef enum McSolverEngineGeometryKind {
    MCSOLVERENGINE_GEOMETRY_POINT            = 0,  // 点
    MCSOLVERENGINE_GEOMETRY_LINE_SEGMENT     = 1,  // 线段
    MCSOLVERENGINE_GEOMETRY_CIRCLE           = 2,  // 圆
    MCSOLVERENGINE_GEOMETRY_ARC              = 3,  // 圆弧
    MCSOLVERENGINE_GEOMETRY_ELLIPSE          = 4,  // 椭圆
    MCSOLVERENGINE_GEOMETRY_ARC_OF_ELLIPSE   = 5,  // 椭圆弧
    MCSOLVERENGINE_GEOMETRY_ARC_OF_HYPERBOLA = 6,  // 双曲线弧
    MCSOLVERENGINE_GEOMETRY_ARC_OF_PARABOLA  = 7,  // 抛物线弧
    MCSOLVERENGINE_GEOMETRY_BSPLINE          = 8   // B 样条
} McSolverEngineGeometryKind;
```

### 2.3 约束类型

```c
typedef enum McSolverEngineConstraintKind {
    MCSOLVERENGINE_CONSTRAINT_COINCIDENT         = 0,  // 重合
    MCSOLVERENGINE_CONSTRAINT_HORIZONTAL         = 1,  // 水平
    MCSOLVERENGINE_CONSTRAINT_VERTICAL           = 2,  // 垂直
    MCSOLVERENGINE_CONSTRAINT_DISTANCE_X         = 3,  // 水平距离
    MCSOLVERENGINE_CONSTRAINT_DISTANCE_Y         = 4,  // 垂直距离
    MCSOLVERENGINE_CONSTRAINT_DISTANCE           = 5,  // 距离
    MCSOLVERENGINE_CONSTRAINT_PARALLEL           = 6,  // 平行
    MCSOLVERENGINE_CONSTRAINT_TANGENT            = 7,  // 相切
    MCSOLVERENGINE_CONSTRAINT_PERPENDICULAR      = 8,  // 垂直
    MCSOLVERENGINE_CONSTRAINT_ANGLE              = 9,  // 角度
    MCSOLVERENGINE_CONSTRAINT_RADIUS             = 10, // 半径
    MCSOLVERENGINE_CONSTRAINT_DIAMETER           = 11, // 直径
    MCSOLVERENGINE_CONSTRAINT_EQUAL              = 12, // 相等
    MCSOLVERENGINE_CONSTRAINT_SYMMETRIC          = 13, // 对称
    MCSOLVERENGINE_CONSTRAINT_POINT_ON_OBJECT    = 14, // 点在对象上
    MCSOLVERENGINE_CONSTRAINT_INTERNAL_ALIGNMENT = 15, // 内部对齐
    MCSOLVERENGINE_CONSTRAINT_SNELLS_LAW         = 16, // 斯涅尔定律
    MCSOLVERENGINE_CONSTRAINT_BLOCK              = 17, // 固定
    MCSOLVERENGINE_CONSTRAINT_WEIGHT             = 18  // 权重
} McSolverEngineConstraintKind;
```

### 2.4 基础结构体

```c
typedef struct McSolverEnginePoint2 {
    double x;
    double y;
} McSolverEnginePoint2;

typedef struct McSolverEnginePlacement {
    double px;   // 基点 X
    double py;   // 基点 Y
    double pz;   // 基点 Z
    double qx;   // 旋转四元数 X
    double qy;   // 旋转四元数 Y
    double qz;   // 旋转四元数 Z
    double qw;   // 旋转四元数 W
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

### 2.5 几何记录

```c
typedef struct McSolverEngineGeometryRecord {
    int geometryIndex;          // SketchModel 几何列表中的索引
    int originalId;             // Document.xml 中的原始几何 ID
    McSolverEngineGeometryKind kind;
    int construction;           // 1 表示构造几何
    int external;              // 1 表示外部参考几何
    int blocked;               // 1 表示求解器锁定（固定）

    // 以下字段根据几何类型填充（未使用的字段为零值/null）
    McSolverEnginePoint2 point;       // Point
    McSolverEnginePoint2 start;       // LineSegment 起点
    McSolverEnginePoint2 end;         // LineSegment 终点
    McSolverEnginePoint2 center;      // Circle / Arc / Ellipse / ArcOfEllipse / ArcOfHyperbola
    McSolverEnginePoint2 focus1;      // Ellipse / ArcOfEllipse / ArcOfHyperbola / ArcOfParabola
    McSolverEnginePoint2 vertex;       // ArcOfParabola 顶点
    double radius;                    // Circle / Arc
    double minorRadius;               // Ellipse / ArcOfEllipse / ArcOfHyperbola
    double startAngle;                // Arc / ArcOfEllipse / ArcOfHyperbola / ArcOfParabola
    double endAngle;                  // Arc / ArcOfEllipse / ArcOfHyperbola / ArcOfParabola
    int degree;                       // BSpline 阶数
    int periodic;                     // BSpline 周期标志
    int poleCount;                    // BSpline 极点数量
    const McSolverEngineBSplinePole* poles;  // BSpline 极点数组
    int knotCount;                    // BSpline 节点数量
    const McSolverEngineBSplineKnot* knots;  // BSpline 节点数组

    // 表达式驱动的约束引用（可能为 0/null）
    int constraintCount;
    const McSolverEngineConstraintRef* constraints;
} McSolverEngineGeometryRecord;
```

### 2.6 约束引用

```c
typedef struct McSolverEngineConstraintRef {
    McSolverEngineConstraintKind kind;
    int originalIndex;           // 在 Document.xml ConstraintList 中的索引
    const char* expression;      // UTF-8 表达式字符串，可为 NULL
} McSolverEngineConstraintRef;
```

### 2.7 结果结构体

#### Geometry 结果

```c
typedef struct McSolverEngineGeometryResult {
    // --- 导入元数据 ---
    const char* sketchName;          // UTF-8
    const char* importStatus;        // "Success" | "Partial" | "Failed"
    int skippedConstraints;
    int messageCount;
    char** messages;                 // UTF-8 导入/处理消息数组

    // --- 求解元数据 ---
    const char* solveStatus;         // "Success" | "Converged" | "Failed" | "Invalid" | "Unsupported"
    int degreesOfFreedom;
    int conflictingCount;
    const int* conflicting;           // 冲突约束索引数组
    int redundantCount;
    const int* redundant;             // 冗余约束索引数组
    int partiallyRedundantCount;
    const int* partiallyRedundant;    // 部分冗余约束索引数组

    // --- 导出元数据 ---
    const char* exportKind;          // "Geometry"
    const char* exportStatus;        // "Success" | "Failed"

    // --- 结果 ---
    McSolverEnginePlacement placement;
    int geometryCount;
    const McSolverEngineGeometryRecord* geometries;
} McSolverEngineGeometryResult;
```

#### BREP 结果

```c
typedef struct McSolverEngineBRepResult {
    // --- 导入元数据（字段与 GeometryResult 相同）---
    const char* sketchName;
    const char* importStatus;
    int skippedConstraints;
    int messageCount;
    char** messages;

    // --- 求解元数据（字段与 GeometryResult 相同）---
    const char* solveStatus;
    int degreesOfFreedom;
    int conflictingCount;
    const int* conflicting;
    int redundantCount;
    const int* redundant;
    int partiallyRedundantCount;
    const int* partiallyRedundant;

    // --- 导出元数据 ---
    const char* exportKind;          // "BRep"
    const char* exportStatus;        // "Success" | "Failed" | "OpenCascadeUnavailable"

    // --- 结果 ---
    McSolverEnginePlacement placement;
    const char* brepUtf8;            // 完整 BREP 文本（UTF-8），可为 NULL
} McSolverEngineBRepResult;
```

---

## 3. 函数

### 3.1 `McSolverEngine_GetVersion`

```c
const char* McSolverEngine_GetVersion(void);
```

返回引擎版本字符串（UTF-8）。指针为静态持有 — 不得释放。

---

### 3.2 `McSolverEngine_SolveToGeometry`

```c
McSolverEngineResultCode McSolverEngine_SolveToGeometry(
    const char* documentXmlUtf8,
    const char* sketchNameUtf8,
    McSolverEngineGeometryResult** result
);
```

完整管线：导入 `Document.xml` → GCS 求解 → 导出结构化几何。

**参数**：
- `documentXmlUtf8` — FreeCAD `Document.xml` 内容，UTF-8 字符串（不是文件路径）
- `sketchNameUtf8` — 要求解的 `Sketcher::SketchObject` 名称（如 `"Sketch"`、`"Sketch001"`）
- `result` — 输出，由原生层分配；调用方通过 `McSolverEngine_FreeGeometryResult` 释放

**`result` 中反映的管线状态**：
- **导入失败**：`importStatus != "Success"`，`solveStatus`/`exportKind`/`exportStatus` 设为 `"Skipped"`，`geometryCount = 0`
- **求解失败**：`importStatus == "Success"`，`solveStatus != "Success"`/`"Converged"`，导出跳过，`geometryCount = 0`
- **导出失败**：导入和求解成功，`exportStatus == "Failed"`，`geometryCount = 0`
- **成功**：导入、求解、导出全部成功；`geometries` 已填充

即使失败，结果结构体仍携带诊断元数据（`messages`、`conflicting`、`redundant`、`partiallyRedundant`）。

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

与 `SolveToGeometry` 相同，但支持 VarSet 参数覆盖。

**参数规则**：
- `parameterKeysUtf8` / `parameterValuesUtf8` — 各含 `parameterCount` 个条目的并行数组
- 键格式：`"VarSetName.PropertyName"`（全名键，推荐）或 `"PropertyName"`（短名键，多个 VarSet 共享同名属性时存在歧义）
- 值必须为**纯数值字符串**（如 `"8.5"`、`"45"`）— 不接受单位后缀（`"8.5 mm"` 会被拒绝）
- 长度约束（DistanceX/Y/Distance/Radius/Diameter）：值按 **mm** 解析
- 角度约束（Angle）：值按 **degree** 解析（内部转换为弧度）
- 覆盖值在表达式求值之前写入 VarSet 数据

**空参数处理**：`parameterCount = 0` 或 `parameterKeysUtf8 = NULL` 且 `result` 有效时，等价于调用不带参数的 `SolveToGeometry`。

---

### 3.4 `McSolverEngine_SolveToBRep`

```c
McSolverEngineResultCode McSolverEngine_SolveToBRep(
    const char* documentXmlUtf8,
    const char* sketchNameUtf8,
    McSolverEngineBRepResult** result
);
```

完整管线：导入 → 求解 → 导出 BREP 文本。

语义与 `SolveToGeometry` 相同，区别在于：
- 导出产品为 BREP 文本（`brepUtf8`），而非几何记录
- 如果构建时未启用 OCCT（`MCSOLVERENGINE_WITH_OCCT=OFF`）：`exportStatus = "OpenCascadeUnavailable"`，`brepUtf8 = NULL`，返回码 `MCSOLVERENGINE_RESULT_OPEN_CASCADE_UNAVAILABLE`

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

与 `SolveToBRep` 相同，支持 VarSet 参数覆盖（参数规则与 `SolveToGeometryWithParameters` 相同）。

---

### 3.6 `McSolverEngine_FreeGeometryResult`

```c
void McSolverEngine_FreeGeometryResult(McSolverEngineGeometryResult* value);
```

释放 geometry 结果及其所有子对象（字符串、数组、几何记录、约束引用、BSpline 极点/节点）。传入 `NULL` 安全（无操作）。

---

### 3.7 `McSolverEngine_FreeBRepResult`

```c
void McSolverEngine_FreeBRepResult(McSolverEngineBRepResult* value);
```

释放 BREP 结果及其所有子对象。传入 `NULL` 安全（无操作）。

---

### 3.8 `McSolverEngine_ExtractFCStdDoc`

```c
McSolverEngineFCStdResultCode McSolverEngine_ExtractFCStdDoc(
    const char* fcstdPathUtf8,
    char** documentXmlOut
);
```

从 `.FCStd` 文件（ZIP 归档）中提取 `Document.xml`。支持 STORE（方法 0）和 DEFLATE（方法 8）两种压缩方式。

**参数**：
- `fcstdPathUtf8` — `.FCStd` 文件的文件系统路径（UTF-8）
- `documentXmlOut` — 输出，新分配的 UTF-8 字符串；调用方通过 `McSolverEngine_FreeFCStdDoc` 释放

**实现说明**：内置 ZIP 解析器和 zlib inflate 以符号前缀 `McSolverEngine_` 静态链接（DLL 不导出任何 zlib 符号，与同进程内其他 zlib 实例零冲突）。

---

### 3.9 `McSolverEngine_FreeFCStdDoc`

```c
void McSolverEngine_FreeFCStdDoc(char* documentXml);
```

释放 `McSolverEngine_ExtractFCStdDoc` 返回的字符串。传入 `NULL` 安全（无操作）。

---

### 3.10 `McSolverEngine_GetLastError`

```c
const char* McSolverEngine_GetLastError(void);
```

返回当前线程最近一次 API 调用的错误信息（UTF-8，线程局部，512 字节缓冲区）。在同一线程的下一次 API 调用前有效。无错误时返回空字符串。

---

## 4. 处理管线

`Solve*` 函数遵循四阶段管线。阶段 0 为可选项，仅在输入为 `.FCStd` 文件（而非独立的 `Document.xml` 字符串）时需要。

```
阶段 0（可选）         阶段 1        阶段 2         阶段 3
从 FCStd 文件中     →  导入   →  求解 (GCS)  →  导出 (Geometry 或 BREP)
提取 Document.xml
```

### 阶段 0（可选）：从 FCStd 文件中提取 Document.xml

当输入为 `.FCStd` 文件（ZIP 归档）时，首先提取其中内嵌的 `Document.xml`：

```c
McSolverEngineFCStdResultCode rc = McSolverEngine_ExtractFCStdDoc("sketch.FCStd", &xml);
if (rc != MCSOLVERENGINE_FCSTD_SUCCESS) {
    fprintf(stderr, "提取错误: %s\n", McSolverEngine_GetLastError());
    return;
}
// xml 现在包含 Document.xml 内容 — 送入阶段 1
// ... 使用完毕后，调用 McSolverEngine_FreeFCStdDoc(xml) 释放
```

支持的压缩方法：STORE（方法 0）和 DEFLATE（方法 8）。内置 ZIP 解析器和 zlib inflate 以符号前缀 `McSolverEngine_` 静态链接 — DLL 不导出任何 zlib 符号，与同进程内其他 zlib 实例零冲突。

如果调用方已有 `Document.xml` 内容（例如来自外部解压工具或独立的 `.xml` 文件），可跳过此阶段，直接进入阶段 1。

### 阶段 1：导入（Import）

解析 `Document.xml` 内容中指定名称的草图：
- 识别 `App::VarSet` 对象、其属性及 `ExpressionEngine`
- 解析草图几何（`Geometry`、`ExternalGeo`）和约束（`Constraints`）
- 映射非 XY 平面草图的 `Placement`
- 求值 VarSet 表达式，应用调用方提供的参数覆盖
- 将表达式驱动的维度约束绑定到 VarSet 参数

**导入状态**：
- `"Success"` — 全部数据正常导入
- `"Partial"` — 导入成功但存在警告（部分约束被跳过）
- `"Failed"` — 不可恢复的错误

### 阶段 2：求解（Solve）

运行 GCS 约束求解器（主求解器 DogLeg，回退链：DogLeg → LevenbergMarquardt → BFGS → SQP augmented）。关键参数：
- `convergence = 1e-10`，`maxIter = 100`，`dogLegGaussStep = FullPivLU`

**求解状态**：
- `"Success"` — 完全约束，求解成功
- `"Converged"` — 求解器收敛（欠约束但有效）
- `"Failed"` — 发散或数值失败
- `"Invalid"` — 求解器输入无效
- `"Unsupported"` — 不支持的约束组合

冗余/冲突约束索引即使在部分失败时也会填充。

### 阶段 3：导出（Export）

- **Geometry 导出**：始终可用，无 OCCT 依赖。返回结构化几何记录，包含 `originalId`、`construction`/`external`/`blocked` 标志，以及表达式驱动的约束引用。
- **BREP 导出**：需要 OCCT（`MCSOLVERENGINE_WITH_OCCT=ON`）。生成 OCCT `BRepTools_ShapeSet` VERSION_1 文本。构造几何和外部几何不包含在 BREP 输出中。非 XY 平面草图的 `Placement` 映射到根 shape location。

---

## 5. OCCT 编译开关（`MCSOLVERENGINE_WITH_OCCT`）

McSolverEngine 支持两种构建配置，由 CMake 选项 `-DMCSOLVERENGINE_WITH_OCCT=ON|OFF` 控制。

### 5.1 编译期影响

| 构建 | CMake | OCCT 依赖 | BREP 导出 |
|------|-------|-----------|-----------|
| **含 OCCT** | `-DMCSOLVERENGINE_WITH_OCCT=ON` | 链接 `TKernel`、`TKMath`、`TKBRep` `.lib` | 可用 |
| **不含 OCCT** | `-DMCSOLVERENGINE_WITH_OCCT=OFF`（默认） | 无 OCCT 链接 | 不可用 |

两种配置下 DLL 名称均为 `mcsolverengine_native.dll`。Geometry 导出**始终可用**，不受 OCCT 开关影响。

OCCT 在启用构建中属于**运行时依赖**：除链接期 `.lib` 文件外，运行时还须在 DLL 搜索路径中存在对应的 OCCT `.dll` 文件。

### 5.2 各函数在不同配置下的行为

| 函数 | 含 OCCT | 不含 OCCT |
|------|---------|-----------|
| `McSolverEngine_GetVersion` | 正常 | 正常 |
| `McSolverEngine_SolveToGeometry` | 正常 | 正常 |
| `McSolverEngine_SolveToGeometryWithParameters` | 正常 | 正常 |
| `McSolverEngine_SolveToBRep` | `brepUtf8` 返回 BREP 文本 | 返回 `MCSOLVERENGINE_RESULT_OPEN_CASCADE_UNAVAILABLE`，`exportStatus = "OpenCascadeUnavailable"`，`brepUtf8 = NULL` |
| `McSolverEngine_SolveToBRepWithParameters` | 同上 | 同上 |
| `McSolverEngine_ExtractFCStdDoc` | 正常 | 正常 |
| `McSolverEngine_GetLastError` | 正常 | 正常 |
| Free 函数族 | 正常 | 正常 |

要点：

- **Geometry 函数始终可用** — 它们不依赖 OCCT
- **BREP 函数优雅降级** — 无 OCCT 时返回携带元数据的结构化结果、明确的状态和 NULL `brepUtf8`；调用方可像处理其他失败一样检查 `exportStatus` 和 `messages`
- **不会崩溃** — 无 OCCT 时调用 BREP 函数不会崩溃或抛出异常；错误通过返回码确定性地呈现

### 5.3 NuGet 包

两个预构建 NuGet 变体直接对应 OCCT 开关：

| 包 | OCCT | BREP |
|----|------|------|
| `McSolverEngine_UseOcct` | 运行时必须 | 可用 |
| `McSolverEngine_NoOcct` | 不需要 | 返回 `OpenCascadeUnavailable` |

两个包提供相同的 C API 接口；区别仅在于 OCCT-backed BREP 导出是否可用。

---

## 6. 参数传递约定

### 值格式

通过 `parameterValuesUtf8` 传递的参数值必须是纯数值字符串：

| 约束类型 | 单位 | 示例 |
|----------|------|------|
| DistanceX, DistanceY, Distance | mm | `"8.5"` |
| Radius, Diameter | mm | `"12.0"` |
| Angle | degree | `"45"` |

会被拒绝的格式：`"8.5 mm"`、`"45 deg"`、`"1 cm"`。

### 键格式

- **全名键**（推荐）：`"VarSetName.PropertyName"` — 例如 `"Config.Width"`
- **短名键**：`"PropertyName"` — 例如 `"Width"`（多个 VarSet 共享同名属性时存在歧义，先匹配到的生效）

### 查找顺序

1. 全名键匹配（`VarSetName.PropertyName`）
2. 短名键匹配
3. 回退到导入的默认值（来自 VarSet 默认值或 Document.xml ExpressionEngine）

### VarSet 表达式

Document.xml 中 VarSet 的 `ExpressionEngine` 值在导入阶段求值。支持的表达式子集包括：
- 算术运算：`+`、`-`、`*`、`/`、`%`、`^`（左结合）
- 数学函数：`sin`、`cos`、`tan`、`asin`、`acos`、`atan`、`atan2`、`sinh`、`cosh`、`tanh`、`sqrt`、`cbrt`、`pow`、`hypot`、`exp`、`log`、`log10`、`floor`、`ceil`、`round`、`trunc`、`abs`、`min`、`max`、`sum`、`average`、`count`、`mod`
- 常量：`pi`、`e`（区分大小写）
- VarSet 引用：`Param`（同 VarSet 内）、`VarSetName.Param`、`<<Label>>.Param`
- 有限单位支持：长度（`mm`、`cm`、`m`、`km`、`um`、`nm`、`in`、`ft`）和角度（`deg`、`degree`、`degrees`、`rad`、`radian`、`radians`）
- VarSet 参数循环引用检测

如果表达式使用了超出此子集的 FreeCAD 特性（电子表格引用、几何属性、完整 Quantity/Unit 运算、条件/逻辑表达式等），导入返回 `MCSOLVERENGINE_RESULT_VARSET_EXPRESSION_UNSUPPORTED_SUBSET`，并在诊断消息中附带标签 `MCSOLVERENGINE_IMPORT_VARSET_EXPRESSION_UNSUPPORTED_SUBSET`。

---

## 7. DLL 发现与加载

### 文件名

| 平台 | DLL 名称 |
|------|----------|
| Windows x64 | `mcsolverengine_native.dll` |
| Linux | `libmcsolverengine_native.so` |
| macOS | `libmcsolverengine_native.dylib` |

### C# 包装层绑定

```csharp
[DllImport("mcsolverengine_native", CallingConvention = CallingConvention.Cdecl)]
```

C# 包装层（`McSolverEngineClient.cs`）提供：
- `ConfigureNativeLibrary(string path)` / `ConfigureNativeLibraryDirectory(string dir)` — 显式指定 DLL 位置
- .NET Framework：通过 `kernel32.dll` 的 `LoadLibrary` 加载
- .NET 6+：通过 `NativeLibrary.Load` 加载，自动根据 OS 选择文件名
- 缓存已加载的句柄；切换到另一套原生构建产物需要重启进程

### Python 绑定

```python
# _bindings.py — ctypes
_native = ctypes.CDLL(_discover_library_path("mcsolverengine_native"))
```

从包目录或 `PATH` 自动发现 DLL。

---

## 8. 内存管理

| 分配来源 | 释放函数 |
|----------|----------|
| `McSolverEngineGeometryResult*` | `McSolverEngine_FreeGeometryResult` |
| `McSolverEngineBRepResult*` | `McSolverEngine_FreeBRepResult` |
| `char*`（来自 ExtractFCStdDoc） | `McSolverEngine_FreeFCStdDoc` |
| `McSolverEngine_GetVersion()` 返回值 | 静态持有 — 不得释放 |
| `McSolverEngine_GetLastError()` 返回值 | 线程局部缓冲区 — 不得释放 |

所有结果结构体及其嵌套子对象（几何记录数组、BSpline 极点/节点数组、约束引用数组、字符串数组）均由顶层释放函数一并释放。调用方不得单独释放子对象。

**调用 Free 函数后**：指针即失效；调用方应将其设为 `NULL`。

---

## 9. 线程安全

- `McSolverEngine_GetLastError()` 使用线程局部错误缓冲区 — 每线程一条消息
- 每次 API 调用入口清空线程局部错误缓冲区
- 所有求解/导出管线可重入（无共享可变状态）
- 结果结构体每次调用独立分配

---

## 10. 错误处理模式

```c
McSolverEngineGeometryResult* result = NULL;
McSolverEngineResultCode rc = McSolverEngine_SolveToGeometry(xml, "Sketch", &result);

if (rc != MCSOLVERENGINE_RESULT_SUCCESS) {
    fprintf(stderr, "Error: %s\n", McSolverEngine_GetLastError());
    // result 可能仍携带部分元数据 — 可按需检查
}

if (result) {
    McSolverEngine_FreeGeometryResult(result);
}
```

**要点**：
- 始终检查返回码
- 失败时 `result` 可能非 NULL 并携带部分元数据（messages、conflicting/redundant 索引）
- `GetLastError()` 仅在下一次 API 调用前有效
- 无论返回码如何，`result` 非 NULL 时必须释放
