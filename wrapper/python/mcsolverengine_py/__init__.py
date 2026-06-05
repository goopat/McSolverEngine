"""McSolverEngine Python wrapper."""

from ._engine import (
    Engine,
    GeometryResult,
    BRepResult,
    GeometryRecord,
    Point2,
    Placement,
    BSplinePole,
    BSplineKnot,
    ConstraintRef,
    VarSetPropertyValue,
)

from ._svg import write_visible_geometry_svg

from ._bindings import (
    set_native_lib_path,
    RESULT_SUCCESS,
    RESULT_INVALID_ARGUMENT,
    RESULT_IMPORT_FAILED,
    RESULT_SOLVE_FAILED,
    RESULT_UNSUPPORTED,
    RESULT_GEOMETRY_EXPORT_FAILED,
    RESULT_BREP_EXPORT_FAILED,
    RESULT_OPEN_CASCADE_UNAVAILABLE,
    RESULT_OUT_OF_MEMORY,
    RESULT_VARSET_EXPRESSION_UNSUPPORTED_SUBSET,
    RESULT_VARSET_PARAMETER_VALIDATION_FAILED,
    RESULT_SKETCH_NOT_FOUND,
    GEOMETRY_POINT,
    GEOMETRY_LINE_SEGMENT,
    GEOMETRY_CIRCLE,
    GEOMETRY_ARC,
    GEOMETRY_ELLIPSE,
    GEOMETRY_ARC_OF_ELLIPSE,
    GEOMETRY_ARC_OF_HYPERBOLA,
    GEOMETRY_ARC_OF_PARABOLA,
    GEOMETRY_BSPLINE,
    result_code_name,
)

__all__ = [
    "Engine",
    "GeometryResult",
    "BRepResult",
    "GeometryRecord",
    "Point2",
    "Placement",
    "BSplinePole",
    "BSplineKnot",
    "ConstraintRef",
    "VarSetPropertyValue",
    "write_visible_geometry_svg",
    "set_native_lib_path",
    "RESULT_SUCCESS",
    "RESULT_OPEN_CASCADE_UNAVAILABLE",
    "GEOMETRY_POINT",
    "GEOMETRY_LINE_SEGMENT",
    "GEOMETRY_CIRCLE",
    "GEOMETRY_BSPLINE",
    "result_code_name",
]
