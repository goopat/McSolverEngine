# McSolverEngine 仓库说明文档

## 1. 项目概述

McSolverEngine 是从 FreeCAD Sketcher 中独立抽离的 2D 几何约束求解引擎。它将 FreeCAD 的 PlaneGCS 约束求解核心、Document.xml 数据兼容层、BREP 输出链路和精确几何导出接口整合为一个可独立构建的 C++ 工程，并通过 C ABI 对外提供跨语言调用能力。

**核心能力**：读取 FreeCAD `.FCStd` / `Document.xml` 中的草图数据 → 完成 2D 约束求解 → 输出 BREP 几何结果和结构化精确几何结果，并在 4 个对外求解 API 结果中返回全部 VarSet 标量属性（`string value + string unit`；数值结果按规范单位归一化）。同时提供只读的 Document.xml 检查接口，用于枚举 Sketch 与 VarSet 对象元数据。

## 2. 仓库目录结构

```
McSolverEngine/
├── apps/                          # 可执行程序入口
│   └── mcsolverengine_cli.cpp     #   CLI 命令行工具
│
├── include/McSolverEngine/        # 公共头文件（C++ 和 C API）
│   ├── BRepExport.h               #   BREP 导出接口
│   ├── CApi.h                     #   C ABI 接口定义
│   ├── CompatModel.h              #   兼容模型数据结构
│   ├── CompatSolver.h             #   兼容求解器接口
│   ├── DocumentXml.h              #   Document.xml 导入接口
│   ├── Engine.h                   #   引擎版本/描述
│   ├── Export.h                   #   DLL 导出宏
│   ├── GeometryExport.h           #   精确几何导出接口
│   ├── Version.h                  #   版本号
│   └── ZipExtract.h               #   FCStd ZIP 提取接口
│
├── src/                           # 源码实现
│   ├── core/planegcs/             #   GCS 约束求解核心（来自 FreeCAD）
│   │   ├── Constraints.cpp/h      #     约束定义与表达
│   │   ├── GCS.cpp/h              #     求解器主入口
│   │   ├── Geo.cpp/h              #     几何参数定义
│   │   ├── SubSystem.cpp/h        #     子系统拆分与 QR 诊断
│   │   ├── qp_eq.cpp/h            #     二次规划求解器
│   │   └── Util.h                 #     工具函数
│   ├── third_party/zlib/          #   内置 zlib 1.3.2（仅 inflate 子集）
│   │   ├── mse_zlib_prefix.h      #     符号重命名前缀（隔离用）
│   │   ├── adler32.c / crc32.c/h  #     校验
│   │   ├── inflate.c/h            #     DEFLATE 解压
│   │   ├── inffast.c/h            #     快速解码
│   │   ├── inftrees.c/h           #     Huffman 树
│   │   ├── inffixed.h             #     固定 Huffman 表
│   │   ├── zutil.c/h / zlib.h     #     工具/主头文件
│   │   ├── zconf.h / gzguts.h     #     配置/gzip 内部
│   │   └── LICENSE                #     zlib 许可证
│   ├── BRepExport.cpp             #   OCCT BREP 导出实现
│   ├── CApi.cpp                   #   C ABI 实现
│   ├── CompatModel.cpp            #   兼容模型构建
│   ├── CompatSolver.cpp           #   兼容求解器（SketchModel → GCS → 结果）
│   ├── ConsoleShim.h              #   控制台输出垫片
│   ├── DocumentXml.cpp            #   Document.xml 自定义解析器
│   ├── Engine.cpp                 #   引擎版本/描述
│   ├── GeometryExport.cpp         #   精确几何导出实现
│   ├── ParameterValueUtils.h      #   参数值校验工具
│   ├── SketchShapeBuilder.cpp/h   #   草图形状 → OCCT TopoDS 构建
│   ├── SketcherGlobal.h           #   兼容垫片（SketcherExport → MCSOLVERENGINE_EXPORT）
│   ├── VarSetExpressionEngine.cpp/h  # VarSet 表达式引擎（精简 FreeCAD 子集）
│   ├── Version.cpp                #   版本号实现
│   ├── WindowsAssertMode.h        #   Windows 断言模式（stderr 输出，不弹窗）
│   └── ZipExtract.cpp             #   自定义 ZIP 解析 + DEFLATE 解压
│
├── tests/                         # C++ 测试
│   ├── smoke.cpp                  #   核心库回归测试
│   ├── capi_smoke.cpp             #   C API 回归测试
│   └── unit.cpp                   #   单元测试（内部工具、边界条件、未覆盖约束）
│
├── wrapper/                       # 跨语言包装层
│   ├── csharp/                    #   C# P/Invoke 包装
│   │   ├── McSolverEngine.Wrapper.csproj       # 项目文件（net48;net8.0）
│   │   ├── McSolverEngineClient.cs             # 客户端封装
│   │   ├── McSolverEngineNativeStatus.cs       # 状态枚举
│   │   ├── McSolverEngineResponses.cs          # 响应数据结构
│   │   ├── McSolverEngineStructuredGeometry.cs # 几何数据结构
│   │   └── tests/
│   │       ├── McSolverEngine.Wrapper.Tests.csproj
│   │       ├── WrapperRegressionTests.cs
│   │       └── Program.cs
│   └── python/                    #   Python ctypes 包装
│       ├── mcsolverengine_py/
│       │   ├── __init__.py
│       │   ├── _bindings.py       #   ctypes 绑定（DLL 发现 + 结构体）
│       │   ├── _engine.py         #   高层 Engine 类
│       │   └── _svg.py            #   几何 → SVG 输出
│       └── tests/
│           └── test_wrapper.py    #   Python wrapper 测试
│
├── fcstdDoc/                      # FreeCAD 测试数据（回归语料库）
│   ├── 1.FCStd / 1.xml / 1.brp / 1.solver.brp
│   ├── 2.FCStd / 2.xml（含 3 张不同平面草图）
│   ├── 3.FCStd / 3.xml / 3.Sketch.Shape.brp / 3.Sketch.solver.brp
│   ├── V102.1.FCStd / V102.1.xml / V102.1.brp / V102.1.solver.brp
│   ├── V102.2.FCStd / V102.2.xml（含 3 张多平面草图）
│   ├── V102.4.FCStd / V102.4.xml / V102.4.brp / V102.4.solver.brp
│   ├── V102.4.plus1.FCStd / V102.4.plus1.xml / V102.4.plus1.brp（参数 +1 覆盖）
│   ├── V102.5.FCStd / V102.5.xml / V102.5.brp / V102.5.solver.brp（表达式驱动）
│   ├── V102.6.FCStd / V102.6.xml / V102.6.brp（VarSet + 参数化, V102.6_400.brp 覆盖）
│   ├── V102.7.FCStd / V102.7.xml / V102.7.brp（FCStd 提取 + 参数化 BREP）
│   ├── V102.8.FCStd / V102.8.xml / V102.8.brp
│   └── *.svg                      #   Python wrapper 输出的 SVG 可视化
│
├── scripts/                       # 构建与打包脚本
│   ├── package_nuget.ps1          #   NuGet 打包核心脚本
│   ├── package_use_occt.bat       #   UseOcct 变体快捷调用
│   ├── package_no_occt.bat        #   NoOcct 变体快捷调用
│   └── McSolverEngine.targets     #   MSBuild 集成模板
│
├── tools/                         # 构建工具
│   └── nuget.exe                  #   NuGet CLI 可执行文件
│
├── artifacts/nuget/               # 预构建 NuGet 包
│   └── McSolverEngine_NoOcct.0.1.0.nupkg
│
├── .github/                       # GitHub 配置
│   └── copilot-instructions.md    #   Copilot 指令
│
├── .pixi/                         # pixi conda 环境（Eigen3, Boost, VS 工具链）
├── .claude/                       # Claude Code 配置
│   └── settings.local.json
│
├── CMakeLists.txt                 # CMake 构建配置
├── .gitattributes                 # Git 行尾符规则（fcstdDoc/*.xml → LF）
├── .gitignore
├── pixi.toml / pixi.lock          # pixi 项目/锁定文件
├── License.md                     # 许可证（含第三方依赖许可信息）
├── CLAUDE.md                      # Claude Code 项目指令
├── McSolverEngine_PRD.md          # 产品需求文档
├── McSolverEngine_C_API.md        # C API 中文文档
├── McSolverEngine_C_API_en.md     # C API 英文文档
├── Plan.md                        # 实施计划（Phase 0-5 已完成）
└── REPOSITORY.md                  # 本文件
```

## 3. 构建系统

### 3.1 CMake 构建目标

| 目标 | 类型 | 说明 |
|---|---|---|
| `McSolverEngineCore` | 静态库 | 核心求解器 + 兼容层 + 导入/导出 |
| `McSolverEngineZip` | 静态库 | zlib inflate + ZIP 解析（符号隔离） |
| `McSolverEngineNative` | 动态库 (DLL) | C ABI 接口，输出名 `mcsolverengine_native` |
| `McSolverEngineCli` | 可执行文件 | 命令行工具，输出名 `mcsolverengine` |
| `McSolverEngineSmokeTest` | 可执行文件 | C++ 核心库回归测试 |
| `McSolverEngineCApiSmokeTest` | 可执行文件 | C API 回归测试 |
| `McSolverEngineUnitTest` | 可执行文件 | C++ 单元测试（内部工具、边界条件、未覆盖约束） |
| `McSolverEngineCSharpWrapper` | 自定义目标 | C# 包装层编译 |
| `McSolverEngineCSharpTests` | 自定义目标 | C# 测试编译 |

### 3.2 构建命令

```bash
# 带 OCCT 的完整构建
cmake -B build -DMCSOLVERENGINE_WITH_OCCT=ON
cmake --build build --config Release

# 不带 OCCT 的最小构建
cmake -B build
cmake --build build --config Release
```

### 3.3 CMake 配置选项

| 选项 | 默认值 | 说明 |
|---|---|---|
| `MCSOLVERENGINE_WITH_OCCT` | OFF | 启用 OCCT-backed BREP 导出 |
| `MCSOLVERENGINE_BUILD_TESTS` | ON | 构建测试目标 |

### 3.4 依赖项发现策略

CMake 按以下优先级自动发现依赖：

1. **Eigen3**: 仅查 `.pixi/envs/default/Library/include/eigen3/`；若未找到，则直接配置失败
2. **Boost**: 仅查 `.pixi/envs/default/Library/include/`；若未找到，则直接配置失败
3. **OCCT**: 仅查 `.pixi/envs/default/Library/`（`include/opencascade`、`lib`、`bin` 或其 CMake config）；若 `MCSOLVERENGINE_WITH_OCCT=ON` 且未找到，则直接配置失败
4. **zlib**: 不依赖系统 zlib，始终使用内置 `src/third_party/zlib/`

### 3.5 编译标准

- C++20，关闭编译器扩展
- MSVC 下启用 `/permissive-`
- 动态 CRT（`/MD`）
- MSVC 下启用 `/utf-8`（源文件按 UTF-8 解析，消除 C4819 代码页警告）

### 3.6 编译警告说明

MSVC Release 构建中会出现以下已知警告，均不影响编译结果：

**C4005 — zlib 宏重定义（预期行为）**

仅出现在 `McSolverEngineZip` 目标的 `.c` 源文件中。原因：

1. `mse_zlib_prefix.h` 通过 `/FI` 在 `zlib.h` 之前强制包含，将 `inflateInit` 等符号重定义为 `McSolverEngine_inflateInit`
2. `zlib.h` 内部的 `Z_PREFIX` 机制再次定义了同名宏
3. 两次展开结果相同，编译器报告"宏重定义"

这是符号隔离机制的**正常副作用**。验证方法：

```powershell
# DLL 导出表中应无原始 zlib 符号
dumpbin /exports build/Release/mcsolverengine_native.dll | findstr "inflate deflate adler crc"

# 静态库中所有公开符号应带 McSolverEngine_ 前缀
dumpbin /symbols build/Release/McSolverEngineZip.lib | findstr "inflate deflate adler crc"
```

**C4819 — 代码页不兼容（已消除）**

CMakeLists.txt 中已通过 MSVC `/utf-8` 编译选项解决，当前构建不再出现此警告。

### 3.7 NuGet 打包

两个变体，脚本位于 `scripts/`：

| 包 | 说明 |
|---|---|
| `McSolverEngine_NoOcct` | 无 OCCT 依赖，BREP 导出返回 `OpenCascadeUnavailable` |
| `McSolverEngine_UseOcct` | 依赖 OCCT 运行时 DLL，BREP 导出可用 |

```powershell
.\scripts\package_nuget.ps1 -Variant UseOcct -Version 0.1.0
.\scripts\package_nuget.ps1 -Variant NoOcct -Version 0.1.0
```

输出到 `artifacts/nuget/`，包含头文件、静态库、导入库、运行时 DLL 和 MSBuild 集成 `.targets`。

## 4. 架构分层

引擎采用 10 层自底向上的分层架构：

```
┌─────────────────────────────────────────────┐
│ 10. C# Wrapper  │ P/Invoke, net48;net8.0     │
│  9. Python Wrapper │ ctypes, 纯 Python        │
├─────────────────────────────────────────────┤
│  8. C ABI         │ CApi.h/cpp, 跨语言互操作   │
├─────────────────────────────────────────────┤
│  7. 引擎层        │ Engine::version/describe   │
├─────────────────────────────────────────────┤
│  6. 兼容模型层    │ CompatModel, SketchModel    │
│  5. 兼容求解器层  │ CompatSolver, ParameterMap  │
├─────────────────────────────────────────────┤
│  4. Document.xml  │ DocumentXml, 自定义 XML 解析│
│  3. VarSet 表达式 │ VarSetExpressionEngine     │
├─────────────────────────────────────────────┤
│  2. FCStd ZIP     │ ZipExtract + zlib inflate  │
├─────────────────────────────────────────────┤
│  1. GCS Core      │ planegcs (DogLeg/LM/BFGS/SQP) │
├─────────────────────────────────────────────┤
│  0. 输出层         │ BRepExport / GeometryExport │
└─────────────────────────────────────────────┘
```

### 各层职责

**第 1 层 — GCS Core** (`src/core/planegcs/`)
- 来自 FreeCAD Sketcher 的 PlaneGCS 求解核心
- 四级回退链路：DogLeg → LevenbergMarquardt → BFGS → SQP augmented
- 关键参数：`convergence=1e-10`, `maxIter=100`, `FullPivLU`
- LGPL-2.1-or-later 许可
- 维护原则：尽量保持与 FreeCAD 上游一致，语义修正放在上层

**第 2 层 — FCStd ZIP 提取** (`src/ZipExtract.cpp` + `src/third_party/zlib/`)
- 自定义 ZIP 解析器（不依赖外部 ZIP 库）
- 内置 zlib 1.3.2 inflate 子集，所有公开符号重命名为 `McSolverEngine_<name>` 前缀
- 静态链接进 `McSolverEngineNative` DLL，DLL 导出表不含任何 zlib 符号

**第 3 层 — VarSet 表达式引擎** (`src/VarSetExpressionEngine.cpp`)
- FreeCAD 表达式的精简子集
- 支持：算术运算、三角函数/双曲函数、`pi/e` 常量、幂/根/对数、`min/max/sum/average/count/mod`
- 有限长度/角度单位换算（长度: mm/cm/m/km/um/nm/in/ft，角度: deg/rad）
- Sketch / VarSet 标量 Property 引用链解析 + 循环引用检测
- 非 Sketch / VarSet 对象引用返回专用错误码

**第 4 层 — Document.xml 导入** (`src/DocumentXml.cpp`)
- 自定义轻量 XML 解析器（无外部 XML 库依赖）
- 解析：Geometry、ExternalGeo、Constraints、Placement、ExpressionEngine、VarSet
- 先应用显式 `VarSetObjectNameOrLabel.PropertyName` 参数覆盖，再按依赖顺序求值 Sketch / VarSet 标量 Property 表达式
- 兼容新旧两套约束元素字段（`ElementIds/ElementPositions` 和 `First/Second/Third`）
- 支持导入不完整数据，返回 `ImportStatus::Partial` + `messages` + `skippedConstraints`

**第 5 层 — 兼容求解器** (`src/CompatSolver.cpp`)
- `SketchModel` → GCS 参数翻译 → 求解 → 结果回写
- `ParameterMap` 参数覆盖：只接受显式全名键（`VarSetObjectNameOrLabel.Property`）
- API 参数值约定：长度类 mm，角度类 degree（内部自动换算 radian）

**第 6 层 — 兼容模型** (`src/CompatModel.cpp`)
- `SketchModel`：核心内存模型，`std::variant` 几何/约束类型
- 保留 `originalId`、`construction`、`external`、`blocked` 标志

**第 7 层 — 引擎层** (`src/Engine.cpp`)
- `Engine::version()` / `Engine::describe()`

**第 8 层 — C ABI** (`src/CApi.cpp`, `include/McSolverEngine/CApi.h`)
- 稳定的跨语言 C 接口
- 结果结构体由引擎分配，调用方负责释放
- `GeometryResult` / `BRepResult` 额外携带全部 VarSet 标量属性结果，键固定为 `ObjectName.PropertyName`；可数值求值的项按规范单位输出，`value` / `unit` 都是字符串
- 提供 `InspectDocumentXml` 接口，返回 Sketch 标量属性与 VarSet 参数摘要（含原始 `<Property>` XML 片段）
- 暴露 `GetLastError()`、`GetVersion()`
- 详细文档见 `McSolverEngine_C_API.md`

**第 9-10 层 — Python / C# 包装**
- Python：纯 ctypes，`_bindings.py`（DLL 发现 + 结构体定义）→ `_engine.py`（`Engine.solve_to_geometry/brep/extract_fcstd_doc`）→ `_svg.py`（SVG 输出，可选 rhino3dm 做 B-Spline）
- C#：P/Invoke，无 NuGet 外部依赖，`McSolverEngineClient.cs`

**第 0 层 — 输出层**
- **BREP 导出** (`src/BRepExport.cpp`)：OCCT `TopoDS_Edge/Wire` → `BRepTools_ShapeSet` VERSION_1 文本
- **几何导出** (`src/GeometryExport.cpp`)：结构化精确几何，不依赖 OCCT
- 固定（external/blocked）几何参与求解但排除在 BREP 输出之外
- 非 XY 平面草图通过 `Placement` 映射到 3D 根 shape location

## 5. 命名空间

| 命名空间 | 用途 |
|---|---|
| `McSolverEngine` | 根：`Engine` + `ParameterMap` |
| `McSolverEngine::Compat` | 兼容模型和求解器 |
| `DocumentXml` | XML 导入 |
| `BRep` | BREP 导出 |
| `Geometry` | 精确几何导出 |
| `ZipExtract` | ZIP/FCStd 提取 |

## 6. 测试体系

### 6.1 测试命令

```bash
# 全部测试
ctest --test-dir build -C Release --output-on-failure

# 仅 C++ 测试
ctest --test-dir build -C Release -E csharp --output-on-failure

# 仅 C# 测试
ctest --test-dir build -C Release -L csharp --output-on-failure

# 指定单个测试
ctest --test-dir build -C Release -R "^SmokeTest$" --output-on-failure

# .NET 测试
dotnet test wrapper/csharp/tests/McSolverEngine.Wrapper.Tests.csproj -c Release -f net8.0

# Python 测试
python -m unittest wrapper/python/tests/test_wrapper.py -v
```

### 6.2 测试覆盖

| 测试套件 | 文件 | 说明 |
|---|---|---|
| C++ SmokeTest | `tests/smoke.cpp` | 核心库回归：48+ 用例覆盖所有几何/约束类型、表达式引擎、参数化求解、Block 等 |
| C API SmokeTest | `tests/capi_smoke.cpp` | C ABI 回归：结构化 Geometry/BRep 导出、OCCT/no-OCCT 路径、表达式驱动约束引用 |
| C++ UnitTest | `tests/unit.cpp` | 单元测试：ParameterValueUtils、CompatModel API、Diameter/Equal/Parallel 约束、冲突/冗余检测、ZIP/GeometryExport/XML 边界 |
| C# Wrapper (net48) | `wrapper/csharp/tests/` | P/Invoke 包装层回归：FCStd 提取 + 参数化 BREP 求解 + 表达式约束引用 |
| C# Wrapper (net80) | `wrapper/csharp/tests/` | 同上，.NET 8.0 框架 |
| Python Wrapper | `wrapper/python/tests/` | 19 个用例：求解、参数覆盖、BREP、几何导出、FCStd 提取、SVG 输出、版本 |

**V111.9 BREP 回归校验说明**

`V111.9` 相关参数化场景在 FreeCAD 和本仓库中都观察到同一类非确定性：由于浮点数尾差精度波动，草图约束求解完成后再导出 BREP 时，各几何对象在 `.brp` 文本中的出现位置可能发生变化。即使参数相同，FreeCAD 软件内重复求解也可能复现这种文本顺序漂移。

目前已知最容易复现的是 `V111.9.xml + VarSet.L1=500`，而对 `V111.9.500.xml` 的 FreeCAD 对照测试也发现同类随机性。因此，C++ 与 Python 针对这两条 `V111.9` 相关用例做了**定向放宽**：只放宽几何对象在 `.brp` 文本中的位置，不再要求对象记录顺序严格一致；但几何对象本身的数据内容仍然必须在既有数值容差范围内严格匹配。除这两条 `V111.9` 特例外，其余 BREP 回归样例仍保持严格 token 顺序校验。

### 6.3 回归数据（fcstdDoc/）

`fcstdDoc/` 是权威的回归语料库，共 62 个文件：

- **样本 1-3**：基础单/多平面草图
- **V102.1**：参数化尺寸驱动
- **V102.2**：多平面草图（3 张不同 Placement）
- **V102.4**：VarSet + ExpressionEngine + 参数覆盖
- **V102.5**：表达式驱动角度/尺寸约束
- **V102.6**：VarSet 参数链 + 参数覆盖
- **V102.7**：FCStd 提取 + 参数化 BREP 求解
- **V102.8**：最新样本

每个样本包含 `.FCStd`（原始文件）、`.xml`（Document.xml）、`.brp`（参考 BREP）、`.solver.brp`（求解后 BREP）。

## 7. 依赖项总览

### 7.1 构建时依赖

| 依赖 | 版本 | 类型 | 说明 |
|---|---|---|---|
| Eigen3 | 3.4.0 | header-only | 线性代数 |
| Boost | — | header-only | Graph, Math constants（通过 pixi 提供） |
| OpenCASCADE | 7.9 | 动态库 | 可选，BREP 导出（NuGet opencascade.7.9-native） |
| zlib | 1.3.2 | 静态库 | 内置，仅 inflate 子集，符号隔离 |
| CMake | 3.22+ | 构建工具 | — |
| MSVC | VS2022 | 编译器 | C++20 |

### 7.2 运行时依赖

| 依赖 | 说明 |
|---|---|
| `mcsolverengine_native.dll` | 引擎本体 |
| OCCT DLL（`TKBRep.dll` 等） | 仅 `UseOcct` 变体需要 |

### 7.3 不依赖项

引擎**不依赖**以下 FreeCAD 组件：App、Gui、Document 生命周期、Sketcher 模块运行时、完整 Expression/Quantity/Unit 系统。

## 8. 输入输出

### 8.1 输入路径

```
FCStd 文件 ──→ ZipExtract ──→ Document.xml 文本
Document.xml 文件 ──→ 直接读入
Document.xml 字符串 ──→ 内存解析
                     ↓
              DocumentXml 解析器
                     ↓
              SketchModel（兼容模型）
                     ↓
         CompatSolver（求解器翻译层）
                     ↓
              GCS Core（数值求解）
```

### 8.2 输出路径

```
求解后的 SketchModel
        ↓
   ┌────┴────┐
   ↓         ↓
BRepExport  GeometryExport
(OCCT)      (纯 C++)
   ↓         ↓
.brep 文本   结构化几何记录
   ↓         ↓
C ABI 包装
   ↓
C# / Python / 其他语言消费
```

### 8.3 参数流

```
Document.xml
  ├── App::VarSet 标量属性 + Label + ExpressionEngine
  └── 草图标量 Property + 草图 ExpressionEngine
        ↓
  调用方 ParameterMap 覆盖（显式 VarSetObjectNameOrLabel.Property 键；纯数值字符串，长度 mm / 角度 deg）
        ↓
  Sketch / VarSet 标量 Property 表达式求值（参数链按依赖顺序计算）
        ↓
  Compat::Constraint { parameterName, parameterKey, parameterDefaultValue }
        ↓
  求解器应用（角度自动换算 radian）
```

## 9. 当前约束支持范围

全部 19 种 FreeCAD Sketcher 约束类型已实现：

| 约束 | 状态 |
|---|---|
| Coincident, Horizontal, Vertical | 已支持 |
| DistanceX, DistanceY, Distance | 已支持 |
| Parallel, Perpendicular | 已支持 |
| Angle, Radius, Diameter | 已支持 |
| Equal, Symmetric | 已支持 |
| Tangent, PointOnObject | 已支持 |
| InternalAlignment | 已支持 |
| Block, Weight | 已支持 |
| SnellsLaw | 已支持 |

全部 8 种基础几何类型已支持：Point, LineSegment, Circle, Arc, Ellipse, ArcOfEllipse, ArcOfHyperbola, ArcOfParabola, B-Spline。

## 10. 已知限制

| 限制项 | 说明 |
|---|---|
| VarSet 表达式子集 | 不支持 Spreadsheet 引用、完整 Quantity/Unit 语义、字符串/矩阵/条件表达式等 |
| `updateNonDrivingConstraints` 缺失 | 非驱动约束值在求解后不更新；所有测试数据均为 `IsDriving="1"` |
| Z 坐标/法向量丢弃 | Sketcher 本质 2D，3D 姿态由 Placement 单独处理 |
| 约束 GUI 元数据 | `IsVisible`, `LabelDistance`, `LabelPosition` 不解析（不影响求解） |
| BREP `\r\n` 行尾 | 测试辅助函数 location check 在 Windows 可能有误判，不影响几何验证 |
| 部分约束无直接程序化测试 | Coincident/Symmetric/PointOnObject 仅通过 XML 间接覆盖；Diameter/Equal/Parallel 已由 unit.cpp 直接覆盖 |
| ArcOfEllipse/ArcOfHyperbola | 仅 addGeometry 程序化覆盖，求解路径仍仅通过 XML 间接验证 |

## 11. 许可证

- GCS Core (`src/core/planegcs/`): LGPL-2.1-or-later（来自 FreeCAD）
- zlib (`src/third_party/zlib/`): zlib License
- 其余 McSolverEngine 代码: 见 `License.md`
- 第三方依赖许可信息: 详见 `License.md`

## 12. 相关文档

| 文档 | 内容 |
|---|---|
| `McSolverEngine_PRD.md` | 产品需求、目标架构、兼容策略、输出策略 |
| `Plan.md` | 分阶段实施计划（Phase 0-5 已全部完成） |
| `McSolverEngine_C_API.md` | C API 完整中文文档 |
| `McSolverEngine_C_API_en.md` | C API 完整英文文档 |
| `CLAUDE.md` | Claude Code 项目指令（构建/测试/架构约定） |
| `License.md` | 许可证（含第三方依赖许可） |
