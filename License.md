# License

本仓库中的约束求解相关实现移植、改编自 FreeCAD 的 Sketcher 约束求解代码。

相关上游源文件头（例如 `FreeCAD\src\Mod\Sketcher\App\Sketch.cpp`）注明其遵循 **GNU Library General Public License, version 2 or (at your option) any later version**。因此，本仓库中对应的移植代码、修改代码及其衍生部分继续按照 **LGPL** 开源协议发布和分发。

除非单个文件另有说明，本仓库代码遵循以下原则：

- 保留上游 FreeCAD 的版权与许可声明；
- 对移植代码的修改和衍生部分继续遵循 LGPL 许可义务；
- 在分发源码或二进制时，同时保留相应的许可证说明，并满足 LGPL 对源代码获取与再分发的要求。

FreeCAD 是原始上游项目；本仓库不是 FreeCAD 官方仓库。

## 第三方库依赖

本仓库直接依赖以下第三方库。各库的源码**不包含**在本仓库中（除非特别说明），
需在构建时通过 pixi/conda、NuGet 或系统包管理器获取。

### 运行时依赖

| 库 | 版本 | 许可证 | 使用方式 |
|---|---|---|---|
| **Eigen 3** | 3.4.0 | MPL-2.0 | 仅头文件。线性代数（QR 分解、稠密/稀疏矩阵），被 `planegcs` 求解器使用。通过 pixi (`eigen >=3.3,<5`) 获取；CMake 只接受 `.pixi` 中的 Eigen 头文件。 |
| **Boost** (Graph, Math) | 1.91.0 | BSL-1.0 | 仅头文件。约束图的连通分量算法 (`boost/graph/`) 和数学常量 (`boost/math/`)。通过 pixi (`libboost-devel`) 获取；CMake 只接受 `.pixi` 中的 Boost 头文件。 |
| **OpenCASCADE (OCCT)** | 7.8 | LGPL-2.1（社区版） | **可选**。共享库链接（`TKBRep`、`TKTopAlgo`、`TKShHealing`）。仅用于 BREP 几何导出（`BRepExport.cpp`、`SketchShapeBuilder.cpp`）。通过 pixi/conda 环境中的 `occt` 获取；当 `MCSOLVERENGINE_WITH_OCCT=ON` 时，CMake 只接受 `.pixi` 中的 OCCT。`MCSOLVERENGINE_WITH_OCCT=OFF` 时完全不依赖。 |

### 内置源码（直接包含在本仓库中）

| 库 | 版本 | 许可证 | 使用方式 |
|---|---|---|---|
| **zlib** | 1.3.2 | zlib License | 部分源码编译进 `McSolverEngineZip` 静态库（仅 inflate 解压路径，6 个 `.c` 文件：`adler32.c`、`crc32.c`、`inffast.c`、`inflate.c`、`inftrees.c`、`zutil.c`）。所有公开符号通过 `mse_zlib_prefix.h` 加 `McSolverEngine_` 前缀，避免符号冲突。用于解压 `.FCStd`（ZIP）格式文档。源码位于 `src/third_party/zlib/`。 |
| **FreeCAD PlanGCS** | （移植自 FreeCAD） | LGPL-2.1-or-later | 约束求解器核心（`src/core/planegcs/` 下 9 个文件），移植自 FreeCAD Sketcher。版权：Konstantinos Poulios (2011)、Victor Titov / DeepSOIC (2014)。 |

### 测试依赖（不影响库本身）

| 库 | 版本 | 许可证 | 使用方式 |
|---|---|---|---|
| **MSTest.TestFramework** | 3.6.4 | MIT | .NET 测试框架（NuGet），仅用于 C# 封装层测试。 |
| **Rhino3dm** | 8.\* | MIT | 几何验证（NuGet + PyPI）。仅用于 C# 和 Python 测试中验证几何输出。 |

### 许可证合规说明

- **LGPL-2.1-or-later**（PlanGCS 移植代码、OCCT 社区版）：对源码修改需继续以 LGPL 发布；
  以动态链接方式使用本库（`mcsolverengine_native.dll`）的使用者不受 LGPL 传染。
- **zlib License**：允许任意使用（含商业闭源），要求保留版权声明且不得声称是原创。
- **MPL-2.0**（Eigen）：仅头文件使用，不要求公开源码；修改 Eigen 本身才需公开修改部分。
- **BSL-1.0**（Boost）：仅头文件使用，允许任意使用，无需署名。
- **MIT**（MSTest、Rhino3dm）：仅测试依赖，不进入发布产物。
