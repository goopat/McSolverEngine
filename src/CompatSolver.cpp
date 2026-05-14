#include "McSolverEngine/CompatSolver.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ParameterValueUtils.h"
#include "planegcs/GCS.h"

namespace McSolverEngine::Compat
{

namespace
{

using McSolverEngine::Detail::ParsedApiParameterMap;

struct GeometryBinding
{
    GeometryKind kind {GeometryKind::Point};
    int index {-1};
    int startPointId {-1};
    int endPointId {-1};
    int midPointId {-1};
};

struct SolveContext
{
    struct BSplineKnotAlignment
    {
        int splineGeometryIndex {-1};
        unsigned int knotIndex {0};
    };

    std::vector<std::unique_ptr<double>> storage;
    std::vector<double*> parameters;
    std::vector<double*> fixParameters;
    std::vector<double*> drivenParameters;
    std::vector<GCS::Point> points;
    std::vector<GCS::Line> lines;
    std::vector<GCS::Circle> circles;
    std::vector<GCS::Arc> arcs;
    std::vector<GCS::Ellipse> ellipses;
    std::vector<GCS::ArcOfEllipse> arcOfEllipses;
    std::vector<GCS::ArcOfHyperbola> arcOfHyperbolas;
    std::vector<GCS::ArcOfParabola> arcOfParabolas;
    std::vector<GCS::BSpline> bSplines;
    std::vector<GeometryBinding> geometries;
    std::vector<bool> blockedGeometry;
    std::unordered_map<double*, int> geometryParameterOwners;
    std::unordered_map<int, BSplineKnotAlignment> bsplineKnotAlignments;
    GCS::System system;
    int constraintsCounter {0};

    [[nodiscard]] double* ownParameter(double value)
    {
        storage.push_back(std::make_unique<double>(value));
        return storage.back().get();
    }

    [[nodiscard]] double* ownConstraintValue(double value, bool driving)
    {
        auto* parameter = ownParameter(value);
        if (driving) {
            fixParameters.push_back(parameter);
        }
        else {
            parameters.push_back(parameter);
            drivenParameters.push_back(parameter);
        }
        return parameter;
    }
};

struct BlockAnalysis
{
    std::vector<bool> onlyBlockedGeometry;
    std::vector<bool> blockedGeometry;
    std::vector<bool> internalAlignmentGeometry;
    std::vector<bool> unenforceableConstraints;
    std::vector<int> blockConstraintIndexByGeometry;
    std::vector<int> blockedGeoIds;
    bool doesBlockAffectOtherConstraints {false};
};

void storeParameter(SolveContext& context, double* parameter, bool fixed)
{
    if (fixed) {
        context.fixParameters.push_back(parameter);
    }
    else {
        context.parameters.push_back(parameter);
    }
}

void storeGeometryParameter(SolveContext& context, int geometryIndex, double* parameter, bool fixed)
{
    storeParameter(context, parameter, fixed);
    if (geometryIndex >= 0 && parameter != nullptr) {
        context.geometryParameterOwners.emplace(parameter, geometryIndex);
    }
}

[[nodiscard]] std::optional<double> resolveConstraintValue(
    const Constraint& constraint,
    const ParsedApiParameterMap& parameters
)
{
    const auto tryResolve = [&](const std::string& key) -> std::optional<double> {
        if (key.empty()) {
            return {};
        }
        const auto it = parameters.find(key);
        if (it == parameters.end()) {
            return {};
        }
        return McSolverEngine::Detail::convertApiParameterToInternal(it->second, constraint.kind);
    };

    if (const auto exact = tryResolve(constraint.parameterKey)) {
        return exact;
    }
    if (const auto shortName = tryResolve(constraint.parameterName)) {
        return shortName;
    }
    if (constraint.hasParameterDefaultValue) {
        return constraint.parameterDefaultValue;
    }
    return {};
}

[[nodiscard]] bool constraintTouchesGeometry(const Constraint& constraint, int geometryIndex)
{
    return constraint.first.geometryIndex == geometryIndex || constraint.second.geometryIndex == geometryIndex
        || constraint.third.geometryIndex == geometryIndex;
}

[[nodiscard]] bool isBlockedGeometryRef(const BlockAnalysis& analysis, const ElementRef& ref)
{
    return ref.geometryIndex >= 0 && ref.geometryIndex < static_cast<int>(analysis.blockedGeometry.size())
        && analysis.blockedGeometry[ref.geometryIndex];
}

[[nodiscard]] bool isExternalGeometryRef(const SketchModel& model, const ElementRef& ref)
{
    return ref.geometryIndex >= 0 && ref.geometryIndex < static_cast<int>(model.geometryCount())
        && model.geometries()[static_cast<std::size_t>(ref.geometryIndex)].external;
}

[[nodiscard]] BlockAnalysis analyzeBlockConstraints(const SketchModel& model)
{
    BlockAnalysis analysis;
    analysis.onlyBlockedGeometry.resize(model.geometryCount(), false);
    analysis.blockedGeometry.resize(model.geometryCount(), false);
    analysis.internalAlignmentGeometry.resize(model.geometryCount(), false);
    analysis.unenforceableConstraints.resize(model.constraintCount(), false);
    analysis.blockConstraintIndexByGeometry.resize(model.geometryCount(), -1);

    const auto& constraints = model.constraints();
    for (std::size_t geometryIndex = 0; geometryIndex < model.geometryCount(); ++geometryIndex) {
        bool blockOnly = true;
        bool blockIsDriving = false;
        for (const auto& constraint : constraints) {
            if (constraint.kind == ConstraintKind::Block && constraint.driving
                && constraint.first.geometryIndex == static_cast<int>(geometryIndex)) {
                blockIsDriving = true;
            }
            if (constraint.kind != ConstraintKind::Block && constraint.driving
                && constraintTouchesGeometry(constraint, static_cast<int>(geometryIndex))) {
                blockOnly = false;
            }
        }

        if (!blockIsDriving) {
            continue;
        }
        if (blockOnly) {
            analysis.onlyBlockedGeometry[geometryIndex] = true;
        }
        else {
            analysis.doesBlockAffectOtherConstraints = true;
            analysis.blockedGeoIds.push_back(static_cast<int>(geometryIndex));
        }
    }

    std::vector<int> internalAlignmentConstraintIndices;
    for (std::size_t index = 0; index < constraints.size(); ++index) {
        const auto& constraint = constraints[index];
        if (constraint.kind == ConstraintKind::Block && constraint.first.geometryIndex >= 0
            && constraint.first.geometryIndex < static_cast<int>(analysis.blockedGeometry.size())) {
            analysis.blockedGeometry[constraint.first.geometryIndex] = true;
            analysis.blockConstraintIndexByGeometry[constraint.first.geometryIndex] =
                static_cast<int>(index);
        }
        else if (constraint.kind == ConstraintKind::InternalAlignment) {
            internalAlignmentConstraintIndices.push_back(static_cast<int>(index));
        }
    }

    for (const int constraintIndex : internalAlignmentConstraintIndices) {
        const auto& constraint = constraints[static_cast<std::size_t>(constraintIndex)];
        if (constraint.first.geometryIndex < 0 || constraint.second.geometryIndex < 0
            || constraint.first.geometryIndex >= static_cast<int>(analysis.blockedGeometry.size())
            || constraint.second.geometryIndex >= static_cast<int>(analysis.blockedGeometry.size())) {
            continue;
        }
        if (!analysis.blockedGeometry[constraint.second.geometryIndex]) {
            continue;
        }
        analysis.blockedGeometry[constraint.first.geometryIndex] = true;
        analysis.internalAlignmentGeometry[constraint.first.geometryIndex] = true;
        analysis.blockConstraintIndexByGeometry[constraint.first.geometryIndex] =
            analysis.blockConstraintIndexByGeometry[constraint.second.geometryIndex];
        analysis.unenforceableConstraints[static_cast<std::size_t>(constraintIndex)] = true;
    }

    for (std::size_t index = 0; index < constraints.size(); ++index) {
        const auto& constraint = constraints[index];
        if (!constraint.driving) {
            continue;
        }

        if (constraint.first.geometryIndex >= 0 && constraint.first.geometryIndex < static_cast<int>(model.geometryCount())
            && analysis.internalAlignmentGeometry[constraint.first.geometryIndex]) {
            analysis.unenforceableConstraints[index] = true;
            continue;
        }
        if (constraint.second.geometryIndex >= 0
            && constraint.second.geometryIndex < static_cast<int>(model.geometryCount())
            && analysis.internalAlignmentGeometry[constraint.second.geometryIndex]) {
            analysis.unenforceableConstraints[index] = true;
            continue;
        }
        if (constraint.third.geometryIndex >= 0 && constraint.third.geometryIndex < static_cast<int>(model.geometryCount())
            && analysis.internalAlignmentGeometry[constraint.third.geometryIndex]) {
            analysis.unenforceableConstraints[index] = true;
            continue;
        }

        if (constraint.kind == ConstraintKind::Block || constraint.kind == ConstraintKind::InternalAlignment) {
            continue;
        }

        if (constraint.second.geometryIndex < 0 && constraint.third.geometryIndex < 0
            && constraint.first.geometryIndex >= 0
            && analysis.blockedGeometry[constraint.first.geometryIndex]
            && static_cast<int>(index)
                < analysis.blockConstraintIndexByGeometry[constraint.first.geometryIndex]) {
            analysis.unenforceableConstraints[index] = true;
            continue;
        }

        if (constraint.third.geometryIndex < 0) {
            const bool firstBlocked = isBlockedGeometryRef(analysis, constraint.first);
            const bool secondBlocked = isBlockedGeometryRef(analysis, constraint.second);
            const bool firstExternal = isExternalGeometryRef(model, constraint.first);
            const bool secondExternal = isExternalGeometryRef(model, constraint.second);
            if (((firstBlocked && secondBlocked)
                 && (static_cast<int>(index)
                         < analysis.blockConstraintIndexByGeometry[constraint.first.geometryIndex]
                     || static_cast<int>(index)
                         < analysis.blockConstraintIndexByGeometry[constraint.second.geometryIndex]))
                || (firstExternal && secondBlocked
                    && static_cast<int>(index)
                        < analysis.blockConstraintIndexByGeometry[constraint.second.geometryIndex])
                || (firstBlocked && secondExternal
                    && static_cast<int>(index)
                        < analysis.blockConstraintIndexByGeometry[constraint.first.geometryIndex])) {
                analysis.unenforceableConstraints[index] = true;
            }
            continue;
        }

        const bool firstBlocked = isBlockedGeometryRef(analysis, constraint.first);
        const bool secondBlocked = isBlockedGeometryRef(analysis, constraint.second);
        const bool thirdBlocked = isBlockedGeometryRef(analysis, constraint.third);
        const bool firstExternal = isExternalGeometryRef(model, constraint.first);
        const bool secondExternal = isExternalGeometryRef(model, constraint.second);
        const bool thirdExternal = isExternalGeometryRef(model, constraint.third);
        if (((firstBlocked && secondBlocked && thirdBlocked)
             && (static_cast<int>(index)
                     < analysis.blockConstraintIndexByGeometry[constraint.first.geometryIndex]
                 || static_cast<int>(index)
                     < analysis.blockConstraintIndexByGeometry[constraint.second.geometryIndex]
                 || static_cast<int>(index)
                     < analysis.blockConstraintIndexByGeometry[constraint.third.geometryIndex]))
            || (firstExternal && secondBlocked && thirdBlocked
                && (static_cast<int>(index)
                        < analysis.blockConstraintIndexByGeometry[constraint.second.geometryIndex]
                    || static_cast<int>(index)
                        < analysis.blockConstraintIndexByGeometry[constraint.third.geometryIndex]))
            || (firstBlocked && secondExternal && thirdBlocked
                && (static_cast<int>(index)
                        < analysis.blockConstraintIndexByGeometry[constraint.first.geometryIndex]
                    || static_cast<int>(index)
                        < analysis.blockConstraintIndexByGeometry[constraint.third.geometryIndex]))
            || (firstBlocked && secondBlocked && thirdExternal
                && (static_cast<int>(index)
                        < analysis.blockConstraintIndexByGeometry[constraint.first.geometryIndex]
                    || static_cast<int>(index)
                        < analysis.blockConstraintIndexByGeometry[constraint.second.geometryIndex]))
            || (firstBlocked && secondExternal && thirdExternal
                && static_cast<int>(index)
                    < analysis.blockConstraintIndexByGeometry[constraint.first.geometryIndex])
            || (firstExternal && secondBlocked && thirdExternal
                && static_cast<int>(index)
                    < analysis.blockConstraintIndexByGeometry[constraint.second.geometryIndex])
            || (firstExternal && secondExternal && thirdBlocked
                && static_cast<int>(index)
                    < analysis.blockConstraintIndexByGeometry[constraint.third.geometryIndex])) {
            analysis.unenforceableConstraints[index] = true;
        }
    }

    return analysis;
}

[[nodiscard]] bool analyseBlockedConstraintDependentParameters(
    const SolveContext& context,
    const std::vector<int>& blockedGeoIds,
    std::vector<double*>& paramsToBlock
)
{
    std::vector<std::vector<double*>> groups;
    context.system.getDependentParamsGroups(groups);

    struct ProposedGroup
    {
        std::vector<double*> blockableParams;
        double* blockingParam {nullptr};
    };

    std::vector<ProposedGroup> proposedGroups(groups.size());
    for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex) {
        for (double* parameter : groups[groupIndex]) {
            const auto owner = context.geometryParameterOwners.find(parameter);
            if (owner == context.geometryParameterOwners.end()) {
                continue;
            }
            const auto blocked = std::find(blockedGeoIds.begin(), blockedGeoIds.end(), owner->second);
            if (blocked != blockedGeoIds.end()) {
                proposedGroups[groupIndex].blockableParams.push_back(parameter);
            }
        }
    }

    for (std::size_t groupIndex = proposedGroups.size(); groupIndex-- > 0;) {
        auto& group = proposedGroups[groupIndex];
        for (std::size_t paramIndex = group.blockableParams.size(); paramIndex-- > 0;) {
            double* parameter = group.blockableParams[paramIndex];
            const auto alreadyBlocking = std::find(paramsToBlock.begin(), paramsToBlock.end(), parameter);
            if (alreadyBlocking == paramsToBlock.end()) {
                paramsToBlock.push_back(parameter);
                group.blockingParam = parameter;
                break;
            }
        }
    }

    bool unsatisfiedGroups = false;
    for (const auto& group : proposedGroups) {
        if (!group.blockableParams.empty() && group.blockingParam == nullptr) {
            unsatisfiedGroups = true;
        }
    }
    return unsatisfiedGroups;
}

void fixParametersAndDiagnose(SolveContext& context, const std::vector<double*>& paramsToBlock)
{
    if (paramsToBlock.empty()) {
        return;
    }

    for (double* parameter : paramsToBlock) {
        const auto parameterIt = std::find(context.parameters.begin(), context.parameters.end(), parameter);
        if (parameterIt != context.parameters.end()) {
            context.fixParameters.push_back(*parameterIt);
            context.parameters.erase(parameterIt);
        }
    }

    context.system.clearByTag(GCS::DefaultTemporaryConstraint);
    context.system.invalidatedDiagnosis();
    context.system.declareUnknowns(context.parameters);
    context.system.declareDrivenParams(context.drivenParameters);
    context.system.initSolution(GCS::DogLeg);
}

[[nodiscard]] std::optional<int> pointIdFor(const SolveContext& context, const ElementRef& ref)
{
    if (ref.geometryIndex < 0 || ref.geometryIndex >= static_cast<int>(context.geometries.size())) {
        return std::nullopt;
    }

    const auto& binding = context.geometries[ref.geometryIndex];

    switch (binding.kind) {
        case GeometryKind::Point:
            return binding.startPointId;
        case GeometryKind::LineSegment:
            switch (ref.role) {
                case PointRole::Start:
                    return binding.startPointId;
                case PointRole::End:
                    return binding.endPointId;
                default:
                    return std::nullopt;
            }
        case GeometryKind::Circle:
            if (ref.role == PointRole::Center || ref.role == PointRole::Mid) {
                return binding.midPointId;
            }
            return std::nullopt;
        case GeometryKind::Arc:
            switch (ref.role) {
                case PointRole::Start:
                    return binding.startPointId;
                case PointRole::End:
                    return binding.endPointId;
                case PointRole::Center:
                case PointRole::Mid:
                    return binding.midPointId;
                default:
                    return std::nullopt;
            }
        case GeometryKind::Ellipse:
            if (ref.role == PointRole::Center || ref.role == PointRole::Mid) {
                return binding.midPointId;
            }
            return std::nullopt;
        case GeometryKind::ArcOfEllipse:
        case GeometryKind::ArcOfHyperbola:
        case GeometryKind::ArcOfParabola:
        case GeometryKind::BSpline:
            switch (ref.role) {
                case PointRole::Start:
                    return binding.startPointId;
                case PointRole::End:
                    return binding.endPointId;
                case PointRole::Center:
                case PointRole::Mid:
                    return binding.midPointId;
                default:
                    return std::nullopt;
            }
    }

    return std::nullopt;
}

[[nodiscard]] GCS::Point* pointFor(SolveContext& context, const ElementRef& ref)
{
    auto pointId = pointIdFor(context, ref);
    if (!pointId || *pointId < 0 || *pointId >= static_cast<int>(context.points.size())) {
        return nullptr;
    }
    return &context.points[*pointId];
}

[[nodiscard]] const GeometryBinding* bindingFor(const SolveContext& context, int geometryIndex)
{
    if (geometryIndex < 0 || geometryIndex >= static_cast<int>(context.geometries.size())) {
        return nullptr;
    }
    return &context.geometries[geometryIndex];
}

[[nodiscard]] GCS::Line* lineFor(SolveContext& context, int geometryIndex)
{
    const auto* binding = bindingFor(context, geometryIndex);
    if (!binding || binding->kind != GeometryKind::LineSegment || binding->index < 0
        || binding->index >= static_cast<int>(context.lines.size())) {
        return nullptr;
    }
    return &context.lines[binding->index];
}

[[nodiscard]] GCS::Circle* circleFor(SolveContext& context, int geometryIndex)
{
    const auto* binding = bindingFor(context, geometryIndex);
    if (!binding || binding->kind != GeometryKind::Circle || binding->index < 0
        || binding->index >= static_cast<int>(context.circles.size())) {
        return nullptr;
    }
    return &context.circles[binding->index];
}

[[nodiscard]] GCS::Arc* arcFor(SolveContext& context, int geometryIndex)
{
    const auto* binding = bindingFor(context, geometryIndex);
    if (!binding || binding->kind != GeometryKind::Arc || binding->index < 0
        || binding->index >= static_cast<int>(context.arcs.size())) {
        return nullptr;
    }
    return &context.arcs[binding->index];
}

[[nodiscard]] GCS::Circle* circleLikeFor(SolveContext& context, int geometryIndex)
{
    if (auto* circle = circleFor(context, geometryIndex)) {
        return circle;
    }
    if (auto* arc = arcFor(context, geometryIndex)) {
        return arc;
    }
    return nullptr;
}

[[nodiscard]] GCS::Ellipse* ellipseFor(SolveContext& context, int geometryIndex)
{
    const auto* binding = bindingFor(context, geometryIndex);
    if (!binding || binding->index < 0) {
        return nullptr;
    }

    switch (binding->kind) {
        case GeometryKind::Ellipse:
            if (binding->index >= static_cast<int>(context.ellipses.size())) {
                return nullptr;
            }
            return &context.ellipses[binding->index];
        case GeometryKind::ArcOfEllipse:
            if (binding->index >= static_cast<int>(context.arcOfEllipses.size())) {
                return nullptr;
            }
            return &context.arcOfEllipses[binding->index];
        default:
            return nullptr;
    }
}

[[nodiscard]] GCS::Hyperbola* hyperbolaFor(SolveContext& context, int geometryIndex)
{
    const auto* binding = bindingFor(context, geometryIndex);
    if (!binding || binding->kind != GeometryKind::ArcOfHyperbola || binding->index < 0
        || binding->index >= static_cast<int>(context.arcOfHyperbolas.size())) {
        return nullptr;
    }
    return &context.arcOfHyperbolas[binding->index];
}

[[nodiscard]] GCS::Parabola* parabolaFor(SolveContext& context, int geometryIndex)
{
    const auto* binding = bindingFor(context, geometryIndex);
    if (!binding || binding->kind != GeometryKind::ArcOfParabola || binding->index < 0
        || binding->index >= static_cast<int>(context.arcOfParabolas.size())) {
        return nullptr;
    }
    return &context.arcOfParabolas[binding->index];
}

[[nodiscard]] GCS::ArcOfHyperbola* arcOfHyperbolaFor(SolveContext& context, int geometryIndex)
{
    const auto* binding = bindingFor(context, geometryIndex);
    if (!binding || binding->kind != GeometryKind::ArcOfHyperbola || binding->index < 0
        || binding->index >= static_cast<int>(context.arcOfHyperbolas.size())) {
        return nullptr;
    }
    return &context.arcOfHyperbolas[binding->index];
}

[[nodiscard]] GCS::ArcOfParabola* arcOfParabolaFor(SolveContext& context, int geometryIndex)
{
    const auto* binding = bindingFor(context, geometryIndex);
    if (!binding || binding->kind != GeometryKind::ArcOfParabola || binding->index < 0
        || binding->index >= static_cast<int>(context.arcOfParabolas.size())) {
        return nullptr;
    }
    return &context.arcOfParabolas[binding->index];
}

[[nodiscard]] GCS::BSpline* bSplineFor(SolveContext& context, int geometryIndex)
{
    const auto* binding = bindingFor(context, geometryIndex);
    if (!binding || binding->kind != GeometryKind::BSpline || binding->index < 0
        || binding->index >= static_cast<int>(context.bSplines.size())) {
        return nullptr;
    }
    return &context.bSplines[binding->index];
}

[[nodiscard]] GCS::Curve* curveFor(SolveContext& context, int geometryIndex)
{
    if (auto* line = lineFor(context, geometryIndex)) {
        return line;
    }
    if (auto* circle = circleFor(context, geometryIndex)) {
        return circle;
    }
    if (auto* arc = arcFor(context, geometryIndex)) {
        return arc;
    }
    if (auto* ellipse = ellipseFor(context, geometryIndex)) {
        return ellipse;
    }
    if (auto* hyperbola = hyperbolaFor(context, geometryIndex)) {
        return hyperbola;
    }
    if (auto* parabola = parabolaFor(context, geometryIndex)) {
        return parabola;
    }
    if (auto* spline = bSplineFor(context, geometryIndex)) {
        return spline;
    }
    return nullptr;
}

[[nodiscard]] bool isBSplineGeometry(const SolveContext& context, int geometryIndex)
{
    const auto* binding = bindingFor(context, geometryIndex);
    return binding && binding->kind == GeometryKind::BSpline;
}

[[nodiscard]] double estimateBSplineParameter(const GCS::BSpline& spline, const GCS::Point& point);

bool addPointOnGeometryConstraint(
    SolveContext& context,
    GCS::Point& point,
    int geometryIndex,
    int tag,
    bool driving
)
{
    if (auto* line = lineFor(context, geometryIndex)) {
        context.system.addConstraintPointOnLine(point, *line, tag, driving);
        return true;
    }
    if (auto* circle = circleFor(context, geometryIndex)) {
        context.system.addConstraintPointOnCircle(point, *circle, tag, driving);
        return true;
    }
    if (auto* arc = arcFor(context, geometryIndex)) {
        context.system.addConstraintPointOnArc(point, *arc, tag, driving);
        return true;
    }
    if (auto* ellipse = ellipseFor(context, geometryIndex)) {
        context.system.addConstraintPointOnEllipse(point, *ellipse, tag, driving);
        return true;
    }
    if (auto* hyperbola = arcOfHyperbolaFor(context, geometryIndex)) {
        context.system.addConstraintPointOnHyperbolicArc(point, *hyperbola, tag, driving);
        return true;
    }
    if (auto* parabola = arcOfParabolaFor(context, geometryIndex)) {
        context.system.addConstraintPointOnParabolicArc(point, *parabola, tag, driving);
        return true;
    }
    if (auto* spline = bSplineFor(context, geometryIndex)) {
        auto* pointParam = context.ownParameter(estimateBSplineParameter(*spline, point));
        context.parameters.push_back(pointParam);
        context.system.addConstraintPointOnBSpline(point, *spline, pointParam, tag, driving);
        return true;
    }
    return false;
}

[[nodiscard]] int nextTag(SolveContext& context)
{
    return ++context.constraintsCounter;
}

void indexBSplineKnotAlignments(SolveContext& context, const SketchModel& model)
{
    for (const auto& constraint : model.constraints()) {
        if (constraint.kind != ConstraintKind::InternalAlignment
            || constraint.alignmentType != InternalAlignmentType::BSplineKnotPoint
            || constraint.first.geometryIndex < 0 || constraint.second.geometryIndex < 0
            || constraint.internalAlignmentIndex < 0) {
            continue;
        }

        context.bsplineKnotAlignments.emplace(
            constraint.first.geometryIndex,
            SolveContext::BSplineKnotAlignment {
                .splineGeometryIndex = constraint.second.geometryIndex,
                .knotIndex = static_cast<unsigned int>(constraint.internalAlignmentIndex),
            }
        );
    }
}

[[nodiscard]] const SolveContext::BSplineKnotAlignment* bsplineKnotAlignmentFor(
    const SolveContext& context,
    int pointGeometryIndex
)
{
    const auto it = context.bsplineKnotAlignments.find(pointGeometryIndex);
    return it == context.bsplineKnotAlignments.end() ? nullptr : &it->second;
}

[[nodiscard]] bool usesPointwiseCurveAngleConstraint(const Constraint& constraint)
{
    if (constraint.third.geometryIndex >= 0) {
        return true;
    }
    if (constraint.kind == ConstraintKind::Angle) {
        return false;
    }
    return constraint.first.role != PointRole::None;
}

enum class BSplineKnotTangentResult
{
    NotApplicable,
    Applied,
    Rejected
};

BSplineKnotTangentResult addBSplineKnotTangentConstraint(SolveContext& context, const Constraint& constraint)
{
    if (constraint.kind != ConstraintKind::Tangent || constraint.third.geometryIndex >= 0) {
        return BSplineKnotTangentResult::NotApplicable;
    }

    struct SpecializedTangentInput
    {
        int pointGeometryIndex {-1};
        PointRole pointRole {PointRole::None};
        int lineGeometryIndex {-1};
        PointRole lineRole {PointRole::None};
        int splineGeometryIndex {-1};
        unsigned int knotIndex {0};
    };

    const auto resolveInput = [&](const ElementRef& pointRef, const ElementRef& lineRef)
        -> std::optional<SpecializedTangentInput> {
        if (pointRef.geometryIndex < 0 || pointRef.role == PointRole::None || lineRef.geometryIndex < 0) {
            return std::nullopt;
        }

        const auto* pointBinding = bindingFor(context, pointRef.geometryIndex);
        if (!pointBinding || pointBinding->kind != GeometryKind::Point) {
            return std::nullopt;
        }

        const auto* alignment = bsplineKnotAlignmentFor(context, pointRef.geometryIndex);
        if (!alignment) {
            return std::nullopt;
        }

        if (lineRef.role != PointRole::None && lineRef.role != PointRole::Start
            && lineRef.role != PointRole::End) {
            return std::nullopt;
        }

        if (!lineFor(context, lineRef.geometryIndex) || !bSplineFor(context, alignment->splineGeometryIndex)) {
            return std::nullopt;
        }

        return SpecializedTangentInput {
            .pointGeometryIndex = pointRef.geometryIndex,
            .pointRole = pointRef.role,
            .lineGeometryIndex = lineRef.geometryIndex,
            .lineRole = lineRef.role,
            .splineGeometryIndex = alignment->splineGeometryIndex,
            .knotIndex = alignment->knotIndex,
        };
    };

    auto input = resolveInput(constraint.first, constraint.second);
    if (!input) {
        input = resolveInput(constraint.second, constraint.first);
    }
    if (!input) {
        return BSplineKnotTangentResult::NotApplicable;
    }

    auto* line = lineFor(context, input->lineGeometryIndex);
    auto* spline = bSplineFor(context, input->splineGeometryIndex);
    auto* knotPoint = pointFor(
        context,
        {.geometryIndex = input->pointGeometryIndex, .role = input->pointRole}
    );
    if (!line || !spline || !knotPoint || input->knotIndex >= spline->knots.size()
        || input->knotIndex >= spline->mult.size()) {
        return BSplineKnotTangentResult::Rejected;
    }

    // For periodic B-splines the trailing duplicate closing knot is geometrically
    // identical to knot 0 (FreeCAD GUI: "we do not need the last pole/knot"). Adding
    // the same tangent twice would crash the planegcs flattened-knot indexing and
    // produce redundant constraints; treat it as not applicable here.
    if (spline->periodic && input->knotIndex + 1 == spline->knots.size()) {
        return BSplineKnotTangentResult::NotApplicable;
    }

    if (spline->mult[input->knotIndex] >= spline->degree) {
        return BSplineKnotTangentResult::Rejected;
    }

    const int tag = nextTag(context);
    if (input->lineRole == PointRole::None) {
        context.system.addConstraintPointOnLine(*knotPoint, *line, tag, constraint.driving);
    }
    else {
        auto* endpoint = pointFor(
            context,
            {.geometryIndex = input->lineGeometryIndex, .role = input->lineRole}
        );
        if (!endpoint) {
            return BSplineKnotTangentResult::Rejected;
        }
        context.system.addConstraintP2PCoincident(*endpoint, *knotPoint, tag, constraint.driving);
    }
    context.system.addConstraintTangentAtBSplineKnot(
        *spline,
        *line,
        input->knotIndex,
        tag,
        constraint.driving
    );
    return BSplineKnotTangentResult::Applied;
}

bool addPointwiseCurveAngleConstraint(SolveContext& context, const Constraint& constraint)
{
    if ((constraint.kind != ConstraintKind::Angle && constraint.kind != ConstraintKind::Tangent
         && constraint.kind != ConstraintKind::Perpendicular)
        || !constraint.hasValue) {
        return false;
    }

    const bool avp = constraint.third.geometryIndex >= 0;
    const bool e2c =
        !avp && constraint.first.role != PointRole::None && constraint.second.role == PointRole::None;
    const bool e2e =
        !avp && constraint.first.role != PointRole::None && constraint.second.role != PointRole::None;
    if (!(avp || e2c || e2e)) {
        return false;
    }

    auto* firstCurve = curveFor(context, constraint.first.geometryIndex);
    auto* secondCurve = curveFor(context, constraint.second.geometryIndex);
    if (!firstCurve || !secondCurve) {
        return false;
    }

    GCS::Point* point = nullptr;
    if (avp) {
        point = pointFor(context, constraint.third);
    }
    else {
        point = pointFor(context, constraint.first);
    }
    if (!point) {
        return false;
    }

    GCS::Point* secondPoint = nullptr;
    if (e2e) {
        secondPoint = pointFor(context, constraint.second);
        if (!secondPoint) {
            return false;
        }
    }

    auto* angle = context.ownConstraintValue(constraint.value, constraint.driving);
    if (constraint.kind != ConstraintKind::Angle) {
        double angleOffset = 0.0;
        double angleDesire = 0.0;
        if (constraint.kind == ConstraintKind::Tangent) {
            angleOffset = -std::numbers::pi / 2.0;
            angleDesire = 0.0;
        }
        else {
            angleDesire = std::numbers::pi / 2.0;
        }

        if (std::abs(constraint.value) <= 1e-12) {
            double angleError = context.system.calculateAngleViaPoint(*firstCurve, *secondCurve, *point)
                - angleDesire;
            if (angleError > std::numbers::pi) {
                angleError -= 2.0 * std::numbers::pi;
            }
            if (angleError < -std::numbers::pi) {
                angleError += 2.0 * std::numbers::pi;
            }
            if (std::abs(angleError) > std::numbers::pi / 2.0) {
                angleDesire += std::numbers::pi;
            }
            *angle = angleDesire;
        }
        else {
            *angle = constraint.value - angleOffset;
        }
    }

    const int tag = nextTag(context);
    if (e2c) {
        if (isBSplineGeometry(context, constraint.second.geometryIndex)) {
            auto* spline = bSplineFor(context, constraint.second.geometryIndex);
            if (!spline) {
                return false;
            }
            auto* pointParam = context.ownParameter(estimateBSplineParameter(*spline, *point));
            context.parameters.push_back(pointParam);
            context.system.addConstraintPointOnBSpline(*point, *spline, pointParam, tag, constraint.driving);
            context.system.addConstraintAngleViaPointAndParam(
                *secondCurve,
                *firstCurve,
                *point,
                pointParam,
                angle,
                tag,
                constraint.driving
            );
            return true;
        }
        if (!addPointOnGeometryConstraint(
                context,
                *point,
                constraint.second.geometryIndex,
                tag,
                constraint.driving
            )) {
            return false;
        }
        context.system.addConstraintAngleViaPoint(
            *firstCurve,
            *secondCurve,
            *point,
            angle,
            tag,
            constraint.driving
        );
        return true;
    }

    if (e2e) {
        context.system.addConstraintP2PCoincident(*point, *secondPoint, tag, constraint.driving);
        if (isBSplineGeometry(context, constraint.first.geometryIndex)
            && isBSplineGeometry(context, constraint.second.geometryIndex)) {
            context.system.addConstraintAngleViaTwoPoints(
                *firstCurve,
                *secondCurve,
                *point,
                *secondPoint,
                angle,
                tag,
                constraint.driving
            );
        }
        else {
            context.system.addConstraintAngleViaPoint(
                *firstCurve,
                *secondCurve,
                *point,
                angle,
                tag,
                constraint.driving
            );
        }
        return true;
    }

    const bool firstIsBSpline = isBSplineGeometry(context, constraint.first.geometryIndex);
    const bool secondIsBSpline = isBSplineGeometry(context, constraint.second.geometryIndex);
    if (firstIsBSpline && secondIsBSpline) {
        auto* firstSpline = bSplineFor(context, constraint.first.geometryIndex);
        auto* secondSpline = bSplineFor(context, constraint.second.geometryIndex);
        if (!firstSpline || !secondSpline) {
            return false;
        }
        auto* firstPointParam = context.ownParameter(estimateBSplineParameter(*firstSpline, *point));
        auto* secondPointParam = context.ownParameter(estimateBSplineParameter(*secondSpline, *point));
        context.parameters.push_back(firstPointParam);
        context.parameters.push_back(secondPointParam);
        context.system.addConstraintPointOnBSpline(
            *point,
            *firstSpline,
            firstPointParam,
            tag,
            constraint.driving
        );
        context.system.addConstraintPointOnBSpline(
            *point,
            *secondSpline,
            secondPointParam,
            tag,
            constraint.driving
        );
        context.system.addConstraintAngleViaPointAndTwoParams(
            *firstCurve,
            *secondCurve,
            *point,
            firstPointParam,
            secondPointParam,
            angle,
            tag,
            constraint.driving
        );
        return true;
    }
    if (firstIsBSpline || secondIsBSpline) {
        GCS::Curve* splineCurve = firstCurve;
        GCS::Curve* otherCurve = secondCurve;
        int splineGeometry = constraint.first.geometryIndex;
        if (!firstIsBSpline) {
            splineCurve = secondCurve;
            otherCurve = firstCurve;
            splineGeometry = constraint.second.geometryIndex;
        }
        auto* spline = bSplineFor(context, splineGeometry);
        if (!spline) {
            return false;
        }
        auto* pointParam = context.ownParameter(estimateBSplineParameter(*spline, *point));
        context.parameters.push_back(pointParam);
        context.system.addConstraintPointOnBSpline(*point, *spline, pointParam, tag, constraint.driving);
        context.system.addConstraintAngleViaPointAndParam(
            *splineCurve,
            *otherCurve,
            *point,
            pointParam,
            angle,
            tag,
            constraint.driving
        );
        return true;
    }

    context.system.addConstraintAngleViaPoint(
        *firstCurve,
        *secondCurve,
        *point,
        angle,
        tag,
        constraint.driving
    );
    return true;
}

[[nodiscard]] Point2 ellipseValue(
    const Point2& center,
    const Point2& focus1,
    double minorRadius,
    double angle
)
{
    const double axisX = focus1.x - center.x;
    const double axisY = focus1.y - center.y;
    const double axisLength = std::sqrt(axisX * axisX + axisY * axisY);
    const double majorRadius = std::sqrt(axisLength * axisLength + minorRadius * minorRadius);
    const double ex = axisLength > 0.0 ? axisX / axisLength : 1.0;
    const double ey = axisLength > 0.0 ? axisY / axisLength : 0.0;
    const double px = -ey;
    const double py = ex;
    return {
        .x = center.x + majorRadius * std::cos(angle) * ex + minorRadius * std::sin(angle) * px,
        .y = center.y + majorRadius * std::cos(angle) * ey + minorRadius * std::sin(angle) * py,
    };
}

[[nodiscard]] Point2 hyperbolaValue(
    const Point2& center,
    const Point2& focus1,
    double minorRadius,
    double angle
)
{
    const double axisX = focus1.x - center.x;
    const double axisY = focus1.y - center.y;
    const double axisLength = std::sqrt(axisX * axisX + axisY * axisY);
    const double majorRadius = std::sqrt(std::max(0.0, axisLength * axisLength - minorRadius * minorRadius));
    const double ex = axisLength > 0.0 ? axisX / axisLength : 1.0;
    const double ey = axisLength > 0.0 ? axisY / axisLength : 0.0;
    const double px = -ey;
    const double py = ex;
    return {
        .x = center.x + majorRadius * std::cosh(angle) * ex + minorRadius * std::sinh(angle) * px,
        .y = center.y + majorRadius * std::cosh(angle) * ey + minorRadius * std::sinh(angle) * py,
    };
}

[[nodiscard]] Point2 parabolaValue(const Point2& vertex, const Point2& focus1, double angle)
{
    const double axisX = focus1.x - vertex.x;
    const double axisY = focus1.y - vertex.y;
    const double focal = std::sqrt(axisX * axisX + axisY * axisY);
    const double ex = focal > 0.0 ? axisX / focal : 1.0;
    const double ey = focal > 0.0 ? axisY / focal : 0.0;
    const double px = -ey;
    const double py = ex;
    return {
        .x = vertex.x + (angle * angle / (4.0 * std::max(focal, 1e-12))) * ex + angle * px,
        .y = vertex.y + (angle * angle / (4.0 * std::max(focal, 1e-12))) * ey + angle * py,
    };
}

[[nodiscard]] double estimateBSplineParameter(const GCS::BSpline& spline, const GCS::Point& point)
{
    if (spline.knots.empty()) {
        return 0.0;
    }

    double minU = *spline.knots.front();
    double maxU = *spline.knots.back();
    if (!spline.flattenedknots.empty()) {
        minU = spline.flattenedknots.front();
        maxU = spline.flattenedknots.back();
    }
    if (maxU < minU) {
        std::swap(minU, maxU);
    }

    double bestU = minU;
    double bestDistance = std::numeric_limits<double>::infinity();
    constexpr int samples = 128;
    for (int i = 0; i <= samples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(samples);
        const double u = minU + (maxU - minU) * t;
        const auto value = spline.Value(u, 0.0);
        const double dx = value.x - *point.x;
        const double dy = value.y - *point.y;
        const double distance = dx * dx + dy * dy;
        if (distance < bestDistance) {
            bestDistance = distance;
            bestU = u;
        }
    }
    return bestU;
}

void addPointGeometry(SolveContext& context, const PointGeometry& geometry, bool fixed, int geometryIndex)
{
    auto* x = context.ownParameter(geometry.point.x);
    auto* y = context.ownParameter(geometry.point.y);
    storeGeometryParameter(context, geometryIndex, x, fixed);
    storeGeometryParameter(context, geometryIndex, y, fixed);

    GCS::Point point;
    point.x = x;
    point.y = y;

    const auto pointId = static_cast<int>(context.points.size());
    context.points.push_back(point);
    context.geometries.push_back({
        .kind = GeometryKind::Point,
        .index = -1,
        .startPointId = pointId,
        .endPointId = pointId,
        .midPointId = pointId,
    });
}

void addLineGeometry(SolveContext& context, const LineSegmentGeometry& geometry, bool fixed, int geometryIndex)
{
    auto* x1 = context.ownParameter(geometry.start.x);
    auto* y1 = context.ownParameter(geometry.start.y);
    auto* x2 = context.ownParameter(geometry.end.x);
    auto* y2 = context.ownParameter(geometry.end.y);
    storeGeometryParameter(context, geometryIndex, x1, fixed);
    storeGeometryParameter(context, geometryIndex, y1, fixed);
    storeGeometryParameter(context, geometryIndex, x2, fixed);
    storeGeometryParameter(context, geometryIndex, y2, fixed);

    GCS::Point start;
    start.x = x1;
    start.y = y1;
    GCS::Point end;
    end.x = x2;
    end.y = y2;

    const auto startPointId = static_cast<int>(context.points.size());
    context.points.push_back(start);
    const auto endPointId = static_cast<int>(context.points.size());
    context.points.push_back(end);

    GCS::Line line;
    line.p1 = start;
    line.p2 = end;
    const auto lineIndex = static_cast<int>(context.lines.size());
    context.lines.push_back(line);

    context.geometries.push_back({
        .kind = GeometryKind::LineSegment,
        .index = lineIndex,
        .startPointId = startPointId,
        .endPointId = endPointId,
        .midPointId = -1,
    });
}

void addCircleGeometry(SolveContext& context, const CircleGeometry& geometry, bool fixed, int geometryIndex)
{
    auto* x = context.ownParameter(geometry.center.x);
    auto* y = context.ownParameter(geometry.center.y);
    auto* radius = context.ownParameter(geometry.radius);
    storeGeometryParameter(context, geometryIndex, x, fixed);
    storeGeometryParameter(context, geometryIndex, y, fixed);
    storeGeometryParameter(context, geometryIndex, radius, fixed);

    GCS::Point center;
    center.x = x;
    center.y = y;
    const auto centerPointId = static_cast<int>(context.points.size());
    context.points.push_back(center);

    GCS::Circle circle;
    circle.center = center;
    circle.rad = radius;
    const auto circleIndex = static_cast<int>(context.circles.size());
    context.circles.push_back(circle);

    context.geometries.push_back({
        .kind = GeometryKind::Circle,
        .index = circleIndex,
        .startPointId = -1,
        .endPointId = -1,
        .midPointId = centerPointId,
    });
}

void addArcGeometry(SolveContext& context, const ArcGeometry& geometry, bool fixed, int geometryIndex)
{
    const auto start = Point2 {
        .x = geometry.center.x + geometry.radius * std::cos(geometry.startAngle),
        .y = geometry.center.y + geometry.radius * std::sin(geometry.startAngle),
    };
    const auto end = Point2 {
        .x = geometry.center.x + geometry.radius * std::cos(geometry.endAngle),
        .y = geometry.center.y + geometry.radius * std::sin(geometry.endAngle),
    };

    auto* sx = context.ownParameter(start.x);
    auto* sy = context.ownParameter(start.y);
    auto* ex = context.ownParameter(end.x);
    auto* ey = context.ownParameter(end.y);
    auto* cx = context.ownParameter(geometry.center.x);
    auto* cy = context.ownParameter(geometry.center.y);
    auto* radius = context.ownParameter(geometry.radius);
    auto* startAngle = context.ownParameter(geometry.startAngle);
    auto* endAngle = context.ownParameter(geometry.endAngle);
    storeGeometryParameter(context, geometryIndex, sx, fixed);
    storeGeometryParameter(context, geometryIndex, sy, fixed);
    storeGeometryParameter(context, geometryIndex, ex, fixed);
    storeGeometryParameter(context, geometryIndex, ey, fixed);
    storeGeometryParameter(context, geometryIndex, cx, fixed);
    storeGeometryParameter(context, geometryIndex, cy, fixed);
    storeGeometryParameter(context, geometryIndex, radius, fixed);
    storeGeometryParameter(context, geometryIndex, startAngle, fixed);
    storeGeometryParameter(context, geometryIndex, endAngle, fixed);

    GCS::Point startPoint;
    startPoint.x = sx;
    startPoint.y = sy;
    GCS::Point endPoint;
    endPoint.x = ex;
    endPoint.y = ey;
    GCS::Point centerPoint;
    centerPoint.x = cx;
    centerPoint.y = cy;

    const auto startPointId = static_cast<int>(context.points.size());
    context.points.push_back(startPoint);
    const auto endPointId = static_cast<int>(context.points.size());
    context.points.push_back(endPoint);
    const auto centerPointId = static_cast<int>(context.points.size());
    context.points.push_back(centerPoint);

    GCS::Arc arc;
    arc.start = startPoint;
    arc.end = endPoint;
    arc.center = centerPoint;
    arc.rad = radius;
    arc.startAngle = startAngle;
    arc.endAngle = endAngle;
    const auto arcIndex = static_cast<int>(context.arcs.size());
    context.arcs.push_back(arc);

    context.geometries.push_back({
        .kind = GeometryKind::Arc,
        .index = arcIndex,
        .startPointId = startPointId,
        .endPointId = endPointId,
        .midPointId = centerPointId,
    });

    if (!fixed) {
        context.system.addConstraintArcRules(context.arcs.back(), nextTag(context));
    }
}

void addEllipseGeometry(SolveContext& context, const EllipseGeometry& geometry, bool fixed, int geometryIndex)
{
    auto* cx = context.ownParameter(geometry.center.x);
    auto* cy = context.ownParameter(geometry.center.y);
    auto* fx = context.ownParameter(geometry.focus1.x);
    auto* fy = context.ownParameter(geometry.focus1.y);
    auto* minorRadius = context.ownParameter(geometry.minorRadius);
    storeGeometryParameter(context, geometryIndex, cx, fixed);
    storeGeometryParameter(context, geometryIndex, cy, fixed);
    storeGeometryParameter(context, geometryIndex, fx, fixed);
    storeGeometryParameter(context, geometryIndex, fy, fixed);
    storeGeometryParameter(context, geometryIndex, minorRadius, fixed);

    GCS::Point center;
    center.x = cx;
    center.y = cy;
    GCS::Point focus1;
    focus1.x = fx;
    focus1.y = fy;

    const auto centerPointId = static_cast<int>(context.points.size());
    context.points.push_back(center);

    GCS::Ellipse ellipse;
    ellipse.center = center;
    ellipse.focus1 = focus1;
    ellipse.radmin = minorRadius;
    const auto ellipseIndex = static_cast<int>(context.ellipses.size());
    context.ellipses.push_back(ellipse);

    context.geometries.push_back({
        .kind = GeometryKind::Ellipse,
        .index = ellipseIndex,
        .startPointId = -1,
        .endPointId = -1,
        .midPointId = centerPointId,
    });
}

void addArcOfEllipseGeometry(SolveContext& context, const ArcOfEllipseGeometry& geometry, bool fixed, int geometryIndex)
{
    const auto start = ellipseValue(geometry.center, geometry.focus1, geometry.minorRadius, geometry.startAngle);
    const auto end = ellipseValue(geometry.center, geometry.focus1, geometry.minorRadius, geometry.endAngle);

    auto* sx = context.ownParameter(start.x);
    auto* sy = context.ownParameter(start.y);
    auto* ex = context.ownParameter(end.x);
    auto* ey = context.ownParameter(end.y);
    auto* cx = context.ownParameter(geometry.center.x);
    auto* cy = context.ownParameter(geometry.center.y);
    auto* fx = context.ownParameter(geometry.focus1.x);
    auto* fy = context.ownParameter(geometry.focus1.y);
    auto* minorRadius = context.ownParameter(geometry.minorRadius);
    auto* startAngle = context.ownParameter(geometry.startAngle);
    auto* endAngle = context.ownParameter(geometry.endAngle);
    storeGeometryParameter(context, geometryIndex, sx, fixed);
    storeGeometryParameter(context, geometryIndex, sy, fixed);
    storeGeometryParameter(context, geometryIndex, ex, fixed);
    storeGeometryParameter(context, geometryIndex, ey, fixed);
    storeGeometryParameter(context, geometryIndex, cx, fixed);
    storeGeometryParameter(context, geometryIndex, cy, fixed);
    storeGeometryParameter(context, geometryIndex, fx, fixed);
    storeGeometryParameter(context, geometryIndex, fy, fixed);
    storeGeometryParameter(context, geometryIndex, minorRadius, fixed);
    storeGeometryParameter(context, geometryIndex, startAngle, fixed);
    storeGeometryParameter(context, geometryIndex, endAngle, fixed);

    GCS::Point startPoint;
    startPoint.x = sx;
    startPoint.y = sy;
    GCS::Point endPoint;
    endPoint.x = ex;
    endPoint.y = ey;
    GCS::Point centerPoint;
    centerPoint.x = cx;
    centerPoint.y = cy;
    GCS::Point focusPoint;
    focusPoint.x = fx;
    focusPoint.y = fy;

    const auto startPointId = static_cast<int>(context.points.size());
    context.points.push_back(startPoint);
    const auto endPointId = static_cast<int>(context.points.size());
    context.points.push_back(endPoint);
    const auto centerPointId = static_cast<int>(context.points.size());
    context.points.push_back(centerPoint);

    GCS::ArcOfEllipse arc;
    arc.start = startPoint;
    arc.end = endPoint;
    arc.center = centerPoint;
    arc.focus1 = focusPoint;
    arc.radmin = minorRadius;
    arc.startAngle = startAngle;
    arc.endAngle = endAngle;
    const auto arcIndex = static_cast<int>(context.arcOfEllipses.size());
    context.arcOfEllipses.push_back(arc);

    context.geometries.push_back({
        .kind = GeometryKind::ArcOfEllipse,
        .index = arcIndex,
        .startPointId = startPointId,
        .endPointId = endPointId,
        .midPointId = centerPointId,
    });

    if (!fixed) {
        context.system.addConstraintArcOfEllipseRules(context.arcOfEllipses.back(), nextTag(context));
    }
}

void addArcOfHyperbolaGeometry(SolveContext& context, const ArcOfHyperbolaGeometry& geometry, bool fixed, int geometryIndex)
{
    const auto start = hyperbolaValue(geometry.center, geometry.focus1, geometry.minorRadius, geometry.startAngle);
    const auto end = hyperbolaValue(geometry.center, geometry.focus1, geometry.minorRadius, geometry.endAngle);

    auto* sx = context.ownParameter(start.x);
    auto* sy = context.ownParameter(start.y);
    auto* ex = context.ownParameter(end.x);
    auto* ey = context.ownParameter(end.y);
    auto* cx = context.ownParameter(geometry.center.x);
    auto* cy = context.ownParameter(geometry.center.y);
    auto* fx = context.ownParameter(geometry.focus1.x);
    auto* fy = context.ownParameter(geometry.focus1.y);
    auto* minorRadius = context.ownParameter(geometry.minorRadius);
    auto* startAngle = context.ownParameter(geometry.startAngle);
    auto* endAngle = context.ownParameter(geometry.endAngle);
    storeGeometryParameter(context, geometryIndex, sx, fixed);
    storeGeometryParameter(context, geometryIndex, sy, fixed);
    storeGeometryParameter(context, geometryIndex, ex, fixed);
    storeGeometryParameter(context, geometryIndex, ey, fixed);
    storeGeometryParameter(context, geometryIndex, cx, fixed);
    storeGeometryParameter(context, geometryIndex, cy, fixed);
    storeGeometryParameter(context, geometryIndex, fx, fixed);
    storeGeometryParameter(context, geometryIndex, fy, fixed);
    storeGeometryParameter(context, geometryIndex, minorRadius, fixed);
    storeGeometryParameter(context, geometryIndex, startAngle, fixed);
    storeGeometryParameter(context, geometryIndex, endAngle, fixed);

    GCS::Point startPoint {sx, sy};
    GCS::Point endPoint {ex, ey};
    GCS::Point centerPoint {cx, cy};
    GCS::Point focusPoint {fx, fy};

    const auto startPointId = static_cast<int>(context.points.size());
    context.points.push_back(startPoint);
    const auto endPointId = static_cast<int>(context.points.size());
    context.points.push_back(endPoint);
    const auto centerPointId = static_cast<int>(context.points.size());
    context.points.push_back(centerPoint);

    GCS::ArcOfHyperbola arc;
    arc.start = startPoint;
    arc.end = endPoint;
    arc.center = centerPoint;
    arc.focus1 = focusPoint;
    arc.radmin = minorRadius;
    arc.startAngle = startAngle;
    arc.endAngle = endAngle;
    const auto arcIndex = static_cast<int>(context.arcOfHyperbolas.size());
    context.arcOfHyperbolas.push_back(arc);

    context.geometries.push_back({
        .kind = GeometryKind::ArcOfHyperbola,
        .index = arcIndex,
        .startPointId = startPointId,
        .endPointId = endPointId,
        .midPointId = centerPointId,
    });

    if (!fixed) {
        context.system.addConstraintArcOfHyperbolaRules(context.arcOfHyperbolas.back(), nextTag(context));
    }
}

void addArcOfParabolaGeometry(SolveContext& context, const ArcOfParabolaGeometry& geometry, bool fixed, int geometryIndex)
{
    const auto start = parabolaValue(geometry.vertex, geometry.focus1, geometry.startAngle);
    const auto end = parabolaValue(geometry.vertex, geometry.focus1, geometry.endAngle);

    auto* sx = context.ownParameter(start.x);
    auto* sy = context.ownParameter(start.y);
    auto* ex = context.ownParameter(end.x);
    auto* ey = context.ownParameter(end.y);
    auto* vx = context.ownParameter(geometry.vertex.x);
    auto* vy = context.ownParameter(geometry.vertex.y);
    auto* fx = context.ownParameter(geometry.focus1.x);
    auto* fy = context.ownParameter(geometry.focus1.y);
    auto* startAngle = context.ownParameter(geometry.startAngle);
    auto* endAngle = context.ownParameter(geometry.endAngle);
    storeGeometryParameter(context, geometryIndex, sx, fixed);
    storeGeometryParameter(context, geometryIndex, sy, fixed);
    storeGeometryParameter(context, geometryIndex, ex, fixed);
    storeGeometryParameter(context, geometryIndex, ey, fixed);
    storeGeometryParameter(context, geometryIndex, vx, fixed);
    storeGeometryParameter(context, geometryIndex, vy, fixed);
    storeGeometryParameter(context, geometryIndex, fx, fixed);
    storeGeometryParameter(context, geometryIndex, fy, fixed);
    storeGeometryParameter(context, geometryIndex, startAngle, fixed);
    storeGeometryParameter(context, geometryIndex, endAngle, fixed);

    GCS::Point startPoint {sx, sy};
    GCS::Point endPoint {ex, ey};
    GCS::Point vertexPoint {vx, vy};
    GCS::Point focusPoint {fx, fy};

    const auto startPointId = static_cast<int>(context.points.size());
    context.points.push_back(startPoint);
    const auto endPointId = static_cast<int>(context.points.size());
    context.points.push_back(endPoint);
    const auto vertexPointId = static_cast<int>(context.points.size());
    context.points.push_back(vertexPoint);

    GCS::ArcOfParabola arc;
    arc.start = startPoint;
    arc.end = endPoint;
    arc.vertex = vertexPoint;
    arc.focus1 = focusPoint;
    arc.startAngle = startAngle;
    arc.endAngle = endAngle;
    const auto arcIndex = static_cast<int>(context.arcOfParabolas.size());
    context.arcOfParabolas.push_back(arc);

    context.geometries.push_back({
        .kind = GeometryKind::ArcOfParabola,
        .index = arcIndex,
        .startPointId = startPointId,
        .endPointId = endPointId,
        .midPointId = vertexPointId,
    });

    if (!fixed) {
        context.system.addConstraintArcOfParabolaRules(context.arcOfParabolas.back(), nextTag(context));
    }
}

void addBSplineGeometry(SolveContext& context, const BSplineGeometry& geometry, bool fixed, int geometryIndex)
{
    std::vector<GCS::Point> poles;
    poles.reserve(geometry.poles.size());
    for (const auto& pole : geometry.poles) {
        auto* px = context.ownParameter(pole.point.x);
        auto* py = context.ownParameter(pole.point.y);
        storeGeometryParameter(context, geometryIndex, px, fixed);
        storeGeometryParameter(context, geometryIndex, py, fixed);
        poles.push_back({px, py});
    }

    std::vector<double*> weights;
    weights.reserve(geometry.poles.size());

    // OCC weight normalization hack (mirrors FreeCAD Sketch.cpp:1437-1464).
    // When exactly one pole weight is 1.0 and the rest differ, OCC's polynomial->rational
    // transition resets the unconstrained pole's weight to 1.0 after the first solve, which
    // visually collapses that pole to a 1 mm circle. Pre-perturb the lone 1.0 to (lastnotone * 0.99)
    // so the solver sees a fully rational set.
    std::vector<double> adjustedWeights;
    adjustedWeights.reserve(geometry.poles.size());
    for (const auto& pole : geometry.poles) {
        adjustedWeights.push_back(pole.weight);
    }
    {
        int lastOneIndex = -1;
        int countOnes = 0;
        double lastNotOne = 1.0;
        for (std::size_t i = 0; i < adjustedWeights.size(); ++i) {
            if (adjustedWeights[i] != 1.0) {
                lastNotOne = adjustedWeights[i];
            }
            else {
                lastOneIndex = static_cast<int>(i);
                ++countOnes;
            }
        }
        if (countOnes == 1 && lastOneIndex >= 0) {
            adjustedWeights[static_cast<std::size_t>(lastOneIndex)] = lastNotOne * 0.99;
        }
    }

    for (std::size_t i = 0; i < geometry.poles.size(); ++i) {
        auto* weight = context.ownParameter(adjustedWeights[i]);
        storeGeometryParameter(context, geometryIndex, weight, fixed);
        weights.push_back(weight);
    }

    std::vector<double*> knots;
    knots.reserve(geometry.knots.size());
    for (const auto& knot : geometry.knots) {
        auto* value = context.ownParameter(knot.value);
        // Knots are not solver parameters (aligned with FreeCAD).
        knots.push_back(value);
    }

    const Point2 startPointValue = geometry.poles.empty() ? Point2 {} : geometry.poles.front().point;
    const Point2 endPointValue = geometry.poles.empty() ? Point2 {} : geometry.poles.back().point;
    auto* sx = context.ownParameter(startPointValue.x);
    auto* sy = context.ownParameter(startPointValue.y);
    auto* ex = context.ownParameter(endPointValue.x);
    auto* ey = context.ownParameter(endPointValue.y);
    storeGeometryParameter(context, geometryIndex, sx, fixed);
    storeGeometryParameter(context, geometryIndex, sy, fixed);
    storeGeometryParameter(context, geometryIndex, ex, fixed);
    storeGeometryParameter(context, geometryIndex, ey, fixed);

    GCS::Point startPoint {sx, sy};
    GCS::Point endPoint {ex, ey};
    const auto startPointId = static_cast<int>(context.points.size());
    context.points.push_back(startPoint);
    const auto endPointId = static_cast<int>(context.points.size());
    context.points.push_back(endPoint);

    GCS::BSpline spline;
    spline.poles = std::move(poles);
    spline.weights = std::move(weights);
    spline.knots = std::move(knots);
    spline.start = startPoint;
    spline.end = endPoint;
    spline.degree = geometry.degree;
    spline.periodic = geometry.periodic;
    spline.mult.reserve(geometry.knots.size());
    spline.knotpointGeoids.resize(geometry.knots.size(), -2000);
    for (const auto& knot : geometry.knots) {
        spline.mult.push_back(knot.multiplicity);
    }
    spline.setupFlattenedKnots();

    const auto splineIndex = static_cast<int>(context.bSplines.size());
    context.bSplines.push_back(std::move(spline));

    if (!fixed && !geometry.periodic && !geometry.knots.empty()) {
        auto& addedSpline = context.bSplines.back();
        if (geometry.knots.front().multiplicity > geometry.degree && !addedSpline.poles.empty()) {
            context.system.addConstraintP2PCoincident(addedSpline.poles.front(), addedSpline.start);
        }
        if (geometry.knots.back().multiplicity > geometry.degree && !addedSpline.poles.empty()) {
            context.system.addConstraintP2PCoincident(addedSpline.poles.back(), addedSpline.end);
        }
    }

    context.geometries.push_back({
        .kind = GeometryKind::BSpline,
        .index = splineIndex,
        .startPointId = startPointId,
        .endPointId = endPointId,
        .midPointId = -1,
    });
}

bool addGeometry(SolveContext& context, const Geometry& geometry, int geometryIndex)
{
    const bool fixed = geometry.external
        || (geometryIndex >= 0 && geometryIndex < static_cast<int>(context.blockedGeometry.size())
            && context.blockedGeometry[geometryIndex]);
    switch (geometry.kind) {
        case GeometryKind::Point:
            addPointGeometry(context, std::get<PointGeometry>(geometry.data), fixed, geometryIndex);
            return true;
        case GeometryKind::LineSegment:
            addLineGeometry(context, std::get<LineSegmentGeometry>(geometry.data), fixed, geometryIndex);
            return true;
        case GeometryKind::Circle:
            addCircleGeometry(context, std::get<CircleGeometry>(geometry.data), fixed, geometryIndex);
            return true;
        case GeometryKind::Arc:
            addArcGeometry(context, std::get<ArcGeometry>(geometry.data), fixed, geometryIndex);
            return true;
        case GeometryKind::Ellipse:
            addEllipseGeometry(context, std::get<EllipseGeometry>(geometry.data), fixed, geometryIndex);
            return true;
        case GeometryKind::ArcOfEllipse:
            addArcOfEllipseGeometry(context, std::get<ArcOfEllipseGeometry>(geometry.data), fixed, geometryIndex);
            return true;
        case GeometryKind::ArcOfHyperbola:
            addArcOfHyperbolaGeometry(context, std::get<ArcOfHyperbolaGeometry>(geometry.data), fixed, geometryIndex);
            return true;
        case GeometryKind::ArcOfParabola:
            addArcOfParabolaGeometry(context, std::get<ArcOfParabolaGeometry>(geometry.data), fixed, geometryIndex);
            return true;
        case GeometryKind::BSpline:
            addBSplineGeometry(context, std::get<BSplineGeometry>(geometry.data), fixed, geometryIndex);
            return true;
    }

    return false;
}

bool addConstraint(SolveContext& context, const Constraint& constraint)
{
    switch (constraint.kind) {
        case ConstraintKind::Coincident: {
            auto* p1 = pointFor(context, constraint.first);
            auto* p2 = pointFor(context, constraint.second);
            if (!p1 || !p2) {
                return false;
            }
            context.system.addConstraintP2PCoincident(*p1, *p2, nextTag(context), constraint.driving);
            return true;
        }
        case ConstraintKind::PointOnObject: {
            auto* point = pointFor(context, constraint.first);
            if (!point || constraint.second.role != PointRole::None) {
                return false;
            }

            if (auto* line = lineFor(context, constraint.second.geometryIndex)) {
                context.system.addConstraintPointOnLine(*point, *line, nextTag(context), constraint.driving);
                return true;
            }
            if (auto* circle = circleFor(context, constraint.second.geometryIndex)) {
                context.system.addConstraintPointOnCircle(*point, *circle, nextTag(context), constraint.driving);
                return true;
            }
            if (auto* arc = arcFor(context, constraint.second.geometryIndex)) {
                context.system.addConstraintPointOnArc(*point, *arc, nextTag(context), constraint.driving);
                return true;
            }
            if (auto* ellipse = ellipseFor(context, constraint.second.geometryIndex)) {
                context.system.addConstraintPointOnEllipse(*point, *ellipse, nextTag(context), constraint.driving);
                return true;
            }
            if (auto* hyperbola = arcOfHyperbolaFor(context, constraint.second.geometryIndex)) {
                context.system.addConstraintPointOnHyperbolicArc(
                    *point,
                    *hyperbola,
                    nextTag(context),
                    constraint.driving
                );
                return true;
            }
            if (auto* parabola = arcOfParabolaFor(context, constraint.second.geometryIndex)) {
                context.system.addConstraintPointOnParabolicArc(
                    *point,
                    *parabola,
                    nextTag(context),
                    constraint.driving
                );
                return true;
            }
            if (auto* spline = bSplineFor(context, constraint.second.geometryIndex)) {
                auto* pointParam = context.ownParameter(estimateBSplineParameter(*spline, *point));
                context.parameters.push_back(pointParam);
                context.system.addConstraintPointOnBSpline(
                    *point,
                    *spline,
                    pointParam,
                    nextTag(context),
                    constraint.driving
                );
                return true;
            }
            return false;
        }
        case ConstraintKind::Horizontal: {
            if (constraint.second.geometryIndex < 0) {
                auto* line = lineFor(context, constraint.first.geometryIndex);
                if (!line) {
                    return false;
                }
                context.system.addConstraintHorizontal(*line, nextTag(context), constraint.driving);
                return true;
            }
            auto* p1 = pointFor(context, constraint.first);
            auto* p2 = pointFor(context, constraint.second);
            if (!p1 || !p2) {
                return false;
            }
            context.system.addConstraintHorizontal(*p1, *p2, nextTag(context), constraint.driving);
            return true;
        }
        case ConstraintKind::Vertical: {
            if (constraint.second.geometryIndex < 0) {
                auto* line = lineFor(context, constraint.first.geometryIndex);
                if (!line) {
                    return false;
                }
                context.system.addConstraintVertical(*line, nextTag(context), constraint.driving);
                return true;
            }
            auto* p1 = pointFor(context, constraint.first);
            auto* p2 = pointFor(context, constraint.second);
            if (!p1 || !p2) {
                return false;
            }
            context.system.addConstraintVertical(*p1, *p2, nextTag(context), constraint.driving);
            return true;
        }
        case ConstraintKind::DistanceX: {
            if (!constraint.hasValue) {
                return false;
            }
            auto* value = context.ownConstraintValue(constraint.value, constraint.driving);
            if (constraint.first.role == PointRole::None && constraint.second.geometryIndex < 0) {
                auto* line = lineFor(context, constraint.first.geometryIndex);
                if (!line) {
                    return false;
                }
                context.system.addConstraintDifference(line->p1.x, line->p2.x, value, nextTag(context), constraint.driving);
                return true;
            }
            if (constraint.second.geometryIndex < 0) {
                auto* point = pointFor(context, constraint.first);
                if (!point) {
                    return false;
                }
                context.system.addConstraintCoordinateX(*point, value, nextTag(context), constraint.driving);
                return true;
            }
            auto* p1 = pointFor(context, constraint.first);
            auto* p2 = pointFor(context, constraint.second);
            if (!p1 || !p2) {
                return false;
            }
            context.system.addConstraintDifference(p1->x, p2->x, value, nextTag(context), constraint.driving);
            return true;
        }
        case ConstraintKind::DistanceY: {
            if (!constraint.hasValue) {
                return false;
            }
            auto* value = context.ownConstraintValue(constraint.value, constraint.driving);
            if (constraint.first.role == PointRole::None && constraint.second.geometryIndex < 0) {
                auto* line = lineFor(context, constraint.first.geometryIndex);
                if (!line) {
                    return false;
                }
                context.system.addConstraintDifference(line->p1.y, line->p2.y, value, nextTag(context), constraint.driving);
                return true;
            }
            if (constraint.second.geometryIndex < 0) {
                auto* point = pointFor(context, constraint.first);
                if (!point) {
                    return false;
                }
                context.system.addConstraintCoordinateY(*point, value, nextTag(context), constraint.driving);
                return true;
            }
            auto* p1 = pointFor(context, constraint.first);
            auto* p2 = pointFor(context, constraint.second);
            if (!p1 || !p2) {
                return false;
            }
            context.system.addConstraintDifference(p1->y, p2->y, value, nextTag(context), constraint.driving);
            return true;
        }
        case ConstraintKind::Distance: {
            if (!constraint.hasValue) {
                return false;
            }
            auto* value = context.ownConstraintValue(constraint.value, constraint.driving);
            if (constraint.second.geometryIndex < 0) {
                if (constraint.first.role == PointRole::None) {
                    if (auto* line = lineFor(context, constraint.first.geometryIndex)) {
                        context.system.addConstraintP2PDistance(
                            line->p1,
                            line->p2,
                            value,
                            nextTag(context),
                            constraint.driving
                        );
                        return true;
                    }
                    if (auto* arc = arcFor(context, constraint.first.geometryIndex)) {
                        context.system.addConstraintArcLength(*arc, value, nextTag(context), constraint.driving);
                        return true;
                    }
                }
                return false;
            }
            if (constraint.second.role != PointRole::None) {
                auto* p1 = pointFor(context, constraint.first);
                auto* p2 = pointFor(context, constraint.second);
                if (!p1 || !p2) {
                    return false;
                }
                context.system.addConstraintP2PDistance(*p1, *p2, value, nextTag(context), constraint.driving);
                return true;
            }
            if (constraint.first.role != PointRole::None) {
                auto* point = pointFor(context, constraint.first);
                if (!point) {
                    return false;
                }
                if (auto* line = lineFor(context, constraint.second.geometryIndex)) {
                    context.system.addConstraintP2LDistance(
                        *point,
                        *line,
                        value,
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
                if (auto* circle = circleLikeFor(context, constraint.second.geometryIndex)) {
                    context.system.addConstraintP2CDistance(
                        *point,
                        *circle,
                        value,
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
                return false;
            }
            if (auto* firstCircle = circleLikeFor(context, constraint.first.geometryIndex)) {
                if (auto* secondLine = lineFor(context, constraint.second.geometryIndex)) {
                    context.system.addConstraintC2LDistance(
                        *firstCircle,
                        *secondLine,
                        value,
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
                if (auto* secondCircle = circleLikeFor(context, constraint.second.geometryIndex)) {
                    context.system.addConstraintC2CDistance(
                        *firstCircle,
                        *secondCircle,
                        value,
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
            }
            if (auto* firstLine = lineFor(context, constraint.first.geometryIndex)) {
                if (auto* secondCircle = circleLikeFor(context, constraint.second.geometryIndex)) {
                    context.system.addConstraintC2LDistance(
                        *secondCircle,
                        *firstLine,
                        value,
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
            }
            return false;
        }
        case ConstraintKind::Parallel: {
            auto* first = lineFor(context, constraint.first.geometryIndex);
            auto* second = lineFor(context, constraint.second.geometryIndex);
            if (!first || !second) {
                return false;
            }
            context.system.addConstraintParallel(*first, *second, nextTag(context), constraint.driving);
            return true;
        }
        case ConstraintKind::Tangent: {
            switch (addBSplineKnotTangentConstraint(context, constraint)) {
                case BSplineKnotTangentResult::Applied:
                    return true;
                case BSplineKnotTangentResult::Rejected:
                    return false;
                case BSplineKnotTangentResult::NotApplicable:
                    break;
            }
            if (usesPointwiseCurveAngleConstraint(constraint)) {
                return addPointwiseCurveAngleConstraint(context, constraint);
            }
            int firstGeometry = constraint.first.geometryIndex;
            int secondGeometry = constraint.second.geometryIndex;
            auto* secondBinding = bindingFor(context, secondGeometry);
            if (!secondBinding) {
                return false;
            }
            if (secondBinding->kind == GeometryKind::LineSegment) {
                if (auto* firstLine = lineFor(context, firstGeometry)) {
                    if (auto* secondLine = lineFor(context, secondGeometry)) {
                        context.system.addConstraintPointOnLine(
                            secondLine->p1,
                            *firstLine,
                            nextTag(context),
                            constraint.driving
                        );
                        context.system.addConstraintPointOnLine(
                            secondLine->p2,
                            *firstLine,
                            nextTag(context),
                            constraint.driving
                        );
                        return true;
                    }
                }
                std::swap(firstGeometry, secondGeometry);
            }

            if (auto* line = lineFor(context, firstGeometry)) {
                if (auto* arc = arcFor(context, secondGeometry)) {
                    context.system.addConstraintTangent(*line, *arc, nextTag(context), constraint.driving);
                    return true;
                }
                if (auto* circle = circleFor(context, secondGeometry)) {
                    context.system.addConstraintTangent(*line, *circle, nextTag(context), constraint.driving);
                    return true;
                }
                if (auto* ellipse = ellipseFor(context, secondGeometry)) {
                    context.system.addConstraintTangent(*line, *ellipse, nextTag(context), constraint.driving);
                    return true;
                }
                return false;
            }

            if (auto* circle = circleFor(context, firstGeometry)) {
                if (auto* secondCircle = circleFor(context, secondGeometry)) {
                    context.system.addConstraintTangent(*circle, *secondCircle, nextTag(context), constraint.driving);
                    return true;
                }
                if (auto* arc = arcFor(context, secondGeometry)) {
                    context.system.addConstraintTangent(*circle, *arc, nextTag(context), constraint.driving);
                    return true;
                }
                return false;
            }

            if (auto* firstArc = arcFor(context, firstGeometry)) {
                if (auto* circle = circleFor(context, secondGeometry)) {
                    context.system.addConstraintTangent(*circle, *firstArc, nextTag(context), constraint.driving);
                    return true;
                }
                if (auto* secondArc = arcFor(context, secondGeometry)) {
                    context.system.addConstraintTangent(*firstArc, *secondArc, nextTag(context), constraint.driving);
                    return true;
                }
            }

            return false;
        }
        case ConstraintKind::Perpendicular: {
            if (usesPointwiseCurveAngleConstraint(constraint)) {
                return addPointwiseCurveAngleConstraint(context, constraint);
            }
            int firstGeometry = constraint.first.geometryIndex;
            int secondGeometry = constraint.second.geometryIndex;
            auto* secondBinding = bindingFor(context, secondGeometry);
            if (secondBinding && secondBinding->kind == GeometryKind::LineSegment) {
                if (auto* firstLine = lineFor(context, firstGeometry)) {
                    if (auto* secondLine = lineFor(context, secondGeometry)) {
                        context.system.addConstraintPerpendicular(
                            *firstLine,
                            *secondLine,
                            nextTag(context),
                            constraint.driving
                        );
                        return true;
                    }
                }
                std::swap(firstGeometry, secondGeometry);
            }
            auto* line = lineFor(context, firstGeometry);
            auto* otherBinding = bindingFor(context, secondGeometry);
            if (!line || !otherBinding) {
                return false;
            }
            if (otherBinding->kind == GeometryKind::Circle || otherBinding->kind == GeometryKind::Arc) {
                auto* center = pointFor(context, {.geometryIndex = secondGeometry, .role = PointRole::Center});
                if (!center) {
                    return false;
                }
                context.system.addConstraintPointOnLine(*center, *line, nextTag(context), constraint.driving);
                return true;
            }
            return false;
        }
        case ConstraintKind::Angle: {
            if (!constraint.hasValue) {
                return false;
            }
            if (constraint.third.geometryIndex >= 0) {
                return addPointwiseCurveAngleConstraint(context, constraint);
            }
            auto* value = context.ownConstraintValue(constraint.value, constraint.driving);
            if (constraint.second.geometryIndex < 0) {
                if (auto* line = lineFor(context, constraint.first.geometryIndex)) {
                    context.system.addConstraintP2PAngle(
                        line->p1,
                        line->p2,
                        value,
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
                if (auto* arc = arcFor(context, constraint.first.geometryIndex)) {
                    context.system.addConstraintL2LAngle(
                        arc->center,
                        arc->start,
                        arc->center,
                        arc->end,
                        value,
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
                return false;
            }
            if (constraint.second.role != PointRole::None) {
                auto* firstStart = pointFor(context, constraint.first);
                auto* secondStart = pointFor(context, constraint.second);
                if (!firstStart || !secondStart) {
                    return false;
                }
                auto* firstEnd = pointFor(context, {.geometryIndex = constraint.first.geometryIndex, .role = constraint.first.role == PointRole::Start ? PointRole::End : PointRole::Start});
                auto* secondEnd = pointFor(context, {.geometryIndex = constraint.second.geometryIndex, .role = constraint.second.role == PointRole::Start ? PointRole::End : PointRole::Start});
                if (!firstEnd || !secondEnd) {
                    return false;
                }
                context.system.addConstraintL2LAngle(
                    *firstStart,
                    *firstEnd,
                    *secondStart,
                    *secondEnd,
                    value,
                    nextTag(context),
                    constraint.driving
                );
                return true;
            }
            auto* firstLine = lineFor(context, constraint.first.geometryIndex);
            auto* secondLine = lineFor(context, constraint.second.geometryIndex);
            if (!firstLine || !secondLine) {
                return false;
            }
            context.system.addConstraintL2LAngle(*firstLine, *secondLine, value, nextTag(context), constraint.driving);
            return true;
        }
        case ConstraintKind::Radius: {
            if (!constraint.hasValue) {
                return false;
            }
            auto* value = context.ownConstraintValue(constraint.value, constraint.driving);
            if (auto* circle = circleFor(context, constraint.first.geometryIndex)) {
                context.system.addConstraintCircleRadius(*circle, value, nextTag(context), constraint.driving);
                return true;
            }
            if (auto* arc = arcFor(context, constraint.first.geometryIndex)) {
                context.system.addConstraintArcRadius(*arc, value, nextTag(context), constraint.driving);
                return true;
            }
            return false;
        }
        case ConstraintKind::Diameter: {
            if (!constraint.hasValue) {
                return false;
            }
            auto* value = context.ownConstraintValue(constraint.value, constraint.driving);
            if (auto* circle = circleFor(context, constraint.first.geometryIndex)) {
                context.system.addConstraintCircleDiameter(*circle, value, nextTag(context), constraint.driving);
                return true;
            }
            if (auto* arc = arcFor(context, constraint.first.geometryIndex)) {
                context.system.addConstraintArcDiameter(*arc, value, nextTag(context), constraint.driving);
                return true;
            }
            return false;
        }
        case ConstraintKind::Equal: {
            int firstGeometry = constraint.first.geometryIndex;
            int secondGeometry = constraint.second.geometryIndex;
            auto* secondBinding = bindingFor(context, secondGeometry);
            if (!secondBinding) {
                return false;
            }
            if (secondBinding->kind == GeometryKind::Circle) {
                if (auto* firstCircle = circleFor(context, firstGeometry)) {
                    auto* secondCircle = circleFor(context, secondGeometry);
                    if (!secondCircle) {
                        return false;
                    }
                    context.system.addConstraintEqualRadius(*firstCircle, *secondCircle, nextTag(context), constraint.driving);
                    return true;
                }
                std::swap(firstGeometry, secondGeometry);
            }
            if (secondBinding->kind == GeometryKind::Ellipse
                || secondBinding->kind == GeometryKind::ArcOfEllipse) {
                if (auto* firstEllipse = ellipseFor(context, firstGeometry)) {
                    auto* secondEllipse = ellipseFor(context, secondGeometry);
                    if (!secondEllipse) {
                        return false;
                    }
                    context.system.addConstraintEqualRadii(
                        *firstEllipse, *secondEllipse, nextTag(context), constraint.driving);
                    return true;
                }
                std::swap(firstGeometry, secondGeometry);
            }
            if (auto* firstLine = lineFor(context, firstGeometry)) {
                auto* secondLine = lineFor(context, secondGeometry);
                if (!secondLine) {
                    return false;
                }
                context.system.addConstraintEqualLength(*firstLine, *secondLine, nextTag(context), constraint.driving);
                return true;
            }
            if (auto* firstCircle = circleFor(context, firstGeometry)) {
                if (auto* secondArc = arcFor(context, secondGeometry)) {
                    context.system.addConstraintEqualRadius(*firstCircle, *secondArc, nextTag(context), constraint.driving);
                    return true;
                }
            }
            if (auto* firstArc = arcFor(context, firstGeometry)) {
                if (auto* secondArc = arcFor(context, secondGeometry)) {
                    context.system.addConstraintEqualRadius(*firstArc, *secondArc, nextTag(context), constraint.driving);
                    return true;
                }
            }
            if (auto* firstEllipse = ellipseFor(context, firstGeometry)) {
                if (auto* secondEllipse = ellipseFor(context, secondGeometry)) {
                    context.system.addConstraintEqualRadii(
                        *firstEllipse,
                        *secondEllipse,
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
            }
            if (auto* firstHyperbola = arcOfHyperbolaFor(context, firstGeometry)) {
                if (auto* secondHyperbola = arcOfHyperbolaFor(context, secondGeometry)) {
                    context.system.addConstraintEqualRadii(
                        *firstHyperbola,
                        *secondHyperbola,
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
            }
            if (auto* firstParabola = arcOfParabolaFor(context, firstGeometry)) {
                if (auto* secondParabola = arcOfParabolaFor(context, secondGeometry)) {
                    context.system.addConstraintEqualFocus(
                        *firstParabola,
                        *secondParabola,
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
            }
            return false;
        }
        case ConstraintKind::Symmetric: {
            auto* p1 = pointFor(context, constraint.first);
            auto* p2 = pointFor(context, constraint.second);
            if (!p1 || !p2) {
                return false;
            }

            if (constraint.third.role != PointRole::None) {
                auto* center = pointFor(context, constraint.third);
                if (!center) {
                    return false;
                }
                context.system.addConstraintP2PSymmetric(*p1, *p2, *center, nextTag(context), constraint.driving);
                return true;
            }

            auto* line = lineFor(context, constraint.third.geometryIndex);
            if (!line) {
                return false;
            }

            // Degenerate case detection (aligned with FreeCAD):
            // When the two symmetric points are endpoints of the same arc AND
            // the arc's center lies on the symmetry line, the perpendicular
            // part of the P2PSymmetric constraint is redundant. We weaken to
            // a midpoint-on-line constraint to avoid solver errors.
            const auto p1Id = pointIdFor(context, constraint.first);
            const auto p2Id = pointIdFor(context, constraint.second);
            if (p1Id && p2Id) {
                for (const auto& binding : context.geometries) {
                    if (binding.kind != GeometryKind::Arc) {
                        continue;
                    }
                    if ((*p1Id == binding.startPointId && *p2Id == binding.endPointId)
                        || (*p1Id == binding.endPointId && *p2Id == binding.startPointId)) {
                        if (binding.midPointId >= 0
                            && binding.midPointId < static_cast<int>(context.points.size())) {
                            const GCS::Point& center = context.points[binding.midPointId];
                            const double dx = *line->p2.x - *line->p1.x;
                            const double dy = *line->p2.y - *line->p1.y;
                            const double lineLenSq = dx * dx + dy * dy;
                            if (lineLenSq > 1e-20) {
                                const double area = (*center.x - *line->p1.x) * dy
                                                  - (*center.y - *line->p1.y) * dx;
                                if (std::abs(area) / std::sqrt(lineLenSq) < 1e-10) {
                                    context.system.addConstraintMidpointOnLine(
                                        *p1, *p2, line->p1, line->p2,
                                        nextTag(context), constraint.driving
                                    );
                                    return true;
                                }
                            }
                        }
                        break;
                    }
                }
            }

            context.system.addConstraintP2PSymmetric(*p1, *p2, *line, nextTag(context), constraint.driving);
            return true;
        }
        case ConstraintKind::InternalAlignment: {
            switch (constraint.alignmentType) {
                case InternalAlignmentType::EllipseMajorDiameter: {
                    auto* ellipse = ellipseFor(context, constraint.second.geometryIndex);
                    auto* line = lineFor(context, constraint.first.geometryIndex);
                    if (!ellipse || !line) {
                        return false;
                    }
                    context.system.addConstraintInternalAlignmentEllipseMajorDiameter(
                        *ellipse,
                        line->p1,
                        line->p2,
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
                case InternalAlignmentType::EllipseMinorDiameter: {
                    auto* ellipse = ellipseFor(context, constraint.second.geometryIndex);
                    auto* line = lineFor(context, constraint.first.geometryIndex);
                    if (!ellipse || !line) {
                        return false;
                    }
                    context.system.addConstraintInternalAlignmentEllipseMinorDiameter(
                        *ellipse,
                        line->p1,
                        line->p2,
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
                case InternalAlignmentType::EllipseFocus1: {
                    auto* ellipse = ellipseFor(context, constraint.second.geometryIndex);
                    auto* point = pointFor(context, constraint.first);
                    if (!ellipse || !point) {
                        return false;
                    }
                    context.system.addConstraintInternalAlignmentEllipseFocus1(
                        *ellipse,
                        *point,
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
                case InternalAlignmentType::EllipseFocus2: {
                    auto* ellipse = ellipseFor(context, constraint.second.geometryIndex);
                    auto* point = pointFor(context, constraint.first);
                    if (!ellipse || !point) {
                        return false;
                    }
                    context.system.addConstraintInternalAlignmentEllipseFocus2(
                        *ellipse,
                        *point,
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
                case InternalAlignmentType::HyperbolaMajor: {
                    auto* hyperbola = hyperbolaFor(context, constraint.second.geometryIndex);
                    auto* line = lineFor(context, constraint.first.geometryIndex);
                    if (!hyperbola || !line) {
                        return false;
                    }
                    context.system.addConstraintInternalAlignmentHyperbolaMajorDiameter(
                        *hyperbola,
                        line->p1,
                        line->p2,
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
                case InternalAlignmentType::HyperbolaMinor: {
                    auto* hyperbola = hyperbolaFor(context, constraint.second.geometryIndex);
                    auto* line = lineFor(context, constraint.first.geometryIndex);
                    if (!hyperbola || !line) {
                        return false;
                    }
                    context.system.addConstraintInternalAlignmentHyperbolaMinorDiameter(
                        *hyperbola,
                        line->p1,
                        line->p2,
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
                case InternalAlignmentType::HyperbolaFocus: {
                    auto* hyperbola = hyperbolaFor(context, constraint.second.geometryIndex);
                    auto* point = pointFor(context, constraint.first);
                    if (!hyperbola || !point) {
                        return false;
                    }
                    context.system.addConstraintInternalAlignmentHyperbolaFocus(
                        *hyperbola,
                        *point,
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
                case InternalAlignmentType::ParabolaFocus: {
                    auto* parabola = parabolaFor(context, constraint.second.geometryIndex);
                    auto* point = pointFor(context, constraint.first);
                    if (!parabola || !point) {
                        return false;
                    }
                    context.system.addConstraintInternalAlignmentParabolaFocus(
                        *parabola,
                        *point,
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
                case InternalAlignmentType::ParabolaFocalAxis: {
                    auto* parabola = arcOfParabolaFor(context, constraint.second.geometryIndex);
                    auto* line = lineFor(context, constraint.first.geometryIndex);
                    if (!parabola || !line) {
                        return false;
                    }
                    const int tag = nextTag(context);
                    context.system.addConstraintP2PCoincident(line->p1, parabola->vertex, tag, constraint.driving);
                    context.system.addConstraintP2PCoincident(line->p2, parabola->focus1, tag, constraint.driving);
                    return true;
                }
                case InternalAlignmentType::BSplineControlPoint: {
                    auto* spline = bSplineFor(context, constraint.second.geometryIndex);
                    auto* circle = circleFor(context, constraint.first.geometryIndex);
                    if (!spline || !circle || constraint.internalAlignmentIndex < 0) {
                        return false;
                    }
                    const auto poleIndex = static_cast<std::size_t>(constraint.internalAlignmentIndex);
                    if (poleIndex >= spline->poles.size() || poleIndex >= spline->weights.size()) {
                        return false;
                    }
                    context.system.addConstraintInternalAlignmentBSplineControlPoint(
                        *spline,
                        *circle,
                        static_cast<unsigned int>(poleIndex),
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
                case InternalAlignmentType::BSplineKnotPoint: {
                    auto* spline = bSplineFor(context, constraint.second.geometryIndex);
                    auto* point = pointFor(context, constraint.first);
                    if (!spline || !point || constraint.internalAlignmentIndex < 0) {
                        return false;
                    }
                    const auto knotIndex = static_cast<std::size_t>(constraint.internalAlignmentIndex);
                    if (knotIndex >= spline->knots.size() || knotIndex >= spline->mult.size()) {
                        return false;
                    }
                    // For periodic B-splines the trailing duplicate closing knot is the
                    // same as knot 0 (FreeCAD GUI "we do not need the last pole/knot").
                    // Adding it would (a) walk the planegcs flattened-knot indexing past
                    // the end and crash in Debug, and (b) duplicate the index-0 alignment.
                    // Treat it as a no-op at the upper layer so planegcs sees the same
                    // constraint set FreeCAD's GUI flow would produce.
                    if (spline->periodic && knotIndex + 1 == spline->knots.size()) {
                        return true;
                    }
                    context.system.addConstraintInternalAlignmentKnotPoint(
                        *spline,
                        *point,
                        static_cast<unsigned int>(knotIndex),
                        nextTag(context),
                        constraint.driving
                    );
                    return true;
                }
                case InternalAlignmentType::Undef:
                    return false;
            }
            return false;
        }
        case ConstraintKind::SnellsLaw: {
            if (!constraint.hasValue) {
                return false;
            }
            auto* ray1 = curveFor(context, constraint.first.geometryIndex);
            auto* ray2 = curveFor(context, constraint.second.geometryIndex);
            auto* boundary = curveFor(context, constraint.third.geometryIndex);
            auto* point1 = pointFor(context, constraint.first);
            auto* point2 = pointFor(context, constraint.second);
            if (!ray1 || !ray2 || !boundary || !point1 || !point2) {
                return false;
            }

            auto* n1 = context.ownConstraintValue(constraint.value, constraint.driving);
            auto* n2 = context.ownConstraintValue(constraint.value, constraint.driving);
            const double ratio = constraint.value;
            if (std::abs(ratio) <= 1e-12) {
                return false;
            }
            if (std::abs(ratio) >= 1.0) {
                *n2 = ratio;
                *n1 = 1.0;
            }
            else {
                *n2 = 1.0;
                *n1 = 1.0 / ratio;
            }

            context.system.addConstraintSnellsLaw(
                *ray1,
                *ray2,
                *boundary,
                *point1,
                n1,
                n2,
                constraint.first.role == PointRole::Start,
                constraint.second.role == PointRole::End,
                nextTag(context),
                constraint.driving
            );
            return true;
        }
        case ConstraintKind::Block:
            return true;
        case ConstraintKind::Weight: {
            if (!constraint.hasValue) {
                return false;
            }
            auto* value = context.ownConstraintValue(constraint.value, constraint.driving);
            if (auto* circle = circleFor(context, constraint.first.geometryIndex)) {
                context.system.addConstraintCircleRadius(*circle, value, nextTag(context), constraint.driving);
                return true;
            }
            return false;
        }
    }

    return false;
}

void updateSolvedGeometry(SketchModel& model, const SolveContext& context)
{
    for (std::size_t i = 0; i < model.geometries().size(); ++i) {
        auto& geometry = model.geometries()[i];
        const auto& binding = context.geometries[i];
        switch (binding.kind) {
            case GeometryKind::Point: {
                auto& point = std::get<PointGeometry>(geometry.data);
                const auto& gcsPoint = context.points[binding.startPointId];
                point.point.x = *gcsPoint.x;
                point.point.y = *gcsPoint.y;
                break;
            }
            case GeometryKind::LineSegment: {
                auto& line = std::get<LineSegmentGeometry>(geometry.data);
                const auto& gcsLine = context.lines[binding.index];
                line.start.x = *gcsLine.p1.x;
                line.start.y = *gcsLine.p1.y;
                line.end.x = *gcsLine.p2.x;
                line.end.y = *gcsLine.p2.y;
                break;
            }
            case GeometryKind::Circle: {
                auto& circle = std::get<CircleGeometry>(geometry.data);
                const auto& gcsCircle = context.circles[binding.index];
                circle.center.x = *gcsCircle.center.x;
                circle.center.y = *gcsCircle.center.y;
                circle.radius = *gcsCircle.rad;
                break;
            }
            case GeometryKind::Arc: {
                auto& arc = std::get<ArcGeometry>(geometry.data);
                const auto& gcsArc = context.arcs[binding.index];
                arc.center.x = *gcsArc.center.x;
                arc.center.y = *gcsArc.center.y;
                arc.radius = *gcsArc.rad;
                arc.startAngle = *gcsArc.startAngle;
                arc.endAngle = *gcsArc.endAngle;
                break;
            }
            case GeometryKind::Ellipse: {
                auto& ellipse = std::get<EllipseGeometry>(geometry.data);
                const auto& gcsEllipse = context.ellipses[binding.index];
                ellipse.center.x = *gcsEllipse.center.x;
                ellipse.center.y = *gcsEllipse.center.y;
                ellipse.focus1.x = *gcsEllipse.focus1.x;
                ellipse.focus1.y = *gcsEllipse.focus1.y;
                ellipse.minorRadius = *gcsEllipse.radmin;
                break;
            }
            case GeometryKind::ArcOfEllipse: {
                auto& arc = std::get<ArcOfEllipseGeometry>(geometry.data);
                const auto& gcsArc = context.arcOfEllipses[binding.index];
                arc.center.x = *gcsArc.center.x;
                arc.center.y = *gcsArc.center.y;
                arc.focus1.x = *gcsArc.focus1.x;
                arc.focus1.y = *gcsArc.focus1.y;
                arc.minorRadius = *gcsArc.radmin;
                arc.startAngle = *gcsArc.startAngle;
                arc.endAngle = *gcsArc.endAngle;
                break;
            }
            case GeometryKind::ArcOfHyperbola: {
                auto& arc = std::get<ArcOfHyperbolaGeometry>(geometry.data);
                const auto& gcsArc = context.arcOfHyperbolas[binding.index];
                arc.center.x = *gcsArc.center.x;
                arc.center.y = *gcsArc.center.y;
                arc.focus1.x = *gcsArc.focus1.x;
                arc.focus1.y = *gcsArc.focus1.y;
                arc.minorRadius = *gcsArc.radmin;
                arc.startAngle = *gcsArc.startAngle;
                arc.endAngle = *gcsArc.endAngle;
                break;
            }
            case GeometryKind::ArcOfParabola: {
                auto& arc = std::get<ArcOfParabolaGeometry>(geometry.data);
                const auto& gcsArc = context.arcOfParabolas[binding.index];
                arc.vertex.x = *gcsArc.vertex.x;
                arc.vertex.y = *gcsArc.vertex.y;
                arc.focus1.x = *gcsArc.focus1.x;
                arc.focus1.y = *gcsArc.focus1.y;
                arc.startAngle = *gcsArc.startAngle;
                arc.endAngle = *gcsArc.endAngle;
                break;
            }
            case GeometryKind::BSpline: {
                auto& spline = std::get<BSplineGeometry>(geometry.data);
                const auto& gcsSpline = context.bSplines[binding.index];
                for (std::size_t poleIndex = 0; poleIndex < spline.poles.size()
                                                 && poleIndex < gcsSpline.poles.size();
                     ++poleIndex) {
                    spline.poles[poleIndex].point.x = *gcsSpline.poles[poleIndex].x;
                    spline.poles[poleIndex].point.y = *gcsSpline.poles[poleIndex].y;
                    spline.poles[poleIndex].weight = *gcsSpline.weights[poleIndex];
                }
                // Knot write-back disabled: knots are not solver parameters
                // (aligned with FreeCAD, which has the same code commented out
                // with note "when/if b-spline gets its full implementation in the solver").
                break;
            }
        }
    }
}

SolveStatus fromGcsStatus(int status)
{
    switch (status) {
        case GCS::Success:
            return SolveStatus::Success;
        case GCS::Converged:
            return SolveStatus::Converged;
        case GCS::SuccessfulSolutionInvalid:
            return SolveStatus::Invalid;
        case GCS::Failed:
        default:
            return SolveStatus::Failed;
    }
}

}  // namespace

SolveResult solveSketch(SketchModel& model)
{
    return solveSketch(model, McSolverEngine::ParameterMap {});
}

SolveResult solveSketch(SketchModel& model, const McSolverEngine::ParameterMap& parameters)
{
    SolveContext context;
    ParsedApiParameterMap parsedParameters;
    if (!McSolverEngine::Detail::tryParseApiParameters(parameters, parsedParameters)) {
        return {.status = SolveStatus::Invalid};
    }

    indexBSplineKnotAlignments(context, model);
    const auto blockAnalysis = analyzeBlockConstraints(model);
    context.blockedGeometry = blockAnalysis.onlyBlockedGeometry;
    for (std::size_t geometryIndex = 0; geometryIndex < model.geometryCount(); ++geometryIndex) {
        if (blockAnalysis.blockConstraintIndexByGeometry[geometryIndex] < 0 && model.geometries()[geometryIndex].blocked) {
            context.blockedGeometry[geometryIndex] = true;
        }
    }

    for (std::size_t geometryIndex = 0; geometryIndex < model.geometryCount(); ++geometryIndex) {
        if (!addGeometry(
                context,
                model.geometries()[geometryIndex],
                static_cast<int>(geometryIndex)
            )) {
            return {.status = SolveStatus::Unsupported};
        }
    }

    for (std::size_t constraintIndex = 0; constraintIndex < model.constraintCount(); ++constraintIndex) {
        const auto& constraint = model.constraints()[constraintIndex];
        if (!isPhase1ConstraintSupported(constraint.kind)) {
            return {.status = SolveStatus::Unsupported};
        }
        if (constraint.kind == ConstraintKind::Block || blockAnalysis.unenforceableConstraints[constraintIndex]) {
            continue;
        }

        Constraint effectiveConstraint = constraint;
        const auto resolvedValue = resolveConstraintValue(constraint, parsedParameters);
        if (resolvedValue) {
            effectiveConstraint.value = *resolvedValue;
        }

        if (!addConstraint(context, effectiveConstraint)) {
            return {.status = SolveStatus::Unsupported};
        }
    }

    context.system.declareUnknowns(context.parameters);
    context.system.declareDrivenParams(context.drivenParameters);
    context.system.initSolution(GCS::DogLeg);

    if (blockAnalysis.doesBlockAffectOtherConstraints) {
        std::vector<double*> paramsToBlock;
        bool unsatisfiedGroups =
            analyseBlockedConstraintDependentParameters(context, blockAnalysis.blockedGeoIds, paramsToBlock);
        while (unsatisfiedGroups) {
            fixParametersAndDiagnose(context, paramsToBlock);
            unsatisfiedGroups =
                analyseBlockedConstraintDependentParameters(context, blockAnalysis.blockedGeoIds, paramsToBlock);
        }
        fixParametersAndDiagnose(context, paramsToBlock);
    }

    SolveResult result;
    int gcsStatus = context.system.solve(true, GCS::DogLeg);

    if (gcsStatus != GCS::Success) {
        gcsStatus = context.system.solve(true, GCS::LevenbergMarquardt);
    }
    if (gcsStatus != GCS::Success) {
        gcsStatus = context.system.solve(true, GCS::BFGS);
    }
    if (gcsStatus != GCS::Success) {
        for (auto* p : context.parameters) {
            double* initParam = context.ownParameter(*p);
            context.system.addConstraintEqual(
                p, initParam, GCS::DefaultTemporaryConstraint);
        }

        context.system.initSolution();
        gcsStatus = context.system.solve(true);
        context.system.clearByTag(GCS::DefaultTemporaryConstraint);
    }

    result.status = fromGcsStatus(gcsStatus);
    result.degreesOfFreedom = context.system.dofsNumber();
    context.system.getConflicting(result.conflicting);
    context.system.getRedundant(result.redundant);
    context.system.getPartiallyRedundant(result.partiallyRedundant);

    if (result.status == SolveStatus::Success) {
        context.system.applySolution();
        updateSolvedGeometry(model, context);
    }

    return result;
}

}  // namespace McSolverEngine::Compat
