#pragma once

#include "McSolverEngine/Export.h"

#if defined(_WIN32)
#    if defined(MCSOLVERENGINE_CAPI_BUILD)
#        define MCSOLVERENGINE_CAPI_EXPORT __declspec(dllexport)
#    else
#        define MCSOLVERENGINE_CAPI_EXPORT __declspec(dllimport)
#    endif
#else
#    define MCSOLVERENGINE_CAPI_EXPORT
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum McSolverEngineResultCode
{
    MCSOLVERENGINE_RESULT_SUCCESS = 0,
    MCSOLVERENGINE_RESULT_INVALID_ARGUMENT = 1,
    MCSOLVERENGINE_RESULT_IMPORT_FAILED = 2,
    MCSOLVERENGINE_RESULT_SOLVE_FAILED = 3,
    MCSOLVERENGINE_RESULT_UNSUPPORTED = 4,
    MCSOLVERENGINE_RESULT_GEOMETRY_EXPORT_FAILED = 5,
    MCSOLVERENGINE_RESULT_BREP_EXPORT_FAILED = 6,
    MCSOLVERENGINE_RESULT_OPEN_CASCADE_UNAVAILABLE = 7,
    MCSOLVERENGINE_RESULT_OUT_OF_MEMORY = 8,
    MCSOLVERENGINE_RESULT_VARSET_EXPRESSION_UNSUPPORTED_SUBSET = 9
} McSolverEngineResultCode;

typedef struct McSolverEnginePoint2
{
    double x;
    double y;
} McSolverEnginePoint2;

typedef struct McSolverEnginePlacement
{
    double px;
    double py;
    double pz;
    double qx;
    double qy;
    double qz;
    double qw;
} McSolverEnginePlacement;

typedef struct McSolverEngineBSplinePole
{
    McSolverEnginePoint2 point;
    double weight;
} McSolverEngineBSplinePole;

typedef struct McSolverEngineBSplineKnot
{
    double value;
    int multiplicity;
} McSolverEngineBSplineKnot;

typedef enum McSolverEngineGeometryKind
{
    MCSOLVERENGINE_GEOMETRY_POINT = 0,
    MCSOLVERENGINE_GEOMETRY_LINE_SEGMENT = 1,
    MCSOLVERENGINE_GEOMETRY_CIRCLE = 2,
    MCSOLVERENGINE_GEOMETRY_ARC = 3,
    MCSOLVERENGINE_GEOMETRY_ELLIPSE = 4,
    MCSOLVERENGINE_GEOMETRY_ARC_OF_ELLIPSE = 5,
    MCSOLVERENGINE_GEOMETRY_ARC_OF_HYPERBOLA = 6,
    MCSOLVERENGINE_GEOMETRY_ARC_OF_PARABOLA = 7,
    MCSOLVERENGINE_GEOMETRY_BSPLINE = 8
} McSolverEngineGeometryKind;

typedef enum McSolverEngineConstraintKind
{
    MCSOLVERENGINE_CONSTRAINT_COINCIDENT = 0,
    MCSOLVERENGINE_CONSTRAINT_HORIZONTAL = 1,
    MCSOLVERENGINE_CONSTRAINT_VERTICAL = 2,
    MCSOLVERENGINE_CONSTRAINT_DISTANCE_X = 3,
    MCSOLVERENGINE_CONSTRAINT_DISTANCE_Y = 4,
    MCSOLVERENGINE_CONSTRAINT_DISTANCE = 5,
    MCSOLVERENGINE_CONSTRAINT_PARALLEL = 6,
    MCSOLVERENGINE_CONSTRAINT_TANGENT = 7,
    MCSOLVERENGINE_CONSTRAINT_PERPENDICULAR = 8,
    MCSOLVERENGINE_CONSTRAINT_ANGLE = 9,
    MCSOLVERENGINE_CONSTRAINT_RADIUS = 10,
    MCSOLVERENGINE_CONSTRAINT_DIAMETER = 11,
    MCSOLVERENGINE_CONSTRAINT_EQUAL = 12,
    MCSOLVERENGINE_CONSTRAINT_SYMMETRIC = 13,
    MCSOLVERENGINE_CONSTRAINT_POINT_ON_OBJECT = 14,
    MCSOLVERENGINE_CONSTRAINT_INTERNAL_ALIGNMENT = 15,
    MCSOLVERENGINE_CONSTRAINT_SNELLS_LAW = 16,
    MCSOLVERENGINE_CONSTRAINT_BLOCK = 17,
    MCSOLVERENGINE_CONSTRAINT_WEIGHT = 18
} McSolverEngineConstraintKind;

typedef struct McSolverEngineConstraintRef
{
    McSolverEngineConstraintKind kind;
    int originalIndex;
    const char* expression;
} McSolverEngineConstraintRef;

typedef struct McSolverEngineGeometryRecord
{
    int geometryIndex;
    int originalId;
    McSolverEngineGeometryKind kind;
    int construction;
    int external;
    int blocked;
    McSolverEnginePoint2 point;
    McSolverEnginePoint2 start;
    McSolverEnginePoint2 end;
    McSolverEnginePoint2 center;
    McSolverEnginePoint2 focus1;
    McSolverEnginePoint2 vertex;
    double radius;
    double minorRadius;
    double startAngle;
    double endAngle;
    int degree;
    int periodic;
    int poleCount;
    const McSolverEngineBSplinePole* poles;
    int knotCount;
    const McSolverEngineBSplineKnot* knots;
    int constraintCount;
    const McSolverEngineConstraintRef* constraints;
} McSolverEngineGeometryRecord;

typedef struct McSolverEngineGeometryResult
{
    const char* sketchName;
    const char* importStatus;
    int skippedConstraints;
    int messageCount;
    char** messages;
    const char* solveStatus;
    int degreesOfFreedom;
    int conflictingCount;
    const int* conflicting;
    int redundantCount;
    const int* redundant;
    int partiallyRedundantCount;
    const int* partiallyRedundant;
    const char* exportKind;
    const char* exportStatus;
    McSolverEnginePlacement placement;
    int geometryCount;
    const McSolverEngineGeometryRecord* geometries;
} McSolverEngineGeometryResult;

typedef struct McSolverEngineBRepResult
{
    const char* sketchName;
    const char* importStatus;
    int skippedConstraints;
    int messageCount;
    char** messages;
    const char* solveStatus;
    int degreesOfFreedom;
    int conflictingCount;
    const int* conflicting;
    int redundantCount;
    const int* redundant;
    int partiallyRedundantCount;
    const int* partiallyRedundant;
    const char* exportKind;
    const char* exportStatus;
    McSolverEnginePlacement placement;
    const char* brepUtf8;
} McSolverEngineBRepResult;

MCSOLVERENGINE_CAPI_EXPORT const char* McSolverEngine_GetVersion(void);

MCSOLVERENGINE_CAPI_EXPORT McSolverEngineResultCode McSolverEngine_SolveToGeometry(
    const char* documentXmlUtf8,
    const char* sketchNameUtf8,
    McSolverEngineGeometryResult** result
);

MCSOLVERENGINE_CAPI_EXPORT McSolverEngineResultCode McSolverEngine_SolveToGeometryWithParameters(
    const char* documentXmlUtf8,
    const char* sketchNameUtf8,
    const char* const* parameterKeysUtf8,
    const char* const* parameterValuesUtf8,
    int parameterCount,
    McSolverEngineGeometryResult** result
);

MCSOLVERENGINE_CAPI_EXPORT McSolverEngineResultCode McSolverEngine_SolveToBRep(
    const char* documentXmlUtf8,
    const char* sketchNameUtf8,
    McSolverEngineBRepResult** result
);

MCSOLVERENGINE_CAPI_EXPORT McSolverEngineResultCode McSolverEngine_SolveToBRepWithParameters(
    const char* documentXmlUtf8,
    const char* sketchNameUtf8,
    const char* const* parameterKeysUtf8,
    const char* const* parameterValuesUtf8,
    int parameterCount,
    McSolverEngineBRepResult** result
);

MCSOLVERENGINE_CAPI_EXPORT void McSolverEngine_FreeGeometryResult(McSolverEngineGeometryResult* value);

MCSOLVERENGINE_CAPI_EXPORT void McSolverEngine_FreeBRepResult(McSolverEngineBRepResult* value);

#ifdef __cplusplus
}
#endif
