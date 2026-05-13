# McSolverEngine（stand-alone porting version of FreeCAD Sketcher PlaneGCS）

## 1. 背景

当前目标不是复制整个 FreeCAD Sketcher，而是将其中可复用的 GCS 约束求解核心抽离出来，形成一个可独立构建的 `McSolverEngine`。新引擎需要：

- 复用 FreeCAD Sketcher 的 GCS solver 核心能力
- 输入侧兼容 FreeCAD `Document.xml` 的数据结构
- 输出侧继续产出 BREP 数据，并补充精确几何导出接口
- 在架构上不依赖 FreeCAD 的 App / Gui / Document 运行时

这意味着本项目本质上是一次“求解核心抽离 + 数据兼容层重建 + BREP 输出链路保留”的工程化拆分，而不是单纯复制 `planegcs` 目录。

## 2. 产品目标

### 2.1 总目标

构建一个独立的 `McSolverEngine`，能够读取 FreeCAD 文档中的草图数据，完成基础 2D 约束求解，并输出对应的 BREP 几何结果与精确几何结果。

### 2.2 成功标准

满足以下条件视为第一阶段目标达成：

1. `McSolverEngine` 可以独立编译，不链接 FreeCAD App / Gui / Sketcher 模块
2. GCS 核心求解可在 `McSolverEngine` 中独立运行
3. 可以从 FreeCAD `Document.xml` 中读取草图对象的最小必要信息
4. 支持基础 2D 几何和基础约束的求解
5. 可输出可校验的 BREP 结果
6. 可输出结构化描述的精确几何结果
7. 能通过一组回归测试，对照 FreeCAD 的基础草图结果

## 3. 范围

### 3.1 第一阶段范围（必须）

#### 输入兼容

- 兼容 FreeCAD `FCStd` / `Document.xml` 的草图数据结构
- 第一阶段只覆盖草图求解所需的最小子集，不追求完整文档语义

#### 基础几何

- Point
- LineSegment
- Circle
- Arc

#### 基础约束

- Coincident
- Horizontal
- Vertical
- DistanceX
- DistanceY
- Distance
- Parallel
- Perpendicular
- Angle
- Radius
- Diameter
- Equal
- Tangent
- Symmetric
- PointOnObject

#### 输出

- 生成基础 BREP 几何结果
- 提供文件输出能力（如 `.brep`）或内存中的等价 BREP 表达
- 提供精确几何导出接口，便于下游不依赖 OCCT/BREP 文本消费解析几何

### 3.2 后续阶段范围（可选）

- Ellipse / ArcOfEllipse
- Hyperbola / Parabola
- B-Spline
- Block 约束
- 拖拽临时约束
- 高级冗余/冲突诊断一致性
- 完整复刻 FreeCAD Sketcher 的交互语义

## 4. 非目标

以下内容不属于当前阶段目标：

- 复制 FreeCAD Sketcher GUI
- 复制 FreeCAD Document / Transaction / Recompute 机制
- 兼容所有 FreeCAD Workbench 数据
- 第一版就完整支持所有 Sketcher 约束和高级曲线
- 第一版就做到与 FreeCAD 100% 数值与文本一致

## 5. 现状分析与可复用代码

### 5.1 可直接复用的核心

FreeCAD Sketcher 的 GCS 求解核心主要集中在：

- `src\Mod\Sketcher\App\planegcs\GCS.h/.cpp`
- `src\Mod\Sketcher\App\planegcs\Geo.h/.cpp`
- `src\Mod\Sketcher\App\planegcs\Constraints.h/.cpp`
- `src\Mod\Sketcher\App\planegcs\SubSystem.h/.cpp`
- `src\Mod\Sketcher\App\planegcs\qp_eq.h/.cpp`

这些文件已经包含：

- 求解状态与算法枚举（BFGS / LevenbergMarquardt / DogLeg）
- 基础约束表达
- 子系统拆分
- QR 诊断
- 冗余/冲突/依赖参数分析

为了让这批抽离后的 `planegcs` 源码继续按原 FreeCAD 目录约定编译，`McSolverEngine` 还保留了一个最小兼容头：

- `McSolverEngine\SketcherGlobal.h`

它不是业务逻辑头文件，而是一个**兼容垫片**，当前主要作用是：

- 为抽离后的 `planegcs` 提供本地的 `SketcherExport` 宏定义
- 让 `planegcs` 中原有的 `#include "../../SketcherGlobal.h"` 继续成立
- 避免 `McSolverEngine` 在这一层重新依赖 FreeCAD 原始 `Sketcher` 模块目录

### 5.2 不能直接忽略的装配层

FreeCAD 中真正把“草图数据”翻译成 GCS 参数和约束的是：

- `src\Mod\Sketcher\App\Sketch.h`
- `src\Mod\Sketcher\App\Sketch.cpp`

这层负责：

- 将几何对象拆成求解参数
- 维护 `Parameters / FixParameters / DrivenParameters`
- 调用 `GCSsys.addConstraint...`
- 管理求解前初始化与求解后诊断结果

因此，`McSolverEngine` 不能只复制 `planegcs`，还必须重建一层自己的“兼容建模与翻译层”。

### 5.3 输入输出链路的参考实现

与本项目相关的 FreeCAD 现有实现包括：

- `src\App\Document.cpp`：`Document.xml` 的写入与恢复入口
- `src\App\ProjectFile.cpp`：项目文件读取
- `src\Ext\freecad\project_utility.py`：`Document.xml` 解析参考
- `src\Mod\Part\App\*`：BREP / TopoShape 相关实现

这些实现说明：

- `Document.xml` 是兼容输入的合理目标
- BREP 输出不需要重新发明格式，可以保留 OCCT 路线

## 6. 可行性结论

结论：**可行，但必须分阶段推进。**

### 6.1 高可行部分

- GCS 求解核心抽离
- 基础 2D 几何与基础约束支持
- 独立 CMake 构建
- 基础 BREP 输出

### 6.2 中风险部分

- FreeCAD `Document.xml` 到内部模型的稳健映射
- 求解参数生命周期管理（当前 GCS 大量使用 `double*`）
- 冗余/冲突诊断与 FreeCAD 行为对齐

### 6.3 高风险部分

- 高级曲线和高级约束一次性全支持
- 与 FreeCAD Sketcher 完整行为一致
- 拖拽、交互、Block 约束等复杂语义直接迁移

## 7. 目标架构

### 7.1 分层结构

`McSolverEngine` 目标架构如下：

1. **Core Solver Layer**
   - 来源：FreeCAD `planegcs`
   - 职责：数值求解、诊断、约束系统管理

2. **Compat Model Layer**
   - 职责：定义 `McSketch`、`McGeometry`、`McConstraint`
   - 作用：替代 FreeCAD `Sketch.cpp` 里与 App/Document 绑定的装配逻辑

3. **XML Import Layer**
   - 职责：从 FreeCAD `Document.xml` 提取草图所需数据
   - 输入：FCStd 包内 `Document.xml` 或解压后的 XML

4. **Geometry Output Layer**
   - 职责：将求解后的基础 2D 几何构造成 BREP 结果或精确几何结果
   - 输出：`.brep` 文件、内存中的 BREP 文本、或结构化精确几何数据

5. **Native ABI / Wrapper Layer**
   - 职责：对外提供稳定的 C ABI，便于 C# / 其他语言通过动态库调用
   - 输出：结构化 Geometry ABI、原始 UTF-8 BREP / 字符串结果、以及 P/Invoke 可直接绑定的导出函数

### 7.2 依赖边界

`McSolverEngine` 第一阶段允许依赖：

- Eigen3
- Boost（与 GCS 当前实现一致）
- OCCT（可选，仅用于 BREP 输出）
- 轻量 XML 解析实现

说明：

- 当前实际的**非 C++ 标准库依赖**只有三类：`Eigen3`、`Boost`、`OpenCASCADE / OCCT`
- 其中 `Eigen3` 与 `Boost` 是当前求解核心的必需依赖；若只保留“`Document.xml` 导入 + 基础 2D 约束求解 + 精确几何导出”，最小非标准库依赖集就是这两项
- `Boost` 当前主要通过 `planegcs` 使用 `Boost.Graph`、`Boost.Regex` 与 `Boost.Math constants`
- 当前链接方式并非“全部动态链接”：
  - `Eigen3` 在本项目中按 header-only 依赖使用
  - `Boost` 当前也按头文件依赖使用，构建脚本未显式链接 Boost 二进制库
  - `OpenCASCADE / OCCT` 在启用 BREP 导出时属于动态运行时依赖：构建期链接 `.lib`，运行期需要对应 `.dll`
  - Windows / MSVC 下当前使用动态 CRT（`/MD` / `/MDd`），不是静态 CRT
- `MCSOLVERENGINE_WITH_OCCT=ON` 且找到 OpenCASCADE 时，启用 OCCT-backed BREP 输出
- 未找到 OCCT 或显式关闭该选项时，核心求解、`Document.xml` 导入与精确几何导出仍可独立构建
- 关闭 OCCT 时，精确几何导出不受影响，因为其实现不依赖 OCCT 几何对象
- 此时 `BRep::exportSketchToBRep(...)` 返回 `brep = null`，`status = OpenCascadeUnavailable`
- 此时 `BRep::exportSketchToBRepFile(...)` 返回 `OpenCascadeUnavailable`
- 当前 `Document.xml` 兼容层未引入额外第三方 XML 库，而是使用 McSolverEngine 内部的轻量解析实现
- 当前原生回归仍以自定义 smoke test 可执行文件 + `CTest` 为主；另有 `wrapper\csharp\tests` 下的 net48 MSTest 项目，通过 `dotnet test` 回归托管包装层
- 当前目标划分为：`McSolverEngineCore`（静态库）、`McSolverEngineNative`（共享库）、`McSolverEngineCli`（CLI 可执行文件）

第一阶段不依赖：

- FreeCAD App
- FreeCAD Gui
- FreeCAD Document 生命周期
- Sketcher 模块运行时

构建与工程工具层面：

- 需要 `CMake 3.22+`
- 当前 C++ 编译标准已收紧为 **C++20**，并显式关闭编译器扩展；MSVC 下额外启用 `/permissive-`
- 原生测试通过 `CTest` 挂接；C# 包装层回归通过 `dotnet test` 运行；这些都属于构建工具链，不属于运行时第三方库依赖

## 8. 输入兼容策略

### 8.1 输入来源

第一阶段支持两类输入：

1. `FCStd` 包中的 `Document.xml`
2. 独立的 `Document.xml`

### 8.2 兼容原则

- **结构兼容优先**：按 FreeCAD `Document.xml` 的对象与属性结构解析
- **语义最小化**：只提取草图求解必需字段
- **渐进扩展**：先支持基础草图，后补高级草图元素

### 8.3 第一阶段最小解析子集

第一阶段优先解析：

- 草图对象标识
- 几何列表
- 约束列表
- 基础定位/平面信息（若求解和 BREP 输出需要）
- `Placement` / 平面姿态信息，用于非 XY 平面的 3D 输出对齐

当前已进一步落地的参数化输入补充包括：

- 草图对象上的 `ExpressionEngine`
- `App::VarSet` 对象中的标量属性（当前按 `<VarSet对象名>.<属性名>` 建立参数键）
- 通过 `Constraints[index]` 或等价约束路径，将维度约束绑定到 `VarSet` 参数
- C++ 层参数覆盖入口：`DocumentXml::importSketchFromDocumentXml(..., const std::map<std::string, std::string>&, ...)`、`importSketchFromDocumentXmlFile(..., const std::map<std::string, std::string>&, ...)`、`Compat::solveSketch(..., const std::map<std::string, std::string>&)`

当前参数化能力的边界与限制：

- 当前不实现完整 FreeCAD 表达式求值器，只支持“直接引用 `VarSet` 属性”或“可直接解析为数值的常量表达式”
- 对**调用方显式传入的 `parameters`**，当前 API 层要求值必须是**纯数值字符串**；不接受 `mm` / `deg` 等单位后缀
- 对 API 参数值，当前固定约定：
  - 长度类约束（`DistanceX / DistanceY / Distance / Radius / Diameter`）按 **mm**
  - 角度类约束（`Angle`）按 **degree**
  - 进入求解器前会自动换算到内部单位，其中角度会换成 **radian**
- 多个不同名称的 `VarSet` 会分别收集，不会合并到同一命名空间
- `<<Label>>.Param` 形式当前通过 `VarSet` 的 `Label` 映射回真实对象名
- 参数覆盖时优先匹配全名键（如 `Config.Width`），查不到时再匹配短名键（如 `Width`）
- 因此当多个 `VarSet` 含有同名属性时，短名覆盖存在歧义；当前推荐调用方使用全名键
- 若多个 `VarSet` 使用相同 `Label`，当前按首次出现的别名映射生效，后续同名 `Label` 不会覆盖前者
- 对来自 `Document.xml` 的 `VarSet` 默认值或常量表达式，当前支持一组受控的长度/角度单位换算：
  - 长度默认按 **mm**，并支持 `mm / cm / m / km / um / nm / in / ft`
  - 角度默认按 **degree**，并支持 `deg / rad`
- 仍不实现完整 FreeCAD `Quantity / Unit` 语义；包含更复杂运算符或表达式的值（如 `mm^2`、`kg/m^3`、`2 * mm`）不属于当前稳定支持范围

不在第一阶段强求：

- 完整表达式系统
- GUI 状态
- 非草图对象之间的复杂引用关系

## 9. 输出策略

### 9.1 输出目标

求解后输出对应的基础几何结果，用于后续几何消费或回归验证。当前同时支持：

- BREP
- 精确几何

### 9.2 第一阶段输出形式

第一阶段优先支持：

- LineSegment -> Edge
- Circle / Arc -> Edge
- 多段边 -> Wire（条件满足时）
- 输出 `.brep`
- 输出结构化精确几何记录

### 9.3 精确几何接口

- 不引入额外几何内核依赖
- 不引入 FreeCAD `DocumentObject` 级别依赖
- 保持输出可用于自动比较和回归测试

新增导出接口保持输入不变，仍以求解后的 `Compat::SketchModel` 为输入：

- `Geometry::exportSketchGeometry(const Compat::SketchModel&)`

当前接口输出结构：

- `GeometryRecord { geometryIndex, geometry }`
- `ExportResult { placement, geometries, messages, status }`

当前精确导出规则：

- 保留解析几何本体，不做离散采样
- 导出结果携带草图级 `Placement`
- 导出几何保留 `geometryIndex`，便于与原 `Compat::SketchModel` 对应
- 过滤 `construction` / `external` 几何，与 BREP 导出范围保持一致

### 9.4 BREP 文本保真策略

当前实现已尽量沿用 FreeCAD / OCCT 的持久化链路：

- 使用 `BRepTools_ShapeSet` 的 VERSION_1 文本写出路径
- 使用经典 locale 与固定小数风格输出
- 在根 shape 上显式保留 identity location，以对齐 FreeCAD 常见的 `Locations 1` / `+1 1` 输出

当前已确认：

- `Locations` 反映的是 OCCT `TopLoc_Location` 表，而不是草图平面本身
- 对于非 XY 平面的草图，真正决定 3D 结果的是草图 `Placement` 被映射到根 shape location

### 9.5 原生 C API 与 C# 包装层

当前已新增一层面向动态调用的稳定边界：

- 原生 C ABI 头文件：`include\McSolverEngine\CApi.h`
- 原生动态库目标：`mcsolverengine_native`
- C# P/Invoke 包装目录：`wrapper\csharp`

当前 C ABI 接口包括：

- `McSolverEngine_GetVersion()`
- `McSolverEngine_SolveToGeometry(...)`
- `McSolverEngine_SolveToGeometryWithParameters(...)`
- `McSolverEngine_SolveToBRep(...)`
- `McSolverEngine_SolveToBRepWithParameters(...)`
- `McSolverEngine_FreeGeometryResult(...)`
- `McSolverEngine_FreeString(...)`

当前跨语言边界采用 **Geometry 走结构化 ABI、BREP 走原始字符串** 的设计：

- `McSolverEngine_SolveToGeometry(...)` 直接返回结构化结果，既包含 `Placement + geometry records`，也包含导入/求解/导出诊断元数据
- 参数化路径当前通过 `const char*[] keys + const char*[] values + count` 跨 ABI 传递参数表，供 `Document.xml + sketchName + parameters` 一起求解；其中值必须是纯数值字符串
- BREP 路径直接返回原始 BREP 文本，不再额外包一层 JSON，也不再提供文件版 C ABI
- 结构化 Geometry 结果由原生层分配，调用方必须通过 `McSolverEngine_FreeGeometryResult(...)` 释放
- UTF-8 字符串结果由原生层分配，调用方必须通过 `McSolverEngine_FreeString(...)` 释放

`wrapper\csharp` 当前提供：

- 目标框架为 `.NET Framework 4.8`
- `McSolverEngineClient`：P/Invoke 入口；Geometry 走结构化 ABI，并可拿到导入/求解/导出元数据；BREP 调用直接接收原始 BREP 文本
- `McSolverEngineClient.SolveGeometryFromDocumentXml(documentXml, sketchName, parameters)`：包装参数化 Geometry 求解
- `McSolverEngineClient.SolveBRepFromDocumentXml(documentXml, sketchName, parameters)`：包装参数化 BREP 求解
- 对包装层透传的 `parameters`，当前沿用原生规则：长度按 **mm**、角度按 **degree**，且参数值必须是纯数值字符串
- `McSolverEngineClient.ConfigureNativeLibrary(...)` / `ConfigureNativeLibraryDirectory(...)`：显式指定原生 DLL 路径，避免调用方只能依赖 `PATH`
- 原生库加载采用显式 `LoadLibrary` 路径；当前包装层会缓存已加载的原生库句柄，因此同一进程内不支持切换到另一套原生构建产物
- `StructuredGeometrySolveResponse` / `BRepSolveResponse`：托管 DTO
- `McSolverEngineNativeStatus`：与原生结果码对齐的枚举
- `wrapper\csharp\tests\McSolverEngine.Wrapper.Tests.csproj`：net48 MSTest 回归项目，当前覆盖 `fcstdDoc\1.xml` 的 Geometry / BREP 两条包装层回归

无 OCCT 构建下：

- Geometry 调用仍可正常返回成功结果
- BREP 调用返回 `OpenCascadeUnavailable`
- 且原生 BREP 输出指针保持为空

### 9.6 数值精度结论

当前调查结论：

- `McSolverEngine` 默认仍使用 FreeCAD `planegcs` 的 DogLeg 路径
- 默认关键参数与 FreeCAD 一致：`convergence = 1e-10`、`maxIter = 100`、`dogLegGaussStep = FullPivLU`
- FreeCAD 允许通过运行时/GUI 高级求解参数覆盖这些默认值，但这些参数不属于 `Document.xml` 的稳定输入语义

因此，当前样本中少量末位差异（约 `1e-15` 量级）更接近：

- 求解器迭代停止点差异
- 线性代数分解路径差异
- OCCT 几何重建与 wire 修正带来的双精度噪声

结论上，第一阶段目标是：

- **几何与拓扑语义对齐**
- **BREP 文本风格尽量贴近**

而不是承诺与 FreeCAD 样本在所有浮点末位上完全逐字一致

## 10. 当前实现状态补充（截至本轮）

与实施计划、里程碑、风险、执行顺序和当前决策相关的内容已迁移至 `Plan.md`；本文件保留产品定义、架构边界与当前实现状态。

### 10.1 已落地的参数化求解能力

当前已打通一条最小可用的“`VarSet` -> 维度约束 -> 参数覆盖 -> 重新求解”链路：

- 导入阶段会扫描文档中的全部 `App::VarSet`
- 草图导入阶段会解析 `ExpressionEngine`，识别指向 `Constraints[index]` 的约束绑定
- 绑定成功后，内部 `Compat::Constraint` 会保留：
  - `parameterName`
  - `parameterKey`
  - `parameterExpression`
  - `parameterDefaultValue`
- 若 `VarSet` 中存在可解析的默认值，导入时会把该值传到对应维度约束
- 若调用方提供 `std::map<std::string, std::string>` 参数表，当前会先校验每个参数值都是纯数值，再用新值覆盖对应维度约束并进入求解
- API 参数值的固定语义当前为：
  - 长度类约束：**mm**
  - 角度类约束：**degree**
  - 进入内部求解器前，角度会换算成 **radian**
- 对来自 `VarSet` 默认值或常量表达式的长度/角度文本，当前会做有限的单位换算，而不是再简单忽略单位后缀

当前已验证的使用形态包括：

- 直接使用 `VarSet` 默认值驱动线性尺寸约束求解
- 直接使用 `VarSet` 默认值驱动角度约束求解，并完成 degree / rad 到内部弧度值的换算
- 在导入阶段通过 `parameters` 覆盖参数值
- 在求解阶段通过 `parameters` 覆盖参数值
- 同时支持短名键（如 `Width`）与全名键（如 `VarSet.Width`）的参数覆盖，其中推荐优先使用全名键
- 对 API `parameters`，当前会拒绝 `8.5 mm`、`45 deg` 这类带单位后缀的输入，要求调用方直接传 `8.5`、`45`

当前明确未覆盖的部分：

- 任意复杂表达式组合、函数调用、跨对象算术表达式
- 对 `FCStd` 压缩包本体的直接解包输入；当前跨语言接口仍以解压后的 `Document.xml` 文本为主

### 10.2 已落地的输入兼容能力

当前 `Document.xml` 导入已覆盖：

- `Geometry`
- `ExternalGeo`
- `Constraints`
- `Placement`

这意味着：

- 不涉及求解的普通几何也会被完整保留
- 外部几何（如轴线）会作为 fixed 参考参与求解
- 不同平面的草图可以在输出阶段恢复为正确的 3D 姿态

### 10.3 已落地的约束覆盖范围

当前实现已覆盖的约束包括：

- Coincident
- Horizontal
- Vertical
- DistanceX
- DistanceY
- Distance
- Parallel
- Tangent
- Perpendicular
- Angle
- Radius
- Diameter
- Equal
- Symmetric
- PointOnObject
- InternalAlignment
- Block
- Weight
- SnellsLaw

其中：

- `PointOnObject` 已用于 `fcstdDoc\2.xml` 的三张草图样本
- `InternalAlignment` 已通过椭圆焦点导入 / 求解 smoke 用例验证
- `Weight` 与 `SnellsLaw` 已通过专项 smoke 用例验证

### 10.4 当前回归样本

当前原生 smoke / regression 已覆盖：

1. 内联最小 `Document.xml` 样本
2. `fcstdDoc\1.xml` -> `1.brp`
3. `fcstdDoc\2.xml` 中三张草图：
   - `Sketch` -> `2.Sketch.Shape.brp`
   - `Sketch001` -> `2.Sketch001.Shape.brp`
   - `Sketch002` -> `2.Sketch002.Shape.brp`
4. `fcstdDoc\3.xml` 中 `Sketch` -> `3.Sketch.Shape.brp`
5. 精确几何导出回归（line / circle / arc / placement）
6. `InternalAlignment` / `Weight` / `SnellsLaw` 专项 smoke 用例
7. `McSolverEngineCApiSmokeTest`：
   - 结构化 Geometry C ABI
   - 原始字符串 BREP C ABI
   - OCCT / no-OCCT 两种构建路径下的导出状态

当前托管包装层回归已覆盖：

1. `wrapper\csharp\tests` 下的 net48 MSTest 项目
2. `fcstdDoc\1.xml` 的 Geometry 包装接口回归
3. `fcstdDoc\1.xml` 的 BREP 包装接口回归
4. 参数化 Geometry 包装接口回归
5. 参数化 BREP 包装接口回归
6. BREP 对照采用 token 级比较 + 数值容差，避免 OCCT 末位浮点抖动带来误报

其中 `2.xml` 的三张草图分别位于不同平面，已用于验证：

- `Placement` 导入
- 根 shape location 对齐
- 非 XY 平面的 3D 输出正确性

新增的参数化专项 smoke 用例当前覆盖：

1. 内联 `App::VarSet + ExpressionEngine` 最小样本
2. `VarSet` 默认值驱动维度约束求解
3. 导入阶段参数覆盖
4. 求解阶段参数覆盖

### 10.5 当前输出接口现状

当前并存三层输出接口：

1. **BREP 输出**
   - `BRep::exportSketchToBRep(...)`
   - `BRep::exportSketchToBRepFile(...)`
   - 若当前构建未启用 OCCT，接口不会尝试降级伪造 BREP，而是直接返回 `OpenCascadeUnavailable`

2. **精确几何输出**
   - `Geometry::exportSketchGeometry(...)`

3. **跨语言包装接口**
   - `McSolverEngine_SolveToGeometry(...)`
   - `McSolverEngine_SolveToBRep(...)`
   - `wrapper\csharp\McSolverEngineClient`
   - 当前托管层对 Geometry 路径直接消费结构化 ABI；对 BREP 路径直接消费原始 BREP 字符串；当前不再保留 JSON 几何接口

当前建议：

- 若目标是与 FreeCAD 持久化/形状结果对照，优先使用 BREP
- 若目标是供其他系统直接消费解析几何，优先使用精确几何输出
- 若目标是从 .NET / C# 动态集成当前引擎，优先使用 `wrapper\csharp` 对 `mcsolverengine_native` 的 P/Invoke 包装
