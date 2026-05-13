# McSolverEngine Plan

本文件承接原 `McSolverEngine_PRD.md` 中与实施计划相关的内容，便于将产品定义与执行规划分开维护。

## 1. 分阶段实施计划（已完成，用于回顾）

以下 Phase 0 ~ Phase 5 已全部落地；本节保留作为实施拆分与验收口径的回顾。

### Phase 0：工程骨架

目标：建立 `McSolverEngine` 的独立构建与目录结构。

交付物：

- `McSolverEngine\CMakeLists.txt`
- `include/`、`src/`、`tests/` 基础结构
- 独立库目标与基础测试入口

### Phase 1：抽离 GCS Core

目标：将 `planegcs` 核心迁移到 `McSolverEngine` 并解除 FreeCAD 运行时绑定。

交付物：

- 独立可编译的 GCS Core
- 导出宏替换
- 日志/调试输出封装
- 对 FreeCAD 特定 include 的隔离或替换
- 最小兼容垫片（例如 `SketcherGlobal.h`）以承接抽离源码中的原始 include / export 宏约定

验收标准：

- 不依赖 `src\Mod\Sketcher\App\Sketch.*` 也能独立编译
- 能通过基础求解单测

### Phase 2：重建 Compat Model

目标：建立内部几何、约束、参数管理层，替代 FreeCAD `Sketch.cpp` 的装配职责。

交付物：

- `McSketch`
- `McGeometry`
- `McConstraint`
- 参数池与 `double*` 生命周期管理
- 基础约束翻译逻辑

验收标准：

- 基础 2D 草图可从内部模型进入求解器

### Phase 3：接入 XML 输入

目标：从 FreeCAD `Document.xml` 中构建内部草图模型。

交付物：

- FCStd / `Document.xml` 读取入口
- 草图对象解析器
- 几何与约束映射器
- `Placement` / 平面姿态导入

验收标准：

- 可对一个基础 FreeCAD 草图样例完成导入并进入求解

### Phase 4：输出 BREP

目标：把求解后的基础几何输出成 BREP，并补充精确几何导出接口。

交付物：

- 几何到 BREP 的转换层
- `.brep` 输出能力
- 精确几何输出能力

验收标准：

- 可对基础草图输出稳定 BREP 结果
- 可对基础草图输出稳定精确几何结果

### Phase 5：回归测试与对照验证

目标：确保 `McSolverEngine` 基础能力与 FreeCAD 结果一致到可接受范围。

交付物：

- 基础求解用例
- XML 导入用例
- BREP 输出用例
- 精确几何输出用例
- 与 FreeCAD 基础草图结果对照

验收标准：

- 核心用例稳定通过
- 基础几何结果可重复

## 2. 里程碑

当前状态：`M1` ~ `M4` 已全部达成。

### M1

完成独立构建骨架与 GCS Core 抽离。

### M2

完成基础几何 / 约束的内部模型与求解闭环。

### M3

完成 `Document.xml` 到内部模型的导入。

### M4

完成 BREP 输出、精确几何输出与基础回归测试。

## 3. 风险与应对

### 风险 1：仅抽离 `planegcs` 后无法直接对接业务输入

应对：

- 明确将 Compat Model 作为独立阶段
- 不把 `Sketch.cpp` 的职责错误地归入求解核心

### 风险 2：参数生命周期复杂

应对：

- 设计统一参数池
- 禁止业务层直接管理裸 `double*`

### 风险 3：高级约束过早进入导致项目失控

应对：

- 第一阶段只支持基础几何与基础约束
- 高级曲线与高级约束延后

### 风险 4：XML 兼容范围膨胀

应对：

- 先兼容草图最小子集
- 不将“兼容整个 FCStd”作为第一阶段目标

### 风险 5：BREP 输出耦合 FreeCAD 运行时

应对：

- 保持 OCCT 级输出
- 避免耦合 DocumentObject / TopoShape 运行时语义

### 风险 6：多平面草图输出虽然求解在 2D 内完成，但 3D 结果若忽略 `Placement` 会与 FreeCAD 样本不一致

应对：

- 将 `Placement` 作为 `Document.xml` 导入的一部分
- 在 BREP 与精确几何导出阶段统一保留草图姿态信息

## 4. 当前执行顺序

当前按以下顺序推进：

1. 完成 `McSolverEngine` 独立构建骨架
2. 抽离 GCS Core
3. 建立内部兼容模型
4. 接入 `Document.xml`
5. 输出 BREP
6. 补齐回归测试

## 5. 当前决策

已确认：

- 目标是“独立构建 + 基础 2D 草图约束求解 + `Document.xml` 输入 + BREP 输出”
- 当前已扩展为“独立构建 + 基础 2D 草图约束求解 + `Document.xml` 输入 + BREP 输出 + 精确几何输出”
- 第一阶段不追求完整复制 FreeCAD Sketcher
- 第一阶段先做基础几何与基础约束
- 后续实现以分阶段迭代为准，而非一次性大迁移
